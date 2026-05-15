# UbixOS Networking Revival Plan

## Current State

UbixOS has a networking stack that was partially functional roughly a decade ago. The
infrastructure is still in-tree but bitrotted:

| Component | Location | Status |
|-----------|----------|--------|
| NIC driver | `sys/pci/lnc.c` | AMD Lance/PCnet — polling only, ISR stubbed, likely non-functional |
| lwIP bridge | `sys/net/netif/ethernetif.c` | Hardcoded MAC `08:00:27:73:C1:B6`, reads via shared `tmpBuf` global |
| lwIP | `contrib/lwip-2.0.3/` | BSD-3-Clause, version 2.0.3 (2017), current is 2.2.0 |
| Init | `sys/net/net/init.c` | Hard-coded static IP `10.50.0.7/255.255.0.0 gw 10.50.0.1` |
| Config | `sys/include/net/lwipopts.h` | DHCP=0, DNS=0, AUTOIP=0, MEM_SIZE=1600 (far too small) |
| QEMU | `Makefile` (run target) | `-device pcnet -net user` |
| NE2000 | `sys/isa/ne2k.c` | Disabled, uses deprecated `struct device` interface |

**Why lnc is not the right base to fix:**
- `lnc_INT()` interrupt handler has a dead infinite loop before any real code
- Actual RX/TX interrupt callbacks (`lnc_rxINT`, `lnc_txINT`) are commented out
- Driver relies on a polling thread with a shared `tmpBuf` global — not thread-safe
- PCnet hardware is fully emulated by QEMU (`-device pcnet`) so it could work, but
  the driver would need essentially a full rewrite to be reliable

---

## Network Stack Decision: Keep lwIP

Alternatives considered:

| Stack | License | Verdict |
|-------|---------|---------|
| lwIP 2.2.0 | BSD-3-Clause | **Keep and update** — integration already done, active project |
| picoTCP | BSD 2-Clause | Unmaintained since 2019, no advantage |
| uIP / Contiki | BSD | Minimal, old, no TCP windowing |
| CycloneTCP | GPLv2/commercial | GPLv2 not compatible with UbixOS license requirements |
| smoltcp | MIT | Rust only |

**Decision: keep lwIP, update `contrib/lwip-2.0.3/` → lwIP 2.2.0.**

The existing `sys_arch.c`, `ethernetif.c`, and `sys/net/net/` wiring is already done
and tested. A new stack means redoing all of that. The 2.0.3→2.2.0 delta is largely
bug fixes, IPv6 improvements, and minor API additions — not a breaking change for our
use pattern.

---

## NIC Driver Decision: Intel e1000 (82540EM)

**Why e1000:**
- QEMU's default NIC model — `qemu … -device e1000` requires no special QEMU build
- PCI vendor/device ID `8086:100E` (82540EM Gigabit) — trivial to detect in existing `pci.c`
- Interrupt-driven by design — no polling thread kludge
- Reads real MAC from `RAL`/`RAH` registers on device (no hardcoding)
- Intel 8254x Software Developer's Manual is public and comprehensive
- Well-documented on OSDev wiki with worked examples
- Used by SerenityOS, ToaruOS, and many other hobby OSes as a reference implementation

**Compared to RTL8139:** RTL8139 is simpler (fewer registers, no descriptor rings for RX),
but e1000 is QEMU's default and gives a more realistic programming model. The extra
complexity is worth it given the documentation quality.

---

## Phases

### Phase 0 — Baseline: verify lnc still transmits anything
**Goal:** Before writing new code, determine exactly what is broken.

- Add `kprintf` to `lnc_thread()` and `ethernetif_input()` to trace packet flow
- Use QEMU's `-object filter-dump,id=f1,netdev=net0,file=/tmp/ubix.pcap` to capture
  packets on the host; open in Wireshark
- Determine: does lnc ever emit an ARP request? Does it receive anything?
- This establishes a known-bad baseline and may reveal if the issue is just DHCP/static IP
- **Time estimate:** 1–2 sessions

**Unblocks:** Go/no-go on quick lnc fix vs. full e1000 implementation.

