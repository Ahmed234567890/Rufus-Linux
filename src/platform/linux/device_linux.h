/*
 * Rufus: The Reliable USB Formatting Utility
 * Linux device enumeration header
 * Copyright © 2024 Pete Batard <pete@akeo.ie>
 */

#pragma once

#include "core/platform.h"

/* Linux device operations */
int linux_init(void);
void linux_cleanup(void);
int linux_enumerate_devices(PlatformDeviceInfo* devices, int max_devices, int* count);
int linux_get_device_info(const char* syspath, PlatformDeviceInfo* info);
int linux_open_device_ro(const char* devpath);
void linux_close_device(int fd);
const char* linux_get_platform_name(void);