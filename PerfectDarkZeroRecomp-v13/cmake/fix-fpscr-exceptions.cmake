# Reapplies the FPSCR host-exception-masking fix to the vendored ReXGlue SDK
# headers. Upstream's FPSCRRegister::enableFlushModeUnconditional /
# disableFlushModeUnconditional / storeFromGuest write a possibly-stale
# in-memory `csr` straight to the host MXCSR without guaranteeing the FP
# exception-mask bits stay set, which can unmask host FP exceptions and turn
# a routine inexact/denormal float op into a hard crash
# (STATUS_FLOAT_INEXACT_RESULT 0xC000008F etc. - see
# /memories/repo/crash-diagnosis.md for the original diagnosis).
#
# The SDK is the thirdparty/rexglue-sdk submodule, so the headers are patched in
# place. The patch is idempotent and is reapplied on every configure, which also
# covers a submodule update or checkout that restores the pristine headers.
#
# Included from the root CMakeLists.txt (REXSDK_DIR is set there), or standalone:
#   cmake -DREXSDK_DIR=<path>/thirdparty/rexglue-sdk -P fix-fpscr-exceptions.cmake

if(NOT DEFINED REXSDK_INCLUDE_DIR)
    if(NOT DEFINED REXSDK_DIR)
        message(FATAL_ERROR "REXSDK_DIR (or REXSDK_INCLUDE_DIR) must be defined")
    endif()
    set(REXSDK_INCLUDE_DIR "${REXSDK_DIR}/include")
endif()

set(fpscr_file "${REXSDK_INCLUDE_DIR}/rex/platform/fpscr.h")
set(context_file "${REXSDK_INCLUDE_DIR}/rex/ppc/context.h")

if(NOT EXISTS "${fpscr_file}" OR NOT EXISTS "${context_file}")
    message(WARNING "[fix-fpscr-exceptions] ${fpscr_file} or ${context_file} not found, skipping")
    return()
endif()

# --- fpscr.h: add a portable DisableExceptions(csr) helper to both platform structs ---
file(READ "${fpscr_file}" fpscr_content)
if(fpscr_content MATCHES "DisableExceptions")
    message(STATUS "[fix-fpscr-exceptions] fpscr.h already patched")
else()
    set(fpscr_patched FALSE)

    set(x86_old "  static inline void InitHostExceptions(u32& csr) noexcept {\n    csr |= ExceptionMask;  // Set mask bits to disable exceptions\n  }\n};\n\n#elif defined(__aarch64__) || defined(_M_ARM64)")
    set(x86_new "  static inline void InitHostExceptions(u32& csr) noexcept {\n    csr |= ExceptionMask;  // Set mask bits to disable exceptions\n  }\n\n  // Forces host FP exceptions masked regardless of csr's prior state. Needed\n  // because callers like enableFlushModeUnconditional/storeFromGuest write a\n  // csr value that may have gone stale/lost its exception-mask bits (e.g. a\n  // fresh CallFrame copy) - blindly setcsr()'ing that would unmask host FP\n  // exceptions and turn a routine inexact/denormal result into a hard trap.\n  static constexpr u32 DisableExceptions(u32 csr) noexcept { return csr | ExceptionMask; }\n};\n\n#elif defined(__aarch64__) || defined(_M_ARM64)")
    string(FIND "${fpscr_content}" "${x86_old}" x86_pos)
    if(NOT x86_pos EQUAL -1)
        string(REPLACE "${x86_old}" "${x86_new}" fpscr_content "${fpscr_content}")
        set(fpscr_patched TRUE)
    endif()

    set(arm_old "  static inline void InitHostExceptions(u32& csr) noexcept {\n    csr &= ~ExceptionMask;  // Clear enable bits to disable exceptions\n  }\n};\n\n#else")
    set(arm_new "  static inline void InitHostExceptions(u32& csr) noexcept {\n    csr &= ~ExceptionMask;  // Clear enable bits to disable exceptions\n  }\n\n  // See x86_64 DisableExceptions above - same purpose, inverted polarity\n  // (ARM: 1 = exception enabled, so it must be cleared instead of set).\n  static constexpr u32 DisableExceptions(u32 csr) noexcept { return csr & ~ExceptionMask; }\n};\n\n#else")
    string(FIND "${fpscr_content}" "${arm_old}" arm_pos)
    if(NOT arm_pos EQUAL -1)
        string(REPLACE "${arm_old}" "${arm_new}" fpscr_content "${fpscr_content}")
        set(fpscr_patched TRUE)
    endif()

    if(fpscr_patched)
        file(WRITE "${fpscr_file}" "${fpscr_content}")
        message(STATUS "[fix-fpscr-exceptions] patched fpscr.h")
    else()
        message(WARNING "[fix-fpscr-exceptions] fpscr.h anchors not found (SDK version changed?) - reapply manually, see /memories/repo/crash-diagnosis.md")
    endif()
endif()

# --- context.h: route setcsr() through Platform::DisableExceptions() in the
# methods that write a cached csr value back to the host unconditionally ---
file(READ "${context_file}" context_content)
if(context_content MATCHES "Platform::DisableExceptions")
    message(STATUS "[fix-fpscr-exceptions] context.h already patched")
else()
    set(context_old [=[  inline void storeFromGuest(uint32_t value) noexcept {
    csr &= ~RoundMaskVal;
    csr |= Platform::GuestToHost[value & kRoundMask];
    setcsr(csr);
  }

  inline void enableFlushModeUnconditional() noexcept {
    csr |= FlushMask;
    setcsr(csr);
  }

  inline void disableFlushModeUnconditional() noexcept {
    csr &= ~FlushMask;
    setcsr(csr);
  }

  inline void enableFlushMode() noexcept {
    if ((csr & FlushMask) != FlushMask) [[unlikely]] {
      csr |= FlushMask;
      setcsr(csr);
    }
  }

  inline void disableFlushMode() noexcept {
    if ((csr & FlushMask) != 0) [[unlikely]] {
      csr &= ~FlushMask;
      setcsr(csr);
    }
  }]=])
    set(context_new [=[  inline void storeFromGuest(uint32_t value) noexcept {
    csr &= ~RoundMaskVal;
    csr |= Platform::GuestToHost[value & kRoundMask];
    setcsr(Platform::DisableExceptions(csr));
  }

  inline void enableFlushModeUnconditional() noexcept {
    csr |= FlushMask;
    setcsr(Platform::DisableExceptions(csr));
  }

  inline void disableFlushModeUnconditional() noexcept {
    csr &= ~FlushMask;
    setcsr(Platform::DisableExceptions(csr));
  }

  inline void enableFlushMode() noexcept {
    if ((csr & FlushMask) != FlushMask) [[unlikely]] {
      csr |= FlushMask;
      setcsr(Platform::DisableExceptions(csr));
    }
  }

  inline void disableFlushMode() noexcept {
    if ((csr & FlushMask) != 0) [[unlikely]] {
      csr &= ~FlushMask;
      setcsr(Platform::DisableExceptions(csr));
    }
  }]=])

    string(FIND "${context_content}" "${context_old}" context_pos)
    if(NOT context_pos EQUAL -1)
        string(REPLACE "${context_old}" "${context_new}" context_content "${context_content}")
        file(WRITE "${context_file}" "${context_content}")
        message(STATUS "[fix-fpscr-exceptions] patched context.h")
    else()
        message(WARNING "[fix-fpscr-exceptions] context.h anchor not found (SDK version changed?) - reapply manually, see /memories/repo/crash-diagnosis.md")
    endif()
endif()
