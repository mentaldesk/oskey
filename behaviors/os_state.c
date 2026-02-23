/*
 * Copyright (c) 2024 mentaldesk
 * SPDX-License-Identifier: MIT
 */

#include <oskey/os_state.h>

#if defined(CONFIG_ZMK_OSKEY_DEFAULT_OS_MACOS)
static uint8_t current_os = OSKEY_OS_MACOS;
#elif defined(CONFIG_ZMK_OSKEY_DEFAULT_OS_LINUX)
static uint8_t current_os = OSKEY_OS_LINUX;
#else
static uint8_t current_os = OSKEY_OS_WINDOWS;
#endif

uint8_t zmk_oskey_get_os(void) { return current_os; }

void zmk_oskey_set_os(uint8_t os) { current_os = os; }
