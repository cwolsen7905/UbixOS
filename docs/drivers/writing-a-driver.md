# Writing a UbixOS Device Driver

This guide covers the structure of a UbixOS ISA device driver and the
`deviceAdd()` registration API.

---

## Overview

Every driver registers itself with the kernel device layer by calling
`deviceAdd()` from its `sampleRegister()` (or equivalent init) function.
The kernel stores the function pointers in a `deviceNode` and dispatches
read/write/ioctl calls through them.

---

## Required Header

```c
#include <sys/device.h>
```

---

## Device Info Struct

Each driver defines a private struct to hold its hardware parameters:

```c
struct devInfo {
    int irq;
    int ioAddr;
};

static struct devInfo sampleInfo;
```

---

## Registration

```c
void sampleRegister(void)
{
    int sampleMajor = 0;   /* major device ID */
    int sampleMinor = 0;   /* minor device ID */

    deviceAdd(sampleMajor, sampleMinor, "c",
              sampleRead, sampleWrite, sampleReset,
              sampleInit, sampleIoctl, sampleStop,
              sampleStart, sampleStandby, &sampleInfo);
}
```

`"c"` denotes a character device. Block devices use `"b"`.

---

## Required Entry Points

All function pointers passed to `deviceAdd()` must have these signatures:

```c
int sampleInit(struct deviceNode *dev);
int sampleRead(struct deviceNode *dev, void *ptr,
               uInt32 offset, uInt32 length);
int sampleWrite(struct deviceNode *dev, void *ptr,
                uInt32 offset, uInt32 length);
int sampleReset(struct deviceNode *dev);
int sampleIoctl(struct deviceNode *dev);
int sampleStop(struct deviceNode *dev);
int sampleStart(struct deviceNode *dev);
int sampleStandby(struct deviceNode *dev);
```

### `sampleInit`

Called once during device enumeration. Set `dev->size` here for character
devices:

```c
int sampleInit(struct deviceNode *dev)
{
    dev->size = 1024;
    return 0;
}
```

### `sampleRead` / `sampleWrite`

Transfer `length` bytes starting at `offset` to/from `ptr`. Return 0 on
success, non-zero on error.

### `sampleStandby`

Put the device into low-power mode. Return 0 when complete.

---

## IRQ and I/O Base Address

Obtain `irq` and `ioAddr` from the PCI enumeration layer (for PCI devices)
or from a known fixed ISA address. Store them in your `devInfo` struct during
`sampleInit`.

For ISA devices the address is typically hard-coded:

```c
sampleInfo.ioAddr = 0x300;
sampleInfo.irq    = 5;
```

For PCI devices, read the BAR and interrupt line from the PCI config space via
the `sys/pci/` helpers.

---

## Example: Minimal ISA Driver Skeleton

```c
#include <sys/device.h>

struct devInfo {
    int irq;
    int ioAddr;
};

static struct devInfo myInfo;

static int myInit(struct deviceNode *dev)
{
    myInfo.ioAddr = 0x300;
    myInfo.irq    = 5;
    dev->size     = 512;
    return 0;
}

static int myRead(struct deviceNode *dev, void *ptr,
                  uInt32 offset, uInt32 length)
{
    return 0;
}

static int myWrite(struct deviceNode *dev, void *ptr,
                   uInt32 offset, uInt32 length)
{
    return 0;
}

static int myReset(struct deviceNode *dev)    { return 0; }
static int myIoctl(struct deviceNode *dev)    { return 0; }
static int myStop(struct deviceNode *dev)     { return 0; }
static int myStart(struct deviceNode *dev)    { return 0; }
static int myStandby(struct deviceNode *dev)  { return 0; }

void myRegister(void)
{
    deviceAdd(7, 0, "c",
              myRead, myWrite, myReset,
              myInit, myIoctl, myStop,
              myStart, myStandby, &myInfo);
}
```

---

## See Also

- `sys/include/sys/device.h` — `deviceNode` struct and `deviceAdd()` prototype
- `sys/isa/` — existing ISA driver implementations (PIC, PIT, AT keyboard, NE2000)
- `sys/pci/` — PCI enumeration helpers for PCI device drivers
