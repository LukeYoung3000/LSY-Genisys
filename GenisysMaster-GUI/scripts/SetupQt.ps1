param(
    [string]$InstallRoot = (Join-Path $PSScriptRoot '..\..\..\Tools\Qt'),
    [string]$SevenZip = 'C:\Program Files\7-Zip\7z.exe'
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $SevenZip)) { throw 'Install 7-Zip or pass -SevenZip with its executable path.' }
$InstallRoot = [IO.Path]::GetFullPath($InstallRoot)
$downloads = Join-Path $InstallRoot 'downloads'
New-Item -ItemType Directory -Force -Path $downloads | Out-Null
$base = 'https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_6113/qt6_6113_msvc2022_64/qt.qt6.6113.win64_msvc2022_64/'
$prefix = '6.11.3-0-202610220312'
$qtRoot = Join-Path $InstallRoot '6.11.3\msvc2022_64'
New-Item -ItemType Directory -Force -Path $qtRoot | Out-Null
foreach ($module in @('qtbase', 'qttools', 'qtsvg', 'qttranslations')) {
    $name = $prefix + $module + '-Windows-Windows_11_24H2-MSVC2022-Windows-Windows_11_24H2-X86_64.7z'
    $archive = Join-Path $downloads $name
    Write-Output "Fetching official Qt 6.11.3 $module archive..."
    if (-not (Test-Path -LiteralPath $archive)) { Invoke-WebRequest -Uri ($base + $name) -OutFile $archive }
    $checksum = (Invoke-WebRequest -Uri ($base + $name + '.sha1')).Content
    if ($checksum -is [byte[]]) { $checksum = [Text.Encoding]::ASCII.GetString($checksum) }
    $expected = ([string]$checksum).Trim()
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA1).Hash -ne $expected) { throw "Checksum mismatch: $name" }
    & $SevenZip x $archive ('-o' + $qtRoot) -y -bso0
    if ($LASTEXITCODE -ne 0) { throw "Extraction failed: $name" }
}
if (-not (Test-Path -LiteralPath (Join-Path $qtRoot 'bin\qmake.exe'))) { throw "Unexpected Qt archive layout under $InstallRoot" }
[IO.File]::WriteAllText((Join-Path $qtRoot 'bin\qt.conf'), "[Paths]`r`nPrefix=..`r`n")
$licenses = Join-Path $qtRoot 'licenses'
New-Item -ItemType Directory -Force -Path $licenses | Out-Null
foreach ($license in @('LGPL-3.0-only.txt', 'GPL-3.0-only.txt', 'GPL-2.0-only.txt', 'Qt-GPL-exception-1.0.txt')) {
    $licenseFile = Join-Path $licenses $license
    if (-not (Test-Path -LiteralPath $licenseFile)) {
        Invoke-WebRequest -Uri ('https://raw.githubusercontent.com/qt/qtbase/v6.11.3/LICENSES/' + $license) -OutFile $licenseFile
    }
}
Write-Output "Qt SDK installed: $qtRoot"
& (Join-Path $qtRoot 'bin\qmake.exe') -query QT_VERSION
if ($LASTEXITCODE -ne 0) { throw 'Qt SDK verification failed.' }
