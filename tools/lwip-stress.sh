#!/bin/sh
# lwip-stress.sh — lwIP reliability repro harness (lwip-audit Phase 0).
#
# Drives a uBixOS box under network load and samples /proc/lwip so the "ping
# drops / connection hangs / works on retry" instability becomes a measurable
# signal instead of a feeling.  It correlates three things over time:
#   1. tcpip_mbox_fetches  — the tcpip_thread liveness tick.  If it STALLS while
#      traffic is arriving, the net thread is being starved (the cooperative-
#      scheduler hypothesis).
#   2. link.drop / *.memerr / pool err — silent drops + pool exhaustion.
#   3. ICMP echo loss + ssh connect failures — the user-visible symptom.
#
# Run it from a host that can reach the box (NOT the box itself).  It needs
# `expect` (password ssh) and, for the ping load, `ping` (+ sudo for -f).
#
#   tools/lwip-stress.sh <host> [seconds] [password]
#   tools/lwip-stress.sh 10.50.7.100 60
#
# Output: one line every ~2 s with the mbox-fetch delta and any pool at/over
# capacity or with errors, plus a PING/CONNECT loss summary at the end.  A
# "STALL" flag means the liveness tick did not advance between samples — the
# smoking gun for tcpip_thread starvation.

set -u

HOST="${1:-}"
SECS="${2:-60}"
PASS="${3:-user}"
USER_NAME="${SSH_USER:-root}"

if [ -z "$HOST" ]; then
	echo "usage: $0 <host> [seconds] [password]" >&2
	exit 2
fi
command -v expect >/dev/null 2>&1 || { echo "need 'expect' in PATH" >&2; exit 2; }

TMP="$(mktemp -d "${TMPDIR:-/tmp}/lwipstress.XXXXXX")"
trap 'kill $PING_PID $CONN_PID 2>/dev/null; rm -rf "$TMP"' EXIT INT TERM

# --- background load: ICMP echo flood (best-effort; needs sudo for -f) -------
PING_LOG="$TMP/ping.log"
if command -v ping >/dev/null 2>&1; then
	( sudo ping -f -w "$SECS" "$HOST" >"$PING_LOG" 2>&1 \
	  || ping -c 1000 -i 0.05 "$HOST" >"$PING_LOG" 2>&1 ) &
	PING_PID=$!
else
	PING_PID=
	echo "(no ping; skipping ICMP load)"
fi

# --- background load: ssh connect/disconnect storm --------------------------
CONN_LOG="$TMP/conn.log"
(
	end=$(( $(date +%s) + SECS ))
	ok=0; fail=0
	while [ "$(date +%s)" -lt "$end" ]; do
		if expect -c "
			set timeout 8
			log_user 0
			spawn ssh -p 22 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
			    -o PreferredAuthentications=password -o PubkeyAuthentication=no \
			    -o ConnectTimeout=6 ${USER_NAME}@${HOST}
			expect {
			    -re {[Pp]assword:} { send \"${PASS}\r\"; exp_continue }
			    -re {[#%\$] \$}    { send \"exit\r\"; exp_continue }
			    eof  { exit 0 }
			    timeout { exit 1 }
			}" >/dev/null 2>&1; then ok=$((ok+1)); else fail=$((fail+1)); fi
	done
	echo "connect ok=$ok fail=$fail" >"$CONN_LOG"
) &
CONN_PID=$!

# --- foreground: one interactive session, sample /proc/lwip on a cadence ----
echo "sampling /proc/lwip on ${HOST} for ${SECS}s under load (ping + connect storm)..."
echo "time  mbox(delta)  notes"

expect -c "
set timeout 12
log_user 0
set fp [open \"$TMP/samples.log\" w]
spawn ssh -p 22 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
    -o PreferredAuthentications=password -o PubkeyAuthentication=no \
    -o ConnectTimeout=10 ${USER_NAME}@${HOST}
expect { -re {[Pp]assword:} { send \"${PASS}\r\" } timeout { exit 1 } }
expect -re {[#%\$] \$}
set deadline [expr {[clock seconds] + ${SECS}}]
while {[clock seconds] < \$deadline} {
    send \"cat /proc/lwip\r\"
    expect -re {[#%\$] \$}
    puts \$fp \"=== [clock seconds] ===\"
    puts \$fp \$expect_out(buffer)
    flush \$fp
    sleep 2
}
send \"exit\r\"
close \$fp
expect eof
" >/dev/null 2>&1

# --- correlate: print mbox-fetch deltas + flag stalls / pool trouble --------
awk '
/^=== / { t=$2; next }
/^tcpip_mbox_fetches:/ {
	cur=$2
	if (have) {
		d=cur-prev
		flag=(d==0)?"  <<< STALL (tcpip_thread not advancing)":""
		printf "%-6s %-12s%s\n", (t-t0), d, flag
	} else { t0=t }
	prev=cur; have=1; next
}
/err=[1-9]/        { print "        pool/proto ERR: " $0 }
/used=([0-9]+) max=\1 total=\1/ { print "        pool FULL: " $0 }
' "$TMP/samples.log" 2>/dev/null

echo "---"
wait $CONN_PID 2>/dev/null
[ -f "$CONN_LOG" ] && cat "$CONN_LOG"
if [ -f "$PING_LOG" ]; then
	grep -iE "packet loss|transmitted" "$PING_LOG" | tail -1
fi
echo "raw samples: $TMP/samples.log  (kept until next reboot if you copy it out)"
trap 'kill $PING_PID $CONN_PID 2>/dev/null' EXIT INT TERM   # keep $TMP for inspection
