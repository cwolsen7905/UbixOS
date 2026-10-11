# Definition of Done

A change is **done** only when all of the following are true and the author
can show the evidence.

1. **Builds clean on both architectures.**
   `tools/buildlock.sh with "<who>" bmake kernel world TARGET=aarch64` and
   `... TARGET=x86_64` succeed with no new warnings. (The lock is mandatory
   for whole-tree builds when other sessions may share the tree; per-directory
   `bmake -C <dir>` builds do not need it.)
2. **Gates pass.** `tools/check/run.sh --changed` exits 0. A gate that is
   advisory (clang-tidy, cppcheck, scan-build) shows no *new* findings versus
   its baseline.
3. **Boots.** Both arches boot headless to the login prompt with no panic
   in the serial log (`tools/check/run.sh boot-smoke`, or the manual
   equivalent on a **copy** of the image). The person's running QEMU window and
   `ubixos*.img` are never touched.
4. **The change is exercised.** The new behaviour *and* a failure path were
   run; the command and observed output are in the commit message or PR.
5. **Docs in the same commit.** CONTRIBUTING.md sync table, `CHANGELOG.md`
   `[Unreleased]`, Doxygen blocks, copyright year.
6. **Reviewed against `review-checklist.md`**, with deviations explained.
7. **Not committed to master.** Work is on a feature branch, pushed, with a PR
   opened against `master`. The person merges. Nothing is committed on a
   subsystem's behalf until the person confirms the change works.

What "done" is *not*: "it compiled", "it booted once", "the agent said so".
