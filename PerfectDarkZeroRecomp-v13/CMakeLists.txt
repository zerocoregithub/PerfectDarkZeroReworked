# perfectdarkzerorecomp - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(perfectdarkzerorecomp LANGUAGES C CXX)

option(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS "Compile legacy input/weapon diagnostic probes" OFF)
if(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
    set(REX_DISABLE_RUNTIME_LOGGING OFF CACHE BOOL "Explicit diagnostic runtime logging" FORCE)
else()
    set(REX_DISABLE_RUNTIME_LOGGING ON CACHE BOOL "Ordinary Reworked builds are sink-free" FORCE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Explicit candidate SDK selection; never modify our preserved baseline SDK.
set(REXSDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../rexglue-sdk-v13" CACHE PATH "Migration SDK")
if(WIN32)
    set(REXGLUE_USE_D3D12 ON CACHE BOOL "D3D12 backend")
    set(REXGLUE_USE_VULKAN ON CACHE BOOL "Vulkan backend")
    include(cmake/fix_symlinks.cmake)
    include(cmake/fix-fpscr-exceptions.cmake)
endif()

include(generated/rexglue.cmake)

# Sources
set(PERFECTDARKZERORECOMP_SOURCES
    src/main.cpp
    src/pdz_overlay.cpp
    src/pdz_qol.cpp
    src/pdz_p9p_model.cpp
    src/pdz_input.cpp
    src/game_cvars.cpp
    src/local_profiles.cpp
    src/pdz_language.cpp
    src/render/gpu_wait_hook.cpp
    src/mod/third_person.cpp
)

if(WIN32)
    add_executable(perfectdarkzerorecomp WIN32 ${PERFECTDARKZERORECOMP_SOURCES})
    set(PDZ_BUILD_LABEL "dev" CACHE STRING "Portable launcher build label")
    add_executable(pdz_portable_launcher WIN32 src/pdz_portable_launcher.cpp)
    set_target_properties(pdz_portable_launcher PROPERTIES
        OUTPUT_NAME "PerfectDarkZeroReworked-${PDZ_BUILD_LABEL}"
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_link_libraries(pdz_portable_launcher PRIVATE user32 comdlg32 bcrypt)
else()
    add_executable(perfectdarkzerorecomp ${PERFECTDARKZERORECOMP_SOURCES})
endif()

rexglue_setup_target(perfectdarkzerorecomp GPU_PLUGINS xenos)

if(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
    target_compile_definitions(perfectdarkzerorecomp PRIVATE PDZ_ENABLE_DEVELOPER_DIAGNOSTICS=1)
    if(TARGET perfectdarkzerorecomp_recomp)
        target_compile_definitions(perfectdarkzerorecomp_recomp PRIVATE PDZ_ENABLE_DEVELOPER_DIAGNOSTICS=1)
    endif()
endif()

# Offline migration validation; not linked into or deployed with the game.
set(PDZ_SHADER_TEST_SOURCES
    flags.cpp
    util/draw.cpp
    registers.cpp
    pipeline/shader/shader.cpp
    pipeline/shader/dxbc.cpp
    pipeline/shader/dxbc_translator.cpp
    pipeline/shader/dxbc_translator_alu.cpp
    pipeline/shader/dxbc_translator_fetch.cpp
    pipeline/shader/dxbc_translator_memexport.cpp
    pipeline/shader/dxbc_translator_om.cpp
    pipeline/shader/translator.cpp
    pipeline/shader/translator_disasm.cpp
    format/ucode.cpp
    pipeline/shader/spirv.cpp
    pipeline/shader/spirv_builder.cpp
    pipeline/shader/spirv_translator.cpp
    pipeline/shader/spirv_translator_alu.cpp
    pipeline/shader/spirv_translator_fetch.cpp
    pipeline/shader/spirv_translator_memexport.cpp
    pipeline/shader/spirv_translator_rb.cpp)
list(TRANSFORM PDZ_SHADER_TEST_SOURCES PREPEND "${REXSDK_DIR}/src/graphics/")
add_executable(pdz_vulkan_shader_test EXCLUDE_FROM_ALL
    tools/test-vulkan-scene-translation.cpp ${PDZ_SHADER_TEST_SOURCES}
    "${REXSDK_DIR}/thirdparty/dxbc/DXBCChecksum.cpp")
target_include_directories(pdz_vulkan_shader_test PRIVATE "${REXSDK_DIR}" "${REXSDK_DIR}/include"
    "${REXSDK_DIR}/thirdparty/dxbc"
    "${REXSDK_DIR}/thirdparty/renderdoc")
target_link_libraries(pdz_vulkan_shader_test PRIVATE rexruntime xxHash::xxhash
    d3dcompiler
    Vulkan::Headers glslang::SPIRV glslang MachineIndependent GenericCodeGen
    OSDependent OGLCompiler)
set(SPIRV-Headers_SOURCE_DIR "${REXSDK_DIR}/thirdparty/spirv-headers")
set(SPIRV_SKIP_TESTS ON CACHE BOOL "No dependency tests")
set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "Validation library only")
add_subdirectory("${REXSDK_DIR}/thirdparty/spirv-tools" spirv-validation EXCLUDE_FROM_ALL)
target_link_libraries(pdz_vulkan_shader_test PRIVATE SPIRV-Tools-static)

# Offline image inspection only: never launches the loaded guest executable.
add_executable(pdz_offline_image EXCLUDE_FROM_ALL tools/pdz-offline-image.cpp)
target_link_libraries(pdz_offline_image PRIVATE rexruntime)
add_executable(pdz_rumble_test EXCLUDE_FROM_ALL tools/test-rumble.cpp)
target_link_libraries(pdz_rumble_test PRIVATE rexruntime)

# Direct audio-driver verification; never launches guest code. Default invocation
# tests silent pacing without touching Windows device configuration.
add_executable(pdz_silent_audio_test EXCLUDE_FROM_ALL
    tools/test-silent-audio.cpp
    "${REXSDK_DIR}/src/audio/sdl/sdl_audio_driver.cpp")
target_link_libraries(pdz_silent_audio_test PRIVATE rexruntime SDL3::SDL3)
add_executable(pdz_audio_queue_test EXCLUDE_FROM_ALL
    tools/test-audio-queue.cpp
    "${REXSDK_DIR}/src/audio/sdl/sdl_audio_driver.cpp")
target_link_libraries(pdz_audio_queue_test PRIVATE rexruntime SDL3::SDL3)

# A real DLL-linked check for ordinary sink-free runtime builds. Not shipped.
add_executable(pdz_quiet_runtime_test EXCLUDE_FROM_ALL tools/test-quiet-runtime.cpp)
target_link_libraries(pdz_quiet_runtime_test PRIVATE rexruntime)
if(MSVC)
    target_compile_options(pdz_quiet_runtime_test PRIVATE /UNDEBUG)
else()
    target_compile_options(pdz_quiet_runtime_test PRIVATE -UNDEBUG)
endif()

# The recompiler emits the guest program across many translation units. Hot,
# tiny guest helpers may otherwise remain calls at every use site. ThinLTO lets
# Clang import and inline them across units. This retains the accepted build
# policy; it is not a measured performance-improvement claim for every machine.
option(PDZ_ENABLE_THIN_LTO "Enable ThinLTO for optimized PDZ builds" ON)
if(PDZ_ENABLE_THIN_LTO AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(perfectdarkzerorecomp_recomp PRIVATE
        $<$<CONFIG:Release>:-flto=thin>)
    target_compile_options(perfectdarkzerorecomp PRIVATE
        $<$<CONFIG:Release>:-flto=thin>)
    target_link_options(perfectdarkzerorecomp PRIVATE
        $<$<CONFIG:Release>:-flto=thin>)
endif()

# The source-SDK interface currently doesn't propagate ImGui's public include
# directory to consuming app sources, though rex/rex_app.h exposes ImGui types.
if(REXSDK_DIR)
    target_include_directories(perfectdarkzerorecomp PRIVATE
        "${REXSDK_DIR}/thirdparty/imgui")
endif()

# Codegen regenerates generated/default/*.cpp from scratch and can reintroduce
# cross-function tail-jumps that don't compile (see  tools/fix-unresolved-tail-jumps.ps1). Reapply the workaround automatically
# after every codegen run, before anything tries to compile the result.
if(WIN32)
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/tailjump_patch.stamp"
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/fix-unresolved-tail-jumps.ps1"
                -GeneratedDir "${CMAKE_CURRENT_SOURCE_DIR}/generated/default"
        COMMAND ${CMAKE_COMMAND} -E touch
                "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/tailjump_patch.stamp"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/codegen.build.stamp"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/fix-unresolved-tail-jumps.ps1"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/patch-generated-runtime.ps1"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/apply-p9p-visibility-hook.ps1"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/patch-vision-color.ps1"
        COMMENT "Patching unresolved cross-function tail-jumps in generated code"
        VERBATIM
    )
    add_custom_target(perfectdarkzerorecomp_fix_tailjumps
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/tailjump_patch.stamp")
    add_dependencies(perfectdarkzerorecomp_recomp perfectdarkzerorecomp_fix_tailjumps)
endif()

# SDL looks for this file (name set by settings/mapping.toml's
# hid_mappings_file) next to the exe at startup to fill in gamepads it
# doesn't already recognize. Sourced from
# https://github.com/mdqinc/SDL_GameControllerDB, see settings/README.md.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/settings/gamecontrollerdb.txt")
    add_custom_command(TARGET perfectdarkzerorecomp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/settings/gamecontrollerdb.txt"
                "$<TARGET_FILE_DIR:perfectdarkzerorecomp>/gamecontrollerdb.txt"
        COMMENT "Copying gamecontrollerdb.txt next to perfectdarkzerorecomp.exe")
endif()
