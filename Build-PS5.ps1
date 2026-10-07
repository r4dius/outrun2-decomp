param([switch]$InstallDependencies, [switch]$SoftwareOnly, [ValidateSet('OpenGL','Vulkan')][string]$Renderer = 'OpenGL')
$ErrorActionPreference = 'Stop'
$windowsPath = $PSScriptRoot.Replace('\', '/')
$linuxPath = & wsl.exe -d Ubuntu-26.04 -- wslpath -a $windowsPath
if ($LASTEXITCODE -ne 0) { throw 'Ubuntu-26.04 WSL is required.' }
if ($InstallDependencies) {
    & wsl.exe -d Ubuntu-26.04 -u root -- apt-get update
    if ($LASTEXITCODE -ne 0) { throw 'apt update failed.' }
    & wsl.exe -d Ubuntu-26.04 -u root -- apt-get install -y build-essential clang clang-18 lld lld-18 llvm-dev cmake ninja-build unzip curl wget git pkg-config libsdl2-dev libvorbis-dev libavformat-dev libavcodec-dev libswscale-dev libswresample-dev libavutil-dev libegl1-mesa-dev libgl-dev
    if ($LASTEXITCODE -ne 0) { throw 'Dependency installation failed.' }
}
& wsl.exe -d Ubuntu-26.04 -- bash "$linuxPath/tools/setup_ps5_sdk.sh"
if ($LASTEXITCODE -ne 0) { throw 'SDK installation failed.' }
if ($Renderer -eq 'Vulkan') {
    & wsl.exe -d Ubuntu-26.04 -- env "OR2_PS5_BUILD_ROOT=$linuxPath/work/ps5" "OR2_PS5_OUTPUT_ROOT=$linuxPath/work/deliver/ps5" bash "$linuxPath/tools/build_ps5.sh"
} else {
    & wsl.exe -d Ubuntu-26.04 -- bash "$linuxPath/tools/build_ps5.sh"
}
if ($LASTEXITCODE -ne 0) { throw 'PS5 build failed.' }
if (-not $SoftwareOnly) {
    if ($Renderer -eq 'Vulkan') {
        $setupArgs = if ($InstallDependencies) { @('--dependencies') } else { @() }
        & wsl.exe -d Ubuntu-26.04 -- bash "$linuxPath/tools/setup_ps5_vulkan.sh" @setupArgs
        if ($LASTEXITCODE -ne 0) { throw 'Vulkan RADV setup failed.' }
        & wsl.exe -d Ubuntu-26.04 -- bash "$linuxPath/tools/build_ps5_vulkan.sh"
        if ($LASTEXITCODE -ne 0) { throw 'Native Vulkan PS5 title build failed.' }
        Write-Host "PS5 Vulkan title: $PSScriptRoot\work\deliver\ps5\OutRun2006-PS5-vulkan-native.zip"
    } else {
        & wsl.exe -d Ubuntu-26.04 -- bash "$linuxPath/tools/setup_ps5_gpu.sh"
        if ($LASTEXITCODE -ne 0) { throw 'Native GPU SDK installation failed.' }
        & wsl.exe -d Ubuntu-26.04 -- bash "$linuxPath/tools/build_ps5_native.sh"
        if ($LASTEXITCODE -ne 0) { throw 'Native PS5 title build failed.' }
        Write-Host "PS5 native title: $PSScriptRoot\deliver\ps5\OutRun2006-PS5-native.zip"
    }
}
if (-not $SoftwareOnly) {
    if (-not (Get-Command dotnet -ErrorAction SilentlyContinue)) { throw 'The .NET 10 SDK is required for the PS5 package.' }
    if ($Renderer -eq 'Vulkan') {
        & "$PSScriptRoot\tools\build_ps5_pkg.ps1" -TitlePath "$PSScriptRoot\work\deliver\ps5\vulkan\PPSA99106" -OutputPath "$PSScriptRoot\work\deliver\ps5\OutRun2006-PS5-vulkan.pkg"
        Write-Host "PS5 package: $PSScriptRoot\work\deliver\ps5\OutRun2006-PS5-vulkan.pkg"
    } else {
        & "$PSScriptRoot\tools\build_ps5_pkg.ps1"
        Write-Host "PS5 package: $PSScriptRoot\deliver\ps5\OutRun2006-PS5.pkg"
    }
}
if ($Renderer -eq 'Vulkan') {
    Write-Host "PS5 ELF: $PSScriptRoot\work\deliver\ps5\OutRun2006-PS5.elf"
} else {
    Write-Host "PS5 ELF: $PSScriptRoot\deliver\ps5\OutRun2006-PS5.elf"
}
