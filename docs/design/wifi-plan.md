# WiFi Support Plan — 802.11 Station + WPA

**Status:** drafted 2026-06-26. Design only. Target: **station (STA) mode** —
join an existing AP, get an IP via the existing lwIP DHCP, route traffic through
the existing socket stack. AP/mesh/monitor modes are out of scope.

> **Chip confirmed (2026-06-26, board marking + deep-research):** the Orange Pi
> Zero 2W's wireless is the **CDTech "20U5622" module = Unisoc UWE5622** silicon
> (WiFi 5 dual-band 802.11a/b/g/n/ac + BT 5.0). **WiFi over SDIO 3.0; BT over a
> separate HCI UART; no USB.** It is **FullMAC** (firmware-driven). **No mainline
> Linux driver** — only the vendor out-of-tree GPL BSP (`uwe5622` / `sprdwl_ng` /
> `unisoc_wifi`, from orangepi-build). **Firmware blobs ARE published** and
> redistributed (orangepi-xunlong/firmware, armbian/firmware). **No register or
> firmware-protocol docs** — the command/WSM protocol must be reverse-engineered
> from the GPL driver. Verdict: **months**; biggest blocker = no firmware-protocol
> docs + the missing 802.11/WPA stack. (Some batches reference an AICSemi
> AIC8800D80, also SDIO FullMAC — but this unit is confirmed UWE5622.)
> Consequence: P0's "firmware load + HIF" is a **sprdwl_ng reverse-engineering**
> task; the SDIO transport (§3) is the gating prerequisite.

## 1. The load-bearing decision: FullMAC, not SoftMAC

| | SoftMAC | **FullMAC (chosen)** |
|---|---|---|
| 802.11 MLME (scan/auth/assoc, retries, rate control) | **host** (a `mac80211`-class stack — ~tens of kLOC) | **chip firmware** |
| Host driver job | huge | **firmware load + a control/data conduit** |
| Feasible for a hobby OS? | no | yes |

uBixOS has no `cfg80211`/`mac80211` and won't grow one. We assume a **FullMAC**
chip: the firmware does the 802.11 state machine; the host sends high-level
commands ("scan", "connect to SSID with key", "disconnect") and pumps data
frames. Essentially every SDIO/USB embedded WiFi part (Broadcom, Realtek-SDIO,
AIC8800, etc.) is FullMAC. **If the researched chip is SoftMAC-only, WiFi is
impractical and we use USB-Ethernet instead** — that is the project's go/no-go.

## 2. Layered architecture (bottom → top)

```
  [ userland: `wifi scan/connect/status`, ubistry net settings ]      P5
  [ WPA key management (4-way handshake / firmware-offload)    ]      P3/P4
  [ control plane: scan / connect / disconnect / events       ]      P1
  [ lwIP netif (TX/RX 802.3 frames, link up on assoc)         ]      P2
  [ chip driver: firmware load + HIF command/event/data       ]      P0
  [ transport: SDIO  (or USB)                                 ]      dependency
```

## 3. Dependency: the transport must exist first

The chip hangs off **SDIO** (most likely) or **USB**. So WiFi is gated on the
board bring-up:
- **SDIO** → needs the **sunxi-mmc** driver (board plan M4) extended to **SDIO
  mode** (4-bit, the SDIO command set, the card-interrupt line). This is more
  than block-storage MMC.
- **USB** → needs **USB host** (board plan M6).

Nothing chip-side runs until one of these is up. (See
`orange-pi-zero2w-bringup.md`.)

## 4. Components & phases

### P0 — Transport + chip probe + firmware load
- Bring the chip out of reset; enumerate it on SDIO/USB.
- Upload the **firmware blob** (the chip runs nothing until firmware is loaded —
  this is the single hardest external dependency; see §6).
- Establish the **HIF** (host interface): the chip's command/event/data ring or
  mailbox protocol. Read back the chip's MAC address = "it's alive."
- **Done-when:** firmware loads, chip reports its MAC.

### P1 — Control plane (scan)
- Encode the firmware's command set: SCAN, and parse SCAN_RESULT events
  (SSID/BSSID/channel/RSSI/security).
- Surface as a kernel control path + a userland `wifi scan`.
- **Done-when:** `wifi scan` lists nearby APs.

### P2 — Open-network association + data path
- CONNECT (open AP) → ASSOC event → link up.
- Wire the chip's data path to a new **lwIP netif** (`wlan0`): RX frames →
  `netif->input`; TX → chip. Reuse the ethernet netif glue (frames are 802.3
  once the firmware strips 802.11).
- DHCP (existing lwIP) → IP.
- **Done-when:** associate to an **open** AP, get a DHCP lease, ping out.
  Proves the entire data path **without crypto**.

### P3 — WPA2-PSK (the real-world case)
Two sub-cases, decided by the chip (research §5):
- **(a) Firmware-offloaded:** pass the PSK in the CONNECT command; firmware runs
  the 4-way handshake. Host work ≈ none beyond passing the key. *Strongly
  preferred.*
