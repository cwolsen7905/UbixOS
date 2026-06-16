#!/usr/bin/env python3
"""Headless x86_64 preemption proof.

Boots, logs in, opens the Terminal, and drives tcsh into a pure-CPU infinite loop
(`while ( 1 ) ... end` — no syscalls, never yields).  Then screendumps twice a few
seconds apart.  If the taskbar CLOCK advances between the two shots while tcsh is
spinning, the timer preempted the CPU-bound user task and scheduled the taskbar —
i.e. preemptive scheduling works.  Without preemption a non-yielding user loop
would starve every other task and the clock would freeze.

Usage: python3 tools/x86_64-test/gui-preempt.py [boot-seconds]
"""
import json
import os
import socket
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
KERNEL = os.path.join(ROOT, "build/x86_64/boot/kernel")
IMAGE = os.path.join(ROOT, "ubixos-x86_64.img")
SERIAL = "/tmp/x86_64-preempt-serial.log"
QMP = "/tmp/x86_64-preempt-qmp.sock"
SHOT_A = "/tmp/x86_64-preempt-A.ppm"
SHOT_B = "/tmp/x86_64-preempt-B.ppm"

BOOT_WAIT = float(sys.argv[1]) if len(sys.argv) > 1 else 20.0


def qmp(sock, cmd, **args):
    obj = {"execute": cmd}
    if args:
        obj["arguments"] = args
    sock.sendall((json.dumps(obj) + "\n").encode())
    time.sleep(0.05)
    return sock.recv(65536)


def key(sock, k):
    qmp(sock, "send-key", keys=[{"type": "qcode", "data": k}])
    time.sleep(0.08)


def chord(sock, *ks):
    """Press several keys together (e.g. shift+9 = '(')."""
    qmp(sock, "send-key", keys=[{"type": "qcode", "data": k} for k in ks])
    time.sleep(0.08)


def move(sock, dx, dy):
    while dx or dy:
        sx = max(-100, min(100, dx))
        sy = max(-100, min(100, dy))
        qmp(sock, "input-send-event", events=[
            {"type": "rel", "data": {"axis": "x", "value": sx}},
            {"type": "rel", "data": {"axis": "y", "value": sy}}])
        dx -= sx
        dy -= sy
        time.sleep(0.03)


def click(sock):
    qmp(sock, "input-send-event", events=[{"type": "btn", "data": {"button": "left", "down": True}}])
    time.sleep(0.12)
    qmp(sock, "input-send-event", events=[{"type": "btn", "data": {"button": "left", "down": False}}])
    time.sleep(0.4)


def main():
    for p in (SERIAL, QMP, SHOT_A, SHOT_B):
        try:
            os.remove(p)
        except OSError:
            pass

    qemu = subprocess.Popen([
        "qemu-system-x86_64", "-m", "256", "-smp", "2",
        "-kernel", KERNEL, "-display", "none", "-vga", "std",
        "-serial", "file:" + SERIAL,
        "-drive", "file=%s,format=raw,if=none,id=hd0" % IMAGE,
        "-device", "virtio-blk-pci,drive=hd0,disable-modern=true",
        "-qmp", "unix:%s,server,nowait" % QMP])
    try:
        sock = None
        for _ in range(50):
            try:
                sock = socket.socket(socket.AF_UNIX)
                sock.connect(QMP)
                break
            except OSError:
                time.sleep(0.1)
        sock.recv(65536)
        qmp(sock, "qmp_capabilities")

        print("booting %.0fs ..." % BOOT_WAIT)
        time.sleep(BOOT_WAIT)
        for k in "root":
            key(sock, k)
        key(sock, "tab")
        for k in "user":
            key(sock, k)
        key(sock, "ret")
        time.sleep(5.0)

        # start menu -> Applications -> Terminal
        print("launching Terminal")
        move(sock, -1400, -1400)
        time.sleep(0.3)
        move(sock, 50, 752)
        click(sock)
        move(sock, 40, -174)
        click(sock)
        move(sock, 192, 0)
        click(sock)
        time.sleep(5.0)

        # Drive tcsh into a pure-CPU infinite loop: `while ( 1 )` <ret> `end` <ret>.
        # No command body -> tcsh just re-evaluates the condition forever, never
        # issuing a syscall, never yielding.
        print("starting an infinite CPU loop in tcsh")
        for k in ("w", "h", "i", "l", "e", "spacebar"):
            key(sock, k)
        chord(sock, "shift", "9")     # (
        key(sock, "spacebar")
        key(sock, "1")
        key(sock, "spacebar")
        chord(sock, "shift", "0")     # )
        key(sock, "ret")
        for k in ("e", "n", "d", "ret"):
            key(sock, k)
        time.sleep(2.0)

        qmp(sock, "screendump", filename=SHOT_A)
        print("shot A taken (tcsh now spinning); waiting 4s ...")
        time.sleep(4.0)
        qmp(sock, "screendump", filename=SHOT_B)
        time.sleep(0.5)
        print("shots: %s , %s  — compare the taskbar clock" % (SHOT_A, SHOT_B))
    finally:
        qemu.terminate()
        try:
            qemu.wait(timeout=5)
        except subprocess.TimeoutExpired:
            qemu.kill()

    print("\n==== serial tail ====")
    try:
        with open(SERIAL) as f:
            for ln in f.readlines()[-12:]:
                sys.stdout.write(ln)
    except OSError as e:
        print("no serial:", e)
    return 0


if __name__ == "__main__":
    sys.exit(main())
