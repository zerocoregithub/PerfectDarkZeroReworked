# Third-party notices and corresponding source — release preparation

This is an inventory, **not a completed redistribution clearance**. Do not remove
original copyright notices. The final binary and corresponding-source bundle must
be reviewed together. A project MIT licence does not cover all dependencies.

## Confirmed components

| Component | Local notice / evidence |
|---|---|
| PerfectDarkZeroRecomp | Root LICENSE, MIT; original author Nikolai Gorb |
| Modified ReXGlue / Xenia portions | Sibling SDK LICENSE, BSD3 with Xenia attribution |
| SDL3 | thirdparty/sdl3/LICENSE.txt; zlib licence |
| Dear ImGui | thirdparty/imgui/LICENSE.txt; MIT |
| SMAA | thirdparty/smaa/LICENSE.txt; retain author notices for shader and lookup textures |
| FFmpeg | thirdparty/FFmpeg/LICENSE.md and COPYING.LGPLv2.1; custom CMake compiles libavcodec/libavutil STATIC |
| libmspack | thirdparty/libmspack/libmspack/COPYING.LIB; cabextract/mspack/lzxd.c identifies LGPL2.1 |
| fmt / spdlog / tomlplusplus / SIMDe / xxHash / UTFCPP | Individual LICENSE/COPYING files; retain each exact text |
| Vulkan headers / VMA / glslang / SPIRV headers/tools | Individual dependency notices; backend/build context matters |
| tiny-AES / aes_128 / o1heap / CLI11 / inja / Tracy | Retain per-file and dependency notices; source/build/runtime use differs |
| Disruptor++ / RenderDoc API / DXBC checksum / DXC headers / stb / PE/disasm helpers | Vendored/header components need notice provenance and binary-specific review; root notice presence alone is insufficient |

Additional texts restored: Disruptor++ MIT from its official repository; DXC's
University of Illinois/NCSA text from Microsoft's repository (matching the
vendored header's stated licence); tiny-AES Unlicense from its official repository;
stb dual-licence and RenderDoc MIT copied from the included headers; DXBC checksum
copyright/MD5 conditions retained verbatim from its source comments. Do not
describe DXC's old header as Apache merely because other newer LLVM code uses it.

The PowerPC disassembler contains GPL2-or-later/GPL1-or-later notices. Its GNU GPL2
text is now included. It links into the **code-generation tool**, not the shipped
game runtime: rexcodegen links rexruntime plus disasm, not the reverse. Runtime
export inspection found no codegen/disassembler symbols. Do not distribute
rexglue.exe as a BSD-only tool; it is not part of the game payload allowlist.

Windows package also contains Microsoft VC runtime DLLs. Confirm the exact
redistributable-file list and distribution terms for the installed toolchain;
shipping arbitrary DLLs copied from a developer machine is not a licence review.

## FFmpeg and libmspack

The inspected Windows x64 FFmpeg config has CONFIG_GPL=0, CONFIG_NONFREE=0 and
CONFIG_VERSION3=0. This is build evidence, not a complete check of every compiled
file's licence. The custom CMake build, overlay-generated lists, config headers,
all modifications and exact source must accompany the corresponding library
delivery. FFmpeg's [official checklist](https://ffmpeg.org/legal.html) recommends
matching source, build instructions and notices; its suggested dynamic-link path
does not describe this statically linked runtime.

Review the actual [LGPL2.1 terms](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html)
for this configuration, including a practical relink route. Do not simply add
the licence text and declare obligations complete. SDK source/build dependencies
and any required relink inputs must reproduce the supplied runtime independently
of the proprietary game inputs. A binary-only download plus an upstream link is
not asserted sufficient here. No arbitrary guest/game object files should be
published to solve a source-delivery problem.

## Provenance

The private release-completion inventory hashes18,906 files across34 dependency
trees and records recovered historical base commits where available. Current
content is identified by file hashes and normalized tree hashes, not falsely
labelled as pristine historical commits. This inventory does not prove every
vendored component is linked or approved for export. Required missing notice
texts and modified-source delivery must be resolved before publishing.

The archive exclusion `*.lib` previously discarded libmspack's COPYING.LIB text.
The exact original local text is restored and snapshot tooling now preserves it.
This restoration changes no runtime code. Historical source snapshots are kept.
