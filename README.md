# Helengine PSP Host

This repository contains the PSP platform host and builder integration for Helengine.

## Build

```powershell
dotnet run --project ..\helengine\tools\build-waiter\helengine.buildwaiter.csproj -- `
  --output ..\helprojs\city\psp-build `
  --require PSP/GAME/HELENGINE/EBOOT.PBP `
  -- powershell -NoProfile -ExecutionPolicy Bypass -File ..\helengine\scripts\build-platform.ps1 `
  -Project ..\helprojs\city\project.heproj `
  -Platform psp `
  -Output ..\helprojs\city\psp-build
```

The Build Waiter returns successfully only after `PSP/GAME/HELENGINE/EBOOT.PBP` is fresh and non-empty.

## Run In Emulator

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\launch_in_emulator.ps1 `
  -ArtifactPath ..\helprojs\city\psp-build\PSP\GAME\HELENGINE\EBOOT.PBP
```

## More Docs

- [Docker Build Notes](docs/Docker.md)