---

### Phase 1 — Intel e1000 PCI driver (`sys/pci/e1000.c`)

**1.1 PCI detection**

In `sys/pci/pci.c`, add detection for `8086:100E`:

```c
if (vendor == 0x8086 && device == 0x100E)
    initE1000(bus, slot, func);
```

**1.2 Register access**

The e1000 exposes a 128KB MMIO region at BAR0. Map it into kernel virtual address
space via `vmm_mapPhysicalPage()` (same pattern as the framebuffer):

```c
#define E1000_REG(reg)  (*((volatile uint32_t *)(e1000_mmio + (reg))))
```

Key registers: `CTRL (0x00)`, `STATUS (0x08)`, `EERD (0x14)`, `ICR (0xC0)`,
`IMS (0xD0)`, `RCTL (0x100)`, `TCTL (0x400)`, `RDBAL/RDLEN/RDH/RDT (0x2800+)`,
`TDBAL/TDLEN/TDH/TDT (0x3800+)`, `RAL/RAH (0x5400/0x5404)`.

**1.3 Initialization sequence**

```
1. Reset: CTRL |= CTRL_RST; delay; CTRL &= ~CTRL_RST
2. Read MAC from RAL0 (0x5400) and RAH0 (0x5404)
3. Enable link: CTRL |= (CTRL_SLU | CTRL_ASDE)
4. Init RX descriptor ring (16 descriptors × 2048-byte buffers)
   RDBAL = physical address of ring; RDLEN = 16 * 16 bytes
   RDH = 0; RDT = 15 (one behind head so driver owns all)
   RCTL = RCTL_EN | RCTL_SBP | RCTL_UPE | RCTL_MPE | RCTL_BAM | RCTL_BSIZE_2048 | RCTL_SECRC
5. Init TX descriptor ring (16 descriptors)
   TDBAL = physical; TDLEN = 16 * 16 bytes
   TDH = 0; TDT = 0
   TCTL = TCTL_EN | TCTL_PSP | (15 << TCTL_CT_SHIFT) | (63 << TCTL_COLD_SHIFT)
6. Enable interrupts: IMS = IMS_RXT0 | IMS_TXQE | IMS_LSC
7. Register IRQ (from PCI config space, offset 0x3C)
```

**1.4 Transmit path**

```c
void e1000_sendPacket(void *data, uint16_t len) {
    uint32_t tail = E1000_REG(TDT);
    tx_desc[tail].addr   = vmm_getPhysicalAddr(tx_buf[tail]);
    tx_desc[tail].length = len;
    tx_desc[tail].cmd    = CMD_EOP | CMD_IFCS | CMD_RS;
    tx_desc[tail].status = 0;
    memcpy(tx_buf[tail], data, len);
    E1000_REG(TDT) = (tail + 1) % 16;
    /* wait for DD bit — or defer to interrupt */
}
```

**1.5 Receive path (interrupt-driven)**

ISR (`e1000_ISR`):
```c
uint32_t icr = E1000_REG(ICR);  /* reading clears it */
if (icr & ICR_RXT0) {
    while (rx_desc[rx_tail].status & RXD_STAT_DD) {
        int len = rx_desc[rx_tail].length;
        /* copy rx_buf[rx_tail][0..len] into nicBuffer, hand to lwIP */
        ethernetif_input(&e1000_netif);
        rx_desc[rx_tail].status = 0;
        E1000_REG(RDT) = rx_tail;
        rx_tail = (rx_tail + 1) % 16;
    }
}
```

**1.6 New lwIP bridge (`sys/net/netif/e1000netif.c`)**

Replace `ethernetif.c`'s `low_level_init` / `low_level_output` / `low_level_input`
with e1000-specific versions:

- `low_level_init`: read MAC from `e1000_mac[6]` (filled during init), set `netif->hwaddr`
- `low_level_output`: call `e1000_sendPacket()`
- `low_level_input`: pull packet from the buffer the ISR deposited

**1.7 Files to add/change**

