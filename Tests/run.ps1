param(
    [ValidateSet('x64', 'Win32')][string]$Platform = 'x64',
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -version '[18.0,19.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio 2026 C++ build tools (v145) were not found.' }
$msbuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
$project = Join-Path $repo 'Battlenet-Launcher.vcxproj'
& $msbuild $project /m /nologo /v:minimal /p:LauncherTests=true "/p:Configuration=$Configuration" "/p:Platform=$Platform"
if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed.' }
& (Join-Path $PSScriptRoot "bin\$Platform\$Configuration\launcher-tests.exe")
if ($LASTEXITCODE -ne 0) { throw 'Launcher tests failed.' }
