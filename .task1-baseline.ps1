$ErrorActionPreference = 'Stop'

$buildConfigPath = 'C:\dev\helprojs\city\user_settings\build_config.json'
$backupPath = $buildConfigPath + '.gpu-lighting-plan.bak'
Copy-Item -LiteralPath $buildConfigPath -Destination $backupPath -Force

try {
    $buildConfig = Get-Content -LiteralPath $buildConfigPath -Raw | ConvertFrom-Json
    $pspPlatform = $buildConfig.platforms | Where-Object { $_.platformId -eq 'psp' }
    if ($null -eq $pspPlatform) {
        throw 'PSP platform configuration was not found.'
    }

    $pspPlatform.sceneOrders = @(
        [pscustomobject]@{ sceneId = 'cube_test'; orderNumber = 1 },
        [pscustomobject]@{ sceneId = 'DemoDiscMainMenu'; orderNumber = 2 },
        [pscustomobject]@{ sceneId = 'colored_cube_grid'; orderNumber = 3 },
        [pscustomobject]@{ sceneId = 'textured_cube_grid'; orderNumber = 4 },
        [pscustomobject]@{ sceneId = 'axis_test'; orderNumber = 5 },
        [pscustomobject]@{ sceneId = 'axis_test2'; orderNumber = 6 },
        [pscustomobject]@{ sceneId = 'directional_shadow_plaza'; orderNumber = 7 },
        [pscustomobject]@{ sceneId = 'spotlight_street_slice'; orderNumber = 8 }
    )

    $buildConfig | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $buildConfigPath

    rtk dotnet 'C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll' --project 'C:\dev\helprojs\city\project.heproj' --build psp --output 'C:\dev\helprojs\output\psp-cube-test-gpu-lighting'

    $workspaceRoot = Get-ChildItem "$env:LOCALAPPDATA\Temp\helengine-platform-build\psp" -Directory | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if ($null -eq $workspaceRoot) {
        throw 'No PSP generated-core workspace was found.'
    }

    $generatedCoreRoot = Join-Path $workspaceRoot.FullName 'generated-core'
    rtk proxy docker run --rm -v C:/dev/helworks/helengine-psp:/workspace -v "${generatedCoreRoot}:/generated-core" -w /workspace helengine-psp make clean all HELENGINE_CORE_CPP_ROOT=/generated-core HELENGINE_PSP_ISOLATED_BOOT=ON HELENGINE_PSP_ENABLE_RUNTIME_STARTUP=ON HELENGINE_PSP_ENABLE_BOOT_TRACE=ON HELENGINE_PSP_ENABLE_RENDER_PROFILER=ON

    $sourceRoot = 'C:\dev\helprojs\output\psp-cube-test-gpu-lighting\PSP\GAME\HELENGINE'
    $targetRoot = 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE'
    if (Test-Path -LiteralPath $targetRoot) {
        Remove-Item -LiteralPath $targetRoot -Recurse -Force
    }

    Copy-Item -LiteralPath $sourceRoot -Destination $targetRoot -Recurse -Force
    Copy-Item -LiteralPath 'C:\dev\helworks\helengine-psp\build\EBOOT.PBP' -Destination (Join-Path $targetRoot 'EBOOT.PBP') -Force

    $bootLog = rtk proxy powershell -File C:\dev\helworks\helengine-psp\tools\run_ppsspp_boot_check.ps1
    $bootLog | Select-String 'PspPerfFrame3D|LoadStartupScene|gpuDirectionalCount|cpuFallbackCount'
} finally {
    if (Test-Path -LiteralPath $backupPath) {
        Move-Item -LiteralPath $backupPath -Destination $buildConfigPath -Force
    }
}
