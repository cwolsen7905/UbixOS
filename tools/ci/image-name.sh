#!/bin/sh
# image-name.sh [ARCH PROFILE] — the single source of the image naming scheme.
#   no args:        print the version  (tag: tag without a leading "v"; else
#                   <version.h>-dev.<shortsha>)
#   ARCH PROFILE:   print ubixos-<version>-<arch>-<profile>.img
# <version.h> is MAJOR.MINOR.PATCH[-TAG] from sys/include/ubixos/version.h.
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
h="$root/sys/include/ubixos/version.h"
v() { sed -n "s/^#define UBIXOS_VERSION_$1[[:space:]]*\"\{0,1\}\([^\"]*\)\"\{0,1\}.*/\1/p" "$h" | head -n1; }
if [ -n "${CI_COMMIT_TAG:-}" ]; then
	ver="${CI_COMMIT_TAG#v}"
else
	ver="$(v MAJOR).$(v MINOR).$(v PATCH)"
	tag="$(v TAG)"
	[ -n "$tag" ] && ver="$ver-$tag"
	sha="${CI_COMMIT_SHORT_SHA:-$(git -C "$root" rev-parse --short HEAD 2>/dev/null || echo local)}"
	ver="$ver-dev.$sha"
fi
if [ $# -eq 0 ]; then echo "$ver"; else echo "ubixos-$ver-$1-$2.img"; fi
