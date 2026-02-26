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

### `hold-fn` — Hold Globe/Fn while pressing a binding

Holds the macOS Globe/Fn key (`C_AC_NEXT_KEYBOARD_LAYOUT_SELECT`) while pressing and releasing a child binding. This is primarily useful for macOS window-management shortcuts that require `Fn` held alongside another key combo (e.g. `Fn+Ctrl+Left` = Tile Left).

```c
mac_tile_left: mac_fn_tile_left {
    compatible = "zmk,behavior-hold-fn";
    #binding-cells = <0>;
    bindings = <&kp GLOBE>, <&kp LC(LEFT)>;
};
```

Combine with `os-key` to send the right shortcut per OS:

```c
ok_win_tile_left: os_key_win_tile_left {
    compatible = "zmk,behavior-os-key";
    #binding-cells = <0>;
    /*                   Win               Mac                Lin  */
    bindings = <&kp LG(LEFT)>, <&mac_tile_left>, <&none>;
};
```

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

## Configuration

### Default OS

The OS that is active on power-up. Set one of the following in your keyboard's `config/prj.conf`:

```ini
CONFIG_ZMK_OSKEY_DEFAULT_OS_WINDOWS=y  # default
CONFIG_ZMK_OSKEY_DEFAULT_OS_MACOS=y
CONFIG_ZMK_OSKEY_DEFAULT_OS_LINUX=y
```

Notes:
- The active OS persists only for the current power cycle; there is no flash storage. The power-up default is configurable — see [Configuration](#configuration).
- When an `os-key` key is held and the OS selection changes before release, the release fires on the **same binding** that was activated at press time, preventing stuck modifier keys.

---

## Usage

### 1. Include the OS constants header in your keymap file

```c
#include <dt-bindings/zmk/oskey.h>
#include <behaviors/oskey.dtsi>
```

### 2. Define an `os-selector` behavior instance

You only need one — it is stateless and reusable.

> **Tip:** if you are already including `<behaviors/oskey.dtsi>` (see [Built-in Common Behaviors](#built-in-common-behaviors)), `&os_sel` is pre-defined and this step can be skipped.

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
            /*          Win          Mac         Lin  */
            bindings = <&kp LCTRL>, <&kp LGUI>, <&kp LCTRL>;
        };

        /* Alt on Windows/Linux, Option (Alt) on macOS — same key, shown for clarity */
        okalt: os_key_alt {
            compatible = "zmk,behavior-os-key";
            #binding-cells = <0>;
            /*          Win          Mac        Lin  */
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

### 5. Combining OS selection with macros

A common pattern is to switch Bluetooth profiles and set the OS in one keypress. Define a macro per profile using `zmk,behavior-macro`:

```c
#include <dt-bindings/zmk/bt.h>
#include <dt-bindings/zmk/oskey.h>

/ {
    macros {
        bt_win: bt_win {
            compatible = "zmk,behavior-macro";
            #binding-cells = <0>;
            bindings = <&bt BT_SEL 0>, <&os_sel OS_WIN>;
        };

        bt_mac: bt_mac {
            compatible = "zmk,behavior-macro";
            #binding-cells = <0>;
            bindings = <&bt BT_SEL 1>, <&os_sel OS_MAC>;
        };

        bt_lin: bt_lin {
            compatible = "zmk,behavior-macro";
            #binding-cells = <0>;
            bindings = <&bt BT_SEL 2>, <&os_sel OS_LIN>;
        };
    };
};
```

Then use `&bt_win`, `&bt_mac`, and `&bt_lin` in your keymap instead of the bare `&bt BT_SEL N` bindings.

---

## Built-in Common Behaviors

oskey ships a ready-made set of OS-agnostic behaviors:

| Label               | Action                          | Win                | Mac                | Lin                |
|---------------------|---------------------------------|--------------------|--------------------|--------------------|
| `&ok_prev_word`     | Move cursor back one word       | `Ctrl+Left`        | `Option+Left`      | `Ctrl+Left`        |
| `&ok_next_word`     | Move cursor forward one word    | `Ctrl+Right`       | `Option+Right`     | `Ctrl+Right`       |
| `&ok_line_start`    | Beginning of line               | `Home`             | `Cmd+Left`         | `Home`             |
| `&ok_line_end`      | End of line                     | `End`              | `Cmd+Right`        | `End`              |
| `&ok_doc_start`     | Beginning of document           | `Ctrl+Home`        | `Cmd+Up`           | `Ctrl+Home`        |
| `&ok_doc_end`       | End of document                 | `Ctrl+End`         | `Cmd+Down`         | `Ctrl+End`         |
| `&ok_sel_prev_word` | Select back one word            | `Shift+Ctrl+Left`  | `Shift+Option+Left`  | `Shift+Ctrl+Left`  |
| `&ok_sel_next_word` | Select forward one word         | `Shift+Ctrl+Right` | `Shift+Option+Right` | `Shift+Ctrl+Right` |
| `&ok_sel_line_start`| Select to beginning of line     | `Shift+Home`       | `Shift+Cmd+Left`   | `Shift+Home`       |
| `&ok_sel_line_end`  | Select to end of line           | `Shift+End`        | `Shift+Cmd+Right`  | `Shift+End`        |
| `&ok_sel_doc_start` | Select to beginning of document | `Shift+Ctrl+Home`  | `Shift+Cmd+Up`     | `Shift+Ctrl+Home`  |
| `&ok_sel_doc_end`   | Select to end of document       | `Shift+Ctrl+End`   | `Shift+Cmd+Down`   | `Shift+Ctrl+End`   |
| `&ok_maximize`      | Maximize window                 | `Win+Up`           | `Fn/Globe+Ctrl+F`  | `Super+Up`         |
| `&ok_tile_left`     | Tile window left                | `Win+Left`         | `Fn/Globe+Ctrl+Left` | `Super+Left`     |
| `&ok_tile_right`    | Tile window right               | `Win+Right`        | `Fn/Globe+Ctrl+Right` | `Super+Right`   |
| `&ok_mission_ctrl`  | Show all windows / task overview | `Win+Tab`         | `Ctrl+Up`          | `none` ¹           |
| `&ok_desktop`       | Show desktop                    | `Win+D`            | `Ctrl+Cmd+D`       | `none` ¹           |
| `&ok_lock`          | Lock the computer               | `Win+L`            | `Ctrl+Cmd+Q`       | `Ctrl+Alt+L` ²     |

¹ Linux window-management shortcuts are desktop-environment specific (GNOME, KDE, Cinnamon, etc.) and have no universal standard. The Linux binding is `&none` by default — override it in your keymap for your DE.

² `Ctrl+Alt+L` is the default lock shortcut in GNOME, KDE Plasma, and Cinnamon. It may differ on other DEs.