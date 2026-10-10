#!/bin/sh
# Cross-build LLVM clang + lld that run natively on UbixOS aarch64, and lay
# out the files the image installs (stage "dist").
#
# Host: Apple Silicon macOS with Homebrew llvm + lld (brew install llvm lld),
#       cmake, ninja, python3.  The full build takes ~30 min.
# Target: aarch64-unknown-linux-musl ABI (UbixOS's patched musl, PIE, dynamic
#         against /lib/ld-musl-aarch64.so.1; libc++/libunwind linked static).
#
# Work tree: $LLVM_WORK (default build/llvm, git-ignored): llvm-project/,
# sysroot/ (a snapshot of this tree's musl build — re-run the sysroot stage
# after a world rebuild), build-*/ and dist/.  tools/mkimage-arm.sh installs
# dist/ into the root pool when it exists (and enlarges the pool for it).
#
# Usage: tools/build-llvm.sh [stage ...]
#   stages: fetch sysroot builtins runtimes hosttools cross dist
#           all (default: every stage in that order)
# Requires `bmake world TARGET=aarch64` first (the sysroot copies its musl).
set -eu

UBIX=${UBIX:-$(cd "$(dirname "$0")/.." && pwd)}
UBIXB=$UBIX/build/aarch64
TOP=${LLVM_WORK:-$UBIX/build/llvm}
mkdir -p "$TOP"
TOP=$(cd "$TOP" && pwd)
LLVM_TAG=${LLVM_TAG:-llvmorg-20.1.8}
SRC=$TOP/llvm-project
SYSROOT=$TOP/sysroot
TRIPLE=aarch64-unknown-linux-musl
HOSTLLVM=${HOSTLLVM:-/opt/homebrew/opt/llvm}
HOSTCLANG_VER=$($HOSTLLVM/bin/clang -dumpversion | cut -d. -f1)
# Private copy of the host clang resource dir, so we can drop the target
# compiler-rt builtins into it without touching Homebrew.
RESDIR=$TOP/resource
JOBS=${JOBS:-$(sysctl -n hw.ncpu)}

CC_T="$HOSTLLVM/bin/clang"
CXX_T="$HOSTLLVM/bin/clang++"
TFLAGS="--target=$TRIPLE --sysroot=$SYSROOT -resource-dir=$RESDIR -fuse-ld=lld --ld-path=${HOSTLLD:-/opt/homebrew/bin/ld.lld}"
# Host clang would otherwise pick Homebrew's own libc++ headers
# (<install>/include/c++/v1) ahead of the sysroot's.
TCXXFLAGS="$TFLAGS -nostdinc++ -isystem $SYSROOT/usr/include/c++/v1"
# Linking: musl libc.so dynamic, compiler-rt builtins, LLVM libunwind, static libc++.
TLDFLAGS="-rtlib=compiler-rt -unwindlib=libunwind -stdlib=libc++ -static-libstdc++ -Wl,--build-id=none"

# Common CMake cross settings for target builds.
cross_cmake_common() {
	echo -DCMAKE_SYSTEM_NAME=Linux \
	     -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
	     -DCMAKE_C_COMPILER=$CC_T -DCMAKE_CXX_COMPILER=$CXX_T \
	     -DCMAKE_ASM_COMPILER=$CC_T \
	     -DCMAKE_C_COMPILER_TARGET=$TRIPLE -DCMAKE_CXX_COMPILER_TARGET=$TRIPLE \
	     -DCMAKE_ASM_COMPILER_TARGET=$TRIPLE \
	     -DCMAKE_SYSROOT=$SYSROOT \
	     -DCMAKE_AR=$HOSTLLVM/bin/llvm-ar -DCMAKE_RANLIB=$HOSTLLVM/bin/llvm-ranlib \
	     -DCMAKE_NM=$HOSTLLVM/bin/llvm-nm \
	     -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
	     -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
	     -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
	     -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY \
	     -DCMAKE_OSX_SYSROOT= -DCMAKE_OSX_DEPLOYMENT_TARGET= -DCMAKE_OSX_ARCHITECTURES=
}

stage_fetch() {
	[ -d "$SRC/.git" ] && return 0
	git clone --depth 1 --branch "$LLVM_TAG" https://github.com/llvm/llvm-project.git "$SRC"
}

