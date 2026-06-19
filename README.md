# Helengine PSP Host

This repository contains the PSP platform host and builder integration for Helengine.

## Build

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ..\helengine\artifacts\build-platform.ps1 `
  -Project ..\helprojs\city\project.heproj `
  -Platform psp `
  -Output ..\helprojs\city\psp-build
```

## Run In Emulator

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\launch_in_emulator.ps1 `
  -ArtifactPath ..\helprojs\city\psp-build\PSP\GAME\HELENGINE\EBOOT.PBP
```

## More Docs

- [Docker Build Notes](docs/Docker.md)
