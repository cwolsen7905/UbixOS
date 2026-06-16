#!/usr/bin/env python3
"""Headless x86_64 graphical-terminal smoke test.

Boots, logs in, then drives the PS/2 mouse to open the start menu and launch the
Terminal (/bin/term), which runs a shell on a kernel pty.  Screendumps along the
way and tails serial.  Verifies the pty/tty path (sys/posix/tty.c) works on
x86_64: a shell renders into a window.

The mouse is relative (PS/2), so we first slam the cursor hard into the top-left
corner (the compositor clamps it to 0,0) to get a known origin, then move by
absolute pixel deltas from there.

Usage: python3 tools/x86_64-test/gui-term.py [boot-seconds]
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
SERIAL = "/tmp/x86_64-term-serial.log"
QMP = "/tmp/x86_64-term-qmp.sock"
SHOT_MENU = "/tmp/x86_64-term-menu.ppm"
SHOT_TERM = "/tmp/x86_64-term-shell.ppm"
SHOT_TYPED = "/tmp/x86_64-term-typed.ppm"

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


def move(sock, dx, dy):
    """Move the relative mouse by (dx,dy) pixels, chunked to <=100 per event."""
    while dx or dy:
        sx = max(-100, min(100, dx))
        sy = max(-100, min(100, dy))
        qmp(sock, "input-send-event", events=[
            {"type": "rel", "data": {"axis": "x", "value": sx}},
            {"type": "rel", "data": {"axis": "y", "value": sy}},
        ])
        dx -= sx
        dy -= sy
        time.sleep(0.03)


def click(sock):
    qmp(sock, "input-send-event", events=[
        {"type": "btn", "data": {"button": "left", "down": True}}])
    time.sleep(0.12)
    qmp(sock, "input-send-event", events=[
        {"type": "btn", "data": {"button": "left", "down": False}}])
    time.sleep(0.4)


def main():
    for p in (SERIAL, QMP, SHOT_MENU, SHOT_TERM):
        try:
            os.remove(p)
        except OSError:
            pass

    qemu = subprocess.Popen([
        "qemu-system-x86_64", "-m", "256", "-smp", "2",
        "-cpu", "qemu64,+x2apic", "-kernel", KERNEL, "-display", "none", "-vga", "std",
        "-serial", "file:" + SERIAL,
        "-drive", "file=%s,format=raw,if=none,id=hd0" % IMAGE,
        "-device", "virtio-blk-pci,drive=hd0,disable-modern=true",
        "-qmp", "unix:%s,server,nowait" % QMP,
    ])
    try:
        sock = None
        for _ in range(50):
            try:
                sock = socket.socket(socket.AF_UNIX)
                sock.connect(QMP)
                break
            except OSError:
                time.sleep(0.1)
        if sock is None:
            print("could not connect QMP")
            return 1
        sock.recv(65536)
        qmp(sock, "qmp_capabilities")

        print("booting %.0fs ..." % BOOT_WAIT)
        time.sleep(BOOT_WAIT)

        # log in
        for k in "root":
            key(sock, k)
        key(sock, "tab")
        for k in "user":
            key(sock, k)
        key(sock, "ret")
        time.sleep(5.0)

        # Slam cursor to the top-left corner (clamped origin), then to the start
        # button at ~ (50, 752).
        print("opening start menu")
        move(sock, -1400, -1400)
        time.sleep(0.3)
        move(sock, 50, 752)
        click(sock)
        time.sleep(0.4)

        # The start menu (at +2+564) has 5 items @28px: Applications(0, y~578),
        # Games(1), Utilities(2), Settings(3), About(4).  Terminal lives under
        # Applications' submenu.  Click "Applications" to open its submenu.
        print("opening Applications submenu")
        move(sock, 40, -174)          # (50,752) -> (90,578) = Applications
        click(sock)
        qmp(sock, "screendump", filename=SHOT_MENU)
        time.sleep(0.4)

        # The submenu opens to the right at x=192, anchored at the parent's y.
        # "Terminal" is its first item (y~578).  Click it to launch /bin/term.
        print("launching Terminal")
        move(sock, 192, 0)            # (90,578) -> (282,578) = Terminal
        click(sock)
        time.sleep(5.0)
        qmp(sock, "screendump", filename=SHOT_TERM)
        time.sleep(0.5)
        print("screendumps: %s , %s" % (SHOT_MENU, SHOT_TERM))

        # Type a command into the terminal (it has focus) to exercise the
        # keyboard -> sys_ptyinject -> shell -> VT100 render path.
        print("typing 'ls' into the terminal")
        for k in ("l", "s", "ret"):
            key(sock, k)
        time.sleep(1.5)
        qmp(sock, "screendump", filename=SHOT_TYPED)
        time.sleep(0.5)
        print("screendump (typed) -> %s" % SHOT_TYPED)
    finally:
        qemu.terminate()
        try:
            qemu.wait(timeout=5)
        except subprocess.TimeoutExpired:
            qemu.kill()

    print("\n==== serial tail ====")
    try:
        with open(SERIAL) as f:
            lines = f.readlines()
        for ln in lines[-30:]:
            sys.stdout.write(ln)
    except OSError as e:
        print("no serial log:", e)
    return 0


if __name__ == "__main__":
    sys.exit(main())
