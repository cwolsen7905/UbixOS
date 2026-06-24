#!/bin/sh
# build.sh — LLVM/Clang/lld Stage-0 cross-build for uBixOS.
#
# Two phases (CMake cross-builds can't run target binaries, but LLVM runs tblgen
# at build time):
#   1. HOST tools  — build llvm-tblgen + clang-tblgen for the build host.
#   2. CROSS build — configure with ubixos.cmake + the host tblgens, build a
#      static clang + lld + llvm-ar/ranlib/objcopy/nm for uBixOS.
# Then compiler-rt is cross-built for the target (separate CMake invocation).
#
# Invoked by the port Makefile.  HEAVY: GB of objects, hours.
set -e
: "${WRKSRC:?}" "${BUILD:?}" "${TARGET:?}" "${SRCTOP:?}" "${PORTDIR:?}"

HOSTBLD="${WRKSRC}/build-host"
CROSSBLD="${WRKSRC}/build-${TARGET}"
LLVM="${WRKSRC}/llvm"

# ── 1. Host-native tblgen (+ the minimal libs they need) ─────────────────────
echo "==> [1/2] host tblgen"
"${CMAKE}" -G Ninja -S "${LLVM}" -B "${HOSTBLD}" \
	-DCMAKE_BUILD_TYPE=Release \
	-DLLVM_ENABLE_PROJECTS=clang \
	-DLLVM_TARGETS_TO_BUILD="X86;AArch64" \
	-DLLVM_ENABLE_RTTI=OFF -DLLVM_ENABLE_EH=OFF \
	-DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_BENCHMARKS=OFF
"${NINJA}" -C "${HOSTBLD}" -j"${JOBS}" llvm-tblgen clang-tblgen

# ── 2. Cross-build clang + lld for uBixOS ────────────────────────────────────
echo "==> [2/2] cross clang+lld for ${TARGET}"
"${CMAKE}" -G Ninja -S "${LLVM}" -B "${CROSSBLD}" \
	-DCMAKE_TOOLCHAIN_FILE="${PORTDIR}/ubixos.cmake" \
	-DUBIXOS_SRCTOP="${SRCTOP}" -DUBIXOS_TARGET="${TARGET}" \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX="${BUILD}/usr" \
	-DLLVM_ENABLE_PROJECTS="clang;lld" \
	-DLLVM_TARGETS_TO_BUILD="X86;AArch64" \
	-DLLVM_TABLEGEN="${HOSTBLD}/bin/llvm-tblgen" \
	-DCLANG_TABLEGEN="${HOSTBLD}/bin/clang-tblgen" \
	-DLLVM_ENABLE_RTTI=OFF -DLLVM_ENABLE_EH=OFF \
	-DLLVM_ENABLE_ZLIB=OFF -DLLVM_ENABLE_ZSTD=OFF -DLLVM_ENABLE_LIBXML2=OFF \
	-DLLVM_ENABLE_TERMINFO=OFF -DLLVM_ENABLE_LIBEDIT=OFF \
	-DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_BENCHMARKS=OFF \
	-DLLVM_BUILD_STATIC=ON -DBUILD_SHARED_LIBS=OFF \
	-DCLANG_DEFAULT_LINKER=lld \
	-DLLVM_HOST_TRIPLE="${TARGET}-ubixos-musl"
# No MCJIT/ORC: saves size + the executable-page mmap churn uBixOS avoids.
"${NINJA}" -C "${CROSSBLD}" -j"${JOBS}" clang lld llvm-ar llvm-ranlib llvm-objcopy llvm-nm
echo "==> Stage-0 cross-build done; install with: ninja -C ${CROSSBLD} install"
