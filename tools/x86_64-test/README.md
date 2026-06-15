# x86_64 execve test binary

A tiny freestanding static ELF64 used to verify the x86_64 execve path
(`x86_64_exec_demo("/hello")`).  Build + install onto the disk image:

```sh
x86_64-elf-gcc -nostdlib -static -Wl,-Ttext=0x40000000 -Wl,-e,_start \
    tools/x86_64-test/hello.S -o /tmp/hello
mcopy -o -i ubixos.img@@1M /tmp/hello ::/hello
```

It uses the FreeBSD amd64 `syscall` ABI (write=4, exit=1).  Superseded by the
real musl world once it is cross-built for x86_64.
