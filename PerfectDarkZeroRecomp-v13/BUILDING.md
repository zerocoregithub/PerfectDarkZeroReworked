# Building and corresponding source

Use the matching game source and modified SDK source bundle supplied with this
candidate, not an arbitrary upstream SDK revision. No ISO, extracted game data,
generated guest C++ or personal settings are included in source delivery.

## Windows prerequisites

Visual Studio 2022 C++ Build Tools (Windows SDK, LLVM/Clang, CMake and Ninja),
PowerShell 7 and Python 3 are required. Place game source and SDK source in sibling
folders named PerfectDarkZeroRecomp-v13 and rexglue-sdk-v13. The SDK source archive
contains an integrity manifest verified during configuration; it is not a signature.

## Rebuild the SDK/runtime independently

Configure the SDK with CMake/Ninja and Clang, setting REXGLUE_SOURCE_ARCHIVE=ON,
REX_DISABLE_RUNTIME_LOGGING=ON and CMAKE_BUILD_TYPE=Release. Build rexruntime and
rexgpu-xenos. No proprietary game input is needed for those library targets.
Retain the complete SDK sources, dependency modifications, build files and notices.
Replacing the corresponding runtime DLLs provides the library replacement route;
the SDK source also permits rebuilding its statically linked FFmpeg/libmspack.
If modifying the archive, regenerate its integrity manifest with the source-export
tool rather than silently bypassing verification. Keep the original snapshot.

## Generate and build the game

The accepted emission contract requires the historical ReXGlue compiler source at
commit f5337cdc947ff6d4c4196737e2c807a48f2a1fc2 from
https://github.com/rexglue/rexglue-sdk . Build that compiler with its pinned recursive
dependencies. On Windows, use cmake/fix_symlinks.cmake when Git has materialized
source symlinks as text files. This compiler is a build tool, not a shipped runtime;
retain its GPL disassembler notices and do not label it BSD-only.

Supply your legally owned supported executable under assets/default.xex. The
build wrapper verifies SHA256 E50192F437BD1FF70A65FAE8B5561AA822AB91052FE35FD500594689E130859E.
Run tools/build-release.ps1 with explicit SDKSource, Recompiler (the historical
compiler), PythonExecutable and a fresh BuildDirectory. The wrapper seeds the
checked-in codegen partition, applies adapters and checks their emission contract.
Do not regenerate with the runtime SDK's newer compiler or mix old generated units.

Normal builds are sink-free; DeveloperDiagnostics is explicit opt-in. Build the
optional quiet-runtime, silent-audio and audio-queue test targets and run them
separately. Compilation and harnesses do not replace gameplay regression testing.

This candidate has an isolated fresh source build, regenerated-unit checks and a
complete delivered-archive rebuild. Final gameplay acceptance remains a separate gate.
No legal certification or public-upload approval is implied by these instructions.
