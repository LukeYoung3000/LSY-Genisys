param(
    [string]$QtRoot = (Join-Path $PSScriptRoot '..\..\..\Tools\Qt\6.11.3\msvc2022_64'),
    [string]$MSBuild = ''
)
# Builds the real window with a QtTest driver and the existing slave UDP peer.
# Runs on loopback only; no physical device, external simulator or user input.
$ErrorActionPreference = 'Stop'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$QtRoot = [IO.Path]::GetFullPath($QtRoot)
if (-not $MSBuild) {
    $vs = & 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe' -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath
    $MSBuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
}
& $MSBuild (Join-Path $project 'GenisysMaster-GUI.vcxproj') /p:Configuration=Debug /p:Platform=x64 /p:GuiChecks=true ('/p:QtSdkRoot=' + $QtRoot) /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw 'GUI checks build failed.' }
$output = Join-Path $project 'builds\x64\Debug'
$report = Join-Path $output 'GuiChecks.txt'
$screenshot = Join-Path $output 'GuiChecks.png'
$process = Start-Process -FilePath (Join-Path $output 'GenisysMaster-GUI-Checks.exe') -ArgumentList @(('"' + $report + '"'), ('"' + $screenshot + '"')) -WindowStyle Hidden -PassThru
if (-not $process.WaitForExit(60000)) {
    # Only terminate the check executable launched by this script.
    $process.Kill()
    throw 'GUI checks exceeded the 60 second limit.'
}
if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report }
if ($process.ExitCode -ne 0) { throw ('GUI checks failed with exit code ' + $process.ExitCode) }
Write-Output "Running-window screenshot: $screenshot"
Write-Output ("Serial-window screenshot: " + (Join-Path $output 'GuiSerialChecks.png'))
