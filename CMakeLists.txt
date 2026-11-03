cmake_minimum_required(VERSION 3.24)

# The game uses Windows GDI and WinHTTP. Only the vendored GLFW is portable.
if(NOT CMAKE_HOST_WIN32 OR (DEFINED CMAKE_SYSTEM_NAME AND
        NOT CMAKE_SYSTEM_NAME STREQUAL "Windows"))
    message(FATAL_ERROR "SimpleGame currently supports native Windows/MSVC builds only.")
endif()

set(CMAKE_SYSTEM_VERSION "10.0" CACHE STRING "Windows SDK family")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
project(SimpleGame LANGUAGES C CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "Use MSVC with the Windows SDK (Visual Studio or Ninja).")
endif()
if(NOT MSVC_C_ARCHITECTURE_ID MATCHES "^(X86|x64)$")
    message(FATAL_ERROR "Only Win32/x86 and x64 are packaged; ARM targets are unsupported.")
endif()
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build configuration" FORCE)
endif()

set(game_dir "${CMAKE_CURRENT_SOURCE_DIR}/SimpleGame")
set(dependency_dir "${game_dir}/Dependencies")

# Build exclusively from the vendored tree. Never fetch or update dependencies.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
set(GLFW_LIBRARY_TYPE STATIC CACHE STRING "GLFW linkage" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "GLFW examples" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "GLFW tests" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "GLFW docs" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "GLFW install rules" FORCE)
set(GLFW_BUILD_WIN32 ON CACHE BOOL "GLFW Win32 backend" FORCE)
set(GLFW_BUILD_COCOA OFF CACHE BOOL "GLFW Cocoa backend" FORCE)
set(GLFW_BUILD_X11 OFF CACHE BOOL "GLFW X11 backend" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "GLFW Wayland backend" FORCE)
set(USE_MSVC_RUNTIME_LIBRARY_DLL ON CACHE BOOL "Match game CRT" FORCE)
add_subdirectory("${dependency_dir}/GLFW" "${CMAKE_CURRENT_BINARY_DIR}/vendor/glfw")
# Upstream is C99. MSVC has no /std:c99; C11 mode supplies its C99 language features.
set_target_properties(glfw PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO DISABLE_PRECOMPILE_HEADERS YES)
target_compile_options(glfw PRIVATE /utf-8)

# OpenGL is provided by the Windows SDK; GLEW is the existing local import library + DLL.
find_package(OpenGL REQUIRED)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(glew_arch x64)
else()
    set(glew_arch Win32)
endif()
if(DEFINED SIMPLEGAME_ARCHITECTURE AND NOT SIMPLEGAME_ARCHITECTURE STREQUAL glew_arch)
    message(FATAL_ERROR
        "This preset requires ${SIMPLEGAME_ARCHITECTURE}; use the matching MSVC developer prompt.")
endif()
file(READ "${dependency_dir}/GLEW/runtime-manifest.json" glew_manifest)
string(JSON glew_runtime_path GET "${glew_manifest}" architectures ${glew_arch} runtime path)
string(JSON glew_runtime_hash GET "${glew_manifest}" architectures ${glew_arch} runtime sha256)
string(JSON glew_library_path GET "${glew_manifest}" architectures ${glew_arch} importLibrary path)
string(JSON glew_library_hash GET "${glew_manifest}" architectures ${glew_arch} importLibrary sha256)
string(JSON glew_machine GET "${glew_manifest}" architectures ${glew_arch} peMachineBytes)
set(glew_runtime "${dependency_dir}/GLEW/${glew_runtime_path}")
set(glew_library "${dependency_dir}/GLEW/${glew_library_path}")

function(check_local_dependency path expected_hash)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Missing packaged dependency: ${path}")
    endif()
    file(SHA256 "${path}" actual_hash)
    string(TOLOWER "${expected_hash}" expected_hash)
    if(NOT actual_hash STREQUAL expected_hash)
        message(FATAL_ERROR "Dependency SHA256 mismatch: ${path}")
    endif()
endfunction()
check_local_dependency("${glew_runtime}" "${glew_runtime_hash}")
check_local_dependency("${glew_library}" "${glew_library_hash}")

# Check the runtime's DOS signature, PE signature, and actual COFF machine as well.
file(READ "${glew_runtime}" dos_signature LIMIT 2 HEX)
file(READ "${glew_runtime}" pe_offset OFFSET 60 LIMIT 4 HEX)
string(REGEX REPLACE "(..)(..)(..)(..)" "0x\\4\\3\\2\\1" pe_offset "${pe_offset}")
math(EXPR pe_offset "${pe_offset}")
math(EXPR pe_machine_offset "${pe_offset} + 4")
file(READ "${glew_runtime}" pe_signature OFFSET ${pe_offset} LIMIT 4 HEX)
file(READ "${glew_runtime}" actual_machine OFFSET ${pe_machine_offset} LIMIT 2 HEX)
if(NOT dos_signature STREQUAL "4d5a" OR NOT pe_signature STREQUAL "50450000" OR
        NOT actual_machine STREQUAL glew_machine)
    message(FATAL_ERROR "GLEW DLL is not a ${glew_arch} PE image: ${glew_runtime}")
