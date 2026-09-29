/*
 * Rufus: The Reliable USB Formatting Utility
 * Linux device enumeration using libudev and libblkid
 * Copyright © 2024 Pete Batard <pete@akeo.ie>
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libudev.h>
#include <blkid/blkid.h>

#include "core/platform.h"
#include "platform/linux/platform_linux.h"

/* Forward declarations */
int linux_init(void);
void linux_cleanup(void);
int linux_enumerate_devices(PlatformDeviceInfo* devices, int max_devices, int* count);
int linux_get_device_info(const char* syspath, PlatformDeviceInfo* info);
int linux_open_device_ro(const char* devpath);
void linux_close_device(int fd);
const char* linux_get_platform_name(void);

/* Static udev context */
static struct udev* g_udev = NULL;
static blkid_cache g_blkid_cache = NULL;

/* Device type detection from udev properties */
static DeviceType detect_device_type(struct udev_device* dev) {
    const char* subsystem = udev_device_get_subsystem(dev);
    const char* id_bus = udev_device_get_property_value(dev, "ID_BUS");
    const char* devtype = udev_device_get_devtype(dev);
    const char* sysname = udev_device_get_sysname(dev);
    
    if (id_bus) {
        if (strcmp(id_bus, "usb") == 0) return DEVICE_TYPE_USB;
        if (strcmp(id_bus, "ata") == 0) return DEVICE_TYPE_SATA;
        if (strcmp(id_bus, "scsi") == 0) return DEVICE_TYPE_SCSI;
        if (strcmp(id_bus, "nvme") == 0) return DEVICE_TYPE_NVME;
    }
    
    if (subsystem && strcmp(subsystem, "nvme") == 0) return DEVICE_TYPE_NVME;
    
    if (devtype && strcmp(devtype, "disk") == 0) {
        if (sysname && strncmp(sysname, "loop", 4) == 0) return DEVICE_TYPE_VIRTUAL;
        if (sysname && strncmp(sysname, "ram", 3) == 0) return DEVICE_TYPE_VIRTUAL;
    }
    
    return DEVICE_TYPE_UNKNOWN;
}

/* Get partition table style via libblkid */
static PartitionStyle get_partition_style(const char* devpath) {
    blkid_probe pr = blkid_new_probe_from_filename(devpath);
    if (!pr) return PARTITION_STYLE_UNKNOWN;
    
    if (blkid_do_probe(pr) != 0) {
        blkid_free_probe(pr);
        return PARTITION_STYLE_UNKNOWN;
    }
    
    const char* ptype = NULL;
    if (blkid_probe_lookup_value(pr, "PTTYPE", &ptype, NULL) == 0) {
        blkid_free_probe(pr);
        if (strcmp(ptype, "gpt") == 0) return PARTITION_STYLE_GPT;
        if (strcmp(ptype, "dos") == 0 || strcmp(ptype, "mbr") == 0) return PARTITION_STYLE_MBR;
        return PARTITION_STYLE_UNKNOWN;
    }
    
    /* Check for raw filesystem (superfloppy) */
    const char* fstype = NULL;
    if (blkid_probe_lookup_value(pr, "TYPE", &fstype, NULL) == 0) {
        blkid_free_probe(pr);
        return PARTITION_STYLE_RAW;
    }
    
    blkid_free_probe(pr);
    return PARTITION_STYLE_UNKNOWN;
}

