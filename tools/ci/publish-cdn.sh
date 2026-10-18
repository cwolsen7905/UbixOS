#!/bin/sh
# publish-cdn.sh — publish a tagged release's images to the CDN, never overwriting.
#
#   CDN_HOST   ssh host (kube-003)         CDN_USER   deploy user (can write only under CDN_ROOT)
#   CDN_ROOT   docroot releases dir        CDN_SSH_KEY  path to the private key (file-type CI variable)
#   CDN_URL    public base URL             (default https://cdn.ubixsys.com)
# Needs images/ from the tag's `image:keep` jobs (xz images + SHA256SUMS-*), run on a protected tag.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$ROOT"
: "${CI_COMMIT_TAG:?publish-cdn.sh runs only on a tag}" "${CDN_HOST:?}" "${CDN_USER:?}" "${CDN_ROOT:?}" "${CDN_SSH_KEY:?}"
CDN_URL="${CDN_URL:-https://cdn.ubixsys.com}"
ver=$(tools/ci/image-name.sh)
dest="$CDN_ROOT/$ver"
ssh_opts="-i $CDN_SSH_KEY -o StrictHostKeyChecking=yes -o BatchMode=yes"
chmod 600 "$CDN_SSH_KEY"

ls images/ubixos-*.img.xz >/dev/null 2>&1 || { echo "publish-cdn: no .img.xz images in images/" >&2; exit 1; }
cat images/SHA256SUMS-* >images/SHA256SUMS
# Versions are immutable: refuse if the release directory already exists.
# shellcheck disable=SC2086
ssh $ssh_opts "$CDN_USER@$CDN_HOST" "test ! -e '$dest' && mkdir -p '$dest'" ||
	{ echo "publish-cdn: $dest already exists (or is unreachable); refusing to overwrite" >&2; exit 1; }
# shellcheck disable=SC2086
rsync -a --ignore-existing -e "ssh $ssh_opts" images/ubixos-*.img.xz images/SHA256SUMS "$CDN_USER@$CDN_HOST:$dest/"
# Read-only from now on, so a mistaken re-run cannot change a published file.
# shellcheck disable=SC2086
ssh $ssh_opts "$CDN_USER@$CDN_HOST" "chmod -R a-w '$dest'"
echo "published $ver to $CDN_URL/releases/$ver/"

# GitLab release object whose asset links point at the CDN files.
links=""
for f in images/ubixos-*.img.xz images/SHA256SUMS; do
	b=$(basename "$f")
	links="$links{\"name\":\"$b\",\"url\":\"$CDN_URL/releases/$ver/$b\"},"
done
curl --fail --silent --show-error --request POST --header "JOB-TOKEN: $CI_JOB_TOKEN" \
	--header "Content-Type: application/json" \
	--data "{\"tag_name\":\"$CI_COMMIT_TAG\",\"name\":\"uBixOS $ver\",\"description\":\"UEFI disk images. Verify with SHA256SUMS. Boot in QEMU, UTM or VirtualBox (EFI enabled); Secure Boot is not supported.\",\"assets\":{\"links\":[${links%,}]}}" \
	"$CI_API_V4_URL/projects/$CI_PROJECT_ID/releases" >/dev/null
echo "release $CI_COMMIT_TAG created"
