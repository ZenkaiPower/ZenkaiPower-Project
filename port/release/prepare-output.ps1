param([Parameter(Mandatory = $true)][string]$RepoRoot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $RepoRoot).Path
$version = (Get-Content -LiteralPath (Join-Path $root 'game-info.json') -Raw | ConvertFrom-Json).version -replace '^[vV]', ''
if ($version -notmatch '^[0-9]+(?:\.[0-9]+)*$') { throw 'Invalid release version.' }
$release = Join-Path $root 'release'
New-Item -ItemType Directory -Force $release | Out-Null
$expected = @(
    "ZenkaiPower-v$version-windows-x64.zip",
    "ZenkaiPower-v$version-windows-x64.zip.sig",
    "ZenkaiPower-v$version-linux-x64.zip",
    "ZenkaiPower-v$version-linux-x64.zip.sig",
    "ZenkaiPower-v$version-android-arm64.apk",
    "ZenkaiPower-v$version-android-arm64.apk.sha256"
)
$extras = @(Get-ChildItem -LiteralPath $release -Force | Where-Object { $_.PSIsContainer -or $_.Name -notin $expected })
if ($extras.Count) {
    $backup = Join-Path $root ('build/release-history/' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force $backup | Out-Null
    $resolvedBackup = (Resolve-Path -LiteralPath $backup).Path
    foreach ($extra in $extras) {
        # Check absolute source and destination before moving a directory tree.
        if ($extra.DirectoryName -and !$extra.PSIsContainer) { $parent = $extra.DirectoryName }
        else { $parent = $extra.Parent.FullName }
        if ($parent -ne $release -or !$resolvedBackup.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Unexpected release output path.'
        }
        Move-Item -LiteralPath $extra.FullName -Destination $resolvedBackup
    }
    Write-Host "Previous extra release outputs preserved in $backup"
}
