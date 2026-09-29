/*
 * Rufus: The Reliable USB Formatting Utility
 * Linux platform implementation header
 * Copyright © 2024 Pete Batard <pete@akeo.ie>
 */

#pragma once

#include "core/platform.h"
#include "platform/linux/device_linux.h"
#include "platform/linux/block_linux.h"

/* Linux-specific platform operations */
extern const PlatformOps platform_linux_ops;