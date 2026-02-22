# oskey — ZMK OS-Aware Key Module

A ZMK module that lets you define per-OS key bindings and switch between Windows, macOS, and Linux operating system profiles at runtime.

## Behaviors

### `os-selector` — select the active OS

Stores the desired OS so that `os-key` behaviors dispatch to the correct binding.

| OS      | Constant  |
|---------|-----------|
| Windows | `OS_WIN`  |
| macOS   | `OS_MAC`  |
| Linux   | `OS_LIN`  |

### `os-key` — OS-dispatched key binding

Holds three bindings (Windows, macOS, Linux). Whichever OS is currently selected determines which binding fires on press/release.

---

## Installation

Add the module to your ZMK config repository's `config/west.yml`:

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: mentaldesk
      url-base: https://github.com/mentaldesk
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    - name: oskey
      remote: mentaldesk
      revision: main
  self:
    path: config
```

---

## Usage

### 1. Include the OS constants header in your keymap file

```c
#include <dt-bindings/zmk/oskey.h>
```

### 2. Define an `os-selector` behavior instance

You only need one — it is stateless and reusable.

```c
/ {
    behaviors {
        os_sel: os_selector {
            compatible = "zmk,behavior-os-selector";
            #binding-cells = <1>;
        };
    };
};
```

### 3. Define `os-key` behavior instances for each key that differs across OSes

```c
/ {
    behaviors {
        /* Ctrl on Windows/Linux, Cmd (GUI) on macOS */
        okctrl: os_key_ctrl {
            compatible = "zmk,behavior-os-key";
            #binding-cells = <0>;
            bindings = <&kp LCTRL>, <&kp LGUI>, <&kp LCTRL>;
            /*                 Win        Mac         Lin  */
        };

        /* Alt on Windows/Linux, Option (Alt) on macOS — same key, shown for clarity */
        okalt: os_key_alt {
            compatible = "zmk,behavior-os-key";
            #binding-cells = <0>;
            bindings = <&kp LALT>, <&kp LALT>, <&kp LALT>;
        };
    };
};
```

### 4. Map the behaviors in your keymap layers

```c
/ {
    keymap {
        compatible = "zmk,keymap";

        default_layer {
            bindings = <
                &okctrl  &okalt  /* ... */
            >;
        };

        /* A dedicated OS-select layer (example) */
        os_layer {
            bindings = <
                &os_sel OS_WIN  &os_sel OS_MAC  &os_sel OS_LIN
            >;
        };
    };
};
```

---

## Notes

- The default OS is **Windows** (`OS_WIN`). It persists only for the current power cycle; there is no flash storage.
- When an `os-key` key is held and the OS selection changes before release, the release fires on the **same binding** that was activated at press time, preventing stuck modifier keys.
- Both behaviors are only compiled for the central role in split keyboards; peripheral halves do not need behavior logic.
