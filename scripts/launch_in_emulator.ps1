param(
    [Parameter(Mandatory = $true)]
    [string]$ArtifactPath
)

$ErrorActionPreference = 'Stop'

$resolvedArtifactPath = [System.IO.Path]::GetFullPath($ArtifactPath)
if (-not (Test-Path -LiteralPath $resolvedArtifactPath -PathType Leaf)) {
    throw "Artifact was not found: $resolvedArtifactPath."
}

if ([System.IO.Path]::GetFileName($resolvedArtifactPath) -ine 'EBOOT.PBP') {
    throw "Expected the PSP artifact to be EBOOT.PBP but got '$resolvedArtifactPath'."
}

$sourceRoot = Split-Path -Path $resolvedArtifactPath -Parent
if (-not (Test-Path -LiteralPath $sourceRoot -PathType Container)) {
    throw "PSP app root was not found at $sourceRoot."
}

$sourceCookedRoot = Join-Path $sourceRoot 'cooked'
if (-not (Test-Path -LiteralPath $sourceCookedRoot -PathType Container)) {
    throw "Expected the PSP app root to contain cooked payloads at $sourceCookedRoot."
}

$ppssppExePath = 'C:\dev\helworks\emus\ppsspp_win\PPSSPPWindows64.exe'
$targetRoot = 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE'
$targetEbootPath = Join-Path $targetRoot 'EBOOT.PBP'

if (-not (Test-Path -LiteralPath $ppssppExePath -PathType Leaf)) {
    throw "PPSSPP executable was not found at $ppssppExePath."
}

$resolvedTargetRoot = [System.IO.Path]::GetFullPath($targetRoot)
if ($resolvedTargetRoot -ne 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE') {
    throw "Refusing to replace unexpected PPSSPP target path $resolvedTargetRoot."
}

$existingPpssppProcesses = @(Get-Process -Name 'PPSSPPWindows64' -ErrorAction SilentlyContinue)
foreach ($process in $existingPpssppProcesses) {
    Stop-Process -Id $process.Id -Force
}

if (Test-Path -LiteralPath $targetRoot) {
    Remove-Item -LiteralPath $targetRoot -Recurse -Force
}

Copy-Item -LiteralPath $sourceRoot -Destination $targetRoot -Recurse -Force

$artifactItem = Get-Item -LiteralPath $resolvedArtifactPath
Write-Output ("ARTIFACT=" + $resolvedArtifactPath)
Write-Output ("ARTIFACT_LAST_WRITE_TIME=" + $artifactItem.LastWriteTime.ToString('O'))
Write-Output ("SOURCE_ROOT=" + $sourceRoot)
Write-Output ("PPSSPP=" + $ppssppExePath)
Write-Output ("TARGET_EBOOT=" + $targetEbootPath)

$process = Start-Process -FilePath $ppssppExePath -ArgumentList $targetEbootPath -WorkingDirectory (Split-Path -Path $ppssppExePath -Parent) -PassThru
Write-Output ("PROCESS_ID=" + $process.Id)
