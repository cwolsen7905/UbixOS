# CI (GitLab)

GitLab (`gitlab.brainchurts.com/ubixos/ubixos`) is the source of truth; the
pipeline is [`.gitlab-ci.yml`](../.gitlab-ci.yml). It replaces
`.github/workflows/ci.yml` (kept until the GitLab jobs have all gone green).
The pipeline runs the same entry point you run locally, `tools/check/run.sh`
([standards](standards/README.md)).

Pipelines run on merge requests, on `master` and on tags.

## Jobs

| Job | Stage | Runs | Blocks? |
|-----|-------|------|---------|
| `toolchain` | toolchain | builds `.gitlab/Dockerfile` and pushes it to the project registry if that hash is not already there | yes |
| `lint` | lint | `tools/check/selftest.sh`; fast gates on files changed vs the MR merge-base; whole-tree `banned-api` and `syscall-table` ratchets | yes |
| `build` / `build:keep` | build | `bmake kernel world` for **aarch64 and x86_64** (matrix); kernel is kept as an artifact | yes |
| `analyze` | analyze | `header-selfcontained warnings clang-tidy cppcheck` on changed files | no (`allow_failure`) |
| `boot` / `boot:keep` | boot | `bmake kernel world image`, then `tools/check/run.sh --strict --arch <arch> boot-smoke` (QEMU TCG, headless) for both arches; image and kernel kept as artifacts | no (`allow_failure`) |

`allow_failure: true` marks a job not yet proven on these runners (as
`continue-on-error` did on GitHub). Drop it once the job has passed.

`build`/`boot` come in two flavours because `artifacts:expire_in` cannot take a
variable: MR pipelines (`build`, `boot`) keep artifacts **1 week**; `master`
and tags (`build:keep`, `boot:keep`) keep them **4 weeks**.

All jobs are `interruptible`: a new push cancels the superseded pipeline.

## Runners and image

| Runner | Executor | Tags | Used for |
|--------|----------|------|----------|
| kube-special (k8s executor) | Kubernetes, amd64 | `k8s` | every job except `toolchain`, inside the toolchain image |
| kube-special (shell executor) | shell with docker + buildx, amd64 | `shell` | `toolchain` only (needs a docker daemon) |

Both are instance runners with `run_untagged: false`, so every job names a tag.
Neither is arm64, so aarch64 is **cross-built** on amd64 with the distro
`gcc-aarch64-linux-gnu`, and aarch64 boot is QEMU TCG emulation. TCG needs no
KVM and no privileged mode.

The toolchain image is `registry.../ubixos/ubixos/ci:<hash of .gitlab/Dockerfile>`
(Ubuntu 24.04 with bmake, clang/lld/llvm 18, the cross gcc, clang-format,
clang-tidy, cppcheck, shellcheck, mtools, QEMU, libpng/bison/flex). Edit the
Dockerfile and the next pipeline builds a new tag automatically.

`tools/ci/` holds the helpers: `crossprefix.sh ARCH` (the `CROSS_PREFIX` the
distro compilers need) and `lint-base.sh` (merge-base for the gates).

## Reproduce locally

```sh
tools/check/selftest.sh
tools/check/run.sh --tier fast                       # lint
bmake kernel world TARGET=aarch64                    # build (macOS: brew toolchain, no CROSS_PREFIX)
bmake kernel world TARGET=x86_64
tools/check/run.sh --tier ci header-selfcontained warnings clang-tidy cppcheck
bmake kernel world image TARGET=aarch64 && tools/check/run.sh --strict --arch aarch64 boot-smoke
```

To use the exact CI environment, run the image:

```sh
docker run --rm -it --platform linux/amd64 -v "$PWD":/src -w /src \
    $CI_REGISTRY_IMAGE/ci:<tag> sh -c \
    'bmake kernel world TARGET=aarch64 CROSS_PREFIX=$(tools/ci/crossprefix.sh aarch64)'
```

or build it yourself: `docker build -t ubixos-ci -f .gitlab/Dockerfile .gitlab`.
