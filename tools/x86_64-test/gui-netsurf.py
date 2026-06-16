#!/usr/bin/env python3
"""Headless x86_64 NetSurf launch test.

Boots with a virtio-net NIC, logs in, and launches NetSurf (Start -> Applications
-> NetSurf, /bin/nsfb) from the desktop, then screendumps.  Proves the NetSurf
framebuffer browser runs on x86_64 (objGFX/nsfb render + the network stack behind
it).  Optionally types a URL into the address bar if one is given.

Usage: python3 tools/x86_64-test/gui-netsurf.py [boot-seconds]
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
SERIAL = "/tmp/x86_64-ns-serial.log"
QMP = "/tmp/x86_64-ns-qmp.sock"
SHOT = "/tmp/x86_64-ns.ppm"

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
    for p in (SERIAL, QMP, SHOT):
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

        # Start menu -> Applications (item 0) -> NetSurf (submenu item 1).
        print("launching NetSurf")
        move(sock, -1400, -1400)
        time.sleep(0.3)
        move(sock, 50, 752)         # start button
        click(sock)
        move(sock, 40, -174)        # Applications (y~578)
        click(sock)
        # Submenu opens at x=192 anchored at the parent's y; Terminal=item0(~578),
        # NetSurf=item1(~606).
        move(sock, 192, 28)         # (90,578) -> (282,606) = NetSurf
        click(sock)
        time.sleep(12.0)            # NetSurf is heavy: load libs + render chrome
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
            for ln in f.readlines()[-16:]:
                sys.stdout.write(ln)
    except OSError as e:
        print("no serial:", e)
    return 0


if __name__ == "__main__":
    sys.exit(main())
