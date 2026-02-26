/*
 * Copyright (c) 2026 mentaldesk
 * SPDX-License-Identifier: MIT
 *
 * hold-fn behavior
 *
 * Holds one key binding while pressing (and later releasing) a second.
 * Intended primarily for macOS window-management shortcuts that require
 * Globe/Fn held alongside another key combo, e.g. Globe+Ctrl+Left (Tile Left):
 *
 *   mac_tile_left: mac_fn_tile_left {
 *       compatible = "zmk,behavior-hold-fn";
 *       #binding-cells = <0>;
 *       bindings = <&kp GLOBE>, <&kp LC(LEFT)>;
 *   };
 *
 * Press sequence at the HID level:
 *   GLOBE down  (consumer report)
 *   <binding> press
 *   ---  key held  ---
 *   <binding> release
 *   GLOBE up
 *
 * Note: GLOBE is a consumer-control key (C_AC_NEXT_KEYBOARD_LAYOUT_SELECT)
 * so it rides a separate HID report from keyboard keys.  Both reports are
 * queued within the same event-loop pass and transmitted together, which is
 * sufficient for macOS to recognise the chord — the same mechanism that
 * makes ZMK macros work for these shortcuts.
 */

#define DT_DRV_COMPAT zmk_behavior_hold_fn

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct behavior_hold_fn_config {
    struct zmk_behavior_binding hold_binding; /* held for the duration */
    struct zmk_behavior_binding tap_binding;  /* pressed then released  */
};

static int on_hold_fn_binding_pressed(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_hold_fn_config *cfg = dev->config;

    int ret = zmk_behavior_invoke_binding(&cfg->hold_binding, event, true);
    if (ret < 0) {
        LOG_ERR("hold-fn: failed to press hold binding (%d)", ret);
        return ret;
    }

    return zmk_behavior_invoke_binding(&cfg->tap_binding, event, true);
}

static int on_hold_fn_binding_released(struct zmk_behavior_binding *binding,
                                       struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_hold_fn_config *cfg = dev->config;

    /* Release order is the inverse of press order */
    int ret = zmk_behavior_invoke_binding(&cfg->tap_binding, event, false);

    int hold_ret = zmk_behavior_invoke_binding(&cfg->hold_binding, event, false);
    if (hold_ret < 0) {
        LOG_ERR("hold-fn: failed to release hold binding (%d)", hold_ret);
    }

    return ret;
}

static const struct behavior_driver_api behavior_hold_fn_driver_api = {
    .binding_pressed  = on_hold_fn_binding_pressed,
    .binding_released = on_hold_fn_binding_released,
};

static int behavior_hold_fn_init(const struct device *dev) { return 0; }

#define _HOLD_FN_BINDING(n, idx)                                                                    \
    {                                                                                              \
        .behavior_dev = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(n, bindings, idx)),                 \
        .param1 = COND_CODE_0(DT_INST_PHA_HAS_CELL_AT_IDX(n, bindings, idx, param1), (0),         \
                              (DT_INST_PHA_BY_IDX(n, bindings, idx, param1))),                    \
        .param2 = COND_CODE_0(DT_INST_PHA_HAS_CELL_AT_IDX(n, bindings, idx, param2), (0),         \
                              (DT_INST_PHA_BY_IDX(n, bindings, idx, param2))),                    \
    }

#define HOLD_FN_INST(n)                                                                            \
    static struct behavior_hold_fn_config behavior_hold_fn_config_##n = {                         \
        .hold_binding = _HOLD_FN_BINDING(n, 0),                                                   \
        .tap_binding  = _HOLD_FN_BINDING(n, 1),                                                   \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_hold_fn_init, NULL, NULL,                                 \
                            &behavior_hold_fn_config_##n, POST_KERNEL,                            \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                  \
                            &behavior_hold_fn_driver_api);

DT_INST_FOREACH_STATUS_OKAY(HOLD_FN_INST)