/* Probe partitions using libblkid */
static void probe_partitions(const char* devpath, PlatformDeviceInfo* info) {
    blkid_probe pr = blkid_new_probe_from_filename(devpath);
    if (!pr) return;
    
    if (blkid_do_probe(pr) != 0) {
        blkid_free_probe(pr);
        return;
    }
    
    blkid_partlist ls = blkid_probe_get_partitions(pr);
    if (!ls) {
        blkid_free_probe(pr);
        return;
    }
    
    int nparts = blkid_partlist_numof_partitions(ls);
    for (int i = 0; i < nparts && i < PLATFORM_MAX_PARTITIONS; i++) {
        blkid_partition part = blkid_partlist_get_partition(ls, i);
        if (!part) continue;
        
        char part_path[PLATFORM_MAX_PATH];
        const char* part_name = blkid_partition_get_name(part);
        if (!part_name) continue;
        
        snprintf(part_path, sizeof(part_path), "/dev/%s", part_name);
        
        int part_idx = info->partition_count++;
        info->partitions[part_idx].offset = (uint64_t)blkid_partition_get_start(part) * 512;  /* blkid uses 512-byte sectors */
        info->partitions[part_idx].size = (uint64_t)blkid_partition_get_size(part) * 512;
        info->partitions[part_idx].number = blkid_partition_get_partno(part);
        
        /* Check boot flag - bit 0 is typically the boot flag */
        unsigned long flags = blkid_partition_get_flags(part);
        info->partitions[part_idx].bootable = (flags & 0x1) ? true : false;
        
        /* Probe filesystem on partition */
        blkid_probe pr_part = blkid_new_probe_from_filename(part_path);
        if (pr_part) {
            if (blkid_do_probe(pr_part) == 0) {
                const char* fstype = NULL;
                const char* label = NULL;
                
                if (blkid_probe_lookup_value(pr_part, "TYPE", &fstype, NULL) == 0 && fstype) {
                    strncpy(info->partitions[part_idx].fstype_str, fstype, sizeof(info->partitions[part_idx].fstype_str) - 1);
                    
                    /* Map to our FSType enum */
                    if (strcasecmp(fstype, "vfat") == 0 || strcasecmp(fstype, "fat") == 0 ||
                        strcasecmp(fstype, "fat16") == 0) info->partitions[part_idx].fs_type = FS_TYPE_FAT16;
                    else if (strcasecmp(fstype, "fat32") == 0) info->partitions[part_idx].fs_type = FS_TYPE_FAT32;
                    else if (strcasecmp(fstype, "ntfs") == 0) info->partitions[part_idx].fs_type = FS_TYPE_NTFS;
                    else if (strcasecmp(fstype, "udf") == 0) info->partitions[part_idx].fs_type = FS_TYPE_UDF;
                    else if (strcasecmp(fstype, "exfat") == 0) info->partitions[part_idx].fs_type = FS_TYPE_EXFAT;
                    else if (strcasecmp(fstype, "ext2") == 0) info->partitions[part_idx].fs_type = FS_TYPE_EXT2;
                    else if (strcasecmp(fstype, "ext3") == 0) info->partitions[part_idx].fs_type = FS_TYPE_EXT3;
                    else if (strcasecmp(fstype, "ext4") == 0) info->partitions[part_idx].fs_type = FS_TYPE_EXT4;
                    else info->partitions[part_idx].fs_type = FS_TYPE_UNKNOWN;
                }
                
                if (blkid_probe_lookup_value(pr_part, "LABEL", &label, NULL) == 0 && label) {
                    strncpy(info->partitions[part_idx].label, label, sizeof(info->partitions[part_idx].label) - 1);
                }
            }
            blkid_free_probe(pr_part);
        }
    }
    
    blkid_free_probe(pr);
}

/* Check if device should be listed (removable, USB, NVMe, or user-enabled HDD) */
static bool should_list_device(struct udev_device* dev) {
    const char* removable = udev_device_get_sysattr_value(dev, "removable");
    const char* id_bus = udev_device_get_property_value(dev, "ID_BUS");
    const char* devtype = udev_device_get_devtype(dev);
    const char* devpath = udev_device_get_devpath(dev);
    
    /* Must be a disk device */
    if (!devtype || strcmp(devtype, "disk") != 0) return false;
    
    /* Removable devices are always listed */
    if (removable && strcmp(removable, "1") == 0) return true;
    
    /* USB devices are always listed */
    if (id_bus && strcmp(id_bus, "usb") == 0) return true;
    
    /* NVMe devices are listed - check devpath for /nvme/ or ID_BUS for nvme */
    if (devpath && strstr(devpath, "/nvme/")) return true;
    if (id_bus && strcmp(id_bus, "nvme") == 0) return true;
    
    /* For now, don't list fixed SATA/SCSI drives by default */
    /* This matches Rufus Windows behavior (requires "List USB Hard Drives" option) */
    return false;
}

