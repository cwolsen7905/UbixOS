# Views Protocol Reference

The wire protocol between a views client and the compositor process
(`bin/views/views`).  All transport is MPI; the authoritative header is
[`include/views/display_proto.h`](../../include/views/display_proto.h)
and this document mirrors it for reference.

If you're writing an app in C++ with objGFX, you almost certainly want
the tutorial — [`writing-a-views-app.md`](writing-a-views-app.md) — not
this document.  This is the layer below: useful if you're porting a
non-C++ toolkit, writing language bindings, or debugging the compositor.

The architecture overview is [`docs/architecture/display.md`](../architecture/display.md).

---

## Transport

- MPI mailboxes (`<sys/mpi.h>`).  The compositor listens on the mailbox
  named `"views"`.  Each client creates its own mailbox of an arbitrary
  name and tells the compositor what to call it in the claim request.
- Every message is an `mpi_message_t`: a 248-byte `data[]` payload + a
  `uint32_t header` (the message type) + a `next` link (unused on the
  wire).  Payload structs are cast directly from `data[]`.
- All payload structs must fit within `MESSAGE_LENGTH` (248) bytes — the
  current largest, `display_claim_req`, is 192 bytes.
- **No drawing commands cross MPI.**  Pixels move only through the
  shared-memory buffer the compositor hands back in `DISPLAY_ACK` /
  `DISPLAY_WINRESIZE`.

---

## Message catalogue

Direction is from the client's perspective: `→` = client to compositor,
`←` = compositor to client.

| #  | Name | Dir | Payload | Purpose |
|----|------|-----|---------|---------|
| 1  | `DISPLAY_CLAIM`           | → | `display_claim_req`  | Request a window. |
| 2  | `DISPLAY_FLIP`            | → | `display_flip`       | "I drew; please composite." |
| 3  | `DISPLAY_RELEASE`         | → | `display_release`    | Close my window. |
| 4  | `DISPLAY_ACK`             | ← | `display_ack`        | Claim granted; here's your buffer. |
| 5  | `DISPLAY_DENIED`          | ← | *(none)*             | Claim refused. |
| 6  | `DISPLAY_KEY`             | ← | `display_key`        | Keyboard event, focused window. |
| 7  | `DISPLAY_MOUSE`           | ← | `display_mouse_ev`   | Mouse event, focused window. |
| 8  | `DISPLAY_QUERY`           | → | `display_query`      | What's the screen geometry? |
| 9  | `DISPLAY_INFO`            | ← | `display_info`       | Response to QUERY. |
| 10 | `DISPLAY_CLOSE`           | ← | `display_close`      | User clicked the X — please exit. |
| 11 | `DISPLAY_RAISE`           | → | `display_raise`      | Raise & focus this window. |
| 12 | `DISPLAY_NOTIFY`          | ← | `display_notify`     | (To taskbar) window added/removed. |
| 13 | `DISPLAY_SETTITLE`        | → | `display_settitle`   | Change my title bar text. |
| 14 | `DISPLAY_REFRESH_DESKTOP` | → | *(none)*             | Re-read desktop settings, repaint. |
| 15 | `DISPLAY_SET_USER`        | → | `display_set_user`   | (vlogin) switch session user. |
| 16 | `DISPLAY_THEME`           | → | *(none)*             | (To taskbar) re-read theme. |
| 17 | `DISPLAY_SETMODE`         | → | `display_setmode`    | Switch VBE mode (live). |
| 18 | `DISPLAY_RESIZE`          | ← | `display_resize`     | Screen geometry changed — re-claim. |
| 19 | `DISPLAY_WINRESIZE`       | ← | `display_winresize`  | This window resized — re-attach. |

---

## Lifecycle

### Open

```
client                                  compositor
  | mpi_createMbox("myapp")                |
  |                                        |
  | DISPLAY_CLAIM (req)            ─────►  |
  |                                        |  (allocate window, share buffer)
  | DISPLAY_ACK (window_id, shm)  ◄─────   |
  |                                        |
  | ogAttach(shm), draw, FLIP ─────────►   |
  |                                        |  (composite to screen)
```

The `reply` field in `display_claim_req` names the mailbox where the
ACK lands.  This is almost always the client's own inbox.

### Resize (live, user-driven)

```
client                                  compositor
  |                                        |  (user drags the resize grip)
  | DISPLAY_WINRESIZE (new shm,w,h) ◄────  |
  | ogAttach(new shm); redraw; FLIP ─────► |
  |                                        |  (composite at new size)
```

The old `shm_base` is invalid the moment `WINRESIZE` lands.  Drawing
into it after that point is undefined.

### Close

Two paths.  The user clicks the X:

```
client                                  compositor
  | DISPLAY_CLOSE                  ◄─────  |  (window already removed)
  | DISPLAY_RELEASE                ─────►  |  (idempotent; safe to send)
  | mpi_destroyMbox(...); exit             |
```

Or the app exits voluntarily:

```
client                                  compositor
  | DISPLAY_RELEASE                ─────►  |
  | mpi_destroyMbox(...); exit             |
```

