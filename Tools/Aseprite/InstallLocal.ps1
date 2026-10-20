param(
    [string]$BuildRoot = (Join-Path $env:TEMP 'star-drifter-aseprite'),
    [string]$InstallRoot = (Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'StarDrifterTools/Aseprite/1.3.18.6')
)
$ErrorActionPreference = 'Stop'
$bin = Join-Path ([IO.Path]::GetFullPath($BuildRoot)) 'build/bin'
$InstallRoot = [IO.Path]::GetFullPath($InstallRoot)
if (!(Test-Path -LiteralPath (Join-Path $bin 'aseprite.exe'))) { throw 'Build Aseprite first.' }
New-Item -ItemType Directory -Force -Path $InstallRoot | Out-Null
foreach ($file in (Get-ChildItem -LiteralPath $bin -File)) {
    if ($file.Name -eq 'aseprite.exe' -or $file.Name -eq 'icudtl.dat' -or $file.Extension -eq '.dll') {
        Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $InstallRoot $file.Name)
    }
}
& robocopy.exe (Join-Path $bin 'data') (Join-Path $InstallRoot 'data') /E /XD .git /R:1 /W:1 /NFL /NDL /NJH /NJS
if ($LASTEXITCODE -ge 8) { throw 'Could not copy Aseprite resources.' }
Copy-Item -LiteralPath (Join-Path $BuildRoot 'source/EULA.txt') -Destination (Join-Path $InstallRoot 'EULA.txt')
Write-Host "Personal portable Aseprite installed: $InstallRoot"
