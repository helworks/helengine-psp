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

    $psp.selectedSceneIds = @('DemoDiscMainMenu')
    $psp.sceneOrders = @(
        [pscustomobject]@{
            sceneId = 'DemoDiscMainMenu'
            orderNumber = 1
        }
    )

    $json | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $BuildConfigPath
}

Update-PlatformsConfig -PlatformsPath 'C:\dev\helworks\helengine\user_settings\platforms.json'
Update-CityBuildConfig -BuildConfigPath 'C:\dev\helprojs\city\user_settings\build_config.json'
