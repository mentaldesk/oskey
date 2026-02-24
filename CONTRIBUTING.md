# Testing

Tests use ZMK's host-based simulation framework (`native_sim` board) — no hardware required. All builds run inside the `zmkfirmware/zmk-build-arm` Docker image, so the environment is identical locally and in CI.

## Prerequisites

[Docker Desktop](https://docs.docker.com/desktop/) (macOS or Linux)

## Setup

Run the init script once to fetch the west workspace dependencies:

```sh
./init-tests
```

The fetched dependencies (`tests/zmk/`, `tests/zephyr/`, etc.) are gitignored.

## Running the tests

From the repo root (rerun whenever code changes):

```sh
./run-tests                          # run all tests
./run-tests tests/oskey/default-win  # run a single test
```

## Updating snapshots

If a test fails due to a snapshot mismatch rather than a logic error, regenerate the golden file with:

```sh
ZMK_TESTS_AUTO_ACCEPT=1 ./run-tests tests/oskey/select-mac
```

## Test cases

| Test | What it verifies |
|------|-----------------|
| `default-win` | `os_test` fires `W` with no OS selected (default = Windows) |
| `select-mac` | `os_test` fires `M` after selecting macOS |
| `select-lin` | `os_test` fires `L` after selecting Linux |
| `select-win-after-mac` | Switching from macOS back to Windows fires `W` again |
