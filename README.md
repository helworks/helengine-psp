# Helengine PSP Host

This repository contains the native PSP host scaffold for Helengine.

## Current milestone

- Docker-only build using the official PSPDEV SDK image
- Standard PSP homebrew packaging ending in `EBOOT.PBP`
- Generated Helengine runtime sources still bundled into the PSP executable
- Isolated blank-frame boot remains the default verification baseline
- Checkpointed runtime startup can be enabled explicitly to diagnose PSP startup through startup-scene load

## Build

Default isolated baseline:

```bash
docker build -t helengine-psp .
docker run --rm -v "$PWD":/workspace -v "$HELENGINE_CORE_CPP_ROOT":/generated-core -w /workspace helengine-psp make HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=OFF
```

Checkpointed runtime-startup bring-up:

```bash
docker build -t helengine-psp .
docker run --rm -v "$PWD":/workspace -v "$HELENGINE_CORE_CPP_ROOT":/generated-core -w /workspace helengine-psp make HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON
```

If Docker Desktop's credential helper blocks anonymous pulls on this machine, use:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker build -t helengine-psp .
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -v "$HELENGINE_CORE_CPP_ROOT":/generated-core -w /workspace helengine-psp make HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=OFF
```

The build emits `build/EBOOT.PBP`.

## Generated core seam

The native PSP build requires `HELENGINE_CORE_CPP_ROOT` to point at the generated core output produced by the editor build graph. The generated core root must contain:

- `helcpp_config.hpp`
- `helengine_core_amalgamated.cpp`
- `runtime/runtime_startup_manifest.cpp`
- `runtime/runtime_scene_catalog_manifest.cpp`

When runtime code modules exist, the native build also picks up `runtime/runtime_code_module_manifest.cpp`.

## End-to-end PSP build

The real PSP build is editor-driven. The editor regenerates generated-core C++ for platform `psp`, cooks the selected scene and assets, and then invokes the PSP builder assembly in this repository.

Expected staged output:

- `output/psp/PSP/GAME/HELENGINE/EBOOT.PBP`
- `output/psp/PSP/GAME/HELENGINE/cooked/scenes/rendering/cube_test.hasset`

## Boot check

Load `build/EBOOT.PBP` in PPSSPP.

- With `HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=OFF`, the expected result is a stable blank frame with no immediate crash or reset loop.
- With `HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON`, the expected result is either successful startup-scene load or an on-screen fatal diagnostic that names the failing checkpoint.