Sending `DISPLAY_RELEASE` after `DISPLAY_CLOSE` is harmless — the
compositor ignores it.

---

## Payload reference

The structs are the source of truth in `display_proto.h`; here they're
annotated with what each field actually means in practice.

### `display_claim_req` (DISPLAY_CLAIM)

```c
struct display_claim_req {
    int32_t x, y, w, h;        /* requested position+size; (x,y) ignored if fullscreen */
    int32_t sender_pid;        /* getpid() — REQUIRED for vmm_share_region */
    char    title[64];         /* shown in the server-drawn title bar */
    char    reply[64];         /* your mailbox name for the ACK reply */
    uint8_t no_decor;          /* 1 = no title bar (panels, flyouts) */
    int32_t min_w, min_h,
            max_w, max_h;      /* resize bounds — all zero = fixed at w×h */
};
```

`sender_pid` is mandatory because the compositor uses `vmm_share_region`
to map the pixel buffer into your address space; the kernel needs to
know which process to map into.

Resize policy:
- All four bounds zero → fixed, no resize grip.
- `min_w == max_w` → fixed width, variable height (macOS Mail-style).
- `min_w < max_w` and `min_h < max_h` → freely resizable in both axes.

### `display_ack` (DISPLAY_ACK)

```c
struct display_ack {
    uint32_t window_id;        /* opaque handle for FLIP/RELEASE/RAISE/SETTITLE */
    void    *shm_base;         /* pixel buffer in YOUR address space, 32 bpp */
    uint16_t pitch;            /* bytes per scanline (usually w*4) */
    int32_t  x, y, w, h;       /* ACTUAL region granted — may not match request */
};
```

Always use `ack->w` / `ack->h`, not the values you asked for — the
compositor can clamp to screen bounds.

### `display_flip` (DISPLAY_FLIP)

```c
struct display_flip {
    uint32_t window_id;
    int32_t  dirty_x, dirty_y, dirty_w, dirty_h;   /* 0,0,w,h = whole window */
};
```

A tight dirty rect saves the compositor work but is purely an
optimisation hint — there's no correctness issue with over-dirtying.

### `display_winresize` (DISPLAY_WINRESIZE)

```c
struct display_winresize {
    uint32_t window_id;
    void    *shm_base;          /* NEW buffer; old one is invalid */
    uint16_t pitch;
    int32_t  w, h;              /* new content size */
};
```

Most-commonly-mishandled message.  Apps that paint a stale `shm_base`
after this lands corrupt the next window's buffer in the slab.

### `display_key` (DISPLAY_KEY)

```c
struct display_key {
    uint32_t window_id;
    uint32_t keycode;
    uint8_t  pressed;           /* 1 = down, 0 = up */
};
```

Keycodes are raw scancodes from the kernel keyboard driver.

### `display_mouse_ev` (DISPLAY_MOUSE)

```c
struct display_mouse_ev {
    uint32_t window_id;
    int16_t  x, y;              /* relative to window origin (0,0 = top-left) */
    int16_t  dx, dy;            /* delta since last event */
    uint8_t  buttons;           /* bit 0 = left, bit 1 = right */
};
```

Mouse coordinates are in window-content space, so y=0 is **below** the
server-drawn title bar (or at the very top for `no_decor` windows).

### Other payloads

- `display_query` / `display_info` — `QUERY` sends the reply mailbox
  name; `INFO` returns `screen_w`, `screen_h`, `bpp`.
- `display_release` / `display_close` — just a `window_id`.
- `display_raise` — just a `window_id`.
- `display_settitle` — `window_id` + new `title[64]`.
- `display_set_user` — `user[64]`; empty string resets to system default.
- `display_setmode` — `mode` (VBE mode number).
- `display_resize` — `screen_w`, `screen_h` (sent after a live mode
  change; the taskbar uses this to re-claim full-width).
- `display_notify` — `window_id`, `added` (1/0), `title[64]`.  Sent only
  to the taskbar mailbox.

---

## Constants

| Symbol | Value | Meaning |
|--------|-------|---------|
| `MESSAGE_LENGTH` | 248 | Max payload bytes (in `<sys/mpi.h>`). |
| `DECOR_H`        | 18  | Height of the server-drawn title bar. `no_decor` windows have 0. |

---

## Quirks to know

- **MPI is unreliable in the "your reply got lost" sense only if you
  destroy your mailbox.**  If your inbox exists, messages queue
  indefinitely.  Polling with `mpi_fetchMessage` + `sched_yield` is the
  standard idiom; there's no `select`/`poll` over MPI today.
- **`DISPLAY_DENIED` carries no payload.**  Just check `reply.header`.
- **The `reply` mailbox in `display_claim_req` does not have to be your
  own mailbox** — it just has to exist by the time the compositor posts
  to it.  Apps with multiple windows sometimes use one inbox per window
  so events route directly to the right handler.
- **Window IDs are not PIDs.**  Don't try to derive one from the other.
  The compositor's table is opaque; treat the ID as a cookie.
- **The title is captured by value in `claim_req`.**  Change it later
  with `DISPLAY_SETTITLE`, not by mutating your local buffer.
