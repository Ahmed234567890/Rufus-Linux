/*
 * Rufus: The Reliable USB Formatting Utility
 * Platform abstraction layer - shared interface for Windows/Linux
 * Copyright © 2024 Pete Batard <pete@akeo.ie>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

/* Platform-independent constants */
#define PLATFORM_MAX_DEVICES        64
#define PLATFORM_MAX_PARTITIONS     16
#define PLATFORM_MAX_PATH           4096
#define PLATFORM_MAX_LABEL          64

/* Partition styles */
typedef enum {
    PARTITION_STYLE_UNKNOWN = -1,
    PARTITION_STYLE_MBR = 0,
    PARTITION_STYLE_GPT = 1,
    PARTITION_STYLE_RAW = 2,  /* Super Floppy Disk */
} PartitionStyle;

/* Device types */
typedef enum {
    DEVICE_TYPE_UNKNOWN = 0,
    DEVICE_TYPE_USB,
    DEVICE_TYPE_SATA,
    DEVICE_TYPE_NVME,
    DEVICE_TYPE_SCSI,
    DEVICE_TYPE_VIRTUAL,
    DEVICE_TYPE_MMC,        /* SD/MMC/eMMC */
    DEVICE_TYPE_OPTICAL,    /* CD/DVD/BD */
    DEVICE_TYPE_LOOP,       /* Loop device */
    DEVICE_TYPE_DM,         /* Device-mapper */
    DEVICE_TYPE_MPATH,      /* Multipath */
    DEVICE_TYPE_FLOPPY,     /* Floppy */
} DeviceType;

/* Filesystem types (matching existing FS_* in rufus.h) */
typedef enum {
    FS_TYPE_UNKNOWN = -1,
    FS_TYPE_FAT16 = 0,
    FS_TYPE_FAT32,
    FS_TYPE_NTFS,
    FS_TYPE_UDF,
    FS_TYPE_EXFAT,
    FS_TYPE_REFS,
    FS_TYPE_EXT2,
    FS_TYPE_EXT3,
    FS_TYPE_EXT4,
    FS_TYPE_MAX
} FSType;

/* Block device information */
typedef struct {
    uint64_t size;                    /* Total size in bytes */
    uint32_t logical_sector_size;     /* Logical sector size (usually 512 or 4096) */
    uint32_t physical_sector_size;    /* Physical sector size (usually 4096) */
    uint32_t io_min_size;             /* Minimum I/O size */
    uint32_t io_opt_size;             /* Optimal I/O size */
    uint32_t alignment_offset;        /* Alignment offset in bytes */
    uint64_t cylinders;               /* CHS geometry (legacy) */
    uint32_t heads;
    uint32_t sectors_per_track;
    bool read_only;                   /* Read-only device */
    bool supports_discard;            /* Supports discard/trim */
    bool supports_zeroout;            /* Supports BLKZEROOUT */
} BlockDeviceInfo;

/* Device geometry (legacy CHS) */
typedef struct {
    uint64_t cylinders;
    uint32_t heads;
    uint32_t sectors_per_track;
    uint64_t start_sector;
} DeviceGeometry;

/* Device information structure */
typedef struct {
    char syspath[PLATFORM_MAX_PATH];      /* /sys/block/sdX or Windows device path */
    char devpath[PLATFORM_MAX_PATH];      /* /dev/sdX or \\.\PhysicalDriveX */
    char name[PLATFORM_MAX_LABEL * 2];    /* Vendor + Model */
    char vendor[PLATFORM_MAX_LABEL];
    char model[PLATFORM_MAX_LABEL];
    char serial[PLATFORM_MAX_LABEL];
    char bus[32];                         /* usb, nvme, ata, scsi, etc. */
    char transport[32];                   /* Transport: usb, sata, nvme, mmc, scsi, virtio, etc. */
    char wwn[64];                         /* World Wide Name */
    char hctl[32];                        /* Host:Channel:Target:LUN (for SCSI) */
    
    uint64_t size;                        /* Total size in bytes */
    uint32_t logical_sector_size;         /* Logical sector size (usually 512 or 4096) */
    uint32_t physical_sector_size;        /* Physical sector size (usually 4096) */
    uint32_t io_min_size;                 /* Minimum I/O size */
    uint32_t io_opt_size;                 /* Optimal I/O size */
    uint32_t alignment_offset;            /* Alignment offset in bytes */
    
    PartitionStyle partition_style;       /* MBR, GPT, or RAW */
    bool removable;                       /* Removable media */
    bool read_only;                       /* Write protected */
    DeviceType type;                      /* USB, NVMe, etc. */
    
    /* Parent/child relationship */
    bool is_partition;                    /* True if this is a partition */
    char parent_devpath[PLATFORM_MAX_PATH]; /* Parent device path (for partitions) */
    char parent_syspath[PLATFORM_MAX_PATH]; /* Parent syspath (for partitions) */
    
    /* Partitions (for whole disks) */
    int partition_count;
    struct {
        uint64_t offset;                  /* Start offset in bytes */
        uint64_t size;                    /* Partition size in bytes */
        int number;                       /* Partition number */
        FSType fs_type;                   /* Filesystem type */
        char label[PLATFORM_MAX_LABEL];   /* Volume label */
        char fstype_str[32];              /* Filesystem type string */
        bool bootable;                    /* Boot flag */
        char devpath[PLATFORM_MAX_PATH];  /* Partition device path */
        char syspath[PLATFORM_MAX_PATH];  /* Partition syspath */
    } partitions[PLATFORM_MAX_PARTITIONS];
} PlatformDeviceInfo;