endif()
add_library(GLEW::GLEW SHARED IMPORTED GLOBAL)
set_target_properties(GLEW::GLEW PROPERTIES
    IMPORTED_IMPLIB "${glew_library}"
    IMPORTED_LOCATION "${glew_runtime}"
    INTERFACE_INCLUDE_DIRECTORIES "${dependency_dir}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${glew_runtime}" "${glew_library}" "${dependency_dir}/GLEW/runtime-manifest.json")

# Keep this list in sync with SimpleGame.vcxproj; SimpleGame.cpp is a legacy entry point.
set(game_sources
    Renderer.cpp
    RenderStats.cpp
    Monitoring.cpp
    MonitorPublisher.cpp
    Main.cpp
    Canvas.cpp
    SpriteAnimation.cpp
    SpriteSheet.cpp
    Actor.cpp
    SceneGraph.cpp
    WorldActors.cpp
    NavigationGrid.cpp
    WorldScene.cpp
    WorldNavigation.cpp
    GameScene.cpp
    ShaderLoader.cpp
    GameWorld.cpp
    LevelOne.cpp
    PlanetGeneration.cpp
    PlanetSimulation.cpp
    ModelAsset.cpp
    ModelAnimation.cpp
    ModelLibrary.cpp
    ModelActor.cpp
    ModelRenderer.cpp
    ModelVerification.cpp
    GameModels.cpp
    cgltf_impl.cpp
    Combat.cpp
    GameCombat.cpp
    Game.cpp
    PrototypeVerification.cpp
    stb_image_impl.cpp
    stdafx.cpp
)
list(TRANSFORM game_sources PREPEND "${game_dir}/")
add_executable(SimpleGame WIN32 ${game_sources}
    "${game_dir}/Input.h" "${game_dir}/WindowMetrics.h" "${game_dir}/GlfwInput.h")
target_compile_features(SimpleGame PRIVATE cxx_std_17)
set_target_properties(SimpleGame PROPERTIES CXX_EXTENSIONS NO
    DISABLE_PRECOMPILE_HEADERS YES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/$<CONFIG>"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:SimpleGame>")
# Existing #include "stdafx.h" works normally; CMake does not require a PCH.
target_include_directories(SimpleGame PRIVATE "${game_dir}")
target_compile_definitions(SimpleGame PRIVATE UNICODE _UNICODE NOMINMAX GLFW_INCLUDE_NONE)
target_compile_options(SimpleGame PRIVATE /W3 /utf-8 /sdl)
target_link_options(SimpleGame PRIVATE /ENTRY:mainCRTStartup)
target_link_libraries(SimpleGame PRIVATE glfw GLEW::GLEW OpenGL::GL
    winhttp kernel32 user32 gdi32 winspool comdlg32 advapi32 shell32
    ole32 oleaut32 uuid odbc32 odbccp32)

# Generate a short deployment script instead of exceeding Windows command-line limits.
# Always run it once before SimpleGame, including explicit and asset-only builds.
string(TOLOWER "${glew_runtime_hash}" glew_runtime_hash_lower)
set(deployment_script [=[
file(SHA256 "@glew_runtime@" actual_hash)
if(NOT actual_hash STREQUAL "@glew_runtime_hash_lower@")
    message(FATAL_ERROR "Packaged GLEW runtime SHA256/architecture mismatch")
endif()
file(COPY "@game_dir@/Shaders" DESTINATION "$<TARGET_FILE_DIR:SimpleGame>")
file(COPY "@game_dir@/Resources" DESTINATION "$<TARGET_FILE_DIR:SimpleGame>"
    PATTERN "*.[bB][lL][eE][nN][dD]*" EXCLUDE
    PATTERN "*[pP][rR][eE][vV][iI][eE][wW]*" EXCLUDE)
file(COPY
    "@dependency_dir@/cgltf.LICENSE"
    "@dependency_dir@/json.LICENSE"
    "@dependency_dir@/ModelLoader_NOTICES.md"
    "@dependency_dir@/GLEW/LICENSE.txt"
    "@dependency_dir@/GLEW/runtime-manifest.json"
    "@dependency_dir@/GLEW/README.md"
    "@dependency_dir@/GLFW/LICENSE.md"
    "@dependency_dir@/GLFW.UPSTREAM.json"
    "@dependency_dir@/stb_image.h"
    DESTINATION "$<TARGET_FILE_DIR:SimpleGame>/Licenses")
file(COPY_FILE "@glew_runtime@" "$<TARGET_FILE_DIR:SimpleGame>/glew32.dll" ONLY_IF_DIFFERENT)
]=])
string(CONFIGURE "${deployment_script}" deployment_script @ONLY)
set(deployment_script_path "${CMAKE_CURRENT_BINARY_DIR}/DeploySimpleGame-$<CONFIG>.cmake")
file(GENERATE OUTPUT "${deployment_script_path}" CONTENT "${deployment_script}")
# CMP0112 is NEW with our minimum CMake version: TARGET_FILE_DIR adds no reverse edge.
add_custom_target(DeploySimpleGame
    COMMAND "${CMAKE_COMMAND}" -P "${deployment_script_path}"
    COMMENT "Deploying shaders, runtime resources, licenses, and architecture-matched GLEW"
    VERBATIM)
add_dependencies(SimpleGame DeploySimpleGame)
