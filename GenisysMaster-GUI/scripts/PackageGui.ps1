param(
    [string]$QtRoot = (Join-Path $PSScriptRoot '..\..\..\Tools\Qt\6.11.3\msvc2022_64'),
    [string]$MSBuild = ''
)
$ErrorActionPreference = 'Stop'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$QtRoot = [IO.Path]::GetFullPath($QtRoot)
if (-not (Test-Path -LiteralPath (Join-Path $QtRoot 'bin\windeployqt.exe'))) { throw 'Qt SDK missing. Run SetupQt.ps1 or pass -QtRoot.' }
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath
if (-not $MSBuild) { $MSBuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe' }
& $MSBuild (Join-Path $project 'GenisysMaster-GUI.vcxproj') /p:Configuration=Release /p:Platform=x64 ('/p:QtSdkRoot=' + $QtRoot) /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw 'Release GUI build failed.' }
# Always use a new folder; never delete an existing package or its user files.
$name = 'GenisysMaster-GUI-win-x64-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
$package = Join-Path $project ('builds\x64\Release\packages\' + $name)
New-Item -ItemType Directory -Path $package -Force | Out-Null
$executable = Join-Path $package 'GenisysMaster-GUI.exe'
Copy-Item -LiteralPath (Join-Path $project 'builds\x64\Release\GenisysMaster-GUI.exe') -Destination $executable
& (Join-Path $QtRoot 'bin\windeployqt.exe') --release --no-translations --no-opengl-sw --no-system-dxc-compiler --no-compiler-runtime --skip-plugin-types generic,networkinformation,tls --include-plugins qoffscreen --dir $package $executable
if ($LASTEXITCODE -ne 0) { throw 'Qt runtime deployment failed.' }
# Bundle the official installer rather than arbitrary runtime DLLs from this PC.
$redist = Get-ChildItem -LiteralPath (Join-Path $vs 'VC\Redist\MSVC') -Filter 'vc_redist.x64.exe' -Recurse | Sort-Object FullName -Descending | Select-Object -First 1
if (-not $redist) { throw 'Official Visual C++ x64 Redistributable installer not found.' }
Copy-Item -LiteralPath $redist.FullName -Destination (Join-Path $package 'vc_redist.x64.exe')
$licenseFolder = Join-Path $package 'licenses\qt'
New-Item -ItemType Directory -Force -Path $licenseFolder | Out-Null
Get-ChildItem -LiteralPath (Join-Path $project 'third_party\qt-licenses') -File | Copy-Item -Destination $licenseFolder
$sbomFolder = Join-Path $package 'licenses\sbom'
New-Item -ItemType Directory -Force -Path $sbomFolder | Out-Null
foreach ($module in @('qtbase','qtsvg')) {
    Copy-Item -LiteralPath (Join-Path $QtRoot ('sbom\' + $module + '-6.11.3.spdx.json')) -Destination $sbomFolder
}
Copy-Item -LiteralPath (Join-Path $project 'THIRD-PARTY-NOTICES.txt') -Destination $package
Copy-Item -LiteralPath (Join-Path $project '..\LICENSE.txt') -Destination $package
Copy-Item -LiteralPath (Join-Path $project 'PACKAGE-README.txt') -Destination (Join-Path $package 'README.txt')
[IO.File]::WriteAllText((Join-Path $package 'qt.conf'), "[Paths]`r`nPrefix=.`r`nPlugins=.`r`n")
Copy-Item -LiteralPath (Join-Path $project 'GenisysMaster.example.xml') -Destination $package
Copy-Item -LiteralPath (Join-Path $project 'GenisysMaster.serial.example.xml') -Destination $package
$pugiLicense = Join-Path $package 'licenses\pugixml'
New-Item -ItemType Directory -Force -Path $pugiLicense | Out-Null
Copy-Item -LiteralPath (Join-Path $project '..\GenisysMaster-Console\third_party\pugixml\LICENSE.md') -Destination $pugiLicense
$zip = $package + '.zip'
Compress-Archive -Path (Join-Path $package '*') -DestinationPath $zip
Write-Output "Package directory: $package"
Write-Output "ZIP package: $zip"
