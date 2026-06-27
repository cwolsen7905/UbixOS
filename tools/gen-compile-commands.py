#!/usr/bin/env python3
"""
Generate compile_commands.json for UbixOS.

Bear doesn't work reliably on macOS with SIP because DYLD_INSERT_LIBRARIES
is blocked for sub-processes.  This script walks the source tree and emits
one entry per .c/.cc/.S file using the known per-subsystem compile flags.

Usage:
    python3 tools/gen-compile-commands.py
    python3 tools/gen-compile-commands.py --world   # include userland too

Output: compile_commands.json in the repo root.
"""

import json
import os
import sys
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SYS  = ROOT / "sys"


KERNEL_CC   = "/opt/homebrew/bin/x86_64-elf-gcc"
KERNEL_CXX  = "/opt/homebrew/bin/x86_64-elf-g++"

KERNEL_CFLAGS = [
    "-m32",
    "-DDEBUG_SYSCTL",
    "-O",
    "-Wall",
    "-Wno-incompatible-pointer-types",
    "-nostdlib",
    "-nostdinc",
    "-fno-builtin",
    "-fno-exceptions",
    "-ffreestanding",
    "-fno-pie",
    "-fno-pic",
    "-fno-stack-protector",
    "-mno-sse",
    "-mno-sse2",
    "-mno-mmx",
    "-mno-3dnow",
    f"-I{SYS}/include",
]

KERNEL_CXXFLAGS = KERNEL_CFLAGS + ["-DNOBOOL", "-std=gnu++20"]

# Extra include paths for specific subsystems (keyed by directory name).
SUBSYSTEM_EXTRA = {
    "net":  [f"-I{ROOT}/contrib/lwip/src/include"],
    "core": [f"-I{ROOT}/contrib/lwip/src/include"],
    "api":  [f"-I{ROOT}/contrib/lwip/src/include"],
    "netif":[f"-I{ROOT}/contrib/lwip/src/include"],
}

# Userland common flags
MUSL_SRC    = ROOT / "contrib" / "musl"
LIBCXX_SRC  = ROOT / "contrib" / "libcxx"
LIBCXXABI   = ROOT / "contrib" / "libcxxabi"
BUILD_OBJ   = ROOT / "build" / "obj"

WORLD_CC  = "/opt/homebrew/bin/x86_64-elf-gcc"
WORLD_CXX = "/opt/homebrew/bin/x86_64-elf-g++"

WORLD_CFLAGS = [
    "-m32",
    "-nostdinc",
    "-nostdinc++",
    "-fno-builtin",
    "-ffreestanding",
    "-fno-pie",
    "-fno-pic",
    "-mno-sse",
    "-mno-sse2",
    "-mno-mmx",
    "-mno-3dnow",
    f"-I{ROOT}/include",
    f"-I{MUSL_SRC}/include",
    f"-I{BUILD_OBJ}/musl/obj/include",
    f"-I{MUSL_SRC}/arch/i386",
    f"-I{MUSL_SRC}/arch/generic",
]

WORLD_CXXFLAGS = WORLD_CFLAGS + [
    f"-I{LIBCXX_SRC}/include",
    f"-I{LIBCXXABI}/include",
    "-std=c++20",
    "-fno-rtti",
    "-fno-exceptions",
    "-D_LIBCPP_HAS_NO_EXCEPTIONS",
]


def kernel_entry(path: Path) -> dict:
    subsystem = path.parent.name
    suffix = path.suffix

    if suffix in (".cc", ".cpp", ".C"):
        compiler = KERNEL_CXX
        args = list(KERNEL_CXXFLAGS)
    else:
        compiler = KERNEL_CC
        args = list(KERNEL_CFLAGS)

    args += SUBSYSTEM_EXTRA.get(subsystem, [])
    args += ["-c", str(path)]

    return {
        "file":      str(path),
        "directory": str(path.parent),
        "arguments": [compiler] + args,
    }


def world_entry(path: Path) -> dict:
    suffix = path.suffix
    if suffix in (".cc", ".cpp", ".C"):
        compiler = WORLD_CXX
        args = list(WORLD_CXXFLAGS)
    else:
        compiler = WORLD_CC
        args = list(WORLD_CFLAGS)

    args += ["-c", str(path)]

    return {
        "file":      str(path),
        "directory": str(path.parent),
        "arguments": [compiler] + args,
    }


def collect_kernel() -> list:
    entries = []
    skip_dirs = {"compile"}   # null.c timestamp file — skip
    for src in sorted(SYS.rglob("*")):
        if not src.is_file():
            continue
        if src.suffix not in (".c", ".cc", ".cpp", ".C", ".S"):
            continue
        if src.parent.name in skip_dirs:
            continue
        # Skip contrib sources inside sys/ (lwip lives under sys/net/)
        entries.append(kernel_entry(src))
    return entries


def collect_world() -> list:
    entries = []
    world_roots = [ROOT / d for d in ("bin", "sbin", "usr.bin", "usr.sbin", "tests", "lib", "libexec")]
    skip_dirs = {"contrib"}
    for wr in world_roots:
        if not wr.exists():
            continue
        for src in sorted(wr.rglob("*")):
            if not src.is_file():
                continue
            if src.suffix not in (".c", ".cc", ".cpp", ".C"):
                continue
            # skip contrib sub-trees that have their own flags
            if any(p.name in skip_dirs for p in src.parents):
                continue
            entries.append(world_entry(src))
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--world", action="store_true",
                        help="also include userland (bin/ sbin/ usr.bin/ usr.sbin/ tests/ lib/ libexec/)")
    parser.add_argument("-o", "--output", default=str(ROOT / "compile_commands.json"),
                        help="output path (default: compile_commands.json)")
    args = parser.parse_args()

    entries = collect_kernel()
    if args.world:
        entries += collect_world()

    out = Path(args.output)
    out.write_text(json.dumps(entries, indent=2) + "\n")
    print(f"Wrote {len(entries)} entries to {out}")


if __name__ == "__main__":
    main()
