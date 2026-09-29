/*
 * Rufus: The Reliable USB Formatting Utility
 * Linux platform registration
 * Copyright © 2024 Pete Batard <pete@akeo.ie>
 */

#include "core/platform.h"
#include "platform/linux/platform_linux.h"
#include "platform/linux/block_linux.h"

/* Linux platform operations */
const PlatformOps platform_linux_ops = {
    .init = linux_init,
    .cleanup = linux_cleanup,
    .enumerate_devices = linux_enumerate_devices,
    .get_device_info = linux_get_device_info,
    .open_device_ro = linux_open_device_ro,
    .open_device_rw = linux_open_device_rw,
    .close_device = linux_close_device,
    .read_sectors = linux_read_sectors,
    .write_sectors = linux_write_sectors,
    .sync_device = linux_sync_device,
    .flush_device = linux_flush_device,
    .get_block_info = linux_get_block_info,
    .get_geometry = linux_get_device_geometry,
    .discard_sectors = linux_discard_sectors,
    .zero_sectors = linux_zero_sectors,
    .get_platform_name = linux_get_platform_name,
};

/* Global platform operations - set at startup */
const PlatformOps* g_platform_ops = &platform_linux_ops;