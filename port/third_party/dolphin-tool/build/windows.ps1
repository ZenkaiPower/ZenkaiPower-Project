param(
    [string]$Source = (Join-Path $PSScriptRoot '../dolphin'),
    [string]$Build = (Join-Path $PSScriptRoot '../build-windows'),
    [int]$Jobs = 2
)
$ErrorActionPreference = 'Stop'
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (!$cmake) {
    $vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
    $vs = & $vswhere -latest -products '*' -property installationPath
    $cmake = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
}
$options = (Get-Content -Raw (Join-Path $PSScriptRoot 'options.txt')).Trim().Split(' ')
& $cmake -S $Source -B $Build -G 'Visual Studio 17 2022' -A x64 @options
if ($LASTEXITCODE) { throw 'Dolphin configure failed' }
$previousCompilerOptions = $env:_CL_
try {
    $env:_CL_ = "$previousCompilerOptions /MP2"
    & $cmake --build $Build --config Release --target dolphin-tool --parallel $Jobs
    if ($LASTEXITCODE) { throw 'Dolphin build failed' }
} finally {
    $env:_CL_ = $previousCompilerOptions
}
if ($LASTEXITCODE) { throw 'Dolphin build failed' }
Get-ChildItem $Build -Recurse -Filter DolphinTool.exe | Select-Object -ExpandProperty FullName
