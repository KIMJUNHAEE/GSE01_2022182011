param(
    [string]$WorkRoot = (Join-Path $env:TEMP 'star-drifter-aseprite'),
    [switch]$DownloadOnly
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$env:VSLANG = '1033'
$aseVersion = '1.3.18.6'
$skiaVersion = 'm124-08a5439a6b'
$WorkRoot = [IO.Path]::GetFullPath($WorkRoot)
New-Item -ItemType Directory -Force -Path $WorkRoot | Out-Null
$aseSource = Join-Path $WorkRoot 'source'
$skiaRoot = Join-Path $WorkRoot 'skia'
$sourceZip = Join-Path $WorkRoot "Aseprite-v$aseVersion-Source.zip"
$skiaZip = Join-Path $WorkRoot 'Skia-Windows-Release-x64.zip'

# Windows build settings adapted from the user-selected builder:
# https://github.com/a1393323447/aseprite-builder/blob/main/.github/workflows/build_and_release.yaml
# Download only official upstream source and the exact Skia release used by that workflow.
if (!(Test-Path -LiteralPath $sourceZip)) {
    Write-Host 'Downloading official Aseprite source...'
    Invoke-WebRequest "https://github.com/aseprite/aseprite/releases/download/v$aseVersion/Aseprite-v$aseVersion-Source.zip" -OutFile $sourceZip
}
if ((Get-FileHash -LiteralPath $sourceZip -Algorithm SHA256).Hash -ne 'FA9DD07A0C2A5EC91A4166333296BBB9E5C237933B59875D0CFEE849D2358306') {
    throw 'Aseprite source checksum mismatch.'
}
if (!(Test-Path -LiteralPath $skiaZip)) {
    Write-Host 'Downloading official Skia dependency...'
    Invoke-WebRequest "https://github.com/aseprite/skia/releases/download/$skiaVersion/Skia-Windows-Release-x64.zip" -OutFile $skiaZip
}
if (!(Test-Path -LiteralPath (Join-Path $aseSource 'CMakeLists.txt'))) {
    Write-Host 'Extracting source...'
    Expand-Archive -LiteralPath $sourceZip -DestinationPath $aseSource
}
if (!(Test-Path -LiteralPath (Join-Path $skiaRoot 'out/Release-x64/skia.lib'))) {
    Write-Host 'Extracting Skia...'
    Expand-Archive -LiteralPath $skiaZip -DestinationPath $skiaRoot
}
if ($DownloadOnly) { return }

$vsRoot = 'C:/Program Files/Microsoft Visual Studio/18/Community'
$vsDevShell = Join-Path $vsRoot 'Common7/Tools/Launch-VsDevShell.ps1'
$cmake = Join-Path $vsRoot 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ninja = Join-Path $vsRoot 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
if (!(Test-Path -LiteralPath $vsDevShell)) { throw 'Visual Studio C++ developer tools not found.' }
& $vsDevShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
if ($LASTEXITCODE) { throw 'Could not initialize Visual Studio developer environment.' }
$aseBuild = Join-Path $WorkRoot 'build'
& $cmake -S $aseSource -B $aseBuild -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$ninja" -DCMAKE_BUILD_TYPE=Release `
    -DENABLE_TESTS=OFF -DENABLE_I18N_STRINGS=ON -DENABLE_SCRIPTING=ON `
    -DENABLE_CCACHE=OFF -DLAF_BACKEND=skia -DOPENSSL_USE_STATIC_LIBS=ON `
    "-DSKIA_DIR=$skiaRoot" "-DSKIA_LIBRARY_DIR=$skiaRoot/out/Release-x64" `
    "-DSKIA_LIBRARY=$skiaRoot/out/Release-x64/skia.lib"
if ($LASTEXITCODE -ne 0) { throw 'Aseprite CMake configuration failed.' }
& $cmake --build $aseBuild --target aseprite --parallel 6 2>&1 | ForEach-Object {
    if ($_ -notmatch '^\s*(참고: 포함 파일:|Note: including file:)') { $_ }
}
if ($LASTEXITCODE -ne 0) { throw 'Aseprite compilation failed.' }
$aseExe = Join-Path $aseBuild 'bin/aseprite.exe'
$versionLog = Join-Path $WorkRoot 'version.stdout.log'
$versionError = Join-Path $WorkRoot 'version.stderr.log'
$versionProcess = Start-Process -FilePath $aseExe -ArgumentList '--version' `
    -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $versionLog `
    -RedirectStandardError $versionError
if ($versionProcess.ExitCode -ne 0) { throw 'Aseprite executable check failed.' }
Get-Content -LiteralPath $versionLog
Write-Host "Aseprite ready: $aseExe"
