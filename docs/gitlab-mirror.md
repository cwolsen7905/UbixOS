# GitLab mirror of GitHub (2026-10-17)

GitHub (`git@github.com:cwolsen7905/UbixOS.git`) is the source of truth.
GitLab (`git@gitlab.brainchurts.com:ubixos/ubixos.git`) was made to match it
exactly (same branches, tags and shas) so a GitLab -> GitHub push mirror can be
enabled.

## Before: GitLab inventory

| Ref | sha |
|-----|-----|
| refs/heads/dev (default) | a84fff3f6378456baf1ab8a52f1ba1f18ecb2c6f ("Initial Commit") |

0 tags. `dev` was the only protected branch (Maintainers push/merge, force-push allowed).

## Before: GitHub inventory (42 branches, 5 tags)

```
133c057bbc81cfacea951785b0eab2052a991fc3  refs/heads/docs/agent-on-ubixos
f587f0e35847156aed5351ca8bade73a1fe6be08  refs/heads/docs/agent-on-ubixos-research
2c9bf1ee699805e6f9668f65b5098c3d8d2060ea  refs/heads/docs/arch-refresh
6cca53ec739fa5ee424504d450ffa90bde1723a9  refs/heads/docs/identity
383fb82285002d80c730a0c6627e53e74bf53718  refs/heads/docs/packages-plan
b4c89727a9c449824e72a06a687b5bd94ed8187f  refs/heads/docs/standards-and-checks
1e58311e745bd3bd1ec5d25ad0c942adf27457e3  refs/heads/docs/work-matrix
988b90de6b5a7d63203ba1e6acb0677524232134  refs/heads/docs/work-matrix-sync
de4c8ec4c54fb4998b462609110865e3e8f62902  refs/heads/fix/ci-llvm-tools
9bebc73494dfa7357f7eef7f64962ae9e2a324b1  refs/heads/fix/mkimage-linux
675548363047fd88aa4a8d531de57392bfa27d44  refs/heads/fix/procfs-snprintf
af19b744a19d7012158a1557c8d3ed7d5eef427b  refs/heads/fix/ubfs-ino-64bit
6ad1495c6b56c383879fb81f90ab3358e7649fa7  refs/heads/fix/ubfs-m1-lock-comment
efa530d6bedf7e6b4c5d7f5481cd335fb9fcb791  refs/heads/fix/ubfs-qwen36-findings
98d2c8e956158e83fd56a2e4f9f9dc4160807d16  refs/heads/fix/ubfs-qwen36-l1-l3
94154e73050b8cff555037b0184a56c68ed03f27  refs/heads/fix/ubfs-vfs-prefix-stack
88f0b790ae1dc0f4fbde2e5cf7afd94bbca6542f  refs/heads/installer/b1-uefi-x86_64
b9907986cdb43d023b2cd0607a7ed41be82fe6f5  refs/heads/installer/b2-uefi-aarch64
7340ea07e649dff60a0178e3a2ed538ba3335012  refs/heads/installer/b5-disk-gpt
8535335cc64ed1138d56825b673f56d6cf15cf7a  refs/heads/master
7342b8d28b1388954be46545dddd1f3cfbe28fec  refs/heads/packages/p1-libpkg
8c1ae6d490ae41cd76d66cfd4e6c918d87f1c28a  refs/heads/packages/p2-ubpkg
0e9cdcbc06be54131ed9bcfe4e7f21f1a348e258  refs/heads/packages/p3-pkg-device
290532ad3a45fb4df883cdbe61a50c6819988513  refs/heads/release/3.1.0
6a63631a4084412e891a95199db38e62871eda35  refs/heads/releng/2
b659112e2c42ebb919ce3f859165350f424f2301  refs/heads/review/ubfs-qwen36
d0f25c926cac0547153b289383bd76d73e90e5fc  refs/heads/v3-abi-audit
0904527dbcf13b5b49d6f1a4b9a512e6ead995ae  refs/heads/v3-backlog
6af099d83de95a23a934f97e31d0549f8f41eddd  refs/heads/v3-exit-leaks
a7e9ded0fe0d0549542a5305f2d8367c0ee74278  refs/heads/v3-exit-leaks-master
6e0dd1b20ac9071b9867810ef02f3e65d655748b  refs/heads/v3-inos-output
db7a44098b65b88b5f37318e33cf82fc0740d29d  refs/heads/v3-installer-plan
57d77e518859a6cd22670dbf89abca6bacfb7792  refs/heads/v3-musl-bmake
74057ee94778886fcaf90b070186b7a3bccadd84  refs/heads/v3-perf-profile
cadeb23f19609086535a53f8d78b1d7b7d8acb2b  refs/heads/v3-provenance-audit
0d1c8b9b278a6785dd6aca1d30a186f283193428  refs/heads/v3-release-3.1.0
4a39bb7b9f684db8330f103081d831f0bb4f3987  refs/heads/v3-selfhost-fixes
68ab27078ee4e4652f65cd6df8fdc2c981801eaa  refs/heads/v3-sigchld-race
57ffdcfdbdc9ac1ba037b4b4472502d456b2c9f7  refs/heads/v3-small-cleanups
251724c3ca0b2ae230f40ebbdedd2e2613aa8335  refs/heads/v3-stability
078b9853cb9946005edb08efd503574bc4665cd7  refs/heads/v3-world-inos
bf81a714b29fa9924a42d73758f9e0c8753ca898  refs/heads/v3-world-sh-bmake
691928f13cf882c143e60bb967dc2d5cbfa4efd0  refs/tags/smp-scaffold-safe
5b77db8f44e410202e34ff9f24146059b5f9a2a8  refs/tags/smp-scaffold-safe^{}
f5a06b3a155dd8d120da987821efd2d3035330e2  refs/tags/v2.0.0-BETA
83ba0c687a37687b2a1ab00b235a7013fb87c19e  refs/tags/v2.0.0-BETA^{}
cd4c6427b8208496b64357447122d4e20c069860  refs/tags/v2.0.1-BETA
79cfef0c0d1cae183e6fb3505cde75e5705966db  refs/tags/v2.0.1-BETA^{}
0b1d408426eddbe3f823a3f29c68e04c7a2c6ae0  refs/tags/v2.2.0-BETA
158852856f4cbeb843956f3f993ee0a34021c5ba  refs/tags/v2.2.0-BETA^{}
55372301a43af6bee1e4954f9bed518abbabfb0e  refs/tags/v2.4.0-BETA
6a63631a4084412e891a95199db38e62871eda35  refs/tags/v2.4.0-BETA^{}
```

