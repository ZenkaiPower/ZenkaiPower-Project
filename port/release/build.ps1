param(
    [ValidateSet('all', 'linux', 'win', 'android')]
    [string]$Platform = 'win',
    [Alias('-release')]
    [switch]$Release
)

$ErrorActionPreference = 'Stop'
function Get-DockerEngine {
    # Windows PowerShell can otherwise turn native stderr into a terminating error.
    $ErrorActionPreference = 'Continue'
    $engine = & docker info --format '{{.OSType}}' 2>$null
    if ($LASTEXITCODE -eq 0) { return $engine }
}
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $repoRoot
try {
    if ($Platform -eq 'android') {
        & (Join-Path $repoRoot 'android/build.ps1') -Release:$Release
        if ($LASTEXITCODE) { throw 'Android release build failed.' }
        return
    }
    if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
        throw 'Docker was not found. Install Docker Desktop and enable Linux containers, then retry F5.'
    }
    $dockerEngine = Get-DockerEngine
    if (-not $dockerEngine) {
        # Docker Desktop is commonly stopped when the editor is first opened.
        $dockerExe = (Get-Command docker).Source
        $desktopPath = Join-Path (Split-Path (Split-Path (Split-Path $dockerExe))) 'Docker Desktop.exe'
        if (-not (Test-Path -LiteralPath $desktopPath)) {
            throw 'Docker is not running. Start Docker Desktop, wait for its Linux engine to be ready, then retry F5.'
        }
        Write-Host 'Starting Docker Desktop; waiting up to 120 seconds for the Linux engine...'
        Start-Process -FilePath $desktopPath -WindowStyle Hidden
        $deadline = (Get-Date).AddSeconds(120)
        do {
            Start-Sleep -Seconds 2
            $dockerEngine = Get-DockerEngine
            $dockerReady = [bool]$dockerEngine
        } until ($dockerReady -or (Get-Date) -ge $deadline)
        if (-not $dockerReady) {
            throw 'Docker Desktop did not become ready. Open Docker Desktop, resolve any startup errors, enable Linux containers, then retry F5.'
        }
    }
    if ($dockerEngine -ne 'linux') {
        throw 'The release build requires Linux containers. Switch Docker Desktop to Linux containers, then retry F5.'
    }
    if ($env:ZENKAIPOWER_SKIP_IMAGE_BUILD -eq '1') {
        & docker image inspect zenkaipower | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw 'Release image not found. Unset ZENKAIPOWER_SKIP_IMAGE_BUILD and retry F5 to build it.'
        }
    } else {
        & docker build --progress=plain --platform linux/amd64 -t zenkaipower .
        if ($LASTEXITCODE -ne 0) {
            throw 'Release Docker image build failed. See the Docker output above for the failing step.'
        }
    }

    # Keep compiler intermediates on Linux storage. Docker Desktop's Windows
    # bind mounts can transiently lose files during parallel compilation.
    # Scope the persistent cache to this checkout so other worktrees are isolated.
    $hasher = [System.Security.Cryptography.SHA256]::Create()
    try {
        $checkoutHash = [BitConverter]::ToString($hasher.ComputeHash(
            [System.Text.Encoding]::UTF8.GetBytes($repoRoot.ToLowerInvariant())
        )).Replace('-', '').Substring(0, 12).ToLowerInvariant()
    } finally {
        $hasher.Dispose()
    }
    $buildVolume = "zenkaipower-release-$checkoutHash"
    $buildMutex = [System.Threading.Mutex]::new($false, "Local\$buildVolume")
    $ownsBuildMutex = $false
    try {
        try {
            $ownsBuildMutex = $buildMutex.WaitOne(0)
        } catch [System.Threading.AbandonedMutexException] {
            $ownsBuildMutex = $true
        }
        if (-not $ownsBuildMutex) {
            throw 'A release build is already running for this checkout. Wait for it to finish before pressing F5 again.'
        }
        $activeBuilds = & docker ps -q --filter "volume=$buildVolume"
        if ($LASTEXITCODE -ne 0) { throw 'Could not check for an existing release build.' }
        if ($activeBuilds) {
            throw 'A release container is already using this checkout cache. Wait for it to finish before pressing F5 again.'
        }
        $releaseContainer = "$buildVolume-output"
        # All generated files stay on Linux storage. Export through Docker's
        # copy API, avoiding EIO from writes through Windows bind mounts.
        $releaseVolume = "$buildVolume-artifacts"
        # Native releases do not use the decomp entrypoint's PS2 tool links.
        # Creating those links on a Windows bind mount can fail before the build.
        $dockerArgs = @('run', '--name', $releaseContainer, '--entrypoint', '/bin/sh', '-e', 'HOME=/tmp')
        if ([string]::IsNullOrEmpty($env:ZENKAIPOWER_JOBS)) {
            # Docker may expose many CPUs while its VM has only 2 GB of RAM.
            # LLVM pointer conversion and code generation run in each worker.
            $dockerResources = & docker info --format '{{json .}}' | ConvertFrom-Json
            if ($LASTEXITCODE -ne 0) { throw 'Could not inspect Docker build resources.' }
            $memoryJobs = [Math]::Max(1, [Math]::Floor([double]$dockerResources.MemTotal / 2GB))
            $defaultJobs = [int][Math]::Max(1, [Math]::Min(8, [Math]::Min($dockerResources.NCPU, $memoryJobs)))
            $dockerArgs += @('-e', "ZENKAIPOWER_JOBS=$defaultJobs")
            Write-Host "Using $defaultJobs compiler worker(s) for Docker's memory limit. Set ZENKAIPOWER_JOBS to override."
        } else {
            $dockerArgs += @('-e', 'ZENKAIPOWER_JOBS')
        }
        foreach ($name in @(
            'ZENKAIPOWER_FULL_REBUILD', 'ZENKAIPOWER_ZIP_LEVEL', 'ZENKAIPOWER_BUILD_KEY',
            'ZENKAIPOWER_RELEASE_TAG', 'ZENKAIPOWER_DOLPHIN_SOURCE_URL', 'GITHUB_REPOSITORY'
        )) {
            $dockerArgs += @('-e', $name)
        }
        $dockerArgs += @(
            '--mount', "type=bind,source=$repoRoot,target=/src",
            '--mount', "type=volume,source=$buildVolume,target=/src/port/build",
            '--mount', "type=volume,source=$releaseVolume,target=/src/release",
            '-w', '/src', 'zenkaipower',
            'port/release/inside.sh', $Platform
        )
        if ($Release) { $dockerArgs += '--release' }
        # Remove a stopped helper from an interrupted previous invocation.
        $previousContainer = & docker ps -aq --filter "name=^/$releaseContainer`$"
        if ($previousContainer) {
            & docker rm $releaseContainer | Out-Null
            if ($LASTEXITCODE -ne 0) { throw 'Could not remove the previous release helper.' }
        }
        try {
            & docker @dockerArgs
            if ($LASTEXITCODE -ne 0) { throw 'Release build failed.' }
            # The persistent artifacts volume also holds older versions. Export
            # only this build so locally deleted archives stay deleted.
            $version = (Get-Content -LiteralPath (Join-Path $repoRoot 'game-info.json') -Raw | ConvertFrom-Json).version
            if ($version -notmatch '^[vV]?[0-9]+(?:\.[0-9]+)*$') { throw 'Invalid release version.' }
            $version = $version -replace '^[vV]', ''
            & (Join-Path $PSScriptRoot "prepare-output.ps1") -RepoRoot $repoRoot
            $exportPlatforms = if ($Platform -eq 'all') { @('linux', 'windows') } elseif ($Platform -eq 'win') { @('windows') } else { @('linux') }
            New-Item -ItemType Directory -Force (Join-Path $repoRoot "release") | Out-Null
            foreach ($exportPlatform in $exportPlatforms) {
                $folder = "$exportPlatform-x64"
                $buildFolder = if ($exportPlatform -eq 'linux') { 'ZenkaiPower-Linux' } else { 'ZenkaiPower-Windows' }
                $destination = Join-Path $repoRoot "build/$buildFolder"
                New-Item -ItemType Directory -Force $destination | Out-Null
                & docker cp "${releaseContainer}:/src/release/$folder/." $destination
                if ($LASTEXITCODE -ne 0) { throw 'Release built, but exporting the game folder failed. Outputs remain in the Docker artifacts volume.' }
                $archive = "ZenkaiPower-v$version-$exportPlatform-x64.zip"
                & docker cp "${releaseContainer}:/src/release/$archive" (Join-Path $repoRoot "release/$archive")
                if ($LASTEXITCODE -ne 0) { throw 'Release built, but exporting the archive failed. Outputs remain in the Docker artifacts volume.' }
                & docker cp "${releaseContainer}:/src/release/$archive.sig" (Join-Path $repoRoot "release/$archive.sig")
                if ($LASTEXITCODE -ne 0) { throw 'Release built, but exporting the signature failed. Outputs remain in the Docker artifacts volume.' }
            }
        } finally {
            & docker rm $releaseContainer | Out-Null
        }
        Write-Host 'Binary releases: release/'
    } finally {
        if ($ownsBuildMutex) { $buildMutex.ReleaseMutex() }
        $buildMutex.Dispose()
    }
} finally {
    Pop-Location
}
