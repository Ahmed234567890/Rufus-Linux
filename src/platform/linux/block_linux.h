/*
 * Rufus: The Reliable USB Formatting Utility
 * Linux block device operations header
 * Copyright © 2024 Pete Batard <pete@akeo.ie>
 */

#ifndef RUFUS_BLOCK_LINUX_H
#define RUFUS_BLOCK_LINUX_H

#include "core/platform.h"

/* Block device operations */
typedef struct {
    int (*open_ro)(const char* devpath);
    int (*open_rw)(const char* devpath);
    void (*close)(int fd);
    ssize_t (*read_sectors)(int fd, void* buf, uint64_t offset, size_t count);
    ssize_t (*write_sectors)(int fd, const void* buf, uint64_t offset, size_t count);
    int (*sync)(int fd);
    int (*flush)(int fd);
    int (*get_info)(int fd, BlockDeviceInfo* info);
    int (*get_geometry)(int fd, DeviceGeometry* geo);
    int (*discard)(int fd, uint64_t offset, size_t count);
    int (*zero)(int fd, uint64_t offset, size_t count);
} BlockOps;

/* Function declarations */
int linux_open_device_ro(const char* devpath);
int linux_open_device_rw(const char* devpath);
void linux_close_device(int fd);
ssize_t linux_read_sectors(int fd, void* buf, uint64_t offset, size_t count);
ssize_t linux_write_sectors(int fd, const void* buf, uint64_t offset, size_t count);
int linux_sync_device(int fd);
int linux_flush_device(int fd);
int linux_get_block_info(int fd, BlockDeviceInfo* info);
int linux_get_device_geometry(int fd, DeviceGeometry* geo);
int linux_discard_sectors(int fd, uint64_t offset, size_t count);
int linux_zero_sectors(int fd, uint64_t offset, size_t count);

extern const BlockOps block_linux_ops;

#endif /* RUFUS_BLOCK_LINUX_H */