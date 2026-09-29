/*
 * Rufus: The Reliable USB Formatting Utility
 * Linux native entry point
 * Copyright © 2024 Pete Batard <pete@akeo.ie>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <inttypes.h>

#include "core/platform.h"

/* Human-readable size formatting */
static void format_size(char* buf, size_t bufsize, uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int unit = 0;
    double size = (double)bytes;
    
    while (size >= 1024.0 && unit < 5) {
        size /= 1024.0;
        unit++;
    }
    
    if (unit == 0) {
        snprintf(buf, bufsize, "%.0f %s", size, units[unit]);
    } else {
        snprintf(buf, bufsize, "%.2f %s", size, units[unit]);
    }
}

/* Partition style to string */
static const char* partition_style_str(PartitionStyle style) {
    switch (style) {
        case PARTITION_STYLE_MBR: return "MBR";
        case PARTITION_STYLE_GPT: return "GPT";
        case PARTITION_STYLE_RAW: return "RAW (superfloppy)";
        default: return "Unknown";
    }
}

/* Device type to string */
static const char* device_type_str(DeviceType type) {
    switch (type) {
        case DEVICE_TYPE_USB: return "USB";
        case DEVICE_TYPE_SATA: return "SATA";
        case DEVICE_TYPE_NVME: return "NVMe";
        case DEVICE_TYPE_SCSI: return "SCSI";
        case DEVICE_TYPE_VIRTUAL: return "Virtual";
        default: return "Unknown";
    }
}

/* FS type to string */
static const char* fs_type_str(FSType type) {
    switch (type) {
        case FS_TYPE_FAT16: return "FAT16";
        case FS_TYPE_FAT32: return "FAT32";
        case FS_TYPE_NTFS: return "NTFS";
        case FS_TYPE_UDF: return "UDF";
        case FS_TYPE_EXFAT: return "exFAT";
        case FS_TYPE_REFS: return "ReFS";
        case FS_TYPE_EXT2: return "ext2";
        case FS_TYPE_EXT3: return "ext3";
        case FS_TYPE_EXT4: return "ext4";
        default: return "Unknown";
    }
}

/* Print device info */
static void print_device(const PlatformDeviceInfo* dev) {
    char size_str[32];
    format_size(size_str, sizeof(size_str), dev->size);
    
    printf("%s  %-32s  %8s  %s  %s\n",
           dev->devpath,
           dev->name,
           size_str,
           device_type_str(dev->type),
           partition_style_str(dev->partition_style));
    
    if (dev->vendor[0] || dev->model[0] || dev->serial[0]) {
        printf("    Vendor: %s  Model: %s  Serial: %s\n",
               dev->vendor[0] ? dev->vendor : "N/A",
               dev->model[0] ? dev->model : "N/A",
               dev->serial[0] ? dev->serial : "N/A");
        printf("    Bus: %s  Removable: %s  Read-only: %s\n",
               dev->bus[0] ? dev->bus : "N/A",
               dev->removable ? "Yes" : "No",
               dev->read_only ? "Yes" : "No");
        printf("    Logical sector: %u  Physical sector: %u\n",
               dev->logical_sector_size, dev->physical_sector_size);
    }
    
    if (dev->partition_count > 0) {
        printf("    Partitions (%d):\n", dev->partition_count);
        for (int i = 0; i < dev->partition_count; i++) {
            char psize[32];
            format_size(psize, sizeof(psize), dev->partitions[i].size);
            printf("      %d: offset=%" PRIu64 "  size=%s  type=%s  label='%s'  bootable=%s\n",
                   dev->partitions[i].number, dev->partitions[i].offset, psize, fs_type_str(dev->partitions[i].fs_type),
                   dev->partitions[i].label[0] ? dev->partitions[i].label : "N/A", dev->partitions[i].bootable ? "Yes" : "No");
        }
    }
    printf("\n");
}

/* List devices command */
static int cmd_list_devices(void) {
    PlatformDeviceInfo devices[PLATFORM_MAX_DEVICES];
    int count = 0;
    
    printf("Enumerating devices using %s platform...\n\n", platform_get_name());
    
    int ret = platform_enumerate_devices(devices, PLATFORM_MAX_DEVICES, &count);
    if (ret != 0) {
        fprintf(stderr, "Failed to enumerate devices\n");
        return 1;
    }
    
    if (count == 0) {
        printf("No suitable devices found.\n");
        printf("Note: By default, only removable/USB/NVMe devices are listed.\n");
        return 0;
    }
    
    printf("Found %d device(s):\n\n", count);
    
    for (int i = 0; i < count; i++) {
        print_device(&devices[i]);
    }
    
    return 0;
}

/* Usage */
static void usage(const char* progname) {
    printf("Usage: %s [OPTIONS]\n", progname);
    printf("\n");
    printf("Options:\n");
    printf("  --list-devices     List available block devices\n");
    printf("  --help             Show this help\n");
    printf("  --version          Show version\n");
    printf("\n");
    printf("Rufus Linux Native - Milestone 1\n");
    printf("Platform: %s\n", platform_get_name());
}

int main(int argc, char* argv[]) {
    static struct option long_options[] = {
        {"list-devices", no_argument, 0, 'l'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    bool list_devices = false;
    bool show_version = false;
    
    while ((opt = getopt_long(argc, argv, "lhv", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'l':
                list_devices = true;
                break;
            case 'h':
                usage(argv[0]);
                return 0;
            case 'v':
                show_version = true;
                break;
            default:
                usage(argv[0]);
                return 1;
        }
    }
    
    if (show_version) {
        printf("rufus-linux 4.15 (Linux native)\n");
        printf("Platform: %s\n", platform_get_name());
        return 0;
    }
    
    if (!list_devices) {
        usage(argv[0]);
        return 1;
    }
    
    /* Initialize platform */
    if (platform_init() != 0) {
        fprintf(stderr, "Failed to initialize platform\n");
        return 1;
    }
    
    int ret = 0;
    if (list_devices) {
        ret = cmd_list_devices();
    }
    
    platform_cleanup();
    return ret;
}