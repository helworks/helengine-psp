# Sony PSP Bootstrap Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first `helengine-psp` Dockerized PSP homebrew scaffold that packages to `EBOOT.PBP` and boots in PPSSPP to a stable solid orange screen with no input dependency.

**Architecture:** Keep the same repo shape as the recent console hosts, but use the real PSP GU/display path from the start so the first bootstrap is already aligned with future 3D rendering work. `main.cpp` should hand off immediately to `PspBootHost`, and the build should preserve the same generated-core seam pattern used across the platform repos.

**Tech Stack:** Docker, PSP toolchain/homebrew SDK, GNU Make, C++17, PSP GU, PPSSPP

---

## File Structure

- Create: `Dockerfile`
  Purpose: define the PSP build container with the PSP homebrew toolchain needed to compile and package `EBOOT.PBP`.
- Create: `Makefile`
  Purpose: compile the PSP bootstrap, preserve the generated-core seam, and package normal PSP homebrew output.
- Create: `README.md`
  Purpose: document the Docker build flow, expected packaged output, and the PPSSPP orange-screen verification target.
- Create: `src/main.cpp`
  Purpose: enter the PSP bootstrap and immediately delegate to the PSP-specific boot host.
- Create: `src/platform/psp/PspBootHost.hpp`
  Purpose: declare the PSP-specific boot host and the minimal lifecycle needed for the first render loop.
- Create: `src/platform/psp/PspBootHost.cpp`
  Purpose: initialize the PSP display/GU path, render orange, and keep the frame alive.

### Task 1: Establish The PSP Build Skeleton

**Files:**
- Create: `Dockerfile`
- Create: `Makefile`

- [ ] **Step 1: Inspect an existing platform scaffold before drafting the PSP equivalents**

Run:

```bash
rtk sed -n '1,220p' /mnt/c/dev/helworks/helengine-wii/Dockerfile
rtk sed -n '1,260p' /mnt/c/dev/helworks/helengine-wii/Makefile
```

Expected: the Wii scaffold shows the repo-level conventions we want to preserve even though the PSP toolchain details will differ.

- [ ] **Step 2: Write the PSP Dockerfile**

Create `Dockerfile` with a PSP-homebrew-focused image/setup. Start with:

```dockerfile
FROM ps2dev/pspdev:latest

WORKDIR /workspace
CMD ["/bin/bash"]
```

If the actual toolchain image available on the machine differs, adjust it during verification rather than redesigning the scaffold.

- [ ] **Step 3: Write the initial PSP Makefile**

Create `Makefile` with:

```makefile
HELENGINE_CORE_CPP_ROOT ?=

TARGET := helengine_psp
BUILD_DIR := build
TARGET_ELF := $(BUILD_DIR)/EBOOT.ELF
TARGET_PBP := $(BUILD_DIR)/EBOOT.PBP
SOURCE_DIR := src
SOURCES := \
	$(SOURCE_DIR)/main.cpp \
	$(SOURCE_DIR)/platform/psp/PspBootHost.cpp
OBJECTS := $(patsubst $(SOURCE_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))

CPPFLAGS := \
	-I$(SOURCE_DIR)

ifeq ($(strip $(HELENGINE_CORE_CPP_ROOT)),)
CPPFLAGS += -DHELENGINE_PSP_HAS_GENERATED_CORE=0
else
CPPFLAGS += -DHELENGINE_PSP_HAS_GENERATED_CORE=1 -I$(HELENGINE_CORE_CPP_ROOT)
endif

CXXFLAGS := \
	-std=gnu++17 \
	-O2 \
	-Wall \
	-Wextra \
	-ffunction-sections \
	-fdata-sections
```

- [ ] **Step 4: Add the PSP packaging-oriented build rules**

Append to `Makefile`:

```makefile
LDLIBS := \
	-lpspgum \
	-lpspgu \
	-lpspdisplay \
	-lpspge \
	-lpspctrl \
	-lpspsdk \
	-lpsprtc \
	-lpspuser

.PHONY: all clean

all: $(TARGET_PBP)

$(TARGET_PBP): $(TARGET_ELF)
	@mkdir -p $(dir $@)
	pack-pbp $@ PARAM.SFO NULL NULL NULL NULL NULL $<

$(TARGET_ELF): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR)
```

- [ ] **Step 5: Add the PSP-specific toolchain macros once the SDK path is known**

After the first container inspection, update `Makefile` to include the SDK’s required PSP build definitions, such as:

```makefile
PSP_EBOOT_TITLE := Helengine PSP
```

and any required PSP SDK include fragments or flags discovered from the container’s standard build environment.

Expected: the Makefile remains small and repo-shaped, but is compatible with the actual PSP SDK packaging flow present in the container.

- [ ] **Step 6: Commit the build skeleton**

Run:

```bash
rtk git add Dockerfile Makefile
rtk git commit -m "Add PSP build scaffold"
```

