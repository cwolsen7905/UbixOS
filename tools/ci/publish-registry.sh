#!/bin/sh
# publish-registry.sh — upload images/ubixos-* to the GitLab generic package registry:
#   $CI_API_V4_URL/projects/$CI_PROJECT_ID/packages/generic/ubixos-images/<version>/<file>
# Version is unique per commit (image-name.sh), so nothing is overwritten.
# Auth: CI_JOB_TOKEN.  Fails if a file already exists (HTTP 4xx) rather than replacing it.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$ROOT"
ver=$(tools/ci/image-name.sh)
base="$CI_API_V4_URL/projects/$CI_PROJECT_ID/packages/generic/ubixos-images/$ver"
n=0
for f in images/ubixos-* images/SHA256SUMS-*; do
	[ -f "$f" ] || continue
	echo "upload $f -> $base/$(basename "$f")"
	curl --fail --silent --show-error --header "JOB-TOKEN: $CI_JOB_TOKEN" \
		--upload-file "$f" "$base/$(basename "$f")"
	n=$((n + 1))
done
[ "$n" -gt 0 ] || { echo "publish-registry: no images in images/" >&2; exit 1; }
echo "published $n files as ubixos-images/$ver"
