# Xbox port (xbox/): the game on Xbox One / Series X|S in Developer Mode
# (UWP, CMAKE_SYSTEM_NAME=WindowsStore) and the same platform layer as a
# Win32 program for testing on a PC. Direct3D 11 over the D3D9 device,
# XAudio2, Windows.Gaming.Input. See xbox/README.md.
#
# Two configurations. The PC one (clang-cl) builds the engine and every Xbox
# source except the UWP entry into static libraries, and the PC test
# program. The UWP one (MSVC: Visual Studio has no clang-cl toolset for UWP
# projects) compiles only source/window_uwp.cpp and links the PC
# configuration's libraries (OR2_XBOX_ENGINE_DIR) into the package.
# PECompact extraction is provided by src/ports/pecompact_memory.cpp,
# compiled by the root CMake into outrun_native_runtime and outrun_driving.
# The PC build links these engines; UWP reuses their prebuilt libraries.
# Do not compile a second decoder into outrun_xbox_core or the UWP entry.
# clang-cl objects call the compiler-rt builtins (128-bit division in the engine).
file(GLOB OR2_CLANG_BUILTINS "${CMAKE_GENERATOR_INSTANCE}/VC/Tools/Llvm/x64/lib/clang/*/lib/windows/clang_rt.builtins-x86_64.lib")
if(NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(OR2_XBOX_SHADERS "${CMAKE_CURRENT_BINARY_DIR}/generated/pc_hlsl_shaders.hpp")
    add_custom_command(OUTPUT "${OR2_XBOX_SHADERS}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_hlsl_shaders.py" "${OR2_XBOX_SHADERS}"
        DEPENDS tools/generate_hlsl_shaders.py ../switch/shaders/pc_ffp_fsh.glsl ../switch/shaders/pc_ps11_uber_fsh.glsl ../switch/shaders/pc_ps14_uber_fsh.glsl)
    add_library(outrun_xbox_core STATIC
        source/main.cpp source/renderer.cpp source/setup_display.cpp source/audio.cpp source/platform_api.cpp
        source/d3d11_display.cpp source/d3d11_d3d9.cpp "${PROJECT_SOURCE_DIR}/src/runtime/game_host.cpp" "${OR2_XBOX_SHADERS}")
    # C++/WinRT needs C++20 coroutines under clang-cl.
    target_compile_features(outrun_xbox_core PUBLIC cxx_std_20)
    target_include_directories(outrun_xbox_core PUBLIC source PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_options(outrun_xbox_core PUBLIC /EHsc /bigobj)
    # windows.h without the min/max macros the shared headers collide with.
    target_compile_definitions(outrun_xbox_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(outrun_xbox_core PUBLIC outrun_ps5_platform outrun_native_runtime d3d11 dxgi d3dcompiler xaudio2 windowsapp ${OR2_CLANG_BUILTINS})
    # Streamed music (Sound/*.ogg): a libogg + libvorbis install built with clang-cl.
    set(OR2_XBOX_VORBIS_ROOT "" CACHE PATH "Install folder of libogg and libvorbis (include/, lib/ogg.lib, vorbis.lib, vorbisfile.lib)")
    if(OR2_XBOX_VORBIS_ROOT)
        target_include_directories(outrun_native_runtime PRIVATE "${OR2_XBOX_VORBIS_ROOT}/include")
        target_compile_definitions(outrun_native_runtime PRIVATE OR2_HAVE_VORBIS=1)
        target_link_libraries(outrun_xbox_core PUBLIC "${OR2_XBOX_VORBIS_ROOT}/lib/vorbisfile.lib" "${OR2_XBOX_VORBIS_ROOT}/lib/vorbis.lib" "${OR2_XBOX_VORBIS_ROOT}/lib/ogg.lib")
    endif()
    # Title movie (Media/*.bik): a minimal static FFmpeg (Bink demuxer and decoders, swscale, swresample).
    set(OR2_XBOX_FFMPEG_ROOT "" CACHE PATH "Install folder of the minimal FFmpeg (include/, lib/lib*.a)")
    if(OR2_XBOX_FFMPEG_ROOT)
        target_include_directories(outrun_native_runtime PRIVATE "${OR2_XBOX_FFMPEG_ROOT}/include")
        target_compile_definitions(outrun_native_runtime PRIVATE OUTRUN_WITH_FFMPEG=1)
        foreach(_l avformat avcodec swscale swresample avutil)
            target_link_libraries(outrun_xbox_core PUBLIC "${OR2_XBOX_FFMPEG_ROOT}/lib/lib${_l}.a")
        endforeach()
        target_link_libraries(outrun_xbox_core PUBLIC bcrypt)
    endif()
    add_executable(OutRunXboxPC WIN32 source/window_win32.cpp)
    target_link_libraries(OutRunXboxPC PRIVATE outrun_xbox_core shell32)
    # A PDB with the public symbols, to name the function of a crash address.
    target_link_options(OutRunXboxPC PRIVATE /DEBUG /STACK:16777216)
else()
    set(OR2_XBOX_ENGINE_DIR "" CACHE PATH "Build folder of the PC configuration (clang-cl), built first in the same configuration (Release)")
    if(NOT OR2_XBOX_ENGINE_DIR)
        message(FATAL_ERROR "The UWP package links the PC configuration's libraries: set OR2_XBOX_ENGINE_DIR (see xbox/README.md)")
    endif()
    set(_lib "${OR2_XBOX_ENGINE_DIR}")
    set(OR2_XBOX_VORBIS_ROOT "" CACHE PATH "Same libogg + libvorbis install as the PC configuration (empty: no music)")
    if(OR2_XBOX_VORBIS_ROOT)
        set(OR2_XBOX_MEDIA_LIBRARIES "${OR2_XBOX_VORBIS_ROOT}/lib/vorbisfile.lib" "${OR2_XBOX_VORBIS_ROOT}/lib/vorbis.lib" "${OR2_XBOX_VORBIS_ROOT}/lib/ogg.lib")
    endif()
    set(OR2_XBOX_FFMPEG_ROOT "" CACHE PATH "Same minimal FFmpeg as the PC configuration (empty: no title movie)")
    if(OR2_XBOX_FFMPEG_ROOT)
        foreach(_l avformat avcodec swscale swresample avutil)
            list(APPEND OR2_XBOX_MEDIA_LIBRARIES "${OR2_XBOX_FFMPEG_ROOT}/lib/lib${_l}.a")
        endforeach()
        list(APPEND OR2_XBOX_MEDIA_LIBRARIES bcrypt)
    endif()
    file(GLOB OR2_XBOX_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/package/Assets/*.png")
    set_source_files_properties(${OR2_XBOX_ASSETS} PROPERTIES VS_DEPLOYMENT_CONTENT 1 VS_DEPLOYMENT_LOCATION "Assets")
    add_executable(OutRunXbox WIN32 source/window_uwp.cpp package/Package.appxmanifest ${OR2_XBOX_ASSETS})
    target_compile_features(OutRunXbox PRIVATE cxx_std_20)
    target_compile_options(OutRunXbox PRIVATE /EHsc)
    target_compile_definitions(OutRunXbox PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(OutRunXbox PRIVATE
        "${_lib}/xbox/Release/outrun_xbox_core.lib" "${_lib}/ps5/Release/outrun_ps5_platform.lib"
        "${_lib}/Release/outrun_native_runtime.lib" "${_lib}/Release/outrun_driving.lib" "${_lib}/Release/outrun_pc_pmt.lib"
        ${ZLIB_LIBRARY} ${OR2_XBOX_MEDIA_LIBRARIES} ${OR2_CLANG_BUILTINS} d3d11 dxgi d3dcompiler xaudio2 windowsapp)
endif()
