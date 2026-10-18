# CI-built UEFI disk images — design + build plan

Status: **approved 2026-10-17 and implemented in this lane**, except the two
changes outside its scope (section 11), which the CI jobs depend on.

Approved choices: branded host A now, B (or C) before the first public
announcement; desktop smoke + base build-only on MRs; `\EFI\BOOT\kernel` as the
single UEFI kernel path; MBR for step 1; `.img.xz` only for releases; manual
`publish:cdn`; GitLab canonical; Dockerfile change in scope of a wider lane.

Goal: CI builds a UEFI-bootable disk image per architecture, with the loader and
kernel already on the image's ESP, and proves each one boots to the login prompt
under OVMF / AAVMF with no QEMU `-kernel`. The same image boots in QEMU, UTM and
VirtualBox. This is step 1 toward Ubuntu-style media
([installer-plan.md](installer-plan.md): B3/B4 live image = W-056/W-057,
installer app = B7/B8 = W-060/W-061). The design stays compatible with that
and builds none of it.

## 1. Where we are

- `tools/mkimage.sh` writes an **MBR** image: FAT32 (64 MB, type 0x0C) + swap
  (256 MB) + UbixFS pool (3080 MB, type 0x9C); about 3.4 GB, sparse. The
  FAT holds the kernel only for the QEMU `-kernel`/U-Boot paths: aarch64 gets the
  flat `Image` at `/kernel/kernel` (U-Boot `booti`), x86_64 the ELF at
  `/boot/kernel/kernel`. No loader is on it.
- `BOOTX64.EFI` / `BOOTAA64.EFI` (`stand/`, `bmake efi`) are built but are only
  added to a *private copy* of the image at run time by
  `tools/run-uefi-x86_64.sh` / `tools/run-aavmf-aarch64.sh` (`mmd`/`mcopy` into
  partition 1 at 1 MB, plus the ELF kernel refreshed from `build/`).
- The loader (`stand/efi/loader.c`) looks for an **ELF** kernel at
  `\boot\kernel\kernel`, then `\kernel\kernel`, then `\EFI\BOOT\kernel`. On
  aarch64 `\kernel\kernel` is the flat Image, which fails `load_elf`, and there is
  no `\boot\kernel\kernel`. So today an unmodified aarch64 image cannot be
  UEFI-booted without the script's kernel swap.
- CI (`.gitlab-ci.yml`): `boot` / `boot:keep` build `kernel world image` per
  arch and run `tools/check/checks/boot-smoke.sh`, which uses QEMU `-kernel`
  (so it never exercises the loader, ESP or firmware).
- Known script inconsistency: `boot-smoke.sh` waits for `Login: ` (bin/login);
  the two run scripts grep `login:`. One marker is needed.

## 2. Image changes (`tools/mkimage.sh`, both arches identical)

1. **The FAT partition becomes the ESP in fact.** Install, at image build time:
   - `\EFI\BOOT\BOOTX64.EFI` (x86_64) / `\EFI\BOOT\BOOTAA64.EFI` (aarch64), from
     `build/${ARCH}/boot/` (loader path overridable with `LOADER=`; build fails
     if it is missing unless `NO_LOADER=1`, so a CI image can never silently lack
     it).
   - `\EFI\BOOT\kernel` — the **ELF** kernel (`build/${ARCH}/boot/kernel`), the
     loader's existing third search path. Same path on both arches, so the
     "kernel next to loader" layout is the one thing to explain in docs.
2. **Keep the legacy placements** that other consumers read: aarch64
   `/kernel/kernel` (flat Image, U-Boot / Pi) and x86_64 `/boot/kernel/kernel`.
   Problem: the loader prefers `\boot\kernel\kernel`, so on x86_64 a stale or
   different kernel there would win over `\EFI\BOOT\kernel`. Decision (recommended):
   on x86_64 stop writing `/boot/kernel/kernel` *unless* something still needs it
   (to be checked in step A: `tools/update-image.sh`, `tools/extract-kernel.sh`,
   Makefile line ~1103, `tools/grub.cfg`). If something does, write the same ELF
   to both. Rule: **one kernel build, every copy byte-identical** — mkimage
   asserts this with `cmp`.
