#!/bin/sh
# make-images.sh ARCH — cut the CI images for ARCH from an already built world+kernel+loader,
# boot-smoke them as shipped (UEFI, no -kernel), compress, and write SHA256SUMS to images/.
#
#   PROFILES        profiles to build        (default "desktop base")
#   SMOKE_PROFILES  profiles to boot-smoke   (default: all built)
#   COMPRESS        zstd | xz                (default xz on a tag, else zstd)
#
# Needs: build/ARCH/{boot/kernel,boot/BOOT*.EFI,lib/libc.so}  (bmake kernel world; bmake -C stand).
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$ROOT"
arch="${1:?usage: make-images.sh aarch64|x86_64}"
PROFILES="${PROFILES:-desktop base}"
SMOKE_PROFILES="${SMOKE_PROFILES:-$PROFILES}"
COMPRESS="${COMPRESS:-$([ -n "${CI_COMMIT_TAG:-}" ] && echo xz || echo zstd)}"
mkdir -p images
rc=0

for p in $PROFILES; do
	img="images/$(tools/ci/image-name.sh "$arch" "$p")"
	echo "== $arch/$p: $img"
	ARCH=$arch PROFILE=$p REQUIRE_LOADER=1 tools/ci/quiet.sh "image-$arch-$p.log" sh tools/mkimage.sh "$img" "build/$arch"
	du -h "$img" | sed 's/^/   allocated: /'
	case " $SMOKE_PROFILES " in
	*" $p "*)
		case "$arch" in aarch64) var=CHECK_IMAGE_AARCH64 ;; *) var=CHECK_IMAGE_X86_64 ;; esac
		# Several profiles share one boot-smoke run each; a failure is recorded, the rest still built.
		if env "$var=$img" CHECK_BOOT_MODE=uefi CHECK_BOOT_TIMEOUT="${CHECK_BOOT_TIMEOUT:-480}" \
			tools/check/run.sh --strict --arch "$arch" boot-smoke; then
			echo "   smoke: PASS"
		else
			echo "   smoke: FAIL"; rc=1
		fi ;;
	*) echo "   smoke: skipped (build only)" ;;
	esac
	case "$COMPRESS" in
	xz) xz -T0 -9 --force "$img" ;;
	zstd) zstd -T0 -19 --rm -f -q "$img" ;;
	*) echo "unknown COMPRESS=$COMPRESS" >&2; exit 2 ;;
	esac
	ls -l "$img".* | awk '{print "   compressed: " $5 " bytes " $9}'
done
(cd images && sha256sum ubixos-*."$( [ "$COMPRESS" = xz ] && echo xz || echo zst )" >"SHA256SUMS-$arch")
exit $rc
