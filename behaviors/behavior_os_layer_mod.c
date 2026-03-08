/*
 * Copyright (c) 2026 mentaldesk
 * SPDX-License-Identifier: MIT
 *
 * OS Layer Mod behavior: hold = activate a layer + hold the OS-appropriate
 * app-switching modifier (Left Alt on Win/Lin, Left GUI/Cmd on macOS),
 * keeping the OS app-switcher menu open; tap = emit a normal keycode.
 *
 * On release after a hold, the layer is deactivated and the modifier is
 * released, dismissing the OS switcher menu and confirming the selection.
 */

#define DT_DRV_COMPAT zmk_behavior_os_layer_mod

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/keymap.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <oskey/os_state.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* ── Configuration ───────────────────────────────────────────────────── */

struct behavior_os_layer_mod_config {
    int tapping_term_ms;
    struct zmk_behavior_binding win_mod; /* modifier binding for Windows */
    struct zmk_behavior_binding mac_mod; /* modifier binding for macOS   */
    struct zmk_behavior_binding lin_mod; /* modifier binding for Linux   */
};

/* ── Per-press state ─────────────────────────────────────────────────── */

enum os_layer_mod_state {
    OLM_UNDECIDED, /* timer still running; not yet decided    */
    OLM_TAP,       /* released before timer; tap emitted      */
    OLM_HOLD,      /* timer fired; layer + modifier active    */
};

#define ZMK_BHV_MAX_ACTIVE_OS_LAYER_MODS 10

struct active_os_layer_mod {
    bool active;
    uint32_t position;
    int64_t  press_timestamp;
    uint32_t layer;                          /* param1: layer to activate on hold        */
    uint32_t tap_keycode;                    /* param2: keycode to emit on tap           */
    struct zmk_behavior_binding mod_binding; /* OS modifier captured at press time       */
    enum os_layer_mod_state state;
    const struct behavior_os_layer_mod_config *config;
    struct k_work_delayable work;
};

static struct active_os_layer_mod active_os_layer_mods[ZMK_BHV_MAX_ACTIVE_OS_LAYER_MODS];

/* ── Helpers ─────────────────────────────────────────────────────────── */

static const struct zmk_behavior_binding *
select_mod_binding(const struct behavior_os_layer_mod_config *cfg) {
    switch (zmk_oskey_get_os()) {
    case OSKEY_OS_MACOS:
        return &cfg->mac_mod;
    case OSKEY_OS_LINUX:
        return &cfg->lin_mod;
    case OSKEY_OS_WINDOWS:
    default:
        return &cfg->win_mod;
    }
}

static struct active_os_layer_mod *find_active(uint32_t position) {
    for (int i = 0; i < ZMK_BHV_MAX_ACTIVE_OS_LAYER_MODS; i++) {
        if (active_os_layer_mods[i].active && active_os_layer_mods[i].position == position) {
            return &active_os_layer_mods[i];
        }
    }
    return NULL;
}

static struct active_os_layer_mod *
alloc_active(uint32_t position, int64_t press_timestamp, uint32_t layer, uint32_t tap_keycode,
             const struct zmk_behavior_binding *mod_binding,
             const struct behavior_os_layer_mod_config *cfg) {
    for (int i = 0; i < ZMK_BHV_MAX_ACTIVE_OS_LAYER_MODS; i++) {
        if (!active_os_layer_mods[i].active) {
            struct active_os_layer_mod *olm = &active_os_layer_mods[i];
            olm->active          = true;
            olm->position        = position;
            olm->press_timestamp = press_timestamp;
            olm->layer           = layer;
            olm->tap_keycode     = tap_keycode;
            olm->mod_binding     = *mod_binding;
            olm->state           = OLM_UNDECIDED;
            olm->config          = cfg;
            return olm;
        }
    }
    return NULL;
}

/* ── Hold activation (called from the Zephyr work queue on timer expiry) */

static void do_hold(struct active_os_layer_mod *olm) {
    olm->state = OLM_HOLD;
    struct zmk_behavior_binding_event mod_event = {
        .position  = olm->position,
        .timestamp = k_uptime_get(),
    };
    /* Hold the modifier so the OS app-switcher menu stays visible. */
    zmk_behavior_invoke_binding(&olm->mod_binding, mod_event, true);
    /* Activate the designated layer (e.g. an app-switch layer). */
    zmk_keymap_layer_activate(olm->layer, false);
}

