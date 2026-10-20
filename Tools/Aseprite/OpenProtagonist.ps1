$ErrorActionPreference = 'Stop'
$aseprite = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'StarDrifterTools/Aseprite/1.3.18.6/aseprite.exe'
$document = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../SimpleGame/Resources/Art/Protagonist/protagonist_walk.aseprite'))
if (!(Test-Path -LiteralPath $aseprite)) { throw 'Run Tools/Aseprite/InstallLocal.ps1 first.' }
if (!(Test-Path -LiteralPath $document)) { throw 'Rebuild the protagonist art first.' }
Start-Process -FilePath $aseprite -ArgumentList ('"' + $document + '"') -WindowStyle Normal
