param(
    [switch]$SkipBuild,
    [string]$Serial = $env:ANDROID_SERIAL
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

$adbCommand = Get-Command adb -ErrorAction SilentlyContinue
$adbPath = if ($adbCommand) { $adbCommand.Source } else { $null }
if (!$adbPath) {
    foreach ($sdkRoot in @($env:ANDROID_HOME, $env:ANDROID_SDK_ROOT, (Join-Path $env:LOCALAPPDATA 'Android/Sdk'))) {
        if (!$sdkRoot) { continue }
        $candidate = Join-Path $sdkRoot 'platform-tools/adb.exe'
        if (Test-Path -LiteralPath $candidate) { $adbPath = $candidate; break }
    }
}
if (!$adbPath) { throw 'ADB not found. Add Android platform-tools to PATH or set ANDROID_HOME.' }

# Check the device before spending time on the engine build.
$deviceLines = & $adbPath devices
if ($LASTEXITCODE) { throw 'ADB could not list Android devices.' }
$ready = @($deviceLines | ForEach-Object {
    if ($_ -match '^(\S+)\s+device$') { $Matches[1] }
})
if ($Serial) {
    if ($ready -notcontains $Serial) { throw "Android device '$Serial' is not ready. Check adb devices and authorize USB debugging." }
} elseif ($ready.Count -eq 1) {
    $Serial = $ready[0]
} elseif ($ready.Count -gt 1) {
    Write-Host 'Choose the Android device to launch:'
    for ($deviceIndex = 0; $deviceIndex -lt $ready.Count; $deviceIndex++) {
        Write-Host ("  {0}. {1}" -f ($deviceIndex + 1), $ready[$deviceIndex])
    }
    do {
        $answer = Read-Host "Device number (1-$($ready.Count)), or Q to cancel"
        if ($answer -match '^[qQ]$') { throw 'Android launch cancelled.' }
        $selection = 0
        $valid = [int]::TryParse($answer, [ref]$selection) -and $selection -ge 1 -and $selection -le $ready.Count
        if (!$valid) { Write-Host 'Enter one of the listed device numbers.' }
    } while (!$valid)
    $Serial = $ready[$selection - 1]
} else {
    throw 'No ready Android device. Connect a device with USB debugging enabled and authorize this computer, or start an arm64 emulator.'
}

if (!$SkipBuild) { & (Join-Path $PSScriptRoot 'build.ps1') }
$apkPath = Join-Path $projectRoot 'build/android-arm64/ZenkaiPower-debug.apk'
if (!(Test-Path -LiteralPath $apkPath)) { throw "Debug APK missing: $apkPath" }

Write-Host "Installing Android debug build on $Serial"
& $adbPath -s $Serial install -r $apkPath
if ($LASTEXITCODE) { throw 'Android APK installation failed. See the ADB error above.' }
& $adbPath -s $Serial shell am force-stop com.zenkaipower.android.debug
if ($LASTEXITCODE) { throw 'Could not stop the previous Android instance.' }
& $adbPath -s $Serial shell am start -W -n com.zenkaipower.android.debug/com.zenkaipower.android.LauncherActivity
if ($LASTEXITCODE) { throw 'Android app launch failed.' }
