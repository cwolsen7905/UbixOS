#!/usr/bin/env python3
"""Headless x86_64 Start-menu app launcher (app-suite verification).

Boots, logs in, opens the Start menu, navigates to a menu item, launches it, and
screendumps.  Used to exercise each desktop app on x86_64 and catch arch-specific
breakage.

Start menu (top-left, opens upward at x=2): rows are 28px tall from the menu top.
The 5 top entries are Applications(0) Games(1) Utilities(2) Settings(3) About(4);
the first three open submenus to the right at x=192 anchored at the parent row.

Usage: python3 gui-app.py <name> <parent_row> [sub_row] [boot-seconds] [wait]
  e.g. gui-app.py vdoom 1 0      (Games -> vDoom)
       gui-app.py files 2 0      (Utilities -> Files)
       gui-app.py settings 3     (Settings, no submenu)
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

NAME = sys.argv[1] if len(sys.argv) > 1 else "app"
PARENT_ROW = int(sys.argv[2]) if len(sys.argv) > 2 else 0
SUB_ROW = int(sys.argv[3]) if len(sys.argv) > 3 else -1
BOOT_WAIT = float(sys.argv[4]) if len(sys.argv) > 4 else 20.0
APP_WAIT = float(sys.argv[5]) if len(sys.argv) > 5 else 8.0

SERIAL = "/tmp/x86_64-app-%s-serial.log" % NAME
QMP = "/tmp/x86_64-app-%s-qmp.sock" % NAME
SHOT = "/tmp/x86_64-app-%s.ppm" % NAME

MENU_TOP = 516  # 1280x720: taskbar at 688, start menu anchored above


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
    while dx or dy:
        sx = max(-100, min(100, dx))
        sy = max(-100, min(100, dy))
        qmp(sock, "input-send-event", events=[
            {"type": "rel", "data": {"axis": "x", "value": sx}},
            {"type": "rel", "data": {"axis": "y", "value": sy}}])
        dx -= sx
        dy -= sy
        time.sleep(0.03)


def moveto(sock, x, y):  # from the clamped top-left origin
    move(sock, -1400, -1400)
    time.sleep(0.2)
    move(sock, x, y)


def click(sock):
    qmp(sock, "input-send-event", events=[{"type": "btn", "data": {"button": "left", "down": True}}])
    time.sleep(0.12)
    qmp(sock, "input-send-event", events=[{"type": "btn", "data": {"button": "left", "down": False}}])
    time.sleep(0.5)


def main():
    for p in (SERIAL, QMP, SHOT):
        try:
            os.remove(p)
        except OSError:
            pass

    qemu = subprocess.Popen([
        "qemu-system-x86_64", "-m", "256", "-smp", "2", "-cpu", "qemu64,+x2apic",
        "-kernel", KERNEL, "-display", "none", "-vga", "std",
        "-serial", "file:" + SERIAL,
        "-drive", "file=%s,format=raw,if=none,id=hd0" % IMAGE,
        "-device", "virtio-blk-pci,drive=hd0,disable-modern=true",
        "-netdev", "user,id=n0", "-device", "virtio-net-pci,netdev=n0,disable-modern=true",
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

        print("launching %s (parent row %d, sub row %d)" % (NAME, PARENT_ROW, SUB_ROW))
        moveto(sock, 50, 752)          # start button
        click(sock)
        parent_y = MENU_TOP + 28 * PARENT_ROW + 14
        moveto(sock, 90, parent_y)     # parent menu item
        click(sock)
        if SUB_ROW >= 0:
            sub_y = MENU_TOP + 28 * PARENT_ROW + 28 * SUB_ROW + 14
            moveto(sock, 282, sub_y)   # submenu item (opens at x=192, ~item width 180)
            click(sock)
        time.sleep(APP_WAIT)
        qmp(sock, "screendump", filename=SHOT)
        time.sleep(0.5)
        print("screendump -> %s" % SHOT)
    finally:
        qemu.terminate()
        try:
            qemu.wait(timeout=5)
        except subprocess.TimeoutExpired:
            qemu.kill()

    print("\n==== serial tail ====")
    try:
        with open(SERIAL) as f:
            for ln in f.readlines()[-14:]:
                sys.stdout.write(ln)
    except OSError as e:
        print("no serial:", e)
    return 0


if __name__ == "__main__":
    sys.exit(main())
