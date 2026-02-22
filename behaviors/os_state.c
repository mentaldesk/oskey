/*
 * Copyright (c) 2024 mentaldesk
 * SPDX-License-Identifier: MIT
 */

#include <oskey/os_state.h>

static uint8_t current_os = OSKEY_OS_WINDOWS;

uint8_t zmk_oskey_get_os(void) { return current_os; }

void zmk_oskey_set_os(uint8_t os) { current_os = os; }