static void os_layer_mod_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = CONTAINER_OF(work, struct k_work_delayable, work);
    struct active_os_layer_mod *olm = CONTAINER_OF(dwork, struct active_os_layer_mod, work);

    if (olm->active && olm->state == OLM_UNDECIDED) {
        do_hold(olm);
    }
}

/* ── Behavior callbacks ──────────────────────────────────────────────── */

static int on_os_layer_mod_binding_pressed(struct zmk_behavior_binding *binding,
                                           struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_os_layer_mod_config *cfg = dev->config;

    /* Capture the OS modifier binding at press time so that a mid-hold OS
     * switch doesn't result in a mismatched modifier on release. */
    struct active_os_layer_mod *olm =
        alloc_active(event.position, event.timestamp, binding->param1, binding->param2,
                     select_mod_binding(cfg), cfg);
    if (!olm) {
        LOG_ERR("os-layer-mod: no free active slots");
        return ZMK_BEHAVIOR_OPAQUE;
    }

    k_work_init_delayable(&olm->work, os_layer_mod_work_handler);
    k_work_schedule(&olm->work, K_MSEC(cfg->tapping_term_ms));

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_os_layer_mod_binding_released(struct zmk_behavior_binding *binding,
                                            struct zmk_behavior_binding_event event) {
    struct active_os_layer_mod *olm = find_active(event.position);
    if (!olm) {
        LOG_ERR("os-layer-mod: release with no matching press at position %d", event.position);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    if (olm->state == OLM_UNDECIDED) {
        /* Released before the timer fired — it's a tap. */
        k_work_cancel_delayable(&olm->work);
        olm->state = OLM_TAP;
        raise_zmk_keycode_state_changed_from_encoded(olm->tap_keycode, true,  event.timestamp);
        raise_zmk_keycode_state_changed_from_encoded(olm->tap_keycode, false, event.timestamp);
    } else if (olm->state == OLM_HOLD) {
        /* Held past the timer — deactivate the layer and release the
         * modifier, dismissing the OS app-switcher menu. */
        zmk_keymap_layer_deactivate(olm->layer, false);
        zmk_behavior_invoke_binding(&olm->mod_binding, event, false);
    }

    olm->active = false;
    return ZMK_BEHAVIOR_OPAQUE;
}

/* ── Driver wiring ───────────────────────────────────────────────────── */

static const struct behavior_driver_api behavior_os_layer_mod_driver_api = {
    .binding_pressed  = on_os_layer_mod_binding_pressed,
    .binding_released = on_os_layer_mod_binding_released,
};

static int behavior_os_layer_mod_init(const struct device *dev) { return 0; }

#define _OLM_MOD_BINDING(n, idx)                                                                   \
    {                                                                                              \
        .behavior_dev = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(n, bindings, idx)),                 \
        .param1 = COND_CODE_0(DT_INST_PHA_HAS_CELL_AT_IDX(n, bindings, idx, param1), (0),         \
                              (DT_INST_PHA_BY_IDX(n, bindings, idx, param1))),                    \
        .param2 = COND_CODE_0(DT_INST_PHA_HAS_CELL_AT_IDX(n, bindings, idx, param2), (0),         \
                              (DT_INST_PHA_BY_IDX(n, bindings, idx, param2))),                    \
    }

#define OS_LAYER_MOD_INST(n)                                                                       \
    static const struct behavior_os_layer_mod_config behavior_os_layer_mod_config_##n = {          \
        .tapping_term_ms = DT_INST_PROP(n, tapping_term_ms),                                       \
        .win_mod         = _OLM_MOD_BINDING(n, 0),                                                 \
        .mac_mod         = _OLM_MOD_BINDING(n, 1),                                                 \
        .lin_mod         = _OLM_MOD_BINDING(n, 2),                                                 \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_os_layer_mod_init, NULL, NULL,                             \
                            &behavior_os_layer_mod_config_##n, POST_KERNEL,                        \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_os_layer_mod_driver_api);

DT_INST_FOREACH_STATUS_OKAY(OS_LAYER_MOD_INST)