3. **ESP size 64 → 128 MB** (installer-plan default for an installed disk is 256
   MB; 128 is enough for loader + ELF + a later ramdisk-free state). Verify the
   kernel ELF size on both arches first; ramdisk (B3) will need the ESP larger
   again, which B4 owns. mkimage already derives all offsets from the `*_MB`
   variables.
4. **Partition scheme: stay MBR for step 1** and leave the type at 0x0C. edk2
   (OVMF, AAVMF, UTM, VirtualBox) mounts a FAT volume on an MBR partition and
   runs `\EFI\BOOT\BOOT*.EFI`; the existing run scripts already prove it. The
   kernel's partition parser and `aarch64_virtio_blk_pool_minor()` key off the
   MBR 0x9C partition; GPT parsing exists (`sys/dev/partition.c`,
   `gpt_part.h`) but the pool-root-on-GPT path is unproven on both arches, and
   changing it here would couple step 1 to kernel risk. The move to **GPT with an
   EF00 ESP** is B4's job (installer-plan decision: GPT for media) and mkimage's
   layout code is already isolated in one Python block to swap out.
5. **Size knob for CI.** Add `POOL_MB` / `POOL_FS_MB` env overrides (defaults
   unchanged) so CI and the base profile can build a smaller pool. Whether this
   is needed depends on the artifact-size measurement in step A (section 6).
6. **Profiles.** `PROFILE=desktop|base` already exists. No change except the
   output name: CI passes `ubixos-${ARCH}-${PROFILE}.img` (the Makefile's
   `DISK_IMAGE_*` names stay for local use).
7. **Arch parity.** Both arches go through the same code path; the only
   per-arch lines are the loader file name and the legacy kernel placement.
   Note: aarch64 UEFI boot is QEMU virt + AAVMF only (no DTB from firmware
   assumed beyond what B2 does); the Pi keeps its U-Boot SD image unchanged.

## 3. Run scripts and boot-smoke

- `tools/run-uefi-x86_64.sh`, `tools/run-aavmf-aarch64.sh`: **stop
  modifying the image copy by default** — the image under test is the image
  shipped. Keep the private copy (so the artifact is never written to by a
  guest). Move the loader/kernel injection behind `--inject` for the dev
  edit-rebuild loop (`bmake run-uefi-x86_64` keeps working). Add firmware
  autodetection: probe `$FW`, then QEMU's share dir (Homebrew), then
  `/usr/share/OVMF/OVMF_CODE_4M.fd` + `OVMF_VARS_4M.fd` (Debian/Ubuntu) and
  `/usr/share/AAVMF/AAVMF_CODE.fd` + `AAVMF_VARS.fd`. Fail with a message that
  names what was searched.
- Marker: both scripts and `boot-smoke.sh` use one regex, default
  `[Ll]ogin: `, overridable by `CHECK_BOOT_MARKER`; all three also fail fast on
  `panic|assertion failed`, and the scripts print the serial tail on failure
  (they already do).
