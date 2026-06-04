# Headless Mac as a UbixOS Dev Workstation

The setup: a desktop Mac (Mini, Studio, Pro) does all the work — builds,
QEMU, IntelliSense, Claude Code — while you work from a laptop (or
iPad, or anything that can SSH and render VNC).  This nets you the
desktop's RAM/cores/cooling without giving up portability.

This doc is opinionated and concrete.  Adjust if you prefer different
tools, but the *shape* of the setup is the same.

---

## What you end up with

- `ssh mini` from anywhere → drop into a persistent tmux session, exactly
  where you left off.
- VS Code on the laptop, editing files that live on the Mini, with
  IntelliSense parsing the kernel using the Mini's RAM.
- `bmake world` runs on the Mini's fanned M-series cores.
- QEMU boots on the Mini.  For text-mode work the serial console
  streams over SSH; for the views compositor you connect a VNC client
  on the laptop.
- Long builds keep running while the laptop is closed or you're on a
  meeting.

---

## One-time setup

### 1. Tailscale (5 minutes, free for personal use)

Install on **both** machines from <https://tailscale.com/download>.
Sign in with the same account on both.

Result: each machine gets a stable `*.tailnet.ts.net` name reachable
from anywhere — coffee shop, hotel WiFi, LTE — without port forwarding
or VPN config.  Encrypted by default.

Confirm: `tailscale status` should list both machines as online.

### 2. SSH access to the Mini

On the Mini, enable Remote Login:
*System Settings → General → Sharing → Remote Login → on*.

On the laptop, generate a key if you don't already have one:

```sh
ssh-keygen -t ed25519 -C "laptop"
ssh-copy-id <user>@mini.tailnet-name.ts.net
```

Add an SSH config entry on the laptop so you can type just `ssh mini`:

```
# ~/.ssh/config
Host mini
    HostName mini.tailnet-name.ts.net
    User <your-user>
    ForwardAgent yes
    ServerAliveInterval 30
    ServerAliveCountMax 10
```

`ForwardAgent` lets `git push` from the Mini use the laptop's
GitHub key.  `ServerAliveInterval` keeps the connection alive across
network blips.

### 3. tmux on the Mini

```sh
brew install tmux
```

Minimal `~/.tmux.conf` worth keeping:

```
set -g mouse on
set -g history-limit 10000
set -g default-terminal "screen-256color"
set -g status-bg colour234
set -g status-fg colour250
```

Workflow: `ssh mini`, then `tmux new -s ubix` the first time,
`tmux attach -t ubix` every time after.  Sessions survive SSH
disconnects, laptop sleep, network changes.

### 4. VS Code Remote-SSH (laptop side)

Install VS Code on the laptop.  Install the **Remote - SSH** extension
(`ms-vscode-remote.remote-ssh`).  Open the Command Palette →
"Remote-SSH: Connect to Host..." → pick `mini`.

A second VS Code window opens, but every file read, every terminal,
every IntelliSense lookup runs on the Mini.  The laptop is a render
surface.

First-time tip: open the `UbixOS/` folder once it's connected and let
the C/C++ extension index — it takes a couple of minutes the first
time but is cached afterward.  IntelliSense will use the existing
`compile_commands.json` if present (generate with
`python3 tools/gen-compile-commands.py --world`).

### 5. Claude Code on the Mini

Install Claude Code on the **Mini**, not the laptop.  Run it from
inside a tmux session in the UbixOS repo directory.  When you connect
via VS Code Remote-SSH or plain SSH, you're talking to the Mini's
Claude instance.  Same context, same memory, same file system —
nothing to sync.

---

## Daily workflow

```sh
# Wake the laptop, open a terminal, attach to the dev session:
ssh mini
tmux attach -t ubix

# You're now in /Users/<you>/git/UbixOS on the Mini.  bmake, qemu,
# claude, git — everything is local to the Mini.
bmake kernel
bmake run-debug
```

For the IDE experience, skip the terminal step: open VS Code → "Resume
Remote-SSH: mini" from the recents.

---

## Running QEMU headless

### Text-mode (the default)

`bmake run-debug` (in `Makefile`) launches QEMU with `-nographic -serial
mon:stdio` — the entire boot, kernel logs, and shell stream over your
SSH session.  This is what you want for kernel work, driver bring-up,
or anything that doesn't need pixels.  Logs also land in `serial.log`.

