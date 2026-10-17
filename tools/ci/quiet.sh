#!/bin/sh
# quiet.sh LOG CMD... — run CMD with all output going to LOG, and print only a
# summary.  The world build is far noisier than GitLab's 4 MiB job-log cap, which
# truncates the log and hides the actual failure.  On failure the error lines and
# the tail of LOG are shown; LOG itself is uploaded as a job artifact.
log="$1"; shift
if "$@" >"$log" 2>&1; then
	echo "ok: $* (log: $log, $(wc -l <"$log") lines)"
	exit 0
fi
rc=$?
echo "FAILED rc=$rc: $*"
echo "--- error lines ---"
grep -nE '(^|[^a-z-])(error|Error)[: ]|\*\*\*|Stop\.|undefined reference|No such file' "$log" | grep -v -- '-Werror' | head -60
echo "--- last 60 lines ---"
tail -60 "$log"
exit "$rc"
