#!/usr/bin/env python3
"""Headless x86_64 audio test: boot, log in, launch vDoom (which plays sound via
libaudio -> legacy /dev/audio -> the AC'97 driver), and capture QEMU's AC97
output to a WAV file.  Verifies the /dev/audio DMA path end-to-end by checking the
captured WAV is non-silent.

Usage: python3 gui-audio.py [boot-seconds] [play-seconds]
"""
import json
import os
import socket
import struct
import subprocess
import sys
import time
import wave

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
KERNEL = os.path.join(ROOT, "build/x86_64/boot/kernel")
IMAGE = os.path.join(ROOT, "ubixos-x86_64.img")

BOOT_WAIT = float(sys.argv[1]) if len(sys.argv) > 1 else 20.0
PLAY_WAIT = float(sys.argv[2]) if len(sys.argv) > 2 else 10.0

SERIAL = "/tmp/x86_64-audio-serial.log"
QMP = "/tmp/x86_64-audio-qmp.sock"
WAV = "/tmp/x86_64-audio.wav"

MENU_TOP = 564


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


def moveto(sock, x, y):
    move(sock, -1400, -1400)
    time.sleep(0.2)
    move(sock, x, y)


def click(sock):
    qmp(sock, "input-send-event", events=[{"type": "btn", "data": {"button": "left", "down": True}}])
    time.sleep(0.12)
    qmp(sock, "input-send-event", events=[{"type": "btn", "data": {"button": "left", "down": False}}])
    time.sleep(0.5)


def analyze_wav(path):
    """Return (frames, peak_abs) — peak_abs > 0 means non-silent output."""
    with wave.open(path, "rb") as w:
        n = w.getnframes()
        ch = w.getnchannels()
        sw = w.getsampwidth()
        raw = w.readframes(n)
    if sw != 2 or n == 0:
        return (n, 0)
    count = len(raw) // 2
    peak = 0
    for s in struct.unpack("<%dh" % count, raw[:count * 2]):
        a = -s if s < 0 else s
        if a > peak:
            peak = a
    return (n, peak)


def main():
    for p in (SERIAL, QMP, WAV):
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
        "-audiodev", "wav,id=snd0,path=%s" % WAV, "-device", "AC97,audiodev=snd0",
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

        print("launching vDoom (Games -> vDoom) ...")
        moveto(sock, 50, 752)
        click(sock)
        moveto(sock, 90, MENU_TOP + 28 * 1 + 14)  # Games
        click(sock)
        moveto(sock, 282, MENU_TOP + 28 * 1 + 14)  # submenu row 0 (vDoom)
        click(sock)
        print("letting it play %.0fs (capturing AC97 -> %s) ..." % (PLAY_WAIT, WAV))
        time.sleep(PLAY_WAIT)
        # Clean quit so QEMU's wav backend finalizes the RIFF/data chunk sizes.
        try:
            qmp(sock, "quit")
        except OSError:
            pass
        time.sleep(1.0)
    finally:
        qemu.terminate()
        try:
            qemu.wait(timeout=5)
        except subprocess.TimeoutExpired:
            qemu.kill()

    print("\n==== ac97 serial lines ====")
    try:
        with open(SERIAL) as f:
            for ln in f:
                if any(t in ln.lower() for t in ("ac97", "audio", "codec", "vdoom", "doom")):
                    sys.stdout.write(ln)
    except OSError as e:
        print("no serial:", e)

    print("\n==== WAV analysis ====")
    if os.path.exists(WAV):
        frames, peak = analyze_wav(WAV)
        print("WAV %s: %d frames, peak amplitude %d" % (WAV, frames, peak))
        print("RESULT:", "NON-SILENT (audio works)" if peak > 0 else "SILENT (no output)")
    else:
        print("no WAV produced")
    return 0


if __name__ == "__main__":
    sys.exit(main())