# 1. Sysroot: musl headers merged the way `make install-headers` does, plus the
#    UbixOS-built crt objects and libc (static + shared).
stage_sysroot() {
	M=$UBIX/contrib/musl
	rm -rf "$SYSROOT"
	mkdir -p "$SYSROOT/usr/include/bits" "$SYSROOT/usr/lib" "$SYSROOT/lib"
	cp -R "$M/include/." "$SYSROOT/usr/include/"
	cp "$M/arch/generic/bits/"*.h "$SYSROOT/usr/include/bits/"
	cp "$M/arch/aarch64/bits/"*.h "$SYSROOT/usr/include/bits/"
	cp "$UBIXB/obj/musl/obj/include/bits/"*.h "$SYSROOT/usr/include/bits/"
	for f in crt1.o crti.o crtn.o Scrt1.o rcrt1.o libc.a libm.a libpthread.a \
	         libdl.a librt.a libutil.a libxnet.a libcrypt.a libresolv.a; do
		cp "$UBIXB/obj/musl/lib/$f" "$SYSROOT/usr/lib/"
	done
	cp "$UBIXB/lib/libc.so" "$SYSROOT/usr/lib/libc.so"
	cp "$UBIXB/ld-musl-aarch64.so.1" "$SYSROOT/lib/ld-musl-aarch64.so.1"
	# BLOCKER WORKAROUND: musl ships no Linux UAPI headers; libc++'s atomic.cpp
	# (atomic wait/notify) includes <linux/futex.h> and calls
	# syscall(SYS_futex, ...).  musl's SYS_futex is UbixOS-native 0x8000|64,
	# which the aarch64 kernel routes to sys_futex (FUTEX_WAIT/WAKE).
	mkdir -p "$SYSROOT/usr/include/linux"
	cat > "$SYSROOT/usr/include/linux/futex.h" <<-'FUTEX_EOF'
	/* Minimal <linux/futex.h> shim for UbixOS (musl has no kernel UAPI headers). */
	#ifndef _LINUX_FUTEX_H
	#define _LINUX_FUTEX_H
	#define FUTEX_WAIT		0
	#define FUTEX_WAKE		1
	#define FUTEX_PRIVATE_FLAG	128
	#define FUTEX_WAIT_PRIVATE	(FUTEX_WAIT | FUTEX_PRIVATE_FLAG)
	#define FUTEX_WAKE_PRIVATE	(FUTEX_WAKE | FUTEX_PRIVATE_FLAG)
	#endif
	FUTEX_EOF
}

# 2. compiler-rt builtins for the target (static), installed into $RESDIR.
stage_builtins() {
	B=$TOP/build-builtins
	# Private resource dir (clang builtin headers) for the host clang.
	rm -rf "$RESDIR"
	cp -R "$HOSTLLVM/lib/clang/$HOSTCLANG_VER" "$RESDIR"
	rm -rf "$B"
	cmake -G Ninja -S "$SRC/compiler-rt/lib/builtins" -B "$B" \
		$(cross_cmake_common) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_C_FLAGS="--target=$TRIPLE -resource-dir=$RESDIR" \
		-DCMAKE_ASM_FLAGS="--target=$TRIPLE -resource-dir=$RESDIR" \
		-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
		-DCOMPILER_RT_BAREMETAL_BUILD=OFF \
		-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=ON \
		-DCOMPILER_RT_INSTALL_PATH="$RESDIR" \
		-DLLVM_CMAKE_DIR="$SRC/llvm/cmake/modules"
	ninja -C "$B" -j$JOBS
	ninja -C "$B" install
}