/* Fill device info from udev device */
static int fill_device_info(struct udev_device* dev, PlatformDeviceInfo* info) {
    const char* syspath = udev_device_get_syspath(dev);
    const char* devnode = udev_device_get_devnode(dev);
    const char* sysname = udev_device_get_sysname(dev);
    const char* vendor = udev_device_get_property_value(dev, "ID_VENDOR");
    const char* model = udev_device_get_property_value(dev, "ID_MODEL");
    const char* serial = udev_device_get_property_value(dev, "ID_SERIAL_SHORT");
    const char* id_bus = udev_device_get_property_value(dev, "ID_BUS");
    const char* size_str = udev_device_get_sysattr_value(dev, "size");
    const char* ro_str = udev_device_get_sysattr_value(dev, "ro");
    const char* logical_bs_str = udev_device_get_sysattr_value(dev, "queue/logical_block_size");
    const char* physical_bs_str = udev_device_get_sysattr_value(dev, "queue/physical_block_size");
    const char* removable_str = udev_device_get_sysattr_value(dev, "removable");
    
    if (!syspath || !devnode || !sysname) return -1;
    
    memset(info, 0, sizeof(PlatformDeviceInfo));
    
    strncpy(info->syspath, syspath, sizeof(info->syspath) - 1);
    strncpy(info->devpath, devnode, sizeof(info->devpath) - 1);
    
    if (vendor) strncpy(info->vendor, vendor, sizeof(info->vendor) - 1);
    if (model) strncpy(info->model, model, sizeof(info->model) - 1);
    if (serial) strncpy(info->serial, serial, sizeof(info->serial) - 1);
    if (id_bus) strncpy(info->bus, id_bus, sizeof(info->bus) - 1);
    
    /* Construct display name */
    if (vendor && model) {
        snprintf(info->name, sizeof(info->name), "%s %s", vendor, model);
    } else if (model) {
        strncpy(info->name, model, sizeof(info->name) - 1);
    } else if (vendor) {
        strncpy(info->name, vendor, sizeof(info->name) - 1);
    } else {
        strncpy(info->name, sysname, sizeof(info->name) - 1);
    }
    
    /* Size in 512-byte sectors */
    if (size_str) {
        info->size = (uint64_t)atoll(size_str) * 512;
    }
    
    /* Sector sizes */
    info->logical_sector_size = logical_bs_str ? atoi(logical_bs_str) : 512;
    info->physical_sector_size = physical_bs_str ? atoi(physical_bs_str) : 512;
    
    /* Flags */
    info->removable = (removable_str && strcmp(removable_str, "1") == 0);
    info->read_only = (ro_str && strcmp(ro_str, "1") == 0);
    info->type = detect_device_type(dev);
    
    /* Partition style */
    info->partition_style = get_partition_style(info->devpath);
    
    /* Probe partitions */
    probe_partitions(info->devpath, info);
    
    return 0;
}

/* Platform operations implementation */
int linux_init(void) {
    g_udev = udev_new();
    if (!g_udev) {
        fprintf(stderr, "Failed to create udev context\n");
        return -1;
    }
    
    if (blkid_get_cache(&g_blkid_cache, NULL) != 0) {
        fprintf(stderr, "Warning: Failed to create blkid cache\n");
        g_blkid_cache = NULL;
    }
    
    return 0;
}

void linux_cleanup(void) {
    if (g_blkid_cache) {
        blkid_put_cache(g_blkid_cache);
        g_blkid_cache = NULL;
    }
    if (g_udev) {
        udev_unref(g_udev);
        g_udev = NULL;
    }
}

int linux_enumerate_devices(PlatformDeviceInfo* devices, int max_devices, int* count) {
    if (!g_udev) return -1;
    
    struct udev_enumerate* enumerate = udev_enumerate_new(g_udev);
    if (!enumerate) return -1;
    
    udev_enumerate_add_match_subsystem(enumerate, "block");
    udev_enumerate_add_match_property(enumerate, "DEVTYPE", "disk");
    udev_enumerate_scan_devices(enumerate);
    
    struct udev_list_entry* devices_list = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry* entry;
    int n = 0;
    
    udev_list_entry_foreach(entry, devices_list) {
        if (n >= max_devices) break;
        
        const char* syspath = udev_list_entry_get_name(entry);
        struct udev_device* dev = udev_device_new_from_syspath(g_udev, syspath);
        
        if (dev && should_list_device(dev)) {
            if (fill_device_info(dev, &devices[n]) == 0) {
                n++;
            }
        }
        
        if (dev) udev_device_unref(dev);
    }
    
    *count = n;
    udev_enumerate_unref(enumerate);
    return 0;
}

int linux_get_device_info(const char* syspath, PlatformDeviceInfo* info) {
    if (!g_udev || !syspath || !info) return -1;
    
    struct udev_device* dev = udev_device_new_from_syspath(g_udev, syspath);
    if (!dev) return -1;
    
    int ret = fill_device_info(dev, info);
    udev_device_unref(dev);
    return ret;
}

const char* linux_get_platform_name(void) {
    return "Linux";
}