- **(b) Host supplicant:** the firmware passes EAPOL frames up; the host runs the
  WPA2-PSK 4-way handshake itself:
  - PMK = `PBKDF2-HMAC-SHA1(psk, ssid, 4096, 32)` — we already have PBKDF2 in
    `libpw`; add the SHA1 variant.
  - PTK = `PRF-SHA1(PMK, "Pairwise key expansion", min/max(AA,SPA) ‖ min/max(ANonce,SNonce))`.
  - Verify the AP's EAPOL **MIC** (HMAC-SHA1), install PTK/GTK (the firmware
    holds the keys; host hands them down or the firmware installs).
  - All primitives exist in **BearSSL** (HMAC, SHA1, AES key-unwrap) — ~a few
    hundred lines of EAPOL state machine, testable standalone against the
    RFC/IEEE test vectors **before any hardware**.
- **Done-when:** join a WPA2-PSK network, DHCP, traffic.

### P4 — WPA3-SAE (deferred)
SAE "dragonfly" handshake (hash-to-curve + EC) — much heavier than P3. BearSSL
has the EC primitives, but defer until WPA2 works. Many networks still offer
WPA2; WPA3-only is the minority.

### P5 — Userland + settings
- `wifi scan | connect <ssid> [psk] | status | disconnect`.
- ubistry **network settings** integration (remember networks, auto-connect),
  reusing the existing `net_configure` path.

## 5. The WPA decision hinges on the chip
If the FullMAC firmware offloads the 4-way handshake (P3a), the host "supplicant"
is just *passing the PSK* — we may never need a full supplicant. If not (P3b), we
implement WPA2-PSK EAPOL on BearSSL. **We will not port wpa_supplicant** (it
assumes `nl80211`/`cfg80211`); we implement the minimal PSK handshake directly.
The research tells us which path.

## 6. Biggest blockers (ranked)
1. **SDIO transport** — extending sunxi-mmc to SDIO mode (card interrupt, 4-bit,
   the SDIO cmd set) is real work and is the hard gating prerequisite. The chip
   can't be touched until this exists.
2. **Porting labor, not unknown protocol** — the protocol is NOT a blocker: the
   GPL vendor driver (`sprdwl_ng`/`uwe5622`) *is* the spec (SDIO init, firmware
   load, WSM command/event structs, data framing). The work is **extracting the
   chip-specific ~30-40% and replacing the Linux glue** (mmc/sdio, cfg80211,
   netdev/skb, workqueues, fw-loader) with our SDIO host + lwIP + control plane.
   Laborious + needs on-HW debugging, but bounded — not a research gamble.
3. **Firmware blob** — redistributable, version-matched firmware. Published for
   the UWE5622 (orangepi-xunlong/firmware, armbian/firmware) → gate passable.
4. SoftMAC chip (see §1) — would be fatal, but the UWE5622 is **FullMAC**, so OK.

**Net:** the published GPL driver removes the worst risk (unknown protocol). What
remains is the SDIO host + a large but *mapped* porting effort + HW bring-up
debugging. Months, but tractable.

## 7. What can start NOW (hardware-independent)
- **P3b WPA2-PSK EAPOL state machine** on BearSSL — pure software, unit-testable
  against published test vectors with zero hardware. This is the riskiest
  *software* piece and the most reusable; building it early de-risks P3.
- The **control-plane API shape** (scan/connect/status DTOs) and the **`wlan0`
  lwIP netif** scaffolding (generic ethernet netif, link-state hooks).
- The `wifi` userland command skeleton.
Everything else waits on the transport + the chip's HIF (research-pending).

## 8b. Implementation discipline (GPL-clean)

The `sprdwl_ng`/`uwe5622` reference is **GPL**; uBixOS is permissive. **Do not
paraphrase/translate the driver** — rewriting it (rename vars, reorder) is still a
*derivative work* and stays GPL-encumbered; that does not make it relicensable.
Copyright covers expression (structure, sequence, control flow), not just literal
text. The clean path:
1. Read the GPL driver only to extract the **non-copyrightable facts** — register
   layout, SDIO command sequence, firmware-load steps, the WSM command/event
   struct formats, init order, magic values. These are hardware/firmware
   *interfaces*, not creative expression.
2. Write those facts down as our own **spec**.
3. Implement **original** code from the spec (our own structure + SDIO host + lwIP
   glue) — not a transcription of the driver's flow. Clean-room (one person specs,
   a different person implements) is the ideal.

Also: linking a GPL driver into the **monolithic** kernel would make the
distributed kernel binary a combined GPL work — a second reason to reimplement
clean rather than embed the GPL code. (Not legal advice; confirm before
distributing.)

## 8. Verdict
Feasible **only** as FullMAC-with-firmware. Realistic effort: **months**, and
**front-loaded on the transport (SDIO) + obtaining a usable firmware blob**, not
on the 802.11/WPA software (which, given FullMAC + optional firmware-offloaded
WPA, is modest). If the chip is SoftMAC or has no redistributable firmware, the
answer is **USB-Ethernet instead.**
