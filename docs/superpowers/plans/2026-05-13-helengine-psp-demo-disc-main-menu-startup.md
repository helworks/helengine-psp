# PSP Demo Disc Main Menu Startup Build Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a fresh PSP build that boots into the city demo disc main menu instead of the textured cube grid startup scene.

**Architecture:** Keep the already-merged PSP runtime startup path unchanged and swap only the city PSP persisted startup-scene selection from `textured_cube_grid` to `DemoDiscMainMenu`. Then regenerate the PSP export through the editor CLI, install the produced `PSP/GAME/HELENGINE` tree into the PPSSPP memstick, and verify from the boot log that the runtime loads the cooked main menu scene and reaches the main loop.

**Tech Stack:** PowerShell, .NET 9 editor CLI, city project build config JSON, PSP builder/runtime startup path, PPSSPP

---

### Task 1: Switch The City PSP Startup Scene To The Demo Disc Main Menu

**Files:**
- Modify: `C:\dev\helprojs\city\user_settings\build_config.json`

- [ ] **Step 1: Write the intended PSP build-config shape**

```json
{
  "platformId": "psp",
  "selectedSceneIds": [
    "DemoDiscMainMenu"
  ],
  "sceneOrders": [
    {
      "sceneId": "DemoDiscMainMenu",
      "orderNumber": 1
    }
  ],
  "outputDirectoryPath": "C:\\dev\\helprojs\\output\\psp-platform-info-startup",
  "debugBuild": true
}
```

- [ ] **Step 2: Verify the current PSP startup scene still targets the textured cube grid**

Run:
```powershell
rtk powershell -NoProfile -Command "Get-Content 'C:\dev\helprojs\city\user_settings\build_config.json'"
```

Expected:
- PASS with the PSP platform entry showing:
  - `"selectedSceneIds": [ "textured_cube_grid" ]`
  - `"sceneOrders": [ { "sceneId": "textured_cube_grid", "orderNumber": 1 } ]`

- [ ] **Step 3: Update the PSP platform entry to use only `DemoDiscMainMenu`**

```powershell
$buildConfigPath = 'C:\dev\helprojs\city\user_settings\build_config.json'
$document = Get-Content $buildConfigPath -Raw | ConvertFrom-Json
$pspPlatform = $document.platforms | Where-Object { $_.platformId -eq 'psp' } | Select-Object -First 1
if (-not $pspPlatform) {
    throw 'The city build config is missing the PSP platform entry.'
}

$pspPlatform.selectedSceneIds = @('DemoDiscMainMenu')
$pspPlatform.sceneOrders = @(
    [pscustomobject]@{
        sceneId = 'DemoDiscMainMenu'
        orderNumber = 1
    }
)

$document | ConvertTo-Json -Depth 10 | Set-Content $buildConfigPath
```

- [ ] **Step 4: Re-read the city PSP build config and verify the scene swap**

Run:
```powershell
rtk powershell -NoProfile -Command "Get-Content 'C:\dev\helprojs\city\user_settings\build_config.json'"
```

Expected:
- PASS with the PSP platform entry showing:
  - `"selectedSceneIds": [ "DemoDiscMainMenu" ]`
  - `"sceneOrders": [ { "sceneId": "DemoDiscMainMenu", "orderNumber": 1 } ]`

- [ ] **Step 5: Commit the city startup-scene config change**

```bash
git -C C:\dev\helprojs\city add user_settings/build_config.json
git -C C:\dev\helprojs\city commit -m "Set PSP startup scene to demo disc main menu"
```

### Task 2: Regenerate The PSP Export Through The Existing Editor CLI

**Files:**
- Uses: `C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll`
- Produces: `C:\dev\helprojs\output\psp-platform-info-startup\PSP\GAME\HELENGINE\**`

- [ ] **Step 1: Verify the built editor app exists before invoking the headless build**

Run:
```powershell
rtk powershell -NoProfile -Command "Get-Item 'C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll' | Select-Object FullName,LastWriteTime,Length"
```