## Steps taken

1. Fresh `git clone --mirror` of GitHub into `/tmp/ubixos-mirror-20261017`.
2. Backed up `dev` to GitLab as `gitlab-old/dev` (a84fff3f). No GitLab tag was
   overwritten (GitLab had none), so no tag backups were needed.
3. Pushed `refs/heads/*` and `refs/tags/*` (no `--mirror`, no force needed:
   nothing but `dev` existed on GitLab).
4. Default branch set to `master` via the API.
5. Protected `master` like `dev` was: push = Maintainers, merge = Maintainers,
   force-push allowed. Removed `dev`'s protection.
6. Old history kept off GitLab but not lost: bundle
   `~/git/ubixos-gitlab-old-dev-20261017.bundle` (ref `refs/heads/dev`,
   a84fff3f, `git bundle verify` OK). Then deleted `dev` and `gitlab-old/dev`
   from GitLab so the mirror cannot push them to GitHub. Restore with
   `git fetch ~/git/ubixos-gitlab-old-dev-20261017.bundle dev:gitlab-old/dev`.
7. `~/git/ubixos-gl` (clean, no unpushed work): fetched with prune, `master`
   now tracks `origin/master`, stale local `dev` removed (it is in the bundle).

## Protection changes

| Branch | Before | After |
|--------|--------|-------|
| dev | protected (Maintainers, force-push on) | deleted |
| master | n/a | protected (Maintainers push/merge, force-push on) |

Default branch: `dev` -> `master`.

## Verification

`diff <(git ls-remote --heads --tags GitHub) <(git ls-remote --heads --tags GitLab)`
printed nothing (identical, including peeled `^{}` tag entries).
Final counts: GitHub 42 branches / 5 tags; GitLab 42 branches / 5 tags.
(GitHub additionally has `refs/pull/*`, which are not branches/tags and are not mirrored.)

## Setting up the GitLab -> GitHub push mirror

1. On GitHub create a fine-grained token (or deploy key with write access) for
   `cwolsen7905/UbixOS` with Contents: read/write.
2. GitLab project -> Settings -> Repository -> Mirroring repositories.
3. Git repository URL: `https://github.com/cwolsen7905/UbixOS.git`
   (for a token use user `cwolsen7905`, password = token; for SSH, choose
   SSH public key authentication and add GitLab's key as a GitHub deploy key).
4. Mirror direction: **Push**.
5. "Keep divergent refs": **off**, so GitHub is forced to match GitLab.
6. "Mirror only protected branches": **off**, so all branches are mirrored.
7. Save, then "Update now" and check there is no error.

Caution: once the mirror is on, GitLab is the source; pushes made directly to
GitHub (including PR merges) will be overwritten unless they also reach GitLab.