### Graphical (the views compositor)

When you need to see the UI — testing the compositor, an objGFX change,
the `views`/`taskbar`/`term` stack — run QEMU with a VNC display
instead of the SDL/Cocoa display:

```sh
qemu-system-i386 \
    -m 256 \
    -drive file=ubixos.img,format=raw,if=ide \
    -serial mon:stdio \
    -vnc :1 -k en-us
```

`-vnc :1` makes QEMU listen on TCP port `5901` (5900 + display number).
Connect from the laptop with macOS's built-in screen sharing:

*Finder → Cmd-K → `vnc://mini.tailnet-name.ts.net:5901`*

Or any VNC client (TigerVNC, RealVNC viewer).  Latency on LAN is
~30 ms, fine for interactive UI work.  Over Tailscale to a remote
network expect 50–100 ms — still usable.

Add a `bmake run-vnc` convenience target if you do this often.

### Combined: serial logs + VNC display

Run QEMU with both `-serial mon:stdio` and `-vnc :1`.  The terminal
streams kernel logs, the VNC client shows the screen, and you can
issue QEMU monitor commands (`info registers`, `quit`) at the same
prompt.

---

## Editing and Git from the laptop side

You don't.  The repo lives on the Mini, full stop.  The laptop is a
viewport — never a second copy.  This avoids:

- Sync conflicts between two working trees.
- "Which machine did I commit on?" confusion.
- Half-built `build/` directories that disagree.
- Backups eating laptop storage with duplicated source.

When you want to push to GitHub: `git push` from the Mini.  Agent
forwarding (the `ForwardAgent yes` line above) means the Mini uses
your laptop's GitHub SSH key transparently — no key on the Mini,
nothing to revoke if the Mini is ever compromised.

---

## Backups

The Mini is now your primary dev machine for this project, so its
backup story matters.  Pick one:

- **Time Machine** to an external drive plugged into the Mini.
  Simplest.  Hourly snapshots.
- **Restic / borg** to a remote (B2, S3) for off-site coverage.
  Cheaper than iCloud at OS-dev data sizes.
- **`git push` early and often** as a poor-man's backup for the
  source tree, plus an `rsync` cron for `ubixos.img` and `serial.log`
  to the laptop if you want them locally.

The `build/` directory is *not* worth backing up — it's regenerable
in minutes.  Add it to your backup excludes.

---

## Troubleshooting

| Symptom | Likely cause / fix |
|---------|---------------------|
| `ssh mini` hangs forever | Tailscale not running on one end.  Check `tailscale status` on both. |
| VS Code Remote disconnects every few minutes | Add `ServerAliveInterval 30` to `~/.ssh/config` (above). |
| VNC connection refused | Mac firewall blocking 5901.  *System Settings → Network → Firewall → Options → allow `qemu-system-*`*.  Or run `tailscale serve` to tunnel it. |
| Tmux pane scrollback is missing | You're in default mode.  `Ctrl-b [` enters copy mode; `q` exits.  Use the mouse if you set `mouse on`. |
| `git push` asks for password | Agent forwarding isn't working.  On the laptop run `ssh-add ~/.ssh/id_ed25519` once per login. |
| QEMU prints "could not bind VNC port" | Another instance is already on `:1`.  Use `:2` (port 5902) or kill the stale one (`pkill qemu-system-i386`). |
| IntelliSense in remote VS Code is wrong | Generate `compile_commands.json` on the Mini (`python3 tools/gen-compile-commands.py --world`) and reload the window. |

---

## What this setup does *not* solve

- **Apple Silicon AArch64 dev** still benefits from HVF on either
  machine — both the Mini and the laptop can run
  `qemu-system-aarch64 -accel hvf` at near-native speed.  The Mini
  is faster because of more cores and cooling, not architecture.
- **Long Clang Stage 1 builds inside QEMU** are bound by the guest's
  single-threaded performance — host parallelism doesn't help.  The
  Mini's M4 single-core beats the laptop's A18 Pro by ~10–15%, so
  there's some win, but not the 3–4× you'd expect from "use the
  bigger machine."
- **Battery life on the laptop.**  An SSH+VNC session is light on
  CPU but the screen and radio stay on.  Plan accordingly.