| File | Action |
|------|--------|
| `sys/pci/e1000.c` | New — driver |
| `sys/include/pci/e1000.h` | New — register map and struct |
| `sys/net/netif/e1000netif.c` | New — lwIP bridge (replaces ethernetif.c) |
| `sys/net/net/init.c` | Change — use `e1000_netif`, start DHCP |
| `sys/pci/Makefile` | Add `e1000.o` |
| `sys/net/netif/Makefile` | Add `e1000netif.o`, optionally remove `ethernetif.o` |
| `Makefile` (run targets) | Change `-device pcnet` → `-device e1000` |

**Unblocks:** Phase 2 (can now receive real packets).

---

### Phase 2 — lwIP 2.2.0 update and enable features

**2.1 Update lwIP**

```sh
# Remove old
rm -rf contrib/lwip-2.0.3

# Add new as git subtree
git subtree add \
  --prefix=contrib/lwip \
  https://git.savannah.gnu.org/git/lwip.git \
  STABLE-2_2_0_RELEASE --squash
```

Rename `contrib/lwip-2.0.3/` → `contrib/lwip/`. Update `sys/net/` include paths.
Expected breakage: minor API changes in `pbuf`, `sys_arch` thread/mbox types —
addressable in one session.

**2.2 Enable DHCP and grow memory**

In `sys/include/net/lwipopts.h`:

```c
/* Was 1600 — enough for 1-2 TCP segments, not enough for DHCP + ARP */
#define MEM_SIZE                    32768

/* Was 16 — increase for DHCP and concurrent connections */
#define PBUF_POOL_SIZE              64
#define MEMP_NUM_TCP_PCB            8
#define MEMP_NUM_UDP_PCB            8

/* Enable DHCP */
#define LWIP_DHCP                   1
#define LWIP_DHCP_AUTOIP_COOP       1   /* fall back to AutoIP if no server */
#define LWIP_AUTOIP                 1

/* Enable DNS */
#define LWIP_DNS                    1

/* Grow TCP buffers to something useful */
#define TCP_MSS                     1460
#define TCP_SND_BUF                 (8 * TCP_MSS)
#define TCP_WND                     (8 * TCP_MSS)
```

**2.3 Update init.c for DHCP**

```c
/* Remove hard-coded IP — let DHCP fill it in */
ip4_addr_t ip = {0}, mask = {0}, gw = {0};
netif_add(&e1000_netif, &ip, &mask, &gw, NULL, e1000netif_init, tcpip_input);
netif_set_default(&e1000_netif);
netif_set_up(&e1000_netif);
dhcp_start(&e1000_netif);
```

**2.4 QEMU user networking DNS**

QEMU's `-net user` gives the guest IP `10.0.2.15`, gateway `10.0.2.2`, DNS `10.0.2.3`.
DHCP in user-mode networking hands all of this automatically — no static config needed.

**Unblocks:** Phase 3 (reliable IP, DNS, working TCP).

---

### Phase 3 — Kernel socket syscall bridge

lwIP's socket layer (`LWIP_SOCKET=1`, `LWIP_COMPAT_SOCKETS=1`) provides a BSD-like
`lwip_socket()`, `lwip_connect()`, `lwip_send()` etc. These need to be bridged to the
UbixOS fd table so that userland `socket()` syscalls reach lwIP.

**3.1 New syscalls (POSIX table, `int $0x80`)**

| Syscall | FreeBSD # | Notes |
|---------|-----------|-------|
| `socket` | 97 | `lwip_socket(domain, type, proto)` |
| `bind` | 104 | `lwip_bind(fd, addr, len)` |
| `connect` | 98 | `lwip_connect(fd, addr, len)` |
| `listen` | 106 | `lwip_listen(fd, backlog)` |
| `accept` | 30 | `lwip_accept(fd, addr, len)` |
| `send` | 133 | `lwip_send(fd, buf, len, flags)` |
| `recv` | 135 | `lwip_recv(fd, buf, len, flags)` |
| `sendto` | 133 | `lwip_sendto(...)` |
| `recvfrom` | 125 | `lwip_recvfrom(...)` |

