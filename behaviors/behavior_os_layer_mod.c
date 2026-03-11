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
#include <zmk/keys.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <oskey/os_state.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* ── Last-tapped tracking (for require-prior-idle-ms) ───────────────── */

/* Set to a large negative value so the first keypress always passes the
 * idle check regardless of how require_prior_idle_ms is configured. */
static int64_t last_tapped_timestamp = INT32_MIN;

static void store_last_tapped(int64_t timestamp) {
    if (timestamp > last_tapped_timestamp) {
        last_tapped_timestamp = timestamp;
    }
}

/* ── Configuration ───────────────────────────────────────────────────── */

enum olm_flavor {
    OLM_FLAVOR_HOLD_PREFERRED,       /* hold on other-key-down OR timer   */
    OLM_FLAVOR_BALANCED,             /* hold on other-key-up  OR timer    */
    OLM_FLAVOR_TAP_PREFERRED,        /* hold on timer only (default)      */
    OLM_FLAVOR_TAP_UNLESS_INTERRUPTED, /* hold on other-key-down; tap on timer */
};

struct behavior_os_layer_mod_config {
    int tapping_term_ms;
    int require_prior_idle_ms;
    enum olm_flavor flavor;
    struct zmk_behavior_binding win_mod; /* modifier binding for Windows */
    struct zmk_behavior_binding mac_mod; /* modifier binding for macOS   */
    struct zmk_behavior_binding lin_mod; /* modifier binding for Linux   */
};

/* Returns true if enough idle time has elapsed (or the feature is disabled). */
static bool prior_idle_elapsed(const struct behavior_os_layer_mod_config *cfg,
                               int64_t press_timestamp) {
    if (cfg->require_prior_idle_ms <= 0) {
        return true;
    }
    return (last_tapped_timestamp + cfg->require_prior_idle_ms) <= press_timestamp;
}

/* ── Per-press state ─────────────────────────────────────────────────── */

enum os_layer_mod_state {
    OLM_UNDECIDED, /* timer running; not yet decided                          */
    OLM_TAP,       /* resolved as tap (pre-timer release or timer on tap-u-i) */
    OLM_HOLD,      /* resolved as hold; layer + modifier active               */
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
    bool interrupt_key_down;                 /* balanced: another key was pressed down   */
    const struct behavior_os_layer_mod_config *config;
    struct k_work_delayable work;
};

static struct active_os_layer_mod active_os_layer_mods[ZMK_BHV_MAX_ACTIVE_OS_LAYER_MODS];

/* Points to the active slot that is still undecided, so the position
 * listener can react to interrupt keys without scanning the full array. */
static struct active_os_layer_mod *undecided_olm = NULL;

/* Forward declaration required by ZMK_EVENT_RAISE_AT in release_captured_events
 * below — the listener struct is defined later by ZMK_LISTENER, but the macro
 * needs the symbol visible at the call site. */
const struct zmk_listener zmk_listener_behavior_os_layer_mod;

/* ── Captured interrupt events ───────────────────────────────────────── */
/* While undecided, position events from other keys are captured (stolen
 * from the event queue) so they can be replayed under the correct layer
 * once the hold/tap decision is made.  Only position events are captured;
 * keycode events are always bubbled.                                      */

#define ZMK_BHV_OLM_MAX_CAPTURED_EVENTS 8

static struct zmk_position_state_changed_event
    captured_events[ZMK_BHV_OLM_MAX_CAPTURED_EVENTS];
static int captured_events_count = 0;

static void capture_position_event(const struct zmk_position_state_changed *ev) {
    if (captured_events_count < ZMK_BHV_OLM_MAX_CAPTURED_EVENTS) {
        captured_events[captured_events_count++] = copy_raised_zmk_position_state_changed(ev);
    } else {
        LOG_WRN("os-layer-mod: captured events buffer full");
    }
}

static void release_captured_events(void) {
    for (int i = 0; i < captured_events_count; i++) {
        ZMK_EVENT_RAISE_AT(captured_events[i], behavior_os_layer_mod);
    }
    captured_events_count = 0;
}

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
            olm->mod_binding         = *mod_binding;
            olm->state               = OLM_UNDECIDED;
            olm->interrupt_key_down  = false;
            olm->config              = cfg;
            undecided_olm            = olm;
            return olm;
        }
    }
    return NULL;
}

/* ── Hold activation (called from the Zephyr work queue on timer expiry) */

static void do_hold(struct active_os_layer_mod *olm) {
    olm->state = OLM_HOLD;
    if (undecided_olm == olm) {
        undecided_olm = NULL;
    }
    struct zmk_behavior_binding_event mod_event = {
        .position  = olm->position,
        .timestamp = k_uptime_get(),
    };
    /* Hold the modifier so the OS app-switcher menu stays visible. */
    zmk_behavior_invoke_binding(&olm->mod_binding, mod_event, true);
    /* Activate the designated layer (e.g. an app-switch layer). */
    zmk_keymap_layer_activate(olm->layer);
}

