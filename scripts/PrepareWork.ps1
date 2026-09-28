# Copies the moresampler 0.8.4 release directory into work/moresampler and prints the SHA-256 of
# each file. Analysis and tests use only this copy. The original directory is never written to or
# executed in.
#     scripts/PrepareWork.ps1 [-Source <directory>]

param(
    [string]$Source = 'E:\temp-repos\UTAU\UTAU\tools\moresampler 0.8.4'
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$target = Join-Path $root 'work\moresampler'

New-Item -ItemType Directory -Force $target | Out-Null
Copy-Item -Recurse -Force (Join-Path $Source '*') $target

$expected = '29e88e1c79645a46d3eb346e0b5209cd4e32f4f800a8fe1db7435c0e9e4ebdf2'
Get-ChildItem -Recurse -File $target | ForEach-Object {
    $hash = (Get-FileHash -Algorithm SHA256 $_.FullName).Hash.ToLower()
    '{0}  {1}' -f $hash, $_.FullName.Substring($target.Length + 1)
}

$exeHash = (Get-FileHash -Algorithm SHA256 (Join-Path $target 'moresampler.exe')).Hash.ToLower()
if ($exeHash -ne $expected) {
    throw "moresampler.exe differs from the release 0.8.4 that this project supports."
}
