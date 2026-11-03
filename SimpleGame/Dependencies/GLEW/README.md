# Existing GLEW runtime packaging

The original headers and import libraries remain at `../glew.h`, `../glew32.lib`,
and `../x64/glew32.lib`. This directory packages only the existing runtime DLLs,
their notices, and their identity manifest. No GLEW binary was downloaded or rebuilt.

| Platform | Packaged runtime | Original repository copies | PE machine | Bytes |
| --- | --- | --- | --- | --- |
| Win32 | `bin/Win32/glew32.dll` | `Debug/glew32.dll`, `Release/glew32.dll` | `0x014C` (x86) | 389632 |
| x64 | `bin/x64/glew32.dll` | `x64/Debug/glew32.dll`, `x64/Release/glew32.dll` | `0x8664` (x64) | 422912 |

Both original configurations contain identical bytes within each architecture.
Each packaged DLL is an exact copy of the corresponding original Debug DLL.
Debug and Release therefore share one packaged DLL per architecture. The existing
`glew32.lib` import libraries are used, not the `glew32s.lib` static libraries.
The game's and source-built GLFW's CRT settings are `/MDd` for Debug and `/MD` for
Release; this does not rebuild or change the CRT used by the existing GLEW DLLs.

`runtime-manifest.json` uses schema version 1:

- `architectures.Win32` and `architectures.x64` each contain `peMachine` (hex COFF
  machine value), `peMachineBytes` (little-endian bytes as lowercase hex), `runtime`,
  and `importLibrary`.
- `runtime` contains `path`, `size`, `sha256`, and `originalRepositoryPaths`.
- `importLibrary` contains `path`, `size`, and `sha256`.
- `path` is relative to this directory. `originalRepositoryPaths` are relative to
  the repository root. SHA-256 strings are uppercase hexadecimal.

The DOS/PE signatures and COFF machine values were inspected directly without
loading the DLLs. SHA-256 was checked for both original copies, both packaged DLLs,
and the original import libraries. MSBuild verifies the selected DLL's SHA-256
before building and copying; that hash pins the inspected architecture. CMake also
checks the actual PE machine and the import library's SHA-256 during configuration,
then rechecks the runtime's SHA-256 before deployment.

`LICENSE.txt` reproduces all three leading license blocks from the existing
`../glew.h` verbatim. Neither those headers nor the original libraries are modified.
Only the two exact `bin/*/glew32.dll` paths have Git ignore exceptions. No force-add
is needed, and no FreeGLUT or other DLL is deployed.