static void os_layer_mod_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = CONTAINER_OF(work, struct k_work_delayable, work);
    struct active_os_layer_mod *olm = CONTAINER_OF(dwork, struct active_os_layer_mod, work);

    if (olm->active && olm->state == OLM_UNDECIDED) {
        if (olm->config->flavor == OLM_FLAVOR_TAP_UNLESS_INTERRUPTED) {
            /* Timer expired: for this flavor the timer means a tap. */
            olm->state = OLM_TAP;
            if (undecided_olm == olm) {
                undecided_olm = NULL;
            }
        } else {
            do_hold(olm);
            /* Replay any events captured before the timer fired — they
             * must be processed under the now-active layer, and won't be
             * replayed by the release path (which sees OLM_HOLD). */
            release_captured_events();
        }
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

    /* If a key was tapped too recently, skip the hold timer and treat this
     * press as a tap immediately. The OLM_UNDECIDED release path handles it
     * correctly: k_work_cancel_delayable is a no-op on unscheduled work. */
    if (prior_idle_elapsed(cfg, event.timestamp)) {
        k_work_schedule(&olm->work, K_MSEC(cfg->tapping_term_ms));
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_os_layer_mod_binding_released(struct zmk_behavior_binding *binding,
                                            struct zmk_behavior_binding_event event) {
    struct active_os_layer_mod *olm = find_active(event.position);
    if (!olm) {
        LOG_ERR("os-layer-mod: release with no matching press at position %d", event.position);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    if (olm->state == OLM_UNDECIDED || olm->state == OLM_TAP) {
        /* Tap: either released before timer fired, or timer resolved as
         * tap (tap-unless-interrupted). Cancel work in case it is still
         * pending (no-op if the timer already ran). */
        k_work_cancel_delayable(&olm->work);
        if (undecided_olm == olm) {
            undecided_olm = NULL;
        }
        /* Emit the tap keycode first (it was pressed first), then replay
         * any captured interrupt keys so the output order matches the
         * physical key sequence. */
        raise_zmk_keycode_state_changed_from_encoded(olm->tap_keycode, true,  event.timestamp);
        raise_zmk_keycode_state_changed_from_encoded(olm->tap_keycode, false, event.timestamp);
        release_captured_events();
    } else if (olm->state == OLM_HOLD) {
        /* Held past the timer — deactivate the layer and release the
         * modifier, dismissing the OS app-switcher menu. */
        zmk_keymap_layer_deactivate(olm->layer);
        zmk_behavior_invoke_binding(&olm->mod_binding, event, false);
        /* Safety: discard any captured events that were not replayed
         * (e.g. timer fired while an interrupt key was still down). */
        captured_events_count = 0;
    }

    olm->active = false;
    return ZMK_BEHAVIOR_OPAQUE;
}

/* ── Position-interrupt handler ─────────────────────────────────────── */

/* Called after the interrupt event has already been captured.  Returns
 * true when a hold decision was triggered so the caller can replay the
 * captured buffer into the now-active layer. */
static bool handle_interrupt(struct active_os_layer_mod *olm,
                             const struct zmk_position_state_changed *ev) {
    switch (olm->config->flavor) {
    case OLM_FLAVOR_HOLD_PREFERRED:
    case OLM_FLAVOR_TAP_UNLESS_INTERRUPTED:
        /* Hold the moment another key is pressed. */
        if (ev->state) {
            k_work_cancel_delayable(&olm->work);
            do_hold(olm);
            return true;
        }
        break;
    case OLM_FLAVOR_BALANCED:
        /* Hold only after the interrupting key is both pressed and released. */
        if (ev->state) {
            olm->interrupt_key_down = true;
        } else if (olm->interrupt_key_down) {
            k_work_cancel_delayable(&olm->work);
            do_hold(olm);
            return true;
        }
        break;
    case OLM_FLAVOR_TAP_PREFERRED:
    default:
        /* Ignore interrupts; only the timer decides. */
        break;
    }
    return false;
}

/* ── Combined event listener ─────────────────────────────────────────── */

static int behavior_os_layer_mod_listener(const zmk_event_t *eh) {
    /* Position events drive the interrupt-flavor logic. */
    const struct zmk_position_state_changed *pos_ev = as_zmk_position_state_changed(eh);
    if (pos_ev != NULL) {
        if (undecided_olm != NULL && pos_ev->position != undecided_olm->position &&
            undecided_olm->config->flavor != OLM_FLAVOR_TAP_PREFERRED) {
            /* Capture this event so it can be replayed under the correct
             * layer once the hold/tap decision is made. */
            capture_position_event(pos_ev);
            if (handle_interrupt(undecided_olm, pos_ev)) {
                /* Hold just fired — replay captured events under the now
                 * active layer before any subsequent keys are processed. */
                release_captured_events();
            }
            return ZMK_EV_EVENT_CAPTURED;
        }
        return ZMK_EV_EVENT_BUBBLE;
    }

    /* Keycode events feed the require-prior-idle-ms tracking. */
    const struct zmk_keycode_state_changed *key_ev = as_zmk_keycode_state_changed(eh);
    if (key_ev != NULL && key_ev->state && !is_mod(key_ev->usage_page, key_ev->keycode)) {
        store_last_tapped(key_ev->timestamp);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(behavior_os_layer_mod, behavior_os_layer_mod_listener);
ZMK_SUBSCRIPTION(behavior_os_layer_mod, zmk_position_state_changed);
ZMK_SUBSCRIPTION(behavior_os_layer_mod, zmk_keycode_state_changed);

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
        .tapping_term_ms       = DT_INST_PROP(n, tapping_term_ms),                                 \
        .require_prior_idle_ms = DT_INST_PROP(n, require_prior_idle_ms),                           \
        .flavor                = DT_ENUM_IDX(DT_DRV_INST(n), flavor),                             \
        .win_mod               = _OLM_MOD_BINDING(n, 0),                                                 \
        .mac_mod               = _OLM_MOD_BINDING(n, 1),                                           \
        .lin_mod               = _OLM_MOD_BINDING(n, 2),                                                 \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_os_layer_mod_init, NULL, NULL,                             \
                            &behavior_os_layer_mod_config_##n, POST_KERNEL,                        \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_os_layer_mod_driver_api);

DT_INST_FOREACH_STATUS_OKAY(OS_LAYER_MOD_INST)
