# MR !4 review-thread triage

MR: [!4 ci: add GitLab pipeline (toolchain image, lint, build, analyze, boot)](https://gitlab.brainchurts.com/ubixos/ubixos/-/merge_requests/4)
Source branch: `ci/gitlab-pipeline` -> `master`. State: open. Author/assignee: cwolsen.
Read-only `glab api` queries, checked 2026-10-17.

## Finding: there are no review threads

- `GET /merge_requests/4/discussions` returns 4 discussions. All 4 are system notes, none resolvable:
  [assigned to @cwolsen](https://gitlab.brainchurts.com/ubixos/ubixos/-/merge_requests/4#note_7223),
  and "added 1 commit" for
  [b4ea1969](https://gitlab.brainchurts.com/ubixos/ubixos/-/merge_requests/4#note_7230),
  [395d5ed3](https://gitlab.brainchurts.com/ubixos/ubixos/-/merge_requests/4#note_7233) and
  [9d79b39e](https://gitlab.brainchurts.com/ubixos/ubixos/-/merge_requests/4#note_7234).
- `user_notes_count` is 0 and `blocking_discussions_resolved` is true.
- Counts by verdict: fix 0, reply/won't fix 0, needs the person 0.

So the "lots of issues" are not in review threads. Nothing to group or order.

## Pipeline state (what the issues may actually be)

Head pipeline [#8405](https://gitlab.brainchurts.com/ubixos/ubixos/-/pipelines/8405) on 9d79b39e, status **running** at check time
(no failures yet):

| Job | Status | Blocks? |
|-----|--------|---------|
| toolchain | success | yes |
| lint | success | yes |
| analyze | success | no (allow_failure) |
| build: [aarch64] | running | yes |
| build: [x86_64] | running | yes |
| boot: [aarch64] | running | no (allow_failure) |
| boot: [x86_64] | running | no (allow_failure) |

Previous pipeline [#8404](https://gitlab.brainchurts.com/ubixos/ubixos/-/pipelines/8404) (395d5ed3) had the same job set, also running.
The three follow-up commits (log cap, `quiet.sh` exit status, build-log artifacts) suggest earlier runs hit job-log and exit-status problems.

## Suggested next step

Ask the person where the issues are: possibly an earlier failed pipeline, comments on another MR, or problems in the build/boot job logs once #8405 finishes.
Re-run this triage once the build and boot jobs finish, or if threads get added.
