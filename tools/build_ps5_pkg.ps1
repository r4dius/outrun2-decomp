param([string]$Distro = 'Ubuntu-26.04', [string]$TitlePath, [string]$OutputPath)
# Packs deliver\ps5\native\PPSA99106 (tools/build_ps5_native.sh) into
# deliver\ps5\OutRun2006-PS5.pkg with LibProsperoPkg (tools/ps5_pkg).
# Needs the .NET 10 SDK on Windows.
# Native tools write progress on stderr; failures are checked with $LASTEXITCODE.
$ErrorActionPreference = 'Continue'
$root = Split-Path -Parent $PSScriptRoot
$linuxRoot = & wsl.exe -d $Distro -- wslpath -a ($root.Replace('\', '/'))
$title = if ($TitlePath) { $TitlePath } else { Join-Path $root 'deliver\ps5\native\PPSA99106' }
$package = if ($OutputPath) { $OutputPath } else { Join-Path $root 'deliver\ps5\OutRun2006-PS5.pkg' }
if (-not (Test-Path (Join-Path $title 'eboot.bin'))) { throw "Build the title first (tools/build_ps5_native.sh): $title" }

& wsl.exe -d $Distro -- bash "$linuxRoot/tools/setup_ps5_pkg_tools.sh"
if ($LASTEXITCODE -ne 0) { throw 'LibProsperoPkg setup failed.' }

& dotnet run -c Release --project (Join-Path $root 'tools\ps5_pkg\OutRunPs5Pkg.csproj') -- $title $package
if ($LASTEXITCODE -ne 0) { throw 'PS5 package build failed.' }