Expected: commit succeeds with only the Dockerfile and Makefile staged.

### Task 2: Add The PSP Boot Host

**Files:**
- Create: `src/main.cpp`
- Create: `src/platform/psp/PspBootHost.hpp`
- Create: `src/platform/psp/PspBootHost.cpp`

- [ ] **Step 1: Write the PSP entry point**

Create `src/main.cpp` with:

```cpp
#include "platform/psp/PspBootHost.hpp"

int main() {
    helengine::psp::PspBootHost host;
    return host.Run();
}
```

- [ ] **Step 2: Declare the PSP boot host**

Create `src/platform/psp/PspBootHost.hpp` with:

```cpp
#pragma once

namespace helengine::psp {
    class PspBootHost {
    public:
        PspBootHost();
        int Run();

    private:
        bool InitializeKernel();
        bool InitializeGraphics();
        void PresentFrame();

        void* DisplayList;
    };
}
```

- [ ] **Step 3: Implement the minimal PSP homebrew/GU bootstrap**

Create `src/platform/psp/PspBootHost.cpp` with:

```cpp
#include "platform/psp/PspBootHost.hpp"

#include <cstddef>
#include <cstring>

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>

PSP_MODULE_INFO("helengine_psp", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

namespace helengine::psp {
    namespace {
        alignas(16) unsigned int DisplayListStorage[262144];
        constexpr unsigned int OrangeClearColor = 0xFF0080FF;
    }

    PspBootHost::PspBootHost()
        : DisplayList(DisplayListStorage) {
    }

    int PspBootHost::Run() {
        if (!InitializeKernel()) {
            return 1;
        }

        if (!InitializeGraphics()) {
            return 1;
        }

        while (true) {
            PresentFrame();
        }

        return 0;
    }

    bool PspBootHost::InitializeKernel() {
        return true;
    }

    bool PspBootHost::InitializeGraphics() {
        sceGuInit();

        sceGuStart(GU_DIRECT, DisplayList);
        sceGuDrawBuffer(GU_PSM_8888, reinterpret_cast<void*>(0), 512);
        sceGuDispBuffer(480, 272, reinterpret_cast<void*>(0x88000), 512);
        sceGuDepthBuffer(reinterpret_cast<void*>(0x110000), 512);
        sceGuOffset(2048 - (480 / 2), 2048 - (272 / 2));
        sceGuViewport(2048, 2048, 480, 272);
        sceGuDepthRange(65535, 0);
        sceGuScissor(0, 0, 480, 272);
        sceGuEnable(GU_SCISSOR_TEST);
        sceGuDepthMask(GU_TRUE);
        sceGuDisable(GU_DEPTH_TEST);
        sceGuFrontFace(GU_CW);
        sceGuShadeModel(GU_FLAT);
        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuDisplay(GU_TRUE);

        return true;
    }

    void PspBootHost::PresentFrame() {
        sceGuStart(GU_DIRECT, DisplayList);
        sceGuClearColor(OrangeClearColor);
        sceGuClear(GU_COLOR_BUFFER_BIT);
        sceGuFinish();
        sceGuSync(0, 0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }
}
```

- [ ] **Step 4: Verify the source file shape before container build work**

Run:

```bash
rtk sed -n '1,220p' src/main.cpp
rtk sed -n '1,260p' src/platform/psp/PspBootHost.hpp
rtk sed -n '1,360p' src/platform/psp/PspBootHost.cpp
```

Expected: `main.cpp` delegates immediately to `PspBootHost`, the namespace is consistent, and the PSP module metadata remains in the `.cpp` file.

- [ ] **Step 5: Commit the PSP bootstrap source**

Run:

```bash
rtk git add src/main.cpp src/platform/psp/PspBootHost.hpp src/platform/psp/PspBootHost.cpp
rtk git commit -m "Add PSP orange-screen bootstrap"
```

Expected: commit succeeds with only the new source files staged.

### Task 3: Discover The Actual PSP SDK Layout And Align The Build

**Files:**
- Modify: `Dockerfile`
- Modify: `Makefile`

- [ ] **Step 1: Build the container and inspect the available PSP toolchain commands**

Run:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker build -t helengine-psp .
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm helengine-psp sh -lc 'env | sort | grep -E "PSP|PSPSDK|PSPDEV" || true'
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm helengine-psp sh -lc 'find / -maxdepth 4 \( -name "psp-config*" -o -name "pack-pbp*" -o -name "build.mak" -o -name "pspsdk*" \) 2>/dev/null | sort'
```

Expected: the output reveals the real PSP SDK environment variables, build fragments, and packaging tools available in the image.

- [ ] **Step 2: Update the Makefile to use the discovered PSP SDK conventions**

Modify `Makefile` so it adopts the actual SDK entry points discovered in Step 1. The final structure should include the real PSP SDK include file if present, for example:

```makefile
PSPSDK := /path/discovered/in/container
include $(PSPSDK)/lib/build.mak
```

or the SDK’s equivalent supported build fragment.

Expected: the Makefile stops being a guess and becomes aligned with the actual PSP homebrew SDK available inside the Docker image.

- [ ] **Step 3: Run the first full project build**

Run:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-psp make
```

