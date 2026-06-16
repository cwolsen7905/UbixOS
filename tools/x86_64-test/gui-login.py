#!/usr/bin/env python3
"""Headless x86_64 graphical-login smoke test.

Boots the x86_64 kernel under QEMU with a std-VGA framebuffer, drives the
graphical login (vlogin) via QMP send-key, screendumps the result, and tails the
serial log.  Used to verify the desktop boots, authenticates, and spawns a
session without freezing.

Usage: python3 tools/x86_64-test/gui-login.py [seconds-before-typing]
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
SERIAL = "/tmp/x86_64-gui-serial.log"
QMP = "/tmp/x86_64-gui-qmp.sock"
SHOT = "/tmp/x86_64-gui-shot.ppm"

BOOT_WAIT = float(sys.argv[1]) if len(sys.argv) > 1 else 18.0

# QMP key names for the login: "root" <tab> "user" <ret>
KEYS_USER = list("root")
KEYS_PASS = list("user")


def qmp(sock, cmd, **args):
    obj = {"execute": cmd}
    if args:
        obj["arguments"] = args
    sock.sendall((json.dumps(obj) + "\n").encode())
    time.sleep(0.05)
    return sock.recv(65536)


def send_key(sock, k):
    qmp(sock, "send-key", keys=[{"type": "qcode", "data": k}])
    time.sleep(0.08)


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
        "-qmp", "unix:%s,server,nowait" % QMP,
    ])
    try:
        # connect QMP
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
        sock.recv(65536)                      # greeting
        qmp(sock, "qmp_capabilities")

        print("booting %.0fs ..." % BOOT_WAIT)
        time.sleep(BOOT_WAIT)

        print("typing credentials")
        for k in KEYS_USER:
            send_key(sock, k)
        send_key(sock, "tab")
        for k in KEYS_PASS:
            send_key(sock, k)
        send_key(sock, "ret")

        time.sleep(6.0)                        # let the session spawn
        qmp(sock, "screendump", filename=SHOT)
        time.sleep(0.5)
        print("screendump -> %s" % SHOT)
    finally:
        qemu.terminate()
        try:
            qemu.wait(timeout=5)
        except subprocess.TimeoutExpired:
            qemu.kill()

    # summarize serial tail
    print("\n==== serial tail ====")
    try:
        with open(SERIAL) as f:
            lines = f.readlines()
        for ln in lines[-40:]:
            sys.stdout.write(ln)
    except OSError as e:
        print("no serial log:", e)
    return 0


if __name__ == "__main__":
    sys.exit(main())
