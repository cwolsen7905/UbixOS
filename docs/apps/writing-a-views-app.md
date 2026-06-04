# Writing a Views App

This is the tutorial: a step-by-step walk through building a graphical
application for UbixOS's `views` compositor.  The companion references are
[`objgfx-reference.md`](objgfx-reference.md) (drawing API) and
[`views-protocol.md`](views-protocol.md) (wire protocol).  The architecture
overview lives in [`docs/architecture/display.md`](../architecture/display.md).

The reference implementation is [`bin/hello/`](../../bin/hello/main.cc) —
~200 lines, intentionally minimal.  Read this doc with that file open.

---

## Mental model

A views app is a normal userland process that:

1. **Claims** a window by sending an MPI message to the `"views"` mailbox.
2. Receives a **shared-memory pixel buffer** back from the compositor.
3. **Draws** into that buffer using `objGFX` (or any code that can write
   `0x00RRGGBB` pixels).
4. **Flips** — tells the compositor it's done so the new contents get
   composited to the screen.
5. **Handles events** (key, mouse, resize, close) delivered as MPI messages
   to its own mailbox.

MPI carries signals only — never pixel data.  All drawing happens in
shared memory.  This is the same split macOS uses between
WindowServer/Core Graphics.

The compositor (`bin/views/views`) is the only process that touches the
framebuffer (`sys_mapfb()`).  Apps never do.

---

## Project layout

```
bin/hello/
├── Makefile
└── main.cc
```

That's it.  No headers of your own, no extra source files for the
walkthrough.

### Makefile

```make
include ../../Makefile.incl
include ../Makefile.incl

BINARY = hello
OBJS   = main.o
SRCS   = main.cc

.include "${UBIX_MK}/ubix.musl.cxx.ubix.prog.mk"
```

The ruleset `ubix.musl.cxx.ubix.prog.mk` is the modern C++ recipe — it
links against musl (`libc.so`), `libobjgfx.so`, libcxx/libcxxabi, and
`libgcc`.  Everything except your `main.o` is a shared library, so the
final binary is tiny (the hello example is ~13 KB).

Add `hello` to the `SUBDIRS` line in `bin/Makefile` so `bmake world`
picks it up.

---

## The seven things every views app does

The hello app is structured around seven concerns, each just a handful of
lines.  In order:

### 1. Create your inbox

```c++
mpi_createMbox(g_mbox);   // g_mbox = "hello"
```

You **must** create the mailbox before sending `DISPLAY_CLAIM` — the
compositor will post the `DISPLAY_ACK` reply to it.  Forgetting this
deadlocks the app forever (the reply has nowhere to land).

Mailbox names are global, so pick something unique to your app.

### 2. Claim a window

Fill in a `struct display_claim_req` and post it to the `"views"`
mailbox with header `DISPLAY_CLAIM`:

```c++
struct display_claim_req *req = (struct display_claim_req *)msg.data;
req->x = 0;
req->y = 0;
req->w = WIN_W;
req->h = WIN_H;
req->sender_pid = getpid();   // required — compositor uses it for shm sharing
strncpy(req->title, "Hello, views", sizeof(req->title) - 1);
strncpy(req->reply, "hello", sizeof(req->reply) - 1);  // your mailbox
req->min_w = WIN_W; req->min_h = WIN_H;
req->max_w = 1280;  req->max_h = 720;   // resizable; omit (= 0) for fixed
```

Then `mpi_postMessage("views", DISPLAY_CLAIM, &msg)` and block on
`mpi_fetchMessage` until the reply arrives.  Loop with `sched_yield()` —
MPI is non-blocking by default.

### 3. Wait for the ACK

```c++
struct display_ack *ack = (struct display_ack *)reply.data;
win_id  = ack->window_id;     // opaque handle for FLIP/RELEASE/etc.
win_w   = ack->w;             // ACTUAL size — may differ from requested
win_h   = ack->h;
shm_base = ack->shm_base;     // your pixel buffer
```

If `reply.header == DISPLAY_DENIED` the compositor refused (out of slots,
bad geometry, etc.) — bail.

### 4. Attach an `ogSurface`

```c++
surf.ogAttach(ack->shm_base, (uint32_t)win_w, (uint32_t)win_h, OG_PIXFMT_32BPP);
```