Sockets use a separate fd namespace in lwIP internally. The bridge layer maps
UbixOS kernel fds to lwIP socket fds and routes `sys_read`/`sys_write`/`sys_close`
to the appropriate lwIP calls.

**3.2 File**

`sys/kernel/socket_calls.c` — one function per syscall, registered in `syscalls_posix.c`

**Unblocks:** Phase 4 (userland can open TCP/UDP connections).

---

### Phase 4 — Userland network utilities

Simple tools to exercise the stack:

**4.1 `bin/ping/`** — ICMP echo
- Uses raw lwIP socket (`SOCK_RAW`, `IPPROTO_ICMP`)
- Sends ICMP Echo Request, waits for Echo Reply
- Tests Phase 1+2 end-to-end

**4.2 `bin/nc/`** — minimal netcat
- `nc <host> <port>` — TCP connect, read stdin, write to socket; read from socket, write stdout
- Tests Phase 3 socket syscalls

**4.3 `bin/wget/`** — HTTP GET
- `wget http://<host>/<path>` — TCP connect to port 80, send GET request, write body to stdout or file
- Useful for pulling files from host via QEMU's built-in `guestfwd` or a Python HTTP server

---

## What to Preserve vs. Remove

| Component | Action | Reason |
|-----------|--------|--------|
| `sys/pci/lnc.c` | Remove from build (keep source for history) | Replaced by e1000; polling model |
| `sys/net/netif/ethernetif.c` | Replace with `e1000netif.c` | Hardcoded MAC, shared-global design |
| `sys/net/netif/arp.c` | Re-enable in Makefile | Currently commented out; lwIP needs it |
| `sys/net/netif/loopif.c` | Re-enable | Loopback interface useful for testing |
| `sys/isa/ne2k.c` | Leave disabled | Unmaintained, uses `struct device` (old API) |
| `contrib/lwip-2.0.3/` | Replace with `contrib/lwip/` 2.2.0 | Bug fixes, better IPv6 |
| `sys/net/net/bot.c`, `shell.c`, `udpecho.c` | Remove or keep as test code | lwIP demo apps, only useful for debugging |
| `sys/net/net/init.c` | Update | DHCP, e1000 netif |
| `sys/include/net/lwipopts.h` | Update | Memory sizes, enable DHCP/DNS |

---

## Testing at Each Phase

| Phase | Test |
|-------|------|
| 0 | Wireshark pcap: does UbixOS send any packet at all? |
| 1 (e1000 TX) | Wireshark: ARP request from `08:00:27:73:C1:B6` or whatever MAC e1000 picks |
| 1 (e1000 RX) | Wireshark: ARP reply received, ping response from QEMU gateway `10.0.2.2` |
| 2 (DHCP) | `kprintf` in DHCP callback: print assigned IP |
| 2 (DNS) | `dns_gethostbyname("google.com")` from net test thread |
| 3 (sockets) | `bin/nc localhost 7` to QEMU's echo service (QEMU `-net user,hostfwd=tcp::7000-:7`) |
| 4 | `wget http://10.0.2.2:8000/hello.txt` while host runs `python3 -m http.server 8000` |

---

## QEMU Command After Phase 1+

```sh
qemu-system-i386 \
  -drive file=ubixos.img,format=raw \
  -m 256 \
  -device e1000,netdev=net0 \
  -netdev user,id=net0 \
  -object filter-dump,id=f1,netdev=net0,file=/tmp/ubix.pcap \
  -serial file:serial.log \
  -vga std
```

The `filter-dump` line writes a pcap file readable by Wireshark — essential for
debugging the driver before DHCP works.

---

## Status

Last updated: 2026-05-15

| Phase | Description | Status |
|-------|-------------|--------|
| Phase 0 | Baseline: trace lnc to see what's broken | ⬜ Not started |
| Phase 1 | Intel e1000 driver + lwIP bridge | ⬜ Not started |
| Phase 2 | lwIP 2.2.0 update + DHCP/DNS | ⬜ Not started |
| Phase 3 | Kernel socket syscall bridge | ⬜ Not started |
| Phase 4 | Userland network utilities (ping, nc, wget) | ⬜ Not started |
