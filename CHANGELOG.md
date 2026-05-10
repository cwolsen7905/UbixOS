# Changelog

All notable changes to UbixOS will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- `kprint_len(char *, size_t)` — kernel print function that writes up to a specified number of characters to the display.
- `ARCHITECTURE.md` — technical documentation covering kernel subsystems, memory layout, boot sequence, and design decisions.
- `BUILDING.md` — detailed build guide covering toolchain requirements, make targets, VirtualBox VM workflow, and troubleshooting.
- `CHANGELOG.md` — this file; project now tracks changes under Keep a Changelog format with semantic versioning.

### Changed
- `README.md` — rewritten with feature overview, quick-start build instructions, directory table, and documentation index.
- `doc/vmm.txt` — expanded with clearer memory layout diagram, complete function descriptions, and a page fault handling section.
- `doc/UbixOS_Build.txt` — condensed to a VM workflow summary; full build details moved to `BUILDING.md`.
- Cleaned up extraneous output from several kernel functions.

[Unreleased]: https://github.com/MrOlsen/UbixOS/compare/HEAD...HEAD
