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

### `os-layer-mod` — hold-tap for OS-agnostic app switching

A hold-tap behavior that, when held past the `tapping-term-ms` threshold:
- Activates a specified layer, and
- Holds an OS-appropriate modifier key (configured via `bindings`)

Releasing the key deactivates the layer and releases the modifier, confirming the selection.
When tapped quickly it sends a regular keycode instead.

The built-in `&ok_ltm` instance (from `oskey.dtsi`) defaults to Left Alt on Windows/Linux and Left GUI (Command) on macOS. You can override the `bindings` property for different modifiers.

Example usage in a keymap (hold = app-switch layer with OS modifier, tap = `G`):

```c
&ok_ltm APP_SWITCH_LAYER G
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

oskey ships a ready-made set of OS-agnostic behaviors in `behaviors/oskey.dtsi`.
Include it in your keymap with `#include <behaviors/oskey.dtsi>`.

### Commands

| Label               | Action              | Win              | Mac              | Lin              |
|---------------------|---------------------|------------------|------------------|------------------|
| `&ok_cut`           | Cut                 | `Ctrl+X`         | `Cmd+X`          | `Ctrl+X`         |
| `&ok_copy`          | Copy                | `Ctrl+C`         | `Cmd+C`          | `Ctrl+C`         |
| `&ok_paste`         | Paste               | `Ctrl+V`         | `Cmd+V`          | `Ctrl+V`         |
| `&ok_undo`          | Undo                | `Ctrl+Z`         | `Cmd+Z`          | `Ctrl+Z`         |
| `&ok_redo`          | Redo                | `Ctrl+Y`         | `Cmd+Shift+Z`    | `Ctrl+Shift+Z`   |
| `&ok_sel_all`       | Select All          | `Ctrl+A`         | `Cmd+A`          | `Ctrl+A`         |
| `&ok_find`          | Find                | `Ctrl+F`         | `Cmd+F`          | `Ctrl+F`         |
| `&ok_find_replace`  | Find & Replace      | `Ctrl+H`         | `Cmd+H`          | `Ctrl+H`         |
| `&ok_bold`          | Bold                | `Ctrl+B`         | `Cmd+B`          | `Ctrl+B`         |
| `&ok_italic`        | Italic              | `Ctrl+I`         | `Cmd+I`          | `Ctrl+I`         |
| `&ok_underline`     | Underline           | `Ctrl+U`         | `Cmd+U`          | `Ctrl+U`         |
| `&ok_new_win`       | New window          | `Ctrl+N`         | `Cmd+N`          | `Ctrl+N`         |
| `&ok_new_tab`       | New tab             | `Ctrl+T`         | `Cmd+T`          | `Ctrl+T`         |
| `&ok_close`         | Close tab/window    | `Ctrl+W`         | `Cmd+W`          | `Ctrl+W`         |
| `&ok_reopen_tab`    | Reopen closed tab   | `Ctrl+Shift+T`   | `Cmd+Shift+T`    | `Ctrl+Shift+T`   |
| `&ok_open`          | Open                | `Ctrl+O`         | `Cmd+O`          | `Ctrl+O`         |
| `&ok_save`          | Save                | `Ctrl+S`         | `Cmd+S`          | `Ctrl+S`         |
| `&ok_save_as`       | Save As             | `Ctrl+Shift+S`   | `Cmd+Shift+S`    | `Ctrl+Shift+S`   |
| `&ok_print`         | Print               | `Ctrl+P`         | `Cmd+P`          | `Ctrl+P`         |
| `&ok_zoom_in`       | Zoom in             | `Ctrl+=`         | `Cmd+=`          | `Ctrl+=`         |
| `&ok_zoom_out`      | Zoom out            | `Ctrl+-`         | `Cmd+-`          | `Ctrl+-`         |
| `&ok_zoom_reset`    | Reset zoom          | `Ctrl+0`         | `Cmd+0`          | `Ctrl+0`         |

### Navigation

| Label               | Action                          | Win                | Mac                  | Lin                |
|---------------------|---------------------------------|--------------------|----------------------|--------------------|
| `&ok_prev_word`     | Move cursor back one word       | `Ctrl+Left`        | `Option+Left`        | `Ctrl+Left`        |
| `&ok_next_word`     | Move cursor forward one word    | `Ctrl+Right`       | `Option+Right`       | `Ctrl+Right`       |
| `&ok_line_start`    | Beginning of line               | `Home`             | `Cmd+Left`           | `Home`             |
| `&ok_line_end`      | End of line                     | `End`              | `Cmd+Right`          | `End`              |
| `&ok_doc_start`     | Beginning of document           | `Ctrl+Home`        | `Cmd+Up`             | `Ctrl+Home`        |
| `&ok_doc_end`       | End of document                 | `Ctrl+End`         | `Cmd+Down`           | `Ctrl+End`         |

