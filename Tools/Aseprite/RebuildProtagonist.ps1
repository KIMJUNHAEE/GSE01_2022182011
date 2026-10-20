param(
    [string]$AsepritePath = (Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'StarDrifterTools/Aseprite/1.3.18.6/aseprite.exe'),
    [string]$WorkRoot = (Join-Path $env:TEMP 'star-drifter-hero-art')
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$WorkRoot = [IO.Path]::GetFullPath($WorkRoot)
if (!(Test-Path -LiteralPath $AsepritePath)) { throw 'Build Aseprite first, or provide -AsepritePath.' }
New-Item -ItemType Directory -Force -Path $WorkRoot | Out-Null
function Invoke-Aseprite([string[]]$Arguments, [string]$LogName) {
    # Aseprite is a Windows GUI-subsystem executable. Wait explicitly instead of
    # inspecting a stale LASTEXITCODE immediately after an asynchronous launch.
    $quotedArguments = $Arguments | ForEach-Object { '"' + $_.Replace('"', '\"') + '"' }
    $stdout = Join-Path $WorkRoot "$LogName.stdout.log"
    $stderr = Join-Path $WorkRoot "$LogName.stderr.log"
    $process = Start-Process -FilePath $AsepritePath -ArgumentList $quotedArguments `
        -WorkingDirectory $WorkRoot -WindowStyle Hidden -Wait -PassThru `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    Get-Content -LiteralPath $stdout
    if ($process.ExitCode -ne 0) {
        Get-Content -LiteralPath $stderr
        throw "Aseprite $LogName failed with exit code $($process.ExitCode)."
    }
}
$source = Join-Path $WorkRoot 'source.png'
Copy-Item -LiteralPath (Join-Path $projectRoot 'SimpleGame/Resources/Textures/protagonist_sheet.png') -Destination $source
$buildScript = Join-Path $WorkRoot 'BuildProtagonist.lua'
$validateScript = Join-Path $WorkRoot 'ValidateProtagonist.lua'
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'BuildProtagonist.lua') -Destination $buildScript
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'ValidateProtagonist.lua') -Destination $validateScript
Invoke-Aseprite @('--batch','--script-param',"source=$source",'--script-param',"output=$WorkRoot",'--script',$buildScript) 'generate'
Invoke-Aseprite @('--batch','--script-param',"output=$WorkRoot",'--script',$validateScript) 'validate'
$document = Join-Path $WorkRoot 'protagonist_walk.aseprite'
Invoke-Aseprite @('--batch','--tag','walk',$document,'--save-as',(Join-Path $WorkRoot 'protagonist_walk.gif')) 'gif'
$artRoot = Join-Path $projectRoot 'SimpleGame/Resources/Art/Protagonist'
New-Item -ItemType Directory -Force -Path $artRoot | Out-Null
foreach ($name in @('protagonist_walk.aseprite','protagonist_walk.gif','protagonist_walk_aseprite.json','protagonist_rig.json','reference_idle.png','animation_validation.json')) {
    Copy-Item -LiteralPath (Join-Path $WorkRoot $name) -Destination (Join-Path $artRoot $name)
}
foreach ($name in @('protagonist_walk_aseprite.png','protagonist_idle_aseprite.png')) {
    Copy-Item -LiteralPath (Join-Path $WorkRoot $name) -Destination (Join-Path $projectRoot "SimpleGame/Resources/Textures/$name")
}
Write-Host "Animation ready: $artRoot"
