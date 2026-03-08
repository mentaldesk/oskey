# oskey

ZMK module providing OS-aware key behaviors. Users select an OS at runtime (Windows / macOS / Linux); behaviors dispatch the correct keycodes for that OS automatically.

## Behaviors

### `os-selector` (`zmk,behavior-os-selector`)
- `#binding-cells = <1>` — param is `OS_WIN` / `OS_MAC` / `OS_LIN` (from `dt-bindings/zmk/oskey.h`)
- Stateless; one instance suffices. The built-in instance is `&os_sel`.
- Stores state in `behaviors/os_state.c` via `zmk_oskey_set_os()` / `zmk_oskey_get_os()`.
- Default OS on power-up is `CONFIG_ZMK_OSKEY_DEFAULT_OS_WINDOWS/MACOS/LINUX` (default: Windows).

### `os-key` (`zmk,behavior-os-key`)
- `#binding-cells = <0>` — `bindings` phandle-array: `<&win_binding>, <&mac_binding>, <&lin_binding>`
- On press: selects the binding for the current OS, stores it in an active-key slot (indexed by key position), invokes it.
- On release: retrieves the stored slot and invokes the same binding — prevents stuck modifiers if OS changes mid-hold.

### `os-layer-mod` (`zmk,behavior-os-layer-mod`)
- `#binding-cells = <2>` — param1 = layer index, param2 = tap keycode
- `bindings` phandle-array: `<&win_mod>, <&mac_mod>, <&lin_mod>` — the modifier held during hold.
- `tapping-term-ms` — threshold between tap and hold.
- Hold: activates the layer + presses the OS modifier (keeps OS app-switcher menu open).
- Release after hold: deactivates layer + releases modifier.
- Release before timer: emits the tap keycode.
- Modifier binding is captured at press time; mid-hold OS change cannot cause a mismatched release.
- Built-in instance `&ok_ltm` in `oskey.dtsi` defaults to `LALT / LGUI / LALT`.

## Repository layout

```
behaviors/          C implementations of all behaviors + os_state.c
include/oskey/      os_state.h (public API: zmk_oskey_get_os / zmk_oskey_set_os)
include/dt-bindings/zmk/oskey.h   OS_WIN/OS_MAC/OS_LIN constants
dts/behaviors/oskey.dtsi          Ready-made behavior instances (ok_cut, ok_copy, ok_ltm, etc.)
dts/bindings/behaviors/           Zephyr DTS binding YAML files (one per compatible string)
tests/os-layer-mod/               Tests for os-layer-mod behavior
tests/oskey/                      Tests for os-key and os-selector behaviors
Kconfig                           Kconfig symbols (auto-enabled by DTS compat presence)
CMakeLists.txt                    Conditionally compiles each behavior source file
zephyr/module.yml                 Declares this repo as a Zephyr module
```

## Adding a new behavior

1. Create `behaviors/behavior_<name>.c` — follow the pattern in `behavior_os_key.c`:
   - `#define DT_DRV_COMPAT zmk_behavior_<name>`
   - Implement `binding_pressed` / `binding_released` via a `behavior_driver_api` struct
   - Use `BEHAVIOR_DT_INST_DEFINE` macro for registration
   - If OS-aware: call `zmk_oskey_get_os()` and capture the selected binding/modifier at press time
2. Add a YAML binding in `dts/bindings/behaviors/zmk,behavior-<name>.yaml`
   - `include: zero_param.yaml` (0 cells), `one_param.yaml` (1 cell), or `two_param.yaml` (2 cells)
   - DTS `phandle-array` properties must be named `bindings` so Zephyr resolves `#binding-cells` on target nodes
3. Add a `Kconfig` symbol: `DT_COMPAT_… := zmk,behavior-<name>` + `config ZMK_BEHAVIOR_…` auto-enabled by `dt_compat_enabled`; add to `ZMK_OSKEY_STATE` default deps if OS-aware
4. Add a `target_sources_ifdef` line in `CMakeLists.txt`
5. Add a pre-defined instance to `dts/behaviors/oskey.dtsi` if it belongs in the built-in set
6. Add tests (see below)

## Unit tests

Tests live under `tests/os-layer-mod/` and `tests/oskey/`, mirroring the ZMK test format.

**Structure of each test case:**
```
tests/<suite>/<test-name>/
    native_sim.keymap     — keymap overlay + kscan mock events
    keycode_events.snapshot — expected output (one line per HID event)
    events.patterns       — sed script to filter raw log to relevant lines
```

**Shared keymap:** `tests/<suite>/behavior_keymap.dtsi` — included by all test cases in the suite.

**Running tests:**
```sh
./run-tests tests/os-layer-mod/   # single suite
./run-tests                       # all suites
```
Requires Docker. Set `ZMK_TESTS_AUTO_ACCEPT=1` to overwrite snapshots instead of diffing.

**Snapshot format:** hex digits in keycode fields are uppercase (`0x0A`, `0xE2`). The `events.patterns` sed script selects which log lines appear in the snapshot — typically `s/.*hid_listener_keycode_//p`.

**HID keycodes for common keys** (usage page `0x07`):
- `G` = `0x0A`, `T` = `0x17`, `W` = `0x1A`, `M` = `0x10`, `L` = `0x0F`
- `LALT` = `0xE2`, `LGUI` = `0xE3`, `LCTRL` = `0xE0`, `LSHIFT` = `0xE1`

**Mock event timing:** `tapping-term-ms` for `os-layer-mod` is `200`. Use `ZMK_MOCK_PRESS(row,col,ms)` — `ms` is the delay *after* the event. Hold tests use a delay ≥ tapping-term (e.g. `300`); tap tests use a short delay (e.g. `10`).

## Code style

- 4-space indentation; no tabs.
- Opening braces on the same line for functions and control flow.
- Struct field alignment with spaces (not tabs) — align `=` within a struct initializer block.
- Comment columns: binding order comments use `/* Win  Mac  Lin */` aligned above the `bindings` line.
- Max active slots tracked in a fixed-size array (size `ZMK_BHV_MAX_ACTIVE_…`); allocated linearly, freed by setting `active = false`.
- OS modifier/binding is always captured at press time and stored in the active slot — never re-queried at release.
- `BEHAVIOR_DT_INST_DEFINE` is the last thing in each C file, preceded by `#define … _INST(n)` and `DT_INST_FOREACH_STATUS_OKAY`.
- Copyright header: `/* Copyright (c) 2026 mentaldesk\n * SPDX-License-Identifier: MIT\n */`