### Selection

| Label                | Action                          | Win                  | Mac                    | Lin                  |
|----------------------|---------------------------------|----------------------|------------------------|----------------------|
| `&ok_sel_prev_word`  | Select back one word            | `Shift+Ctrl+Left`    | `Shift+Option+Left`    | `Shift+Ctrl+Left`    |
| `&ok_sel_next_word`  | Select forward one word         | `Shift+Ctrl+Right`   | `Shift+Option+Right`   | `Shift+Ctrl+Right`   |
| `&ok_sel_line_start` | Select to beginning of line     | `Shift+Home`         | `Shift+Cmd+Left`       | `Shift+Home`         |
| `&ok_sel_line_end`   | Select to end of line           | `Shift+End`          | `Shift+Cmd+Right`      | `Shift+End`          |
| `&ok_sel_doc_start`  | Select to beginning of document | `Shift+Ctrl+Home`    | `Shift+Cmd+Up`         | `Shift+Ctrl+Home`    |
| `&ok_sel_doc_end`    | Select to end of document       | `Shift+Ctrl+End`     | `Shift+Cmd+Down`       | `Shift+Ctrl+End`     |

### Window Management

Linux window-management shortcuts are desktop-environment specific and have no universal standard. The Linux binding for DE-specific actions is `&none` by default — override it in your keymap for your DE.

| Label               | Action                           | Win                  | Mac              | Lin              |
|---------------------|----------------------------------|----------------------|------------------|------------------|
| `&ok_ltm`           | App-switch layer mod (hold-tap)¹ | hold `LAlt`          | hold `Cmd`       | hold `LAlt`      |
| `&ok_next_app`      | Next app                         | `Alt+Tab`            | `Cmd+Tab`        | `Alt+Tab`        |
| `&ok_prev_app`      | Previous app                     | `Alt+Shift+Tab`      | `Cmd+Shift+Tab`  | `Alt+Shift+Tab`  |
| `&ok_next_tab`      | Next tab                         | `Ctrl+}`             | `Cmd+}`          | `Ctrl+}`         |
| `&ok_prev_tab`      | Previous tab                     | `Ctrl+{`             | `Cmd+{`          | `Ctrl+{`         |
| `&ok_mission_ctrl`  | Show all windows / task overview | `Win+Tab`            | `Ctrl+Up`        | `none` ²         |
| `&ok_desktop`       | Show desktop                     | `Win+D`              | `Ctrl+Cmd+D`     | `none` ²         |
| `&ok_lock`          | Lock the computer                | `Win+L`              | `Ctrl+Cmd+Q`     | `Ctrl+Alt+L` ³   |
| `&ok_force_quit`    | Force quit / task manager        | `Ctrl+Shift+Esc`     | `Opt+Cmd+Esc`    | `Ctrl+Esc`       |

¹ `&ok_ltm LAYER keycode` — hold activates `LAYER` and keeps the OS app-switch modifier held; tap sends `keycode`. See [`os-layer-mod`](#os-layer-mod--hold-tap-for-os-agnostic-app-switching).

² Linux window-management shortcuts are desktop-environment specific. The Linux binding is `&none` by default — override in your keymap for your DE.

³ Default for GNOME, KDE, and Cinnamon.
| `&ok_mission_ctrl`  | Show all windows / task overview | `Win+Tab`         | `Ctrl+Up`          | `none` ¹           |
| `&ok_desktop`       | Show desktop                    | `Win+D`            | `Ctrl+Cmd+D`       | `none` ¹           |
| `&ok_lock`          | Lock the computer               | `Win+L`            | `Ctrl+Cmd+Q`       | `Ctrl+Alt+L` ²     |

¹ Linux window-management shortcuts are desktop-environment specific (GNOME, KDE, Cinnamon, etc.) and have no universal standard. The Linux binding is `&none` by default — override it in your keymap for your DE.

² `Ctrl+Alt+L` is the default lock shortcut in GNOME, KDE Plasma, and Cinnamon. It may differ on other DEs.