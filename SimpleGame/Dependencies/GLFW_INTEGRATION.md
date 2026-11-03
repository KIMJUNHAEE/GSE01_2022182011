# GLFW build and dependency integration

The game uses a source-built static GLFW 3.5.1 library with the Win32 backend.
The integration assumes the game's GLFW API migration; the legacy FreeGLUT
tutorial `SimpleGame/SimpleGame.cpp` remains in the repository but belongs to
neither active build source list. No dependency is downloaded during a game build.

## Visual Studio / MSBuild

Open `SimpleGame.sln` and select Debug or Release with x86 or x64. Solution x86 maps
to the native projects' Win32 platform. `SimpleGame.vcxproj` references
`Dependencies/GLFW/Build/GLFW.vcxproj`; building the game project by itself also
builds GLFW. Both projects use `$(DefaultPlatformToolset)` when no toolset was
explicitly selected, with installed v145/v144/v143/v142 fallbacks. The SDK family
is `10.0`. Install the C++ desktop tools and a Windows 10/11 SDK before building.

GLFW's 21 C translation units exactly match the common, Windows OS, and Win32
backend groups in its upstream `src/CMakeLists.txt`, including null, EGL, OSMesa,
and Vulkan loader modules. These modules use runtime discovery and do not require
separate Vulkan, EGL, OSMesa, or DirectInput SDK downloads. GLFW has no PCH or DLL
export/import defines. The upstream source language is C99; MSVC's `/std:c11`
supplies the C99 features because MSVC has no `/std:c99` switch. Use MSVC 19.27
(VS 2019 16.7) or newer. The game retains its existing PCH and C++17 settings.
Both projects use Unicode, UTF-8 input, and `/MDd` in Debug or `/MD` in Release.

All include/library/runtime paths derive from `$(ProjectDir)` rather than
`$(SolutionDir)`. Outputs are isolated from the old binary directories:

- Game: `out/msbuild/bin/<Win32|x64>/<Debug|Release>/SimpleGame.exe`.
- GLFW: `out/msbuild/lib/<Win32|x64>/<Debug|Release>/glfw3.lib`.
- Intermediates: `out/msbuild/obj/<platform>/<configuration>/<project>/`.

The final game link supplies OpenGL, GDI, User32, Shell32, WinHTTP, and the existing
Windows system libraries; a static GLFW archive does not package system libraries.
There is no active `freeglut.lib` link and no wildcard DLL copy. The Windows
subsystem and `mainCRTStartup` entry point are retained, so `Main.cpp` still receives
command-line arguments including `--verify`. Runtime resources are alongside the
executable, and the project debugger working directory points to that output.

## Optional CMake / Ninja route

CMake 3.24+, Ninja, MSVC, and a Windows SDK must already be installed. Use a matching
x64 or x86 Native Tools developer prompt; presets do not download or install tools.
They check the compiler's actual architecture rather than trusting the preset name.
For example, the user can run from the repository root:

```powershell
cmake --preset windows-x64-debug
cmake --build --preset windows-x64-debug
Push-Location out/cmake/windows-x64-debug/bin/Debug
.\SimpleGame.exe --verify
Pop-Location
```

The matching x64 Release, x86 Debug, and x86 Release preset names are
`windows-x64-release`, `windows-x86-debug`, and `windows-x86-release`. Each gets its
own `out/cmake/<preset>/` directory. A Visual Studio generator may also be selected
manually with `-S . -B out/cmake/<directory>` and the appropriate `-A` value; no
Visual Studio generator version or platform toolset is hard-coded in CMake.

CMake's explicit 34-file game source list matches the native project, excludes
`SimpleGame.cpp`, and compiles existing `stdafx.h` includes without using a PCH.
`Input.h`, `WindowMetrics.h`, and `GlfwInput.h` are registered with both build paths.
The game's local `Dependencies/GLFW/include/GLFW/glfw3.h` includes work directly;
`GLFW_INCLUDE_NONE` belongs only to the game, never GLFW's implementation sources.
`Input.h` is independent of GLFW; `GlfwInput.h` is its native adapter.

`add_subdirectory` uses the pristine vendored tree with a separate binary directory.
GLFW is forced static; examples, tests, docs, and install rules are disabled. No
fetch operation or mapping-update target is part of the default build. OpenGL's
imported target comes from the Windows SDK. The imported `GLEW::GLEW` target uses
the original local import library and the packaged DLL matching the compiler.
The generated deployment script avoids long Windows command lines. Its always-run
`DeploySimpleGame` target is a prerequisite of `SimpleGame`, so each default or
explicit `--target SimpleGame` build deploys once, including asset-only builds.
`file(GENERATE)` resolves `TARGET_FILE_DIR`; CMake 3.24's CMP0112 NEW behavior adds
no reverse dependency from deployment to the executable.

This route is explicitly Windows/MSVC only: current game GDI and WinHTTP code is
not a Linux/macOS port. GLFW's other backends remain pristine source, not enabled
game targets. No build, compiler invocation, CMake configure, or game execution was
performed while preparing this integration; the commands above are for the user.

## Runtime deployment and provenance

Both build paths deploy `Shaders/`, `Resources/`, a `Licenses/` directory, and exactly
one `glew32.dll`. All resource packs exclude Blender originals/backups (`*.blend*`)
and preview directories/files, including Protagonist3D and nested LowPoly/Rigged
previews. Asset licenses/notices within Resources stay with those assets.
Dependency notices include GLEW, GLFW, cgltf, nlohmann/json, model-loader notices,
and the complete existing `stb_image.h` with its embedded license text.
Use a clean output directory when reviewing packaging: deployment does not remove
obsolete files that someone manually placed into an existing output directory.

`GLEW/README.md` describes the existing DLLs, their original copies, PE machines,
hash checks, import libraries, and preserved header licenses. Debug and Release use
the same original GLEW runtime per architecture. Original GLEW headers/libs and old
FreeGLUT/sample files are retained without changes.

`GLFW.UPSTREAM.json` uses schema version 1. It records `version`,
`annotatedTagObject`, `commit`, `archiveSHA256`, `archivePath`, `vendoredPath`, `fileCount`, and
`files`. `files` maps slash-separated paths relative to `vendoredPath` to uppercase
SHA-256 values for all 159 upstream files. Local `Build/` integration files are
excluded. The supplied annotated tag is `70a9bb3881fe80fd483236e2b203cb451c6ecf40`,
its commit is `d9d6f0f1f967807ffade6598ea9a631ebaf37a56`, and the supplied archive
SHA-256 is `0ADF70BE965AF472C8B8D146D9C795C05510B6C1EF043DF3451AC3E2656902E7`.
The source-file hashes were computed locally. The archive is retained at the
repository-relative `archivePath` under the ignored staging directory. Its SHA-256
was independently verified by the main agent and rechecked during dependency integration.
The archive is not required to configure or build the game.

`.gitattributes` disables text conversion for upstream GLFW to preserve vendor
bytes even with `core.autocrlf=true`. Locally authored `Build/` project files use
Windows line endings; CMake and manifest files use LF. `.gitignore` allows the full
vendored tree and only the two packaged GLEW DLLs. All generated outputs stay under
the ignored `out/` directory, and no force-add is required for vendored files.
