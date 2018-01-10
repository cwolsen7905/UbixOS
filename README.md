UbixOS
===============
Started back in 2002

Installation:

  make all install <- This will build the kernel and components and put them onto a floppy

  Do the following if and only if your boot disk is not bootable and you just need to do 
  it once

  To make a bootable floppy do the following:
    1) cd src/sys/boot/btx;make
    2) cd ../boot2;make
    3) cat boot1 boot2 > /dev/fd0
    4) gcc -o test test.c;./test /dev/fd0 1;rm ./test
Directory Structure:

  bin     - Applications for UbixOS
  include - Include files to build anything userland
  lib     - Userland libraries
  sys     - Kernel code
  tools   - Tool kit required to build and install UbixOS