This wraps the shared buffer in an `ogSurface` object so you can draw
with the objGFX API.  You can also write raw `uint32_t` pixels yourself
if you don't want the dependency — the buffer is always 32 bpp `0x00RRGGBB`.

### 5. Draw

```c++
surf.ogFillRect(0, 0, win_w - 1, win_h - 1, RGB(20, 40, 80));  // background
font.PutString(surf, x, y, "Hello, views");                    // text
```

See [`objgfx-reference.md`](objgfx-reference.md) for the full surface
and font APIs.

### 6. Flip

```c++
struct display_flip *fl = (struct display_flip *)msg.data;
fl->window_id = win_id;
fl->dirty_x = 0; fl->dirty_y = 0;
fl->dirty_w = win_w; fl->dirty_h = win_h;   // 0,0,w,h = whole window
mpi_postMessage("views", DISPLAY_FLIP, &msg);
```

The dirty rectangle lets the compositor avoid rebuilding the whole
screen when only part of your window changed.  You can be lazy and
always pass the full rect during development.

### 7. Pump events, in a loop

```c++
mpi_message_t ev;
if (mpi_fetchMessage("hello", &ev) != 0) { sched_yield(); continue; }
switch (ev.header) {
case DISPLAY_CLOSE:     /* user clicked the X */     release(); return 0;
case DISPLAY_WINRESIZE: /* user resized window */    reattach_and_redraw();
case DISPLAY_KEY:       /* keypress */               handle_key();
case DISPLAY_MOUSE:     /* mouse motion/click */     handle_mouse();
}
```

Note `DISPLAY_WINRESIZE` carries a **new** `shm_base` — the old buffer
is gone, you must `ogAttach()` again before drawing.  This is the most
commonly-forgotten step; if your app draws garbage after a resize this
is almost certainly the cause.

---

## Building and running

```sh
bmake world                    # builds bin/hello/hello
bmake image                    # bake into the disk image
bmake run                      # launch QEMU; double-click hello in the taskbar
```

Use `bmake image` not `bmake install-world` — the latter is slow because
it rsyncs the entire source tree onto the image.

---

## What's not in the hello example (yet)

- **Sub-rectangle dirty FLIPs.**  Hello always flips the whole window.
  For a real app, track what changed and pass a tight `dirty_*` to save
  the compositor work.
- **Mouse handling.**  `DISPLAY_MOUSE` arrives but hello ignores it.
  See `bin/views/settings/main.cc` for a real implementation
  (mouse routing, hit-testing).
- **Server-side title-bar buttons.**  The compositor draws the title
  bar and close button automatically.  If you want a borderless window
  (a panel or flyout), set `no_decor = 1` in the claim request.
- **Pop-up windows / dialogs.**  Just claim another window — there is
  no parent/child relationship at the protocol level.

---

## When to use the `ubix::` C++ wrappers instead

[`bin/tessera/main.cc`](../../bin/tessera/main.cc) and
[`bin/views/settings/main.cc`](../../bin/views/settings/main.cc) use a
higher-level C++ wrapper layer (`<ubix/mailbox.hh>`, `<views/display.hh>`)
that hides the `mpi_message_t`/struct-cast dance.  For new C++ apps,
prefer those — they're more idiomatic and harder to misuse.  The raw API
shown here is the right starting point because it makes the wire
protocol visible.

---

## Common bugs

| Symptom | Cause |
|---------|-------|
| Window never appears, app hangs | Forgot `mpi_createMbox` before `DISPLAY_CLAIM`. |
| `DISPLAY_DENIED` | Bad geometry (zero w/h) or compositor out of window slots. |
| Garbage after resize | Drew into the old `shm_base` after `DISPLAY_WINRESIZE` — must `ogAttach` to the new buffer first. |
| Triple-fault / page fault | Wrote past `win_w * win_h` in the shared buffer. objGFX clips; raw pixel writes don't. |
| Text doesn't appear | Font file missing.  `ogScalableFont::Load` returns false; check with `IsValid()`. Standard path: `/var/fonts/DejaVuSans.ttf`. |
| Title bar text wrong | Title is copied into the claim request — you can't change the pointer afterwards.  Use `DISPLAY_SETTITLE` (header 13) to update it later. |
