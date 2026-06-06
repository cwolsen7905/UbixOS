# UbixOS Session Model — Plan

Give the desktop a real **session** abstraction so the system can log in, log
out, lock, and (eventually) fast-switch between users the way macOS
(`loginwindow`) and Windows (`winlogon`) do — and, as the foundation, tear a
session down cleanly so its memory and windows are reclaimed (the logout leak).

## Problem

There is no object that says *"these processes and these windows belong to
user X's login."* Consequences:
- **Logout doesn't tear the session down** → the user's apps keep running and
  their windows linger across logins (the memory leak).
- **No lock screen** (can't suspend a session without killing it).
- **No user switching** (can't keep one user's apps alive while another logs in).

All three are the same missing abstraction.

## The Session abstraction

A **Session** = `{ id, user, pgrp, windows, state }`, `state ∈ active |
background | locked`.

- **id** — a session identifier every process in the login carries (`setsid`
  at login; children inherit it) and that views stamps on every window the
  session's clients claim. "Kill / hide / show this session" then becomes one
  reliable id comparison instead of walking the process tree.
- **pgrp** — the session's process group, for signalling the whole session.
- **windows** — views owns windows grouped *per session* (not one flat list)
  and composites the active session's set.
- **state** — active (on screen), background (switched away, still running),
  locked (on screen but gated by a lock overlay).

Owned by a small **SessionManager** (in vlogin, the persistent login parent):
one class, clear invariant (the set of live sessions + which is active).

## Lifecycle operations

| Op | Process side | Window side |
|----|--------------|-------------|
| Login | `setsid`; create Session; fork session leader (taskbar) in the new group | views tags new windows with this session id |
| Logout | kill the session group (`kill(-pgid)`) | views frees that session's windows |
| Lock | (nothing — keep running) | views shows a lock overlay above the active session |
| Unlock | — | remove the overlay |
| Switch user | mark current session `background` (keep running); activate/create another | views **hides** the background session's windows, **shows** the target's |

## Phases

### Phase 0 — Clean logout (the leak fix) ✅/🔄 **do now**
The foundation, and it fixes the immediate leak. No Session object yet.
- **vlogin group-kill** on logout — DONE (`82f5a8750`-era): vlogin puts the
  taskbar's session in its own process group and `kill(-pgid, SIGKILL)`s it at
  logout; vlogin survives in its own group to re-show login.
- **views dead-client reaper** — 🔄 this change. views polls each window's
  client liveness (`kill(sender_pid, 0)`) and frees windows whose client has
  died (buffer + struct + z-order + taskbar notify), reclaiming the window
  buffers and removing the "ghost window" left when a killed app never sent
  DISPLAY_RELEASE. Robust to any client death (logout-killed, crashed), not just
  logout. Same window-cleanup logic Phases 1–3 reuse.

### Phase 1 — Formalize Session + session id
- Kernel: a per-process **session id** (via `setsid`/a session field), readable
  so a client's claim can carry it.
- `display_claim_req` carries (or views derives) the client's session id; views
  stores it on each `Window` and groups windows by session.
- `SessionManager` in vlogin owns `Session` objects; login = create, logout =
  destroy (kill group + tell views to drop the session's windows).

### Phase 2 — Lock screen
- A `DISPLAY_LOCK` overlay: vlogin (or the session's agent) claims a top-most
  full-screen window that captures input until unlocked; the session's processes
  keep running underneath. Unlock releases the overlay.

### Phase 3 — Fast user switching
- vlogin does NOT kill the session on switch; marks it `background`.
- views hides the background session's windows (composite only the active
  session's set) and shows the target session's (or the login screen).
- Re-selecting a background session re-shows its windows — its apps never
  stopped.
- Per-user desktop/state already exists (ubistry per-user settings,
  `DISPLAY_SET_USER`); switching re-resolves it.

## Notes / gotchas
- A terminal that `setsid`s for its shell's controlling tty creates a *sub*
  session/group — Phase 1's explicit session id (carried by all descendants
  unless they re-`setsid`) is more robust than process-group walking for
  identifying session membership. Window reaping (Phase 0) covers cleanup
  regardless of process-group escapes.
- views must composite per-session: replace the flat z-stack with "active
  session's z-stack" (Phase 1/3).
- Keep `SessionManager` and `Session` as plain typed objects with clear
  invariants (no catch-all blobs) per the project's SOLID conventions.