- `tools/check/checks/boot-smoke.sh`: add `CHECK_BOOT_MODE=uefi|kernel`.
  `kernel` (today's behaviour) stays the default for the local fast loop;
  CI sets `uefi`, which calls the matching run script with `--smoke` on the
  prebuilt image. `uefi` mode needs the image to *already* contain the loader
  (it never injects), which is what makes the CI smoke a real test of the
  artifact. Keep exit 77 (skip) when qemu/firmware is absent locally; `--strict`
  in CI turns that into a failure, as today.
- Timeouts: TCG is slow; keep 240 s x86_64 / 300 s aarch64 as defaults, and let
  CI raise them via `CHECK_BOOT_TIMEOUT` if the measured boot needs it.

## 4. CI jobs and runners (`.gitlab-ci.yml`, `tools/ci/`)

Reuse the existing runners: `k8s` for everything, `shell` only for `toolchain`
(no change to runners). Every job still starts from the toolchain image; both
arches are TCG (no KVM needed). Firmware: see below.

| Job | Stage | Runs | When |
|-----|-------|------|------|
| `images` (matrix: arch) | boot | `bmake kernel world TARGET=$ARCH`, `bmake -C stand TARGET=$ARCH`, then `mkimage` for `desktop` (and `base` where required, same world, two images), smoke each by `tools/check/run.sh --strict --arch $ARCH boot-smoke` with `CHECK_BOOT_MODE=uefi`, then compress | MR, master, tags |
| `publish:registry` | publish | upload to the generic package registry | master only |
| `publish:cdn` | publish | rsync to the CDN | protected tag, manual |

The existing `boot` / `boot:keep` jobs are **replaced** by `images` (they would
be redundant: same build, weaker smoke). The `-kernel` smoke remains available
locally and as `CHECK_BOOT_MODE=kernel`. `build` / `build:keep` stay (kernel +
world per arch, fast signal, no image) — this keeps `images` off the critical
path of the cheap feedback.

Building the world once and cutting both profiles from it avoids doubling the
long step; only the image step (mkimage, minutes) and the TCG boot repeat.

**Variants.** Four images exist (2 arches × `desktop`/`base`). Recommendation:
MR pipelines build + smoke **desktop** on both arches and build **base**
without smoke-booting it; master and tags smoke all four. This is a trade
between MR latency (an extra TCG boot per arch) and catching base-only breakage
(base is the `views`-absent path that the kernel branches on). If the person
prefers, smoking base on every MR is a one-line `rules` change.

**Firmware in CI.** Ubuntu 24.04 packages the firmware: `ovmf`
(`/usr/share/OVMF/OVMF_CODE_4M.fd`, `OVMF_VARS_4M.fd`) and `qemu-efi-aarch64`
(`/usr/share/AAVMF/AAVMF_CODE.fd`, `AAVMF_VARS.fd`). Baked into the toolchain
image — no per-job download, no network dependency, pinned by the image hash
like everything else. This needs a `.gitlab/Dockerfile` change (**outside this
lane's scope**; see section 9). Rejected: downloading firmware per job (flaky,
unpinned); committing blobs to the repo (binary, license noise); using the
runner's QEMU share dir (not guaranteed on the k8s runner).
Also needed in the image: `lld`'s `lld-link` for the `stand/` loader (already in
`lld-18`; confirm it is on `PATH` in step A), `zstd` and `xz-utils` for
compression, `curl` for the registry upload, `openssh-client` for rsync.

**Job times and sizes (estimates — to be replaced by measurement in step A).**
I have no pipeline numbers for the image step, so treat these as budgets, not
facts:

| Item | Budget |
|------|--------|
| world build (cross, existing `boot` job) | whatever `boot` takes today; reuse its 60 min timeout |
| mkimage (pool populate, per profile) | minutes |
| TCG boot to login, x86_64 / aarch64 | up to 240 s / 300 s per image |
| compress | `zstd -T0 -19` of a mostly-hole 3.4 GB raw: minutes; `xz -9` slower |

Image size is dominated by the pool's real content, not the 3.4 GB logical size;
the hole compresses to nothing. Measure `du` and the compressed size for each
of the four images in step A. If the compressed `desktop` image exceeds the
GitLab artifact limit (the instance default is 100 MB unless the admin raised
it — to be checked), either the MR image artifact is dropped to "kernel + ESP
only" or the limit is raised; this is a question for the person.

**Compression and formats.**
- CI/registry: `.img.zst` (fast, parallel).
- Release: `.img.xz` (widely extractable on macOS/Windows/Linux) plus
  `SHA256SUMS`. Also publish a `.qcow2` (UTM) and `.vdi` (VirtualBox) made with
  `qemu-img convert` from the same raw image, so a user imports without
  converting. Cheap (qemu-utils is already in the toolchain image) but adds
  roughly 2× storage per release; offered, not mandatory — the person's call.
