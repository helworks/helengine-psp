$ErrorActionPreference = 'Stop'

function Update-PlatformsConfig {
    param(
        [string] $PlatformsPath
    )

    $json = Get-Content -LiteralPath $PlatformsPath -Raw | ConvertFrom-Json
    $psp = $json.platforms | Where-Object { $_.platformId -eq 'psp' } | Select-Object -First 1
    if ($null -eq $psp) {
        throw "PSP platform entry was not found in $PlatformsPath."
    }

    $psp.builderAssemblyPath = "../../helengine-psp/builder/bin/Debug/net9.0/helengine.psp.builder.dll"
    $psp.playerSourceRootPath = "../../helengine-psp"

    $json | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $PlatformsPath
}

function Update-CityBuildConfig {
    param(
        [string] $BuildConfigPath
    )

    $json = Get-Content -LiteralPath $BuildConfigPath -Raw | ConvertFrom-Json
    $psp = $json.platforms | Where-Object { $_.platformId -eq 'psp' } | Select-Object -First 1
    if ($null -eq $psp) {
        throw "PSP build entry was not found in $BuildConfigPath."
    }

    $psp.selectedSceneIds = @(
        'DemoDiscMainMenu',
        'cube_test',
        'colored_cube_grid',
        'textured_cube_grid',
        'axis_test',
        'axis_test2',
        'directional_shadow_plaza'
    )
    $psp.sceneOrders = @(
        [pscustomobject]@{
            sceneId = 'DemoDiscMainMenu'
            orderNumber = 1
        },
        [pscustomobject]@{
            sceneId = 'cube_test'
            orderNumber = 2
        },
        [pscustomobject]@{
            sceneId = 'colored_cube_grid'
            orderNumber = 3
        },
        [pscustomobject]@{
            sceneId = 'textured_cube_grid'
            orderNumber = 4
        },
        [pscustomobject]@{
            sceneId = 'axis_test'
            orderNumber = 5
        },
        [pscustomobject]@{
            sceneId = 'axis_test2'
            orderNumber = 6
        },
        [pscustomobject]@{
            sceneId = 'directional_shadow_plaza'
            orderNumber = 7
        }
    )
    $psp.debugBuild = $true
    $psp.selectedBuildProfileId = 'debug'
    $psp.selectedGraphicsProfileId = 'psp-forward'
    $psp.selectedCodegenProfileId = 'default'
    $psp.selectedStorageProfileId = 'homebrew-app'
    $psp.selectedMediaProfileId = 'psp-game-folder'
    $psp.selectedBuildOptionValues = [pscustomobject]@{
        'texture-scale-percent' = '100'
        'shader-variant-pruning' = 'true'
    }
    $psp.selectedGraphicsOptionValues = [pscustomobject]@{
        'default-width' = '480'
        'default-height' = '272'
        'vsync-enabled' = 'true'
        'fullscreen-enabled' = 'false'
    }
    $psp.selectedCodegenOptionValues = [pscustomobject]@{
        'write-conversion-report' = 'true'
        'include-project-defined-preprocessor-symbols' = 'false'
        'load-native-runtime-metadata' = 'true'
    }

    $json | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $BuildConfigPath
}

function Update-CityDemoDiscSceneCatalog {
    param(
        [string] $SceneCatalogPath
    )

    $content = Get-Content -LiteralPath $SceneCatalogPath -Raw
    $spotlightLine = '                new MenuItemDefinition("scene-spotlight-street-slice", "Spotlight Street Slice", "Street-lit showcase that validates spotlights and prop placement.", true, new MenuActionDefinition(MenuActionKind.LoadScene, "spotlight_street_slice")),' + [Environment]::NewLine
    $updatedContent = $content.Replace($spotlightLine, [string]::Empty)
    if ($updatedContent -eq $content) {
        throw "Spotlight Street Slice menu entry was not found in $SceneCatalogPath."
    }

    Set-Content -LiteralPath $SceneCatalogPath -Value $updatedContent
}

Update-PlatformsConfig -PlatformsPath 'C:\dev\helworks\helengine\user_settings\platforms.json'
Update-CityBuildConfig -BuildConfigPath 'C:\dev\helprojs\city\user_settings\build_config.json'
Update-CityDemoDiscSceneCatalog -SceneCatalogPath 'C:\dev\helprojs\city\assets\codebase\menu\DemoDiscSceneCatalog.cs'
