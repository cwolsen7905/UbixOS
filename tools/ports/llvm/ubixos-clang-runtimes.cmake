# ubixos-clang-runtimes.cmake — cross-build the LLVM RUNTIMES
# (libc++ / libc++abi / libunwind) *for* uBixOS using Homebrew **clang@18**.
#
# libc++ 18 uses Clang-only builtins, so it must be compiled with clang, not the
# cross-GCC (proven 2026-06-24).  clang is a native cross-compiler: one binary,
# -target <triple>.  uBixOS is musl-on-aarch64 (FreeBSD syscall *numbers* are a
# musl-runtime detail, invisible at compile time), so we present as linux-musl —
# which also makes libc++ select the pthread thread API.
#
#   UBIXOS_SRCTOP   absolute path to the uBixOS source tree
#   UBIXOS_TARGET   aarch64 | x86_64

set(CMAKE_SYSTEM_NAME Linux)            # musl == Linux ABI for compile purposes
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES UBIXOS_SRCTOP UBIXOS_TARGET)

if(NOT DEFINED UBIXOS_TARGET)
  set(UBIXOS_TARGET aarch64)
endif()
if(UBIXOS_TARGET STREQUAL "x86_64")
  set(CMAKE_SYSTEM_PROCESSOR x86_64)
  set(_musl_arch x86_64)
  set(_triple x86_64-unknown-linux-musl)
else()
  set(CMAKE_SYSTEM_PROCESSOR aarch64)
  set(_musl_arch aarch64)
  set(_triple aarch64-unknown-linux-musl)
endif()

set(_llvm18 "/opt/homebrew/opt/llvm@18/bin")
set(CMAKE_C_COMPILER   "${_llvm18}/clang")
set(CMAKE_CXX_COMPILER "${_llvm18}/clang++")
set(CMAKE_ASM_COMPILER "${_llvm18}/clang")
set(CMAKE_AR     "${_llvm18}/llvm-ar"     CACHE FILEPATH "")
set(CMAKE_RANLIB "${_llvm18}/llvm-ranlib" CACHE FILEPATH "")
set(CMAKE_C_COMPILER_TARGET   "${_triple}")
set(CMAKE_CXX_COMPILER_TARGET "${_triple}")
set(CMAKE_ASM_COMPILER_TARGET "${_triple}")
set(CMAKE_C_COMPILER_WORKS   1)
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(NOT DEFINED UBIXOS_SRCTOP)
  message(FATAL_ERROR "UBIXOS_SRCTOP must be set")
endif()

set(_musl  "${UBIXOS_SRCTOP}/contrib/musl")
set(_build "${UBIXOS_SRCTOP}/build/${UBIXOS_TARGET}")
set(_shim  "${UBIXOS_SRCTOP}/tools/ports/llvm/shim")

# -nostdlibinc: drop the host's libc/C++ include dirs but KEEP clang's own
# resource headers (stddef.h/stdint.h/stdarg.h, the compiler builtins).  Then
# point at OUR musl.  The runtimes ARE libc++, so no pre-existing libc++ here.
set(_inc "-isystem ${_shim} -isystem ${_musl}/include -isystem ${_build}/obj/musl/obj/include -isystem ${_musl}/arch/${_musl_arch} -isystem ${_musl}/arch/generic")

set(CMAKE_C_FLAGS_INIT   "-nostdlibinc ${_inc} -fPIC -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT "-nostdlibinc -nostdinc++ ${_inc} -fPIC -ffunction-sections -fdata-sections")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -L${_build}/lib -fuse-ld=lld")

set(CMAKE_FIND_ROOT_PATH "${_build}" "${_musl}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
