#!/bin/sh
# lint-base.sh — print the ref the gate suite should diff against in CI.
#
# Merge request pipelines: the MR's merge-base (CI_MERGE_REQUEST_DIFF_BASE_SHA).
# Pushes: the previous tip (CI_COMMIT_BEFORE_SHA), or origin/master for a new
# branch (all-zero before sha).  Mirrors the GitHub job's
# `pull_request.base.sha || event.before` logic.
base="${CI_MERGE_REQUEST_DIFF_BASE_SHA:-${CI_COMMIT_BEFORE_SHA:-}}"
case "$base" in
"" | 0000000000000000000000000000000000000000) base=origin/master ;;
esac
git rev-parse --verify --quiet "$base^{commit}" >/dev/null || base=origin/master
echo "$base"
