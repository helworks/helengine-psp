$ErrorActionPreference = 'Stop'

$sourceRoot = 'C:\dev\helprojs\output\psp-platform-info-startup\PSP\GAME\HELENGINE'
$targetRoot = 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE'

if (-not (Test-Path -LiteralPath $sourceRoot)) {
    throw "PSP build output was not found at $sourceRoot."
}

$resolvedTargetRoot = [System.IO.Path]::GetFullPath($targetRoot)
if ($resolvedTargetRoot -ne 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE') {
    throw "Refusing to replace unexpected PPSSPP target path $resolvedTargetRoot."
}

if (Test-Path -LiteralPath $targetRoot) {
    Remove-Item -LiteralPath $targetRoot -Recurse -Force
}

Copy-Item -LiteralPath $sourceRoot -Destination $targetRoot -Recurse -Force
