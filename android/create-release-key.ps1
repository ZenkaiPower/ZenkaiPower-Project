param([string]$KeyDirectory = (Join-Path $env:USERPROFILE '.android'))
$ErrorActionPreference = 'Stop'
$keyDirectoryPath = [IO.Path]::GetFullPath($KeyDirectory)
$propertiesPath = Join-Path $keyDirectoryPath 'zenkaipower-release.properties'
$keystorePath = Join-Path $keyDirectoryPath 'zenkaipower-release.jks'
if ((Test-Path -LiteralPath $propertiesPath) -or (Test-Path -LiteralPath $keystorePath)) {
    throw 'Release signing files already exist. Keep the existing key for app updates; they will not be overwritten.'
}
if ($env:ZENKAIPOWER_SKIP_IMAGE_BUILD -ne '1') {
    $ErrorActionPreference = 'Continue'
    docker build --platform linux/amd64 -t zenkaipower (Join-Path $PSScriptRoot '..')
    $imageExitCode = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($imageExitCode) { throw 'Unified tools image build failed.' }
}
New-Item -ItemType Directory -Force -Path $keyDirectoryPath | Out-Null
$randomBytes = New-Object byte[] 32
$randomGenerator = [Security.Cryptography.RandomNumberGenerator]::Create()
$randomGenerator.GetBytes($randomBytes)
$randomGenerator.Dispose()
$releasePassword = [Convert]::ToBase64String($randomBytes)
$env:ZENKAI_KEY_PASSWORD = $releasePassword
try {
    $ErrorActionPreference = 'Continue'
    docker run --rm --platform linux/amd64 --entrypoint keytool `
        --mount "type=bind,source=$keyDirectoryPath,target=/signing" -e ZENKAI_KEY_PASSWORD `
        zenkaipower -genkeypair -keystore /signing/zenkaipower-release.jks -storetype JKS -alias zenkaipower `
        -keyalg RSA -keysize 3072 -validity 10000 -dname 'CN=Zenkai Power Android' `
        -storepass:env ZENKAI_KEY_PASSWORD -keypass:env ZENKAI_KEY_PASSWORD -noprompt
    $keyExitCode = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($keyExitCode) { throw 'Could not generate the release signing key.' }
    $keystoreProperty = $keystorePath.Replace('\', '/')
    $lines = @("storeFile=$keystoreProperty", "storePassword=$releasePassword", 'keyAlias=zenkaipower', "keyPassword=$releasePassword")
    [IO.File]::WriteAllLines($propertiesPath, $lines, [Text.UTF8Encoding]::new($false))
} finally { Remove-Item Env:ZENKAI_KEY_PASSWORD -ErrorAction SilentlyContinue }
Write-Output "Release signing created in $keyDirectoryPath. Back up both files privately; app updates require this key."
