# Building and Testing

## Prerequisites

- Windows 10 or 11, x64
- Visual Studio 2022 with the Desktop development with C++ workload
- CMake 3.30 or newer
- Internet access during the first full configure, to fetch pinned dependencies

SDL is pinned to release 3.4.12 with a verified source-archive hash. The host
uses one fixed two-pass sharp pixel scaler: exact nearest-neighbor integer
enlargement followed by a controlled final resolve to the monitor-sized,
aspect-correct output rectangle.

## Presets

Core and tests only, without downloading dependencies:

```powershell
cmake --preset windows-core-only
cmake --build --preset windows-core-debug
ctest --preset windows-core-debug
```

Full SDL host:

```powershell
cmake --preset windows-vs2022
cmake --build --preset windows-debug
ctest --preset windows-release
```

> **`ctest --preset windows-debug` is the full Debug ROM sweep** — 219 tests,
> about 25 minutes at 24 jobs and several hours serial. It is a release
> checkpoint, not a build check, so the line above runs the Release suite
> instead. For the cheap Debug tests use `ctest --preset windows-core-debug`
> from the core-only build. (The `cmake --build` preset of the same name is a
> different thing and is only a compile.)

For normal native-port iterations, use the Release build and the owning
family's targeted ROM regression, plus the ROM-free tests in both
configurations. The test presets run 24 jobs, so the whole Release suite is
about 147 seconds and can be a routine check rather than an event.

Debug ROM replay is deliberately exceptional: the ROM-free Debug tests already
carry what a Debug build catches that Release cannot, while a Debug ROM route
adds only cross-configuration artifact equality at a cost of minutes to hours.
The cases that still require one, and the admission rules, are in
[NATIVE_PORT_WORKFLOW.md](NATIVE_PORT_WORKFLOW.md).

## Checking a build

Validate a ROM without opening a window:

```powershell
.\out\build\windows-vs2022\Debug\devils-crush.exe --validate-rom "D:\path\to\Devil's Crush.pce"
```

Run a headless ten-frame emulator check:

```powershell
.\out\build\windows-vs2022\Debug\devils-crush.exe --smoke-test --rom "D:\path\to\Devil's Crush.pce"
```

Exercise the three-window cabinet host for ten frames with automatic exit:

```powershell
.\out\build\windows-vs2022\Debug\devils-crush.exe --cabinet-smoke-test --rom "D:\path\to\Devil's Crush.pce"
```

## Release staging check

Create and verify the current ROM-free Release layout:

```powershell
cmake --install out/build/windows-vs2022 `
  --config Release `
  --prefix out/distribution/devils-crush
cmake `
  -DDISTRIBUTION_DIR="$PWD/out/distribution/devils-crush" `
  -P cmake/VerifyDistribution.cmake
```

The verifier requires exactly `devils-crush.exe`, `devils-crush-launcher.exe`,
`LICENSE`, `README.md`, and `THIRD_PARTY_NOTICES.md`, and independently rejects the supported ROM hash.
CI performs this check for every Release build. This staging layout is not yet
the final source-complete public package.

ROM-required CTest coverage is local-only and disabled by default:

```powershell
cmake --preset windows-vs2022 `
  -DDC_ENABLE_ROM_TESTS=ON `
  -DDC_ROM_PATH="D:\path\to\Devil's Crush.pce"
ctest --test-dir out/build/windows-vs2022 -C Debug -L rom_required
```
