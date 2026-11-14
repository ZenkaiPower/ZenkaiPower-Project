param([switch]$EngineOnly, [switch]$ApkOnly, [switch]$Release)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $projectRoot
try {
    if ($EngineOnly -and $ApkOnly) { throw 'Choose EngineOnly or ApkOnly.' }
    if ($env:ZENKAIPOWER_SKIP_IMAGE_BUILD -ne '1') {
        $ErrorActionPreference = 'Continue'
        docker build --platform linux/amd64 -t zenkaipower .
        $imageExitCode = $LASTEXITCODE
        $ErrorActionPreference = 'Stop'
        if ($imageExitCode) { throw 'Unified tools image build failed.' }
    }
    $buildContainer = 'zenkaipower-android-' + [Guid]::NewGuid().ToString('N')
    $dockerArgs = @('run', '--name', $buildContainer, '--platform', 'linux/amd64', '--entrypoint', '/bin/sh',
        '--mount', "type=bind,source=$projectRoot,target=/host", '-w', '/work',
        '--mount', 'type=volume,source=zenkaipower-android-work,target=/work',
        '--mount', 'type=volume,source=zenkaipower-android-toolchain,target=/opt/android-toolchain',
        '--mount', 'type=volume,source=zenkaipower-gradle,target=/root/.gradle',
        '--mount', 'type=volume,source=zenkaipower-android-user,target=/root/.android',
        '-e', 'HOME=/root', '-e', 'ZENKAIPOWER_JOBS', '-e', 'ZENKAIPOWER_ONLY', '-e', 'ZENKAIPOWER_BUILD_KEY',
        '-e', "ZENKAIPOWER_RELEASE=$([int][bool]$Release)", '-e', 'ZENKAIPOWER_SKELETON=1',
        '-e', 'ZENKAIPOWER_ANDROID_DOCKER_EXPORT=1',
        '-e', "ZENKAIPOWER_ANDROID_ENGINE=$([int](!$ApkOnly))", '-e', "ZENKAIPOWER_ANDROID_APK=$([int](!$EngineOnly))")
    $signingFile = $env:ZENKAI_SIGNING_PROPERTIES
    if (!$signingFile) { $signingFile = Join-Path $env:USERPROFILE '.android/zenkaipower-release.properties' }
    if (Test-Path -LiteralPath $signingFile) {
        $signingFile = (Resolve-Path -LiteralPath $signingFile).Path
        $keySetting = Get-Content -LiteralPath $signingFile | Where-Object { $_ -match '^storeFile=' } | Select-Object -First 1
        if (!$keySetting) { throw 'Signing properties must define storeFile.' }
        $keyFile = $keySetting.Substring('storeFile='.Length).Trim()
        if (![IO.Path]::IsPathRooted($keyFile)) { $keyFile = Join-Path (Split-Path $signingFile) $keyFile }
        $keyFile = (Resolve-Path -LiteralPath $keyFile).Path
        $dockerArgs += @('--mount', "type=bind,source=$signingFile,target=/signing/release.properties,readonly",
            '--mount', "type=bind,source=$keyFile,target=/signing/release.jks,readonly",
            '-e', 'ZENKAI_SIGNING_PROPERTIES=/signing/release.properties', '-e', 'ZENKAI_KEYSTORE=/signing/release.jks')
    }
    $dockerArgs += @('zenkaipower', '/host/android/container-build.sh')
    try {
        # Windows PowerShell otherwise turns harmless native stderr into an exception.
        $ErrorActionPreference = 'Continue'
        docker @dockerArgs
        $dockerExitCode = $LASTEXITCODE
        $ErrorActionPreference = 'Stop'
        if ($dockerExitCode) { throw 'Android container build failed.' }
        if (!$EngineOnly) {
            & (Join-Path $projectRoot "port/release/prepare-output.ps1") -RepoRoot $projectRoot
            $destination = Join-Path $projectRoot 'release'
            New-Item -ItemType Directory -Force $destination | Out-Null
            $apk = if ($Release) { 'ZenkaiPower-android.apk' } else { 'ZenkaiPower-debug.apk' }
            $exports = @()
            if (!$Release) {
                $debugDestination = Join-Path $projectRoot "build/android-arm64"
                New-Item -ItemType Directory -Force $debugDestination | Out-Null
                $exports += ,@("/work/release/android-arm64/$apk", (Join-Path $debugDestination $apk))
            }
            if ($Release) {
                $version = (Get-Content -LiteralPath (Join-Path $projectRoot 'game-info.json') -Raw | ConvertFrom-Json).version -replace '^[vV]', ''
                if ($version -notmatch '^[0-9]+(?:\.[0-9]+)*$') { throw 'Invalid release version.' }
                $stem = "ZenkaiPower-v$version-android-arm64"
                foreach ($suffix in @('.apk', '.apk.sha256')) {
                    $exports += ,@("/work/release/android-arm64/$stem$suffix", (Join-Path $destination "$stem$suffix"))
                }
            }
            foreach ($artifact in $exports) {
                docker cp "${buildContainer}:$($artifact[0])" $artifact[1]
                if ($LASTEXITCODE) { throw 'Android built, but export failed. Artifacts remain in the zenkaipower-android-work Docker volume under /work/release.' }
            }
        }
    } finally {
        docker rm $buildContainer | Out-Null
    }
} finally { Pop-Location }
