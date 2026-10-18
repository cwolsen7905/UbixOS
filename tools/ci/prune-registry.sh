#!/bin/sh
# prune-registry.sh [KEEP] — delete all but the newest KEEP (default 10) ubixos-images
# package versions from the project's generic registry.  Scheduled-pipeline job.
# Auth: PRUNE_TOKEN (project access token, api scope); CI_JOB_TOKEN cannot delete packages.
set -eu
keep="${1:-10}"
api="$CI_API_V4_URL/projects/$CI_PROJECT_ID/packages"
ids=$(curl --fail --silent --show-error --header "PRIVATE-TOKEN: $PRUNE_TOKEN" \
	"$api?package_type=generic&package_name=ubixos-images&order_by=created_at&sort=desc&per_page=100" |
	python3 -I -c 'import json,sys; [print(p["id"], p["version"]) for p in json.load(sys.stdin)]')
# Tagged releases are published to the CDN; registry copies of release versions are kept.
echo "$ids" | awk 'NF && $2 ~ /-dev\./ {print $1}' | tail -n +"$((keep + 1))" | while read -r id; do
	echo "delete package $id"
	curl --fail --silent --show-error --request DELETE --header "PRIVATE-TOKEN: $PRUNE_TOKEN" "$api/$id"
done
