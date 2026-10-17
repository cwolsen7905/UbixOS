# uBixOS CI toolchain image.
#
# Built by the `toolchain` job in .gitlab-ci.yml and pushed to the project
# registry as $CI_REGISTRY_IMAGE/ci:<sha256 of this file>.  Every other job runs
# in it so nothing apt-gets per job.  Package set = the union of what the
# GitHub workflow installed for lint, build, analyze and boot-smoke.
#
# linux/amd64 only (the runners are amd64); aarch64 is cross-built with the
# distro gcc-aarch64-linux-gnu, as on GitHub.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        bmake make binutils gcc gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu \
        clang-18 lld-18 llvm-18 clang-format clang-tidy cppcheck shellcheck \
        libpng-dev pkg-config bison flex \
        mtools qemu-utils qemu-system-arm qemu-system-x86 \
        python3 git ca-certificates rsync file \
    && rm -rf /var/lib/apt/lists/*

# TOOLCHAIN defaults to clang: the makefiles call bare clang, ld.lld and
# llvm-{ar,nm,objcopy,ranlib}; Ubuntu only ships them versioned.
ENV PATH=/usr/lib/llvm-18/bin:${PATH}