Expected:
- PASS with one file entry for `helengine.editor.app.dll`

- [ ] **Step 2: Run the headless PSP export against the city project**

```powershell
$env:HELENGINE_PSP_REPOSITORY_ROOT = 'C:\dev\helworks\helengine-psp'
rtk dotnet 'C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll' --project 'C:\dev\helprojs\city\project.heproj' --build psp --output 'C:\dev\helprojs\output\psp-platform-info-startup'
```

- [ ] **Step 3: Verify the editor CLI export succeeds**

Run:
```powershell
rtk powershell -NoProfile -Command "Test-Path 'C:\dev\helprojs\output\psp-platform-info-startup\PSP\GAME\HELENGINE\EBOOT.PBP'"
```

Expected:
- PASS with output `True`

- [ ] **Step 4: Verify the cooked main menu scene exists in the PSP output tree**

Run:
```powershell
rtk powershell -NoProfile -Command "Get-ChildItem 'C:\dev\helprojs\output\psp-platform-info-startup\PSP\GAME\HELENGINE\cooked\scenes' -Recurse | Select-Object FullName,Length"
```

Expected:
- PASS with a cooked scene file for the demo disc main menu under the output tree

- [ ] **Step 5: Commit only if the PSP repo itself changed during the export**

```bash
git -C C:\dev\helworks\helengine-psp status --short
```

Expected:
- PASS with no new PSP repo changes from the export path

### Task 3: Install The PSP Output Into PPSSPP And Verify The Boot Path

**Files:**
- Uses: `C:\dev\helprojs\output\psp-platform-info-startup\PSP\GAME\HELENGINE\**`
- Writes: `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\**`
- Verify: `C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log`

- [ ] **Step 1: Install the generated PSP app tree into the PPSSPP memstick**

```powershell
$sourceRoot = 'C:\dev\helprojs\output\psp-platform-info-startup\PSP\GAME\HELENGINE'
$memstickRoot = 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE'

New-Item -ItemType Directory -Path $memstickRoot -Force | Out-Null
Copy-Item (Join-Path $sourceRoot '*') $memstickRoot -Recurse -Force
```

- [ ] **Step 2: Launch PPSSPP hidden and capture a fresh boot log**

```powershell
$ppssppExe = 'C:\dev\helworks\emus\ppsspp_win\PPSSPPWindows64.exe'
$ebootPath = 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\EBOOT.PBP'
$bootLogPath = 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log'

Stop-Process -Name 'PPSSPPWindows64' -Force -ErrorAction SilentlyContinue
if (Test-Path $bootLogPath) {
    Remove-Item $bootLogPath -Force
}

$process = Start-Process -FilePath $ppssppExe -ArgumentList $ebootPath -WindowStyle Hidden -PassThru
Start-Sleep -Seconds 20
if (Get-Process -Id $process.Id -ErrorAction SilentlyContinue) {
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
}
Get-Content $bootLogPath -Tail 120
```

- [ ] **Step 3: Verify the boot log loads the cooked main menu scene and reaches the main loop**

Run:
```powershell
rtk powershell -NoProfile -Command "Get-Content 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log' -Tail 120"
```

Expected:
- PASS with boot-log lines that show:
  - `Runtime platform info resolved to 'psp' version '1.0.0'.`
  - `LoadAsset path=ms0:/PSP/GAME/HELENGINE/cooked/scenes/DemoDiscMainMenu...`
  - `Stage complete RuntimeMainLoop`

- [ ] **Step 4: Launch PPSSPP visibly for user confirmation**

```powershell
Stop-Process -Name 'PPSSPPWindows64' -Force -ErrorAction SilentlyContinue
Start-Process -FilePath 'C:\dev\helworks\emus\ppsspp_win\PPSSPPWindows64.exe' -ArgumentList 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\EBOOT.PBP'
```

- [ ] **Step 5: Stop and ask the user what appears on screen**

Expected:
- PASS when the user confirms whether the visible PSP build shows the demo disc main menu or a new runtime issue
