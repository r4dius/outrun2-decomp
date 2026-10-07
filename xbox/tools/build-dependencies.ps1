# Builds the libraries the Xbox port links statically, at pinned versions:
# zlib, libogg + libvorbis (music) and a minimal FFmpeg (Bink title movie:
# demuxer, video/audio decoders, swscale, swresample; LGPL 2.1 or later,
# no GPL or non-free parts).
#
#   powershell -File xbox/tools/build-dependencies.ps1 [-Out <folder>]
#
# Needs Visual Studio 2026 (C++ desktop, C++ Clang tools), CMake and an MSYS2
# bash with make (FFmpeg's configure). Prints the CMake options for xbox/README.md.
param(
    [string]$Out = (Join-Path (Get-Location) "xbox-deps"),
    [string]$Bash = $(if (Test-Path "C:\msys64\usr\bin\bash.exe") { "C:\msys64\usr\bin\bash.exe" } else { "C:\devkitPro\msys2\usr\bin\bash.exe" }),
    [string]$VsDir = "C:\Program Files\Microsoft Visual Studio\18\Community"
)
$ErrorActionPreference = "Stop"
$sources = @(
    @{ name = "zlib-1.3.1.tar.gz";      url = "https://zlib.net/fossils/zlib-1.3.1.tar.gz";                          sha = "9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23" },
    @{ name = "libogg-1.3.5.tar.gz";    url = "https://downloads.xiph.org/releases/ogg/libogg-1.3.5.tar.gz";         sha = "0eb4b4b9420a0f51db142ba3f9c64b333f826532dc0f48c6410ae51f4799b664" },
    @{ name = "libvorbis-1.3.7.tar.gz"; url = "https://downloads.xiph.org/releases/vorbis/libvorbis-1.3.7.tar.gz"; sha = "0e982409a9c3fc82ee06e08205b1355e5c6aa4c36bca58146ef399621b0ce5ab" },
    @{ name = "ffmpeg-7.1.1.tar.xz";    url = "https://ffmpeg.org/releases/ffmpeg-7.1.1.tar.xz";                    sha = "733984395e0dbbe5c046abda2dc49a5544e7e0e1e2366bba849222ae9e3a03b1" }
)
New-Item -ItemType Directory -Force $Out | Out-Null
Set-Location $Out
foreach ($s in $sources) {
    if (-not (Test-Path $s.name)) { Invoke-WebRequest -Uri $s.url -OutFile $s.name }
    if ((Get-FileHash $s.name -Algorithm SHA256).Hash.ToLower() -ne $s.sha) { throw "$($s.name): SHA-256 mismatch" }
    tar -xf $s.name
    if ($LASTEXITCODE -ne 0) { throw "cannot extract $($s.name)" }
}
# Visual Studio's CMake (a cmake from MSYS2 on PATH lacks the Visual Studio generators).
$vsCmake = "$VsDir\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$cmake = if (Test-Path $vsCmake) { $vsCmake } else { "cmake" }
function CMakeLib([string]$src, [string]$build, [string]$prefix, [string[]]$extra) {
    & $cmake -S $src -B $build -G "Visual Studio 18 2026" -A x64 -T ClangCL "-DCMAKE_INSTALL_PREFIX=$prefix" @extra
    if ($LASTEXITCODE -ne 0) { throw "configure $src" }
    & $cmake --build $build --config Release --target INSTALL
    if ($LASTEXITCODE -ne 0) { throw "build $src" }
}
$zlib = Join-Path $Out "zlib"; $vorbis = Join-Path $Out "vorbis"; $ffmpeg = Join-Path $Out "ffmpeg"
# zlib: only the static library (the DLL's resource file does not build with clang-cl).
& $cmake -S "zlib-1.3.1" -B "zlib-build" -G "Visual Studio 18 2026" -A x64 -T ClangCL
if ($LASTEXITCODE -ne 0) { throw "configure zlib" }
& $cmake --build "zlib-build" --config Release --target zlibstatic
if ($LASTEXITCODE -ne 0) { throw "build zlib" }
New-Item -ItemType Directory -Force "$zlib\include", "$zlib\lib" | Out-Null
Copy-Item "zlib-1.3.1\zlib.h", "zlib-build\zconf.h" "$zlib\include"
Copy-Item "zlib-build\Release\zlibstatic.lib" "$zlib\lib"
CMakeLib "libogg-1.3.5" "ogg-build" $vorbis @("-DBUILD_SHARED_LIBS=OFF", "-DCMAKE_POLICY_VERSION_MINIMUM=3.5")
CMakeLib "libvorbis-1.3.7" "vorbis-build" $vorbis @("-DBUILD_SHARED_LIBS=OFF", "-DCMAKE_POLICY_VERSION_MINIMUM=3.5", "-DOGG_INCLUDE_DIR=$vorbis/include", "-DOGG_LIBRARY=$vorbis/lib/ogg.lib")

# FFmpeg: configure needs a POSIX shell; clang-cl and the MSVC environment come from vcvars64.
$prefix = $ffmpeg -replace '\\', '/'
$configure = "./configure --toolchain=msvc --cc=clang-cl --cxx=clang-cl --ld=lld-link --prefix=`"$prefix`" " +
    "--disable-everything --disable-programs --disable-doc --disable-network --disable-x86asm " +
    "--disable-avdevice --disable-avfilter --disable-postproc --disable-debug --enable-static --disable-shared " +
    "--enable-demuxer=bink --enable-decoder=bink,binkaudio_rdft,binkaudio_dct --enable-protocol=file " +
    "--enable-swscale --enable-swresample --extra-cflags=-MD --disable-dxva2 --disable-d3d11va --disable-d3d12va " +
    "--disable-hwaccels --disable-mediafoundation && make -j8 && make install"
$cmd = Join-Path $Out "ffmpeg-build.cmd"
@(
    "@echo off",
    "call `"$VsDir\VC\Auxiliary\Build\vcvars64.bat`" >nul",
    "set PATH=$VsDir\VC\Tools\Llvm\x64\bin;%PATH%;$(Split-Path $Bash)",
    "set MSYS2_PATH_TYPE=inherit",
    "`"$Bash`" -lc `"cd '$((Join-Path $Out 'ffmpeg-7.1.1') -replace '\\', '/')' && $($configure -replace '"', '\"')`""
) | Set-Content $cmd -Encoding ascii
& $env:ComSpec /c $cmd
if ($LASTEXITCODE -ne 0) { throw "FFmpeg build failed" }

"CMake options for both Xbox configurations (xbox/README.md):"
"  -DZLIB_INCLUDE_DIR=$zlib/include -DZLIB_LIBRARY=$zlib/lib/zlibstatic.lib -DOR2_XBOX_VORBIS_ROOT=$vorbis -DOR2_XBOX_FFMPEG_ROOT=$ffmpeg"
