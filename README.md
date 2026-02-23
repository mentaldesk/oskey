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

## Configuration

### Default OS

The OS that is active on power-up. Set one of the following in your keyboard's `config/prj.conf`:

```ini
CONFIG_ZMK_OSKEY_DEFAULT_OS_WINDOWS=y  # default
CONFIG_ZMK_OSKEY_DEFAULT_OS_MACOS=y
CONFIG_ZMK_OSKEY_DEFAULT_OS_LINUX=y
```

The selection persists only for the current power cycle; there is no flash storage.

---

## Testing

Tests use ZMK's host-based simulation framework (`native_sim` board) — no hardware required.

### Prerequisites

```sh
# macOS
brew install cmake ninja python3 dtc
pip3 install west
```
Run the init script once to set up the west workspace:

```sh
./init-tests
```

### Running the tests

From the repo root (rerun whenever code changes):

```sh
./run-tests                     # run all tests
./run-tests tests/default-win   # run a single test
```

The fetched dependencies (`zmk/`, `zephyr/`, etc.) are gitignored.

### Updating snapshots

If a test fails due to a snapshot mismatch rather than a logic error, regenerate the golden file with:

```sh
ZMK_TESTS_AUTO_ACCEPT=1 ./run-tests tests/select-mac
```

### Test cases

| Test | What it verifies |
|------|-----------------|
| `default-win` | `os_test` fires `W` with no OS selected (default = Windows) |
| `select-mac` | `os_test` fires `M` after selecting macOS |
| `select-lin` | `os_test` fires `L` after selecting Linux |
| `select-win-after-mac` | Switching from macOS back to Windows fires `W` again |

---

## Notes

- The active OS persists only for the current power cycle; there is no flash storage. The power-up default is configurable — see [Configuration](#configuration).
- When an `os-key` key is held and the OS selection changes before release, the release fires on the **same binding** that was activated at press time, preventing stuck modifier keys.
- Both behaviors are only compiled for the central role in split keyboards; peripheral halves do not need behavior logic.
