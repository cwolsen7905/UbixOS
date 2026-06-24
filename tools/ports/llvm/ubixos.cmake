# ubixos.cmake — CMake toolchain file to cross-build LLVM/Clang/lld *for* uBixOS.
#
# This describes the TARGET compiler used to build the parts of LLVM that will
# run on uBixOS (the cross-compiled libraries + final clang/lld binaries).  The
# host-native build tools (llvm-tblgen, clang-tblgen) are built separately and
# fed in via -DLLVM_TABLEGEN/-DCLANG_TABLEGEN (see the port Makefile) — CMake
# cross-builds can't run target binaries, and LLVM's build runs tblgen at build
# time.
#
# Required cache vars (passed by the port Makefile via -D):
#   UBIXOS_SRCTOP   absolute path to the uBixOS source tree
#   UBIXOS_TARGET   aarch64 | x86_64
#
# Status: SCAFFOLDING.  Cross-building LLVM is intricate (C++ runtime, tblgen
# bootstrap, GB-class RAM); expect iteration.  See README.md.

set(CMAKE_SYSTEM_NAME Generic)          # uBixOS is not a CMake-known system
set(CMAKE_SYSTEM_VERSION 1)

if(NOT DEFINED UBIXOS_TARGET)
  set(UBIXOS_TARGET aarch64)
endif()
if(NOT DEFINED UBIXOS_SRCTOP)
  message(FATAL_ERROR "UBIXOS_SRCTOP must be set (-DUBIXOS_SRCTOP=...)")
endif()

if(UBIXOS_TARGET STREQUAL "x86_64")
  set(CMAKE_SYSTEM_PROCESSOR x86_64)
  set(_cross x86_64-elf-)
  set(_musl_arch x86_64)
  set(_triple x86_64-ubixos-musl)
else()
  set(CMAKE_SYSTEM_PROCESSOR aarch64)
  set(_cross aarch64-elf-)
  set(_musl_arch aarch64)
  set(_triple aarch64-ubixos-musl)
endif()

set(_musl   "${UBIXOS_SRCTOP}/contrib/musl")
set(_build  "${UBIXOS_SRCTOP}/build/${UBIXOS_TARGET}")
set(_libcxx "${UBIXOS_SRCTOP}/contrib/libcxx")

# uBixOS world toolchain (the same x86_64-elf-/aarch64-elf- cross-gcc + musl
# the rest of the world builds with — see CLAUDE.md).
set(CMAKE_C_COMPILER   "${_cross}gcc")
set(CMAKE_CXX_COMPILER "${_cross}g++")
set(CMAKE_ASM_COMPILER "${_cross}gcc")
set(CMAKE_AR           "${_cross}ar"     CACHE FILEPATH "")
set(CMAKE_RANLIB       "${_cross}ranlib" CACHE FILEPATH "")

# musl-freestanding include search (mirrors tools/ports/{bmake,sh}).
set(_musl_inc "-nostdinc -isystem ${_musl}/include -isystem ${_build}/obj/musl/obj/include -isystem ${_musl}/arch/${_musl_arch} -isystem ${_musl}/arch/generic")
set(_cxx_inc  "-nostdinc++ -isystem ${_libcxx}/include")

# LLVM is C++17 and big; build it without exceptions/RTTI (LLVM supports this via
# LLVM_ENABLE_EH/RTTI=OFF) since uBixOS's libc++ is currently -fno-exceptions.
set(CMAKE_C_FLAGS_INIT   "${_musl_inc} -fPIC -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT "${_musl_inc} ${_cxx_inc} -fPIC -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections")

# Static final binaries (no shared-lib mess on the target yet).
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -L${_build}/lib")

# Look for libs/headers in the uBixOS sysroot, programs on the host.
set(CMAKE_FIND_ROOT_PATH "${_build}" "${_musl}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# CMake can't run target test-binaries; skip its compiler sanity link.
set(CMAKE_C_COMPILER_WORKS   1)
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
