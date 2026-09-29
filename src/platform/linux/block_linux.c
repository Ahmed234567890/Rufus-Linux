/*
 * Rufus: The Reliable USB Formatting Utility
 * Linux block device I/O operations
 * Copyright © 2024 Pete Batard <pete@akeo.ie>
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <linux/hdreg.h>
#include <scsi/sg.h>
#include <errno.h>

#include "core/platform.h"
#include "platform/linux/block_linux.h"

#define RUFUS_SECTOR_SIZE 512

/* Linux block device operations */
int linux_open_device_ro(const char* devpath) {
    if (!devpath) return -1;
    
    int fd = open(devpath, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }
    
    return fd;
}

int linux_open_device_rw(const char* devpath) {
    if (!devpath) return -1;
    
    int fd = open(devpath, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }
    
    return fd;
}

void linux_close_device(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

ssize_t linux_read_sectors(int fd, void* buf, uint64_t offset, size_t count) {
    if (fd < 0 || !buf || count == 0) {
        errno = EINVAL;
        return -1;
    }
    
    off_t offset_ll = (off_t)offset;
    ssize_t result = pread(fd, buf, count, offset_ll);
    return result;
}

ssize_t linux_write_sectors(int fd, const void* buf, uint64_t offset, size_t count) {
    if (fd < 0 || !buf || count == 0) {
        errno = EINVAL;
        return -1;
    }
    
    off_t offset_ll = (off_t)offset;
    ssize_t result = pwrite(fd, buf, count, offset_ll);
    return result;
}

int linux_sync_device(int fd) {
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    
    return fsync(fd);
}

int linux_flush_device(int fd) {
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    
    return fdatasync(fd);
}

int linux_get_block_info(int fd, BlockDeviceInfo* info) {
    if (fd < 0 || !info) {
        errno = EINVAL;
        return -1;
    }
    
    memset(info, 0, sizeof(BlockDeviceInfo));
    
    /* Get device size */
    uint64_t size = 0;
    if (ioctl(fd, BLKGETSIZE64, &size) == 0) {
        info->size = size;
    } else {
        /* Fallback to BLKGETSIZE (returns size in 512-byte sectors) */
        unsigned long sectors = 0;
        if (ioctl(fd, BLKGETSIZE, &sectors) == 0) {
            info->size = (uint64_t)sectors * 512;
        } else {
            return -1;
        }
    }
    
    /* Get logical sector size */
    unsigned int logical_bs = 0;
    if (ioctl(fd, BLKSSZGET, &logical_bs) == 0) {
        info->logical_sector_size = logical_bs;
    } else {
        info->logical_sector_size = RUFUS_SECTOR_SIZE;
    }
    
    /* Get physical sector size */
    unsigned int physical_bs = 0;
    if (ioctl(fd, BLKPBSZGET, &physical_bs) == 0) {
        info->physical_sector_size = physical_bs;
    } else {
        info->physical_sector_size = info->logical_sector_size;
    }
    
    /* Get I/O minimum/optimal */
    unsigned int io_min = 0;
    if (ioctl(fd, BLKIOMIN, &io_min) == 0) {
        info->io_min_size = io_min;
    }
    
    unsigned int io_opt = 0;
    if (ioctl(fd, BLKIOOPT, &io_opt) == 0) {
        info->io_opt_size = io_opt;
    }
    
    /* Get alignment offset */
    unsigned int align_off = 0;
    if (ioctl(fd, BLKALIGNOFF, &align_off) == 0) {
        info->alignment_offset = align_off;
    }
    
    /* Get device geometry */
    struct hd_geometry geo;
    if (ioctl(fd, HDIO_GETGEO, &geo) == 0) {
        info->cylinders = geo.cylinders;
        info->heads = geo.heads;
        info->sectors_per_track = geo.sectors;
    }
    
    /* Check if device is read-only */
    int ro = 0;
    if (ioctl(fd, BLKROGET, &ro) == 0) {
        info->read_only = (ro != 0);
    }
    
    /* Check if device supports discard/trim */
    int discard = 0;
    if (ioctl(fd, BLKDISCARD, &discard) == 0) {
        info->supports_discard = (discard != 0);
    }
    
    return 0;
}

int linux_get_device_geometry(int fd, DeviceGeometry* geo) {
    if (fd < 0 || !geo) {
        errno = EINVAL;
        return -1;
    }
    
    struct hd_geometry hdgeo;
    if (ioctl(fd, HDIO_GETGEO, &hdgeo) != 0) {
        return -1;
    }
    
    geo->cylinders = hdgeo.cylinders;
    geo->heads = hdgeo.heads;
    geo->sectors_per_track = hdgeo.sectors;
    geo->start_sector = hdgeo.start;
    
    return 0;
}

int linux_discard_sectors(int fd, uint64_t offset, size_t count) {
    if (fd < 0 || count == 0) {
        errno = EINVAL;
        return -1;
    }
    
    struct {
        uint64_t start;
        uint64_t len;
    } range = {
        .start = offset,
        .len = count
    };
    
    return ioctl(fd, BLKDISCARD, &range);
}

int linux_zero_sectors(int fd, uint64_t offset, size_t count) {
    if (fd < 0 || count == 0) {
        errno = EINVAL;
        return -1;
    }
    
    /* Use BLKZEROOUT if available (Linux 3.1+) */
    struct {
        uint64_t start;
        uint64_t len;
    } range = {
        .start = offset,
        .len = count
    };
    
    int result = ioctl(fd, BLKZEROOUT, &range);
    if (result == 0) {
        return 0;
    }
    
    /* Fallback: write zeros manually */
    static char* zero_buf = NULL;
    static size_t zero_buf_size = 0;
    
    size_t needed = count;
    if (needed > 1024 * 1024) needed = 1024 * 1024;  /* Max 1MB at a time */
    
    if (zero_buf_size < needed) {
        char* new_buf = realloc(zero_buf, needed);
        if (!new_buf) {
            errno = ENOMEM;
            return -1;
        }
        zero_buf = new_buf;
        zero_buf_size = needed;
        memset(zero_buf, 0, zero_buf_size);
    }
    
    size_t remaining = count;
    uint64_t current_offset = offset;
    
    while (remaining > 0) {
        size_t chunk = (remaining < zero_buf_size) ? remaining : zero_buf_size;
        ssize_t written = pwrite(fd, zero_buf, chunk, (off_t)current_offset);
        if (written != (ssize_t)chunk) {
            return -1;
        }
        current_offset += chunk;
        remaining -= chunk;
    }
    
    return 0;
}

/* Block device operations structure */
const BlockOps block_linux_ops = {
    .open_ro = linux_open_device_ro,
    .open_rw = linux_open_device_rw,
    .close = linux_close_device,
    .read_sectors = linux_read_sectors,
    .write_sectors = linux_write_sectors,
    .sync = linux_sync_device,
    .flush = linux_flush_device,
    .get_info = linux_get_block_info,
    .get_geometry = linux_get_device_geometry,
    .discard = linux_discard_sectors,
    .zero = linux_zero_sectors,
};