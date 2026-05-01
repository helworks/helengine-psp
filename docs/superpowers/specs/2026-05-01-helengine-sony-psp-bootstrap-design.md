# Sony PSP Bootstrap Design

## Goal

Create the first working `helengine-psp` bootstrap that builds in Docker, packages as normal PSP homebrew, and boots in PPSSPP to an immediate solid orange screen with no input dependency.

## Constraints

- The repository should follow the same overall shape as the recent console host repos.
- The first milestone should preserve the generated-core seam used across the platform repos.
- The first runtime result should be minimal and deterministic: initialize, render orange, and keep running.
- The rendering path should prepare for future 3D work rather than depending on 2D-only convenience layers.

## Chosen Approach

Use the real PSP GU rendering path from the beginning.

This keeps the first milestone visually simple while ensuring the bootstrap exercises the rendering stack that future 3D work will depend on. The initial code should not build a text console layer, menu layer, or 2D-only abstraction. It should stand up the real display and GU loop, clear to orange, and remain stable in the emulator.

## Repository Shape

The initial scaffold should include:

- `Dockerfile`
- `Makefile`
- `README.md`
- `src/main.cpp`
- `src/platform/psp/PspBootHost.hpp`
- `src/platform/psp/PspBootHost.cpp`

This matches the established repo pattern and keeps the platform-specific boundary explicit at the boot host layer.

## Build Design

The build should be Dockerized and should produce standard PSP homebrew output, ending in `EBOOT.PBP`.

The Makefile should:

- target PSP homebrew packaging, not a raw ELF-only workflow
- preserve `HELENGINE_CORE_CPP_ROOT ?=`
- define `HELENGINE_PSP_HAS_GENERATED_CORE=0/1`
- compile the minimal source set for the first platform host

The generated-core seam is reserved for future `cs2.cpp` integration, but no generated core code is required for this milestone.

## Runtime Design

`main.cpp` should hand off immediately to a PSP-specific boot host.

`PspBootHost` should:

- initialize the PSP display/GU path needed for a future 3D-capable renderer
- allocate and configure the first frame buffers / command list structures needed by that path
- clear the rendered output to a solid orange color
- present the frame continuously so PPSSPP shows a stable result

No controller input, text rendering, UI system, audio, or filesystem work is required for this milestone.

## Packaging

The first runnable artifact should be normal PSP homebrew packaging suitable for PPSSPP loading, not just a loose ELF. The expected deliverable is an `EBOOT.PBP`-based output flow.

## Platform Boundary

This repository is PSP-specific and should not be diluted by cross-platform abstractions at bootstrap time. Shared engine code can come later above the platform host boundary, but the initial boot path should remain directly PSP-oriented.

## Verification

Success means:

1. the Docker image builds successfully
2. the PSP project builds successfully inside that container
3. the build emits a runnable PSP homebrew artifact ending in `EBOOT.PBP`
4. PPSSPP boots the artifact and shows a stable solid orange screen immediately

## Non-Goals

- text console output
- menu/UI setup
- input handling
- 2D-only rendering helpers
- gameplay or engine runtime beyond the generated-core seam
