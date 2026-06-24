# ubixos-clang.cmake — cross-build clang/lld *for* uBixOS using Homebrew clang@18
# and the full libc++ we built with ubixos-clang-runtimes.cmake.
#
# Differs from the runtimes toolchain: it consumes the just-built libc++ (its
# configured headers + static libs) and builds LLVM's own C++ with -fno-exceptions
# /-fno-rtti (LLVM_ENABLE_EH/RTTI=OFF).
#
#   UBIXOS_SRCTOP   absolute path to the uBixOS source tree
#   UBIXOS_TARGET   aarch64 | x86_64

set(CMAKE_SYSTEM_NAME Linux)
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
# The libc++ built by ubixos-clang-runtimes.cmake (configured headers + .a).
set(_rt    "${UBIXOS_SRCTOP}/build/ports/llvm-18.1.8/build-rt-${UBIXOS_TARGET}")

set(_musl_inc "-isystem ${_shim} -isystem ${_musl}/include -isystem ${_build}/obj/musl/obj/include -isystem ${_musl}/arch/${_musl_arch} -isystem ${_musl}/arch/generic")

set(CMAKE_C_FLAGS_INIT   "-nostdlibinc ${_musl_inc} -fPIC -ffunction-sections -fdata-sections")
# -nostdinc++ + the built libc++'s configured include; -fno-exceptions/-rtti per LLVM.
set(CMAKE_CXX_FLAGS_INIT "-nostdlibinc -nostdinc++ -isystem ${_rt}/include/c++/v1 ${_musl_inc} -fPIC -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections")

# Final static binaries: our libc++/abi/unwind + musl, via lld.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -fuse-ld=lld -L${_rt}/lib -L${_build}/lib -lc++ -lc++abi -lunwind")

set(CMAKE_FIND_ROOT_PATH "${_rt}" "${_build}" "${_musl}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
