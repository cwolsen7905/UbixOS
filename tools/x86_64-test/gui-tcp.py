#!/usr/bin/env python3
"""Headless x86_64 TCP-socket smoke test.

Proves TCP (SOCK_STREAM) works end to end, not just raw ICMP / UDP-DHCP.  Runs a
tiny echo server on the host (127.0.0.1:9999), boots QEMU with a user-net
guestfwd mapping guest 10.0.2.100:9999 -> that host server, logs in, opens the
Terminal, and runs `nc 10.0.2.100 9999` then types a line.  The echo coming back
on screen proves: socket(SOCK_STREAM) -> connect -> write -> lwIP TCP -> virtio
TX -> QEMU -> host echo -> back -> recv -> select, the whole TCP stack.

Usage: python3 tools/x86_64-test/gui-tcp.py [boot-seconds]
"""
import json
import os
import socket
import subprocess
import sys
import threading
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
KERNEL = os.path.join(ROOT, "build/x86_64/boot/kernel")
IMAGE = os.path.join(ROOT, "ubixos-x86_64.img")
SERIAL = "/tmp/x86_64-tcp-serial.log"
QMP = "/tmp/x86_64-tcp-qmp.sock"
SHOT = "/tmp/x86_64-tcp.ppm"
ECHO_PORT = 9999

BOOT_WAIT = float(sys.argv[1]) if len(sys.argv) > 1 else 20.0
_stop = threading.Event()


def echo_server():
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", ECHO_PORT))
    srv.listen(4)
    srv.settimeout(0.5)
    while not _stop.is_set():
        try:
            c, _ = srv.accept()
        except socket.timeout:
            continue
        except OSError:
            break
        threading.Thread(target=_echo_conn, args=(c,), daemon=True).start()
    srv.close()


def _echo_conn(c):
    print("[host echo] guest CONNECTED", flush=True)
    c.settimeout(4.0)
    try:
        c.sendall(b"GREETINGS-FROM-HOST\n")  # tests the guest's TCP recv path
        while not _stop.is_set():
            data = c.recv(512)
            if not data:
                break
            print("[host echo] recv %r -> echoing" % data, flush=True)
            c.sendall(b"echo: " + data)
    except OSError:
        pass
    finally:
        c.close()


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


def typestr(sock, s):
    m = {".": "dot", " ": "spc", "-": "minus"}
    for ch in s:
        key(sock, m.get(ch, ch))


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

    srv_thread = threading.Thread(target=echo_server, daemon=True)
    srv_thread.start()

    qemu = subprocess.Popen([
        "qemu-system-x86_64", "-m", "256", "-smp", "2",
        "-kernel", KERNEL, "-display", "none", "-vga", "std",
        "-serial", "file:" + SERIAL,
        "-drive", "file=%s,format=raw,if=none,id=hd0" % IMAGE,
        "-device", "virtio-blk-pci,drive=hd0,disable-modern=true",
        "-netdev", "user,id=n0,guestfwd=tcp:10.0.2.100:%d-tcp:127.0.0.1:%d" % (ECHO_PORT, ECHO_PORT),
        "-device", "virtio-net-pci,netdev=n0,disable-modern=true",
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

        print("running: nc 10.0.2.100 9999")
        typestr(sock, "nc 10.0.2.100 9999")
        key(sock, "ret")
        time.sleep(3.0)
        print("sending a line")
        typestr(sock, "hello-tcp")
        key(sock, "ret")
        time.sleep(4.0)
        qmp(sock, "screendump", filename=SHOT)
        time.sleep(0.5)
        print("screendump -> %s" % SHOT)
    finally:
        _stop.set()
        qemu.terminate()
        try:
            qemu.wait(timeout=5)
        except subprocess.TimeoutExpired:
            qemu.kill()

    print("\n==== serial tail ====")
    try:
        with open(SERIAL) as f:
            for ln in f.readlines()[-8:]:
                sys.stdout.write(ln)
    except OSError as e:
        print("no serial:", e)
    return 0


if __name__ == "__main__":
    sys.exit(main())