# 3. libunwind + libc++abi + libc++ (LLVM 20, static, threads ON so <mutex>
#    exists — pthread mutexes work single-threaded even with no clone()).
stage_runtimes() {
	B=$TOP/build-runtimes
	rm -rf "$B"
	cmake -G Ninja -S "$SRC/runtimes" -B "$B" \
		$(cross_cmake_common) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_C_FLAGS="$TFLAGS" -DCMAKE_CXX_FLAGS="$TFLAGS" -DCMAKE_ASM_FLAGS="$TFLAGS" \
		-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
		-DCMAKE_INSTALL_PREFIX="$SYSROOT/usr" \
		-DLLVM_ENABLE_RUNTIMES="libunwind;libcxxabi;libcxx" \
		-DLLVM_DEFAULT_TARGET_TRIPLE=$TRIPLE \
		-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF \
		-DLIBUNWIND_ENABLE_SHARED=OFF -DLIBUNWIND_USE_COMPILER_RT=ON \
		-DLIBCXXABI_ENABLE_SHARED=OFF -DLIBCXXABI_USE_COMPILER_RT=ON \
		-DLIBCXXABI_USE_LLVM_UNWINDER=ON -DLIBCXXABI_ENABLE_STATIC_UNWINDER=ON \
		-DLIBCXX_ENABLE_SHARED=OFF -DLIBCXX_USE_COMPILER_RT=ON \
		-DLIBCXX_HAS_MUSL_LIBC=ON \
		-DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=ON \
		-DLIBCXX_INCLUDE_TESTS=OFF -DLIBCXX_INCLUDE_BENCHMARKS=OFF \
		-DLIBCXXABI_INCLUDE_TESTS=OFF -DLIBUNWIND_INCLUDE_TESTS=OFF \
		-DLIBCXX_ENABLE_TIME_ZONE_DATABASE=OFF \
		-DLIBCXXABI_HAS_CXA_THREAD_ATEXIT_IMPL=OFF \
		-DLIBCXX_HAS_ATOMIC_LIB=OFF
	# ^ STATIC_LIBRARY try-compile makes every link probe "succeed"; musl has
	#   no __cxa_thread_atexit_impl (glibc-only) and there is no libatomic.
	ninja -C "$B" -j$JOBS
	ninja -C "$B" install
}

# 4. Native host tools: llvm-tblgen, clang-tblgen (+ llvm-min-tblgen).
stage_hosttools() {
	B=$TOP/build-host
	cmake -G Ninja -S "$SRC/llvm" -B "$B" \
		-DCMAKE_BUILD_TYPE=Release \
		-DLLVM_ENABLE_PROJECTS="clang" \
		-DLLVM_TARGETS_TO_BUILD=AArch64 \
		-DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_EXAMPLES=OFF \
		-DLLVM_INCLUDE_BENCHMARKS=OFF -DCLANG_INCLUDE_TESTS=OFF
	ninja -C "$B" -j$JOBS llvm-tblgen clang-tblgen llvm-min-tblgen
}

