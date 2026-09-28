# Builds MsvcrtProbe.exe as a 32-bit program with MSVC and runs it. The golden data is written to
# moreloader/tests/auto/data/msvcrt, the build products and scratch files to work/probe.
#     moreloader/tests/probe/msvcrt/Build.ps1 [-NoRun]

param(
    [switch]$NoRun
)

$ErrorActionPreference = 'Stop'

$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
$buildDir = Join-Path $root 'work\probe'
$scratch = Join-Path $buildDir 'scratch'
$dataDir = Join-Path $root 'moreloader\tests\auto\data\msvcrt'
New-Item -ItemType Directory -Force $buildDir, $scratch, $dataDir | Out-Null

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars32.bat'

# vcvars32.bat calls vswhere.exe by name and reports an error on stderr if it is not on PATH.
$env:PATH = (Split-Path $vswhere) + ';' + $env:PATH

$source = Join-Path $PSScriptRoot 'MsvcrtProbe.cpp'
$exe = Join-Path $buildDir 'MsvcrtProbe.exe'

# The probe calls msvcrt.dll through GetProcAddress. Its own runtime is linked statically so that
# the probe does not load a second C runtime that could be mistaken for the one under test.
$command = "`"$vcvars`" >nul && cl /nologo /std:c++17 /EHsc /O2 /MT /utf-8 /W3 " +
    "/D_CRT_SECURE_NO_WARNINGS /Fo`"$buildDir\\`" /Fe`"$exe`" `"$source`" " +
    "shell32.lib version.lib"
cmd /c $command
if ($LASTEXITCODE -ne 0) {
    throw "Compilation failed."
}

if (-not $NoRun) {
    & $exe $dataDir $scratch
    if ($LASTEXITCODE -ne 0) {
        throw "The probe failed with exit code $LASTEXITCODE."
    }
    Get-ChildItem $dataDir | Select-Object Name, Length
}
