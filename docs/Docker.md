# PSP Docker Build

Use the low-level native Docker flow when you need to build the PSP host directly instead of using the shared editor wrapper.

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