# 5. The cross build: clang + lld that run on UbixOS.
#    LLVM_ENABLE_THREADS=OFF: aarch64 UbixOS has no working pthread_create
#      (kernel sys_rfork is an ENOSYS stub; musl aarch64 __clone uses Linux
#      svc #220, which is FreeBSD's dead "old __semctl" slot).
#    HAVE_POSIX_SPAWN=0: musl posix_spawn is built on that same __clone
#      (CLONE_VM|CLONE_VFORK), so force LLVM's fork()+execve() path
#      (fork = FreeBSD slot 2, implemented) for clang -> ld.lld spawning.
stage_cross() {
	B=$TOP/build-cross
	H=$TOP/build-host/bin
	cmake -G Ninja -S "$SRC/llvm" -B "$B" \
		$(cross_cmake_common) \
		-DCMAKE_BUILD_TYPE=MinSizeRel \
		-DCMAKE_C_FLAGS="$TFLAGS" -DCMAKE_CXX_FLAGS="$TCXXFLAGS" -DCMAKE_ASM_FLAGS="$TFLAGS" \
		-DCMAKE_EXE_LINKER_FLAGS="$TLDFLAGS" \
		-DCMAKE_SHARED_LINKER_FLAGS="$TLDFLAGS" \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DLLVM_ENABLE_PROJECTS="clang;lld" \
		-DLLVM_TARGETS_TO_BUILD=AArch64 \
		-DLLVM_HOST_TRIPLE=$TRIPLE \
		-DLLVM_DEFAULT_TARGET_TRIPLE=$TRIPLE \
		-DLLVM_TABLEGEN=$H/llvm-tblgen \
		-DCLANG_TABLEGEN=$H/clang-tblgen \
		-DLLVM_NATIVE_TOOL_DIR=$H \
		-DLLVM_ENABLE_THREADS=OFF \
		-DHAVE_POSIX_SPAWN=0 \
		-DLLVM_ENABLE_PIC=ON \
		-DLLVM_BUILD_LLVM_DYLIB=OFF -DLLVM_LINK_LLVM_DYLIB=OFF \
		-DBUILD_SHARED_LIBS=OFF \
		-DLLVM_ENABLE_LIBCXX=ON \
		-DLLVM_ENABLE_ZLIB=OFF -DLLVM_ENABLE_ZSTD=OFF -DLLVM_ENABLE_LIBXML2=OFF \
		-DLLVM_ENABLE_TERMINFO=OFF -DLLVM_ENABLE_LIBEDIT=OFF \
		-DLLVM_ENABLE_LIBPFM=OFF -DLLVM_ENABLE_BACKTRACES=OFF \
		-DLLVM_ENABLE_CRASH_OVERRIDES=OFF \
		-DLLVM_ENABLE_PLUGINS=OFF \
		-DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_EXAMPLES=OFF \
		-DLLVM_INCLUDE_BENCHMARKS=OFF -DLLVM_INCLUDE_DOCS=OFF \
		-DLLVM_INCLUDE_UTILS=OFF -DLLVM_BUILD_UTILS=OFF \
		-DLLVM_BUILD_TOOLS=OFF \
		-DCLANG_INCLUDE_TESTS=OFF -DCLANG_INCLUDE_DOCS=OFF \
		-DCLANG_ENABLE_ARCMT=OFF -DCLANG_ENABLE_STATIC_ANALYZER=OFF \
		-DCLANG_BUILD_TOOLS=OFF \
		-DCLANG_PLUGIN_SUPPORT=OFF \
		-DCLANG_DEFAULT_RTLIB=compiler-rt -DCLANG_DEFAULT_UNWINDLIB=libunwind \
		-DCLANG_DEFAULT_CXX_STDLIB=libc++ -DCLANG_DEFAULT_LINKER=lld \
		-DLLD_BUILD_TOOLS=ON
	ninja -C "$B" -j$JOBS clang lld
	"$HOSTLLVM/bin/llvm-strip" -o "$TOP/out-clang" "$B/bin/clang-20" 2>/dev/null ||
		"$HOSTLLVM/bin/llvm-strip" -o "$TOP/out-clang" "$B/bin/clang"
	"$HOSTLLVM/bin/llvm-strip" -o "$TOP/out-lld" "$B/bin/lld"
	ls -l "$TOP/out-clang" "$TOP/out-lld"
	file "$TOP/out-clang" "$TOP/out-lld"
}

# 6. dist/: the on-target tree tools/mkimage-arm.sh copies into the root pool.
#    clang finds its resource dir relative to its own binary (/bin ->
#    /lib/clang/<ver>), and lld looks for libraries in /usr/lib.
stage_dist() {
	D=$TOP/dist
	V=$(ls "$TOP/build-cross/lib/clang")
	rm -rf "$D"
	mkdir -p "$D/bin" "$D/lib/clang/$V/lib" "$D/usr/lib" "$D/usr/include"
	cp "$TOP/out-clang" "$D/bin/clang"
	cp "$TOP/out-lld" "$D/bin/ld.lld"
	cp -R "$TOP/build-cross/lib/clang/$V/include" "$D/lib/clang/$V/"
	# Headers for other targets / offload runtimes are dead weight here.
	rm -rf "$D/lib/clang/$V/include/"*_wrappers "$D/lib/clang/$V/include/openmp_wrappers"
	cp -R "$RESDIR/lib/$TRIPLE" "$D/lib/clang/$V/lib/"
	for l in libunwind.a libc++.a libc++abi.a; do
		cp "$SYSROOT/usr/lib/$l" "$D/usr/lib/"
	done
	cp -R "$SYSROOT/usr/include/c++" "$D/usr/include/"
	cp -R "$SYSROOT/usr/include/linux" "$D/usr/include/"
	du -sh "$D"
}

[ $# -eq 0 ] && set -- all
for s in "$@"; do
	case $s in
	all) for t in fetch sysroot builtins runtimes hosttools cross dist; do stage_$t; done ;;
	fetch|sysroot|builtins|runtimes|hosttools|cross|dist) stage_$s ;;
	*) echo "unknown stage $s" >&2; exit 2 ;;
	esac
done