Expected: either the build succeeds and produces `EBOOT.PBP`, or it fails with concrete PSP SDK/toolchain errors that can be fixed directly.

- [ ] **Step 4: Fix the first concrete PSP SDK mismatch only**

Based on Step 3 output, make the smallest needed update to `Dockerfile` or `Makefile`. Typical examples:

```text
- add a missing PSP dev package to the image
- switch from a guessed packaging command to the SDK’s actual tool
- add the SDK’s required module/export flags
- correct the final output path or build include fragment
```

Expected: one concrete PSP build mismatch is resolved at a time instead of speculative changes.

- [ ] **Step 5: Re-run the PSP build after the fix**

Run:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker build -t helengine-psp .
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-psp make clean
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-psp make
```

Expected: the build emits a packaged PSP homebrew artifact ending in `EBOOT.PBP`.

- [ ] **Step 6: Commit the SDK alignment changes**

Run:

```bash
rtk git add Dockerfile Makefile
rtk git commit -m "Align PSP build with SDK toolchain"
```

Expected: commit succeeds with only the build-system adjustments staged.

### Task 4: Document And Verify The PSP Flow

**Files:**
- Create: `README.md`

- [ ] **Step 1: Write the README with the exact Docker and PPSSPP workflow**

Create `README.md` with:

````md
# Helengine PSP Host

This repository contains the native PSP host scaffold for Helengine.

## Current milestone

- Docker-only build using a PSP homebrew SDK toolchain
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

## Output

The build produces a PSP homebrew package ending in `EBOOT.PBP`.

## Generated core seam

The native build reserves `HELENGINE_CORE_CPP_ROOT` for later `cs2.cpp` integration, but the first milestone does not compile generated core output yet.

## Boot check

Load the generated `EBOOT.PBP` in PPSSPP. The expected result for this milestone is a solid orange frame with no immediate crash or reset loop.
````

- [ ] **Step 2: Check the README for accuracy against the final build output**

Run:

```bash
rtk sed -n '1,240p' README.md
rtk sed -n '1,260p' Makefile
rtk sed -n '1,200p' Dockerfile
```

Expected: the documented image name, generated-core macro, and packaged output all match the actual scaffold.

- [ ] **Step 3: Verify the packaged artifact in PPSSPP**

Run manually:

```text
Open the generated EBOOT.PBP in PPSSPP and observe the first rendered frame.
```

Expected: PPSSPP immediately shows a stable solid orange screen with no required input.

- [ ] **Step 4: Commit the documentation**

Run:

```bash
rtk git add README.md
rtk git commit -m "Document PSP bootstrap workflow"
```

Expected: commit succeeds with the README only.

### Task 5: Final Verification And Cleanup

**Files:**
- Verify: `Dockerfile`
- Verify: `Makefile`
- Verify: `README.md`
- Verify: `src/main.cpp`
- Verify: `src/platform/psp/PspBootHost.hpp`
- Verify: `src/platform/psp/PspBootHost.cpp`

- [ ] **Step 1: Re-run the full build from a clean state**

Run:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-psp make clean
DOCKER_CONFIG=/tmp/docker-no-creds docker build -t helengine-psp .
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-psp make
```

Expected: the build completes from a clean state and regenerates the packaged PSP homebrew output.

- [ ] **Step 2: Confirm the repo only contains valid tracked files**

Run:

```bash
rtk git status --short
```

Expected: no stray temp files are left behind; only intended ignored build output may remain.

- [ ] **Step 3: Create the final polish commit if verification required code changes**

Run:

```bash
rtk git add Dockerfile Makefile README.md src/main.cpp src/platform/psp/PspBootHost.hpp src/platform/psp/PspBootHost.cpp
rtk git commit -m "Polish PSP bootstrap verification"
```

Expected: skip this step if no files changed during final verification; otherwise create one last small cleanup commit.

## Self-Review

- Spec coverage check:
  - Dockerized PSP build: covered by Tasks 1, 3, and 5
  - Normal PSP homebrew packaging ending in `EBOOT.PBP`: covered by Tasks 1, 3, and 4
  - Generated-core seam: covered by Tasks 1 and 4
  - Real GU rendering path for future 3D readiness: covered by Task 2
  - Orange-screen PPSSPP verification: covered by Tasks 4 and 5
- Placeholder scan:
  - No `TODO`, `TBD`, or “similar to Task N” placeholders remain
- Type/name consistency:
  - All source files consistently use `PspBootHost`
  - The generated-core macro is consistently `HELENGINE_PSP_HAS_GENERATED_CORE`
  - The packaged artifact is consistently described as ending in `EBOOT.PBP`
