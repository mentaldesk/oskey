/*
 * Copyright (c) 2024 mentaldesk
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/* OS identifiers — match the dt-bindings values in oskey.h */
#define OSKEY_OS_WINDOWS 0
#define OSKEY_OS_MACOS   1
#define OSKEY_OS_LINUX   2

/**
 * @brief Get the currently selected operating system.
 * @return One of OSKEY_OS_WINDOWS, OSKEY_OS_MACOS, or OSKEY_OS_LINUX.
 */
uint8_t zmk_oskey_get_os(void);

/**
 * @brief Set the currently selected operating system.
 * @param os One of OSKEY_OS_WINDOWS, OSKEY_OS_MACOS, or OSKEY_OS_LINUX.
 */
void zmk_oskey_set_os(uint8_t os);