/* Platform operations structure - implemented by each platform backend */
typedef struct {
    /* Initialize platform subsystem */
    int (*init)(void);
    
    /* Cleanup platform subsystem */
    void (*cleanup)(void);
    
    /* Enumerate all block devices suitable for Rufus operations */
    int (*enumerate_devices)(PlatformDeviceInfo* devices, int max_devices, int* count);
    
    /* Get detailed info for a specific device */
    int (*get_device_info)(const char* syspath, PlatformDeviceInfo* info);
    
    /* Open block device for read-only access */
    int (*open_device_ro)(const char* devpath);
    
    /* Open block device for read-write access */
    int (*open_device_rw)(const char* devpath);
    
    /* Close block device */
    void (*close_device)(int fd);
    
    /* Read sectors from device */
    ssize_t (*read_sectors)(int fd, void* buf, uint64_t offset, size_t count);
    
    /* Write sectors to device */
    ssize_t (*write_sectors)(int fd, const void* buf, uint64_t offset, size_t count);
    
    /* Sync device (fsync) */
    int (*sync_device)(int fd);
    
    /* Flush device (fdatasync) */
    int (*flush_device)(int fd);
    
    /* Get block device info */
    int (*get_block_info)(int fd, BlockDeviceInfo* info);
    
    /* Get device geometry */
    int (*get_geometry)(int fd, DeviceGeometry* geo);
    
    /* Discard sectors (trim) */
    int (*discard_sectors)(int fd, uint64_t offset, size_t count);
    
    /* Zero sectors */
    int (*zero_sectors)(int fd, uint64_t offset, size_t count);
    
    /* Get platform name */
    const char* (*get_platform_name)(void);
} PlatformOps;

/* Global platform operations - set at startup */
extern const PlatformOps* g_platform_ops;

/* Convenience macros */
#define platform_init()                     g_platform_ops->init()
#define platform_cleanup()                  g_platform_ops->cleanup()
#define platform_enumerate_devices(d, m, c) g_platform_ops->enumerate_devices(d, m, c)
#define platform_get_device_info(p, i)      g_platform_ops->get_device_info(p, i)
#define platform_open_device_ro(p)          g_platform_ops->open_device_ro(p)
#define platform_open_device_rw(p)          g_platform_ops->open_device_rw(p)
#define platform_close_device(f)            g_platform_ops->close_device(f)
#define platform_read_sectors(f, b, o, c)   g_platform_ops->read_sectors(f, b, o, c)
#define platform_write_sectors(f, b, o, c)  g_platform_ops->write_sectors(f, b, o, c)
#define platform_sync_device(f)             g_platform_ops->sync_device(f)
#define platform_flush_device(f)            g_platform_ops->flush_device(f)
#define platform_get_block_info(f, i)       g_platform_ops->get_block_info(f, i)
#define platform_get_geometry(f, g)         g_platform_ops->get_geometry(f, g)
#define platform_discard_sectors(f, o, c)   g_platform_ops->discard_sectors(f, o, c)
#define platform_zero_sectors(f, o, c)      g_platform_ops->zero_sectors(f, o, c)
#define platform_get_name()                 g_platform_ops->get_platform_name()