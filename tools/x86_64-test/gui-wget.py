#!/usr/bin/env python3
"""Headless x86_64 HTTP-fetch test (wget over the network stack).

Runs a tiny host HTTP server (127.0.0.1:8080) that logs requests, boots QEMU with
a user-net guestfwd mapping guest 10.0.2.100:8080 -> that server, logs in, opens
the Terminal, and runs `wget http://10.0.2.100:8080/`.  Deterministic (no external
internet): if the host server logs the GET and wget reports the body, the full
network-app path works on x86_64 -- socket connect/send/recv via lwIP + virtio-net.

Usage: python3 tools/x86_64-test/gui-wget.py [boot-seconds]
"""
import http.server
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
SERIAL = "/tmp/x86_64-wget-serial.log"
QMP = "/tmp/x86_64-wget-qmp.sock"
SHOT = "/tmp/x86_64-wget.ppm"
HTTP_PORT = 8080
BODY = b"<html><body><h1>uBixOS-x86_64 network OK</h1></body></html>\n"

BOOT_WAIT = float(sys.argv[1]) if len(sys.argv) > 1 else 20.0
_got = {"req": None}


class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        _got["req"] = self.path
        print("[host http] GET %s from guest" % self.path, flush=True)
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.send_header("Content-Length", str(len(BODY)))
        self.end_headers()
        self.wfile.write(BODY)

    def log_message(self, *a):
        pass


def serve():
    httpd = http.server.HTTPServer(("127.0.0.1", HTTP_PORT), H)
    httpd.timeout = 0.5
    while not _stop.is_set():
        httpd.handle_request()


_stop = threading.Event()


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
    qmp(sock, "send-key", keys=[{"type": "qcode", "data": k} for k in ks])
    time.sleep(0.08)


def typestr(sock, s):
    simple = {".": "dot", " ": "spc", "-": "minus", "/": "slash"}
    for ch in s:
        if ch == ":":
            chord(sock, "shift", "semicolon")
        elif ch in simple:
            key(sock, simple[ch])
        else:
            key(sock, ch)


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

    threading.Thread(target=serve, daemon=True).start()

    qemu = subprocess.Popen([
        "qemu-system-x86_64", "-m", "256", "-smp", "2",
        "-kernel", KERNEL, "-display", "none", "-vga", "std",
        "-serial", "file:" + SERIAL,
        "-drive", "file=%s,format=raw,if=none,id=hd0" % IMAGE,
        "-device", "virtio-blk-pci,drive=hd0,disable-modern=true",
        "-netdev", "user,id=n0,guestfwd=tcp:10.0.2.100:%d-tcp:127.0.0.1:%d" % (HTTP_PORT, HTTP_PORT),
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

        print("running: wget http://10.0.2.100:8080/")
        typestr(sock, "wget http://10.0.2.100:8080/")
        key(sock, "ret")
        time.sleep(8.0)
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

    print("\n==== RESULT ====")
    print("host server saw request:", _got["req"])
    return 0


if __name__ == "__main__":
    sys.exit(main())
