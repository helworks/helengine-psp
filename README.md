# Helengine PSP Host

This repository contains the native PSP host scaffold for Helengine.

## Current milestone

- Docker-only build using the official PSPDEV SDK image
- Standard PSP homebrew packaging ending in `EBOOT.PBP`
- First boot check with a solid orange screen in PPSSPP

## Build

```bash
docker build -t helengine-psp .
docker run --rm -v "$PWD":/workspace -w /workspace helengine-psp make
```

If Docker Desktop's credential helper blocks anonymous pulls on this machine, use:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker build -t helengine-psp .
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-psp make
```

The build emits `build/EBOOT.PBP`.

## Generated core seam

The native build reserves `HELENGINE_CORE_CPP_ROOT` for later `cs2.cpp` integration, but the first milestone does not compile generated core output yet.

## Boot check

Load `build/EBOOT.PBP` in PPSSPP. The expected result for this milestone is a solid orange frame with no immediate crash or reset loop.