- VM notes for the release page: VirtualBox x86_64 needs "Enable EFI" ticked;
  aarch64 guests need a VirtualBox with arm64 guest support; UTM uses its own
  edk2.

**Helpers.** `tools/ci/` gains: `firmware.sh` (prints the OVMF/AAVMF code+vars
paths, shared by jobs and scripts), `publish-registry.sh`, `publish-cdn.sh`,
and `image-name.sh` (single source of the file-name scheme below).

## 5. Storage and download

Three tiers, increasing in permanence. File names are the same everywhere:
`ubixos-<version>-<arch>-<profile>.img.<ext>`, where `<version>` is
`3.1.0-BETA` style from `sys/include/ubixos/version.h` for tags, and
`<version>-dev.<shortsha>` for non-tags (no `+` in file names).

### 5.1 MR images: job artifacts, 7 days

`images` publishes `ubixos-*.img.zst`, `SHA256SUMS`, the serial boot log and the
kernel as artifacts with `expire_in: 7 days` (today it is "1 week" — the same,
spelled explicitly) and `when: always` so a failed boot keeps its log. The
`.mr-only` / `.keep` split stays because `expire_in` cannot take a variable.
Risk: artifact size limit (section 4); fallback is "log + kernel only on MRs".

### 5.2 master builds: GitLab generic package registry

On every master build, `publish:registry` uploads to
`<project>/-/packages/generic/ubixos-images/<version>+<shortsha>/<file>` using
`CI_JOB_TOKEN` and `curl --upload-file`. Not overwritten: the version is unique
per commit; a tag's `<version>` is the tag name without `v`. A scheduled cleanup (a small `tools/ci/` script calling the
packages API) keeps the last N (default 10) per arch/profile so the registry
doesn't grow without bound; the number is a knob. Needs project registry
storage headroom — about 4 images × compressed size × N; the person should
confirm quota.

### 5.3 Tagged releases: cdn.ubixsys.com

On a protected tag, a **manual** `publish:cdn` job (a deliberate click after
the tag pipeline is green — a bad tag would otherwise burn a version name,
since versions are never overwritten) does:

1. Re-use the tag pipeline's already-smoked images (no rebuild).
2. `ssh kube-003 'test ! -e /srv/cdn/releases/<version>'` — refuse if it
   exists. Then `rsync -a --ignore-existing` into
   `releases/<version>/` and write `SHA256SUMS` (and a detached signature if the
   person wants one — minisign or GPG; open question).
3. Mark the directory read-only on the server (`chmod -R a-w`; `chattr +i` if the
   filesystem supports it) so even a later mistaken rsync cannot overwrite.
4. Create the GitLab release (`release:` keyword) with asset links pointing at
   the CDN URLs, so the release page and CDN agree.

Secrets: an SSH deploy key as a **protected, masked-where-possible CI variable**
available only to protected tags; the rsync destination is restricted
server-side (`command=`/chroot or a dedicated user that can write only under
`releases/`). The k8s runner needs `openssh-client` in the toolchain image.
Paths on the server (`/srv/cdn/...`) are placeholders: the person/admin
supplies the real docroot and user.

### 5.4 Optional branded download host

The person is still choosing. Options, with a recommendation:

