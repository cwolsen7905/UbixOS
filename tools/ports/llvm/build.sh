#!/bin/sh
# build.sh — LLVM/Clang/lld Stage-0 cross-build for uBixOS, driven by Homebrew
# clang@18 (NOT the cross-GCC: libc++ 18 needs Clang-only builtins).
#
# Two phases:
#   1. RUNTIMES — cross-build a full libc++/libc++abi/libunwind for uBixOS
#      (threads/localization/wide-chars on) with clang@18.  The world's minimal
#      contrib/libcxx can't serve LLVM; this is the complete C++ runtime.
#   2. CLANG+LLD — cross-build clang/lld against that libc++, using Homebrew's
#      version-matched llvm-tblgen/clang-tblgen (LLVM_NATIVE_TOOL_DIR), so no
#      host-tblgen build is needed.
#
# Prereqs: brew install llvm@18 cmake ninja.  HEAVY: GB of objects, hours.
set -e
: "${WRKSRC:?}" "${BUILD:?}" "${TARGET:?}" "${SRCTOP:?}" "${PORTDIR:?}"

LLVM18="/opt/homebrew/opt/llvm@18/bin"
RTBLD="${WRKSRC}/build-rt-${TARGET}"
CROSSBLD="${WRKSRC}/build-${TARGET}"

# ── 1. Full libc++ / libc++abi / libunwind for uBixOS ────────────────────────
echo "==> [1/2] runtimes (full libc++) for ${TARGET}"
"${CMAKE}" -G Ninja -S "${WRKSRC}/runtimes" -B "${RTBLD}" \
	-DCMAKE_TOOLCHAIN_FILE="${PORTDIR}/ubixos-clang-runtimes.cmake" \
	-DUBIXOS_SRCTOP="${SRCTOP}" -DUBIXOS_TARGET="${TARGET}" \
	-DCMAKE_BUILD_TYPE=Release \
	-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
	-DLIBCXX_ENABLE_THREADS=ON -DLIBCXX_ENABLE_LOCALIZATION=ON \
	-DLIBCXX_ENABLE_WIDE_CHARACTERS=ON \
	-DLIBCXX_ENABLE_SHARED=OFF -DLIBCXXABI_ENABLE_SHARED=OFF -DLIBUNWIND_ENABLE_SHARED=OFF \
	-DLIBCXX_CXX_ABI=libcxxabi -DLIBCXXABI_USE_LLVM_UNWINDER=ON \
	-DLIBCXXABI_ENABLE_STATIC_UNWINDER=ON -DLIBCXX_HAS_MUSL_LIBC=ON \
	-DLLVM_INCLUDE_TESTS=OFF -DLIBCXX_INCLUDE_BENCHMARKS=OFF \
	-DLIBCXX_INCLUDE_TESTS=OFF -DLIBCXXABI_INCLUDE_TESTS=OFF
"${NINJA}" -C "${RTBLD}"

# ── 2. clang + lld for uBixOS (against the libc++ above) ──────────────────────
echo "==> [2/2] clang+lld for ${TARGET}"
"${CMAKE}" -G Ninja -S "${WRKSRC}/llvm" -B "${CROSSBLD}" \
	-DCMAKE_TOOLCHAIN_FILE="${PORTDIR}/ubixos-clang.cmake" \
	-DUBIXOS_SRCTOP="${SRCTOP}" -DUBIXOS_TARGET="${TARGET}" \
	-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${BUILD}/usr" \
	-DLLVM_ENABLE_PROJECTS="clang;lld" -DLLVM_TARGETS_TO_BUILD="X86;AArch64" \
	-DLLVM_NATIVE_TOOL_DIR="${LLVM18}" \
	-DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_BENCHMARKS=OFF \
	-DLLVM_ENABLE_RTTI=OFF -DLLVM_ENABLE_EH=OFF \
	-DLLVM_ENABLE_ZLIB=OFF -DLLVM_ENABLE_ZSTD=OFF -DLLVM_ENABLE_LIBXML2=OFF \
	-DLLVM_ENABLE_TERMINFO=OFF -DLLVM_ENABLE_LIBEDIT=OFF \
	-DHAVE_CXX_ATOMICS_WITHOUT_LIB=ON -DHAVE_CXX_ATOMICS64_WITHOUT_LIB=ON \
	-DHAVE_C_ATOMICS_WITHOUT_LIB=ON \
	-DLLVM_HOST_TRIPLE="${TARGET}-unknown-linux-musl" \
	-DCLANG_DEFAULT_LINKER=lld
# No MCJIT/ORC: saves size + the exec-page mmap churn uBixOS avoids.
"${NINJA}" -C "${CROSSBLD}" clang lld llvm-ar llvm-ranlib llvm-objcopy llvm-nm
echo "==> Stage-0 cross-build done; install with: ninja -C ${CROSSBLD} install"
