#!/bin/sh
# cc -> tcc (images without a native LLVM toolchain)
exec /usr/bin/tcc "$@"
