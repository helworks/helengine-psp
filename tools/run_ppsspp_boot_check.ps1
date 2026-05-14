$ErrorActionPreference = 'Stop'

$ppssppExePath = 'C:\dev\helworks\emus\ppsspp_win\PPSSPPWindows64.exe'
$ebootPath = 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\EBOOT.PBP'
$bootLogPath = 'C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE\helengine_psp_boot.log'

if (-not (Test-Path -LiteralPath $ppssppExePath)) {
    throw "PPSSPP executable was not found at $ppssppExePath."
}

if (-not (Test-Path -LiteralPath $ebootPath)) {
    throw "EBOOT.PBP was not found at $ebootPath."
}

if (Test-Path -LiteralPath $bootLogPath) {
    Remove-Item -LiteralPath $bootLogPath -Force
}

$process = Start-Process -FilePath $ppssppExePath -ArgumentList $ebootPath -WindowStyle Hidden -PassThru
try {
    Start-Sleep -Seconds 20
    $runningBeforeStop = -not $process.HasExited
    if ($runningBeforeStop) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }

    if (-not (Test-Path -LiteralPath $bootLogPath)) {
        throw "Boot log was not produced at $bootLogPath."
    }

    $processStatus = 'exited-before-stop'
    if ($runningBeforeStop) {
        $processStatus = 'running-before-stop'
    }

    Write-Output "PPSSPP_PROCESS_STATUS=$processStatus"
    Get-Content -LiteralPath $bootLogPath
} finally {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
    }
}