| Option | What it is | Cost | Effort | Notes |
|--------|------------|------|--------|-------|
| A. Use `cdn.ubixsys.com` as is | URLs in docs point at it | $0 | none | Name is not "uBixOS"; if the CDN host changes, every published URL breaks. |
| B. `releases.ubixos.org` → CNAME to the CDN | Alias with its own TLS cert | $0 beyond the domain (if `ubixos.org` is already owned — to confirm) | about half a day: DNS record, vhost for the new name, cert (e.g. cert-manager / Let's Encrypt) | URLs become permanent and branded; origin can move later. Needs the origin to serve the second hostname. |
| C. `releases.ubixos.org` as a small redirector (302 to wherever files live) | Static redirect rule on an ingress | $0 + domain | about half a day, a few lines of ingress config | Same permanence as B; the CDN can be swapped without URL change; costs one extra hop and the redirector itself must stay up. |
| D. Object storage + CDN (e.g. R2 / S3+CloudFront) | New hosting | storage ~cents/GB-month; egress $0 on some, ~$0.08/GB on others | 1–2 days plus cut-over | Only worthwhile if downloads outgrow kube-003. Over-engineered now. |

**Recommendation:** ship with **A** now (it blocks nothing), and do **B** (or C
if the CDN is third-party and not ours to add a vhost to) **before the first
public announcement**. File layout on the CDN is host-independent, so doing B
later needs no rework and no re-upload. Skip D.

## 6. How this relates to the GitHub/GitLab setup and W-057

- **Repo setups disagree today.** `docs/gitlab-mirror.md` says GitHub is the
  source of truth with GitLab matched to it for a push mirror; `docs/ci.md`
  says GitLab is the source of truth and the GitHub workflow is retired
  after GitLab goes green. This plan assumes the `docs/ci.md` direction (GitLab
  runs CI and owns the registry and releases; it has the runners, the package
  registry and the deploy secret). The task brief for this lane also said "push
  to GitHub and open a PR" while the lane's push hook opens a GitLab MR; I
  followed the lane tooling (GitLab MR) and flag it here. It is not a
  plan-blocker, but the person should say which is canonical.
- **GitHub sees nothing new.** No image jobs there; GitHub `ci.yml` stays
  until retired. If GitHub Releases are used, they carry notes + links to the
  CDN/branded URL, not the images (avoids a second copy of ~GB assets).
- **W-057** (Installer B4: `tools/mkmedia.sh` + CI boot-smoke under OVMF/AAVMF)
  is *after* W-056 (ramdisk). This lane delivers its CI half early, for the
  plain disk image: the `images` job, firmware in the toolchain image,
  `CHECK_BOOT_MODE=uefi`, the file-name scheme and the three storage tiers.
  When B4 lands, `mkmedia` outputs `ubixos-<version>-<arch>-live.img` and joins
  the same matrix and publishing path with no CI redesign: the run scripts take
  any image path, the loader finds the kernel at `\EFI\BOOT\kernel` (and the
  ramdisk at `\boot\ramdisk`, which B3 adds). B4's remaining work is the
  GPT layout, the ramdisk, and the live-boot smoke condition.
- **Installer B7/B8 (app).** Nothing here conflicts: the installer runs from
  the live image and writes an ESP like this one; the "kernel at
  `\EFI\BOOT\kernel`" rule is what `ubinstall` must reproduce, so it is also
  the documented contract.
- A `docs/WORK.md` row is needed for this lane (proposed: `W-079 ci: CI-built
  UEFI images, smoke under OVMF/AAVMF, three-tier storage`); W-057's note gets
  "CI half done in W-079". Both are `docs/` edits outside this lane's scope.

## 7. Implementation steps (after approval)

| Step | Work | Files |
|------|------|-------|
| A | Measure first: build both images in CI; record sizes (raw `du`, compressed), job times, kernel ELF size; check where `/boot/kernel/kernel` is still read on x86_64; confirm `lld-link` and firmware paths. Fill section 4's table with real numbers | `tools/ci/` (a throwaway measuring job), this doc |
| B | mkimage: ESP install of loader + `\EFI\BOOT\kernel`, `cmp` assert, 128 MB ESP, `POOL_*` overrides, name parameter | `tools/mkimage.sh` |
| C | Run scripts: no-injection default, `--inject`, firmware autodetect, shared marker | `tools/run-uefi-x86_64.sh`, `tools/run-aavmf-aarch64.sh` |
| D | boot-smoke `CHECK_BOOT_MODE`, marker | `tools/check/checks/boot-smoke.sh` |
| E | `images` job replacing `boot`; artifacts 7 days; profiles; compression | `.gitlab-ci.yml`, `tools/ci/` |
| F | `publish:registry` + cleanup | `.gitlab-ci.yml`, `tools/ci/publish-registry.sh` |
| G | `publish:cdn` + release object + signature (if wanted) | `.gitlab-ci.yml`, `tools/ci/publish-cdn.sh` |
| H | Docs: describe jobs, how to reproduce locally, download page text | `docs/design/ci-images.md`, `docs/ci.md` |

Each of B–D is verified locally on both arches (`bmake image` then the run
script with `--smoke` for aarch64 and x86_64) before CI is touched; `tools/check/run.sh --tier fast` passes before each commit.

## 8. Risks and open points

- **Artifact limit / registry quota** unknown (section 4, 5.2).
- **x86_64 loader preference** for `\boot\kernel\kernel` over
  `\EFI\BOOT\kernel` (section 2.2); resolved by writing identical bytes or
  dropping the old path.
- **aarch64 TCG UEFI boot time** may exceed 300 s with a desktop image; the
  existing `boot` job already tolerates it with `-kernel`, but UEFI adds
  firmware time. Measured in step A.
- **MBR now, GPT later.** Real PCs and some firmware want GPT/EF00; VMs do not.
  Explicitly deferred to B4.
- **Secure Boot** unsupported (installer-plan decision); stated on the download
  page.
- **`Makefile`/`docs/`** targets that hard-code image names (`run-uefi-*`,
  `efi`) keep working; only `-kernel` paths are affected by moving the kernel.

## 9. Files outside this lane's scope that the implementation needs

- `.gitlab/Dockerfile`: add `ovmf`, `qemu-efi-aarch64`, `zstd`, `xz-utils`,
  `curl`, `openssh-client`. Changing it rebuilds the toolchain image (once).
- `docs/ci.md`: job table and storage description.
- `docs/WORK.md`: new row (W-079 proposed), note on W-057.
- `docs/design/installer-plan.md`: B4 status line referencing this plan.
- `Makefile`: optional `bmake image-uefi` convenience target; none required.
- GitLab project settings (not files): max artifact size, registry quota, the
  protected-tag deploy key variable, and the server-side CDN user and docroot.

## 10. Decisions for the person

1. **Branded download host:** A now + B (or C) before announcing (recommended),
   or A only, or B/C from the start, or D.
2. **MR variants:** desktop smoke + base build-only on MRs (recommended), or
   all four smoked every time.
3. **Kernel placement:** `\EFI\BOOT\kernel` as the single UEFI path and drop x86_64
   `/boot/kernel/kernel` if nothing needs it (recommended), or keep both
   byte-identical.
4. **Partition scheme for step 1:** stay MBR (recommended) or go GPT/EF00 now
   (couples this to kernel pool-root work).
5. **Release formats:** `.img.xz` only (recommended), or also `.qcow2` / `.vdi`.
6. **`publish:cdn`:** manual click (recommended) or automatic on a protected tag;
   signing the checksums (minisign/GPG) or not.
7. **Toolchain Dockerfile change** (and that it is outside this lane): OK to ask
   the owning lane or widen scope.
8. **Canonical repo:** confirm GitLab as CI/release home (section 6).
9. Server facts only the person can give: artifact size limit, registry quota,
   CDN docroot/user on kube-003, whether `ubixos.org` is owned and who runs
   its DNS.

## 11. Implementation notes (what was built, what changed from the plan)

Measured locally (Apple Silicon; aarch64 under HVF, x86_64 under TCG), from a
clean `bmake kernel world` + `bmake -C stand`:

| Image | Allocated | `zstd -19` | Smoke |
|-------|-----------|------------|-------|
| aarch64 desktop | 68 MB | 11.4 MB | PASS (login via AAVMF) |
| aarch64 base | 40 MB | 2.9 MB | PASS |
| x86_64 desktop | 67 MB | 11.7 MB | PASS (login via OVMF, TCG) |
| x86_64 base | 39 MB | 3.1 MB | PASS |

The pool is mostly a hole, so the compressed images are tiny. This removes the
artifact-size and registry-quota worry in sections 4 and 5 and makes the
`POOL_*` size knobs optional (kept, harmless). x86_64 `make-images.sh` (two
profiles, two TCG boots) took about 35 s locally; CI will be slower, the
`kernel world` build dominates. Real CI times come from the first pipeline.

Deviations from the plan:

- **`mkimage` is lenient by default.** A missing loader/kernel warns and builds
  a QEMU `-kernel`/U-Boot-only image, so `bmake image` keeps working for local
  use. CI sets `REQUIRE_LOADER=1`, which fails instead. (Plan said "fails unless
  `NO_LOADER=1`".)
- **x86_64 `/boot/kernel/kernel` is dropped**: nothing read it (checked
  `update-image.sh`, `extract-kernel.sh`, `grub.cfg`, the Makefile). aarch64
  keeps the flat Image at `/kernel/kernel` for U-Boot self-hosting.
- **Version names** use `-dev.<sha>`, not `+<sha>`.
- `registry:prune` is a scheduled job (variable `PRUNE_REGISTRY=1`, token
  `PRUNE_TOKEN`).
- `tools/ci/` gained `firmware.sh`, `image-name.sh`, `make-images.sh`,
  `publish-registry.sh`, `prune-registry.sh`, `publish-cdn.sh`.
- Run scripts take `--inject` for the dev loop; the default boots the image as
  shipped. All three smoke paths share the `[Ll]ogin: ` marker.

### Needed from outside this lane

1. **`stand/efi/loader.c`** — kernel search order (done: the lane scope was
   widened; `\EFI\BOOT\kernel` is now tried first, both arches boot). The loader tries
   `\boot\kernel\kernel`, `\kernel\kernel`, `\EFI\BOOT\kernel`. On aarch64
   `\kernel\kernel` is the flat U-Boot Image, so the loader reads it, fails
   `load_elf` ("kernel is not a valid ELF64") and never reaches the ELF. Fix:
   put `\EFI\BOOT\kernel` first. (Verified with a local one-line change:
   aarch64 then boots to login; reverted here.)
2. **`.gitlab/Dockerfile`** (done) — added `ovmf`, `qemu-efi-aarch64`, `zstd`,
   `xz-utils`, `curl`, `openssh-client`.
3. `docs/ci.md`, `docs/WORK.md` (new row, W-057 note),
   `docs/design/installer-plan.md` B4 status.
4. GitLab project settings: protected variables `CDN_HOST`, `CDN_USER`,
   `CDN_ROOT`, file variable `CDN_SSH_KEY` (protected tags only), schedule +
   `PRUNE_REGISTRY`/`PRUNE_TOKEN`; the CDN-side deploy user restricted to
   `releases/`.

First MR pipeline (!7, pipeline 8463): `images` passed on both arches in about
3 min each on the k8s runner (job `images: [aarch64]` 192 s, `[x86_64]` 184 s,
including the world build). Desktop images: 59 MB (aarch64) / 53 MB (x86_64)
allocated, 9.5 MB / 9.4 MB `zstd -19`; base: 41 / 37 MB, 2.4 MB each. The
desktop UEFI boot-smoke passed under OVMF and AAVMF (TCG); base was built, not
smoked (MR policy). Job artifact per arch about 12 MB.

Not verified yet (only run on master / tags): `images:keep`, the registry
upload and prune scripts, and the CDN publish. `.gitlab-ci.yml` was checked
with GitLab's CI lint.

Naming gotcha: a job called `image` collides with the global `image:` keyword,
so the jobs are `images` / `images:keep`.
