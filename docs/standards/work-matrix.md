# Work Matrix

`docs/WORK.md` is the committed list of what is being worked on, what is
queued and what is finished. Shepherd's work view parses it, so the format is
strict. Prose around the table is free; the table is not.

## Format

`docs/WORK.md` holds exactly **one** markdown table, directly under a
`## Work` heading, with exactly these columns in this order:

```
| ID | Area | Item | Status | Lane | PR | Notes |
|----|------|------|--------|------|----|-------|
```

| Column | Content |
|--------|---------|
| `ID` | Stable short id, `W-NNN` (zero-padded, three digits or more). Unique; **never reused**, even for a dropped row. |
| `Area` | Short subsystem label, e.g. `kernel`, `vmm`, `docs`, `ci`, `userland`, `identity`. Free text, no `\|`. |
| `Item` | One line saying what the work is. |
| `Status` | Exactly one of `todo`, `doing`, `blocked`, `done`, `dropped`. |
| `Lane` | The Shepherd lane working it, or `-` when no lane has it. |
| `PR` | `#N` for the pull request that carries or closed it, or `-`. One number only; put others in Notes. |
| `Notes` | Free text. **Required for `blocked`**: say what it waits on. Say `status unclear` when the status is a guess. |

Cells may not contain a literal `|` (use `/` or reword) and a row is a single
line.

Below the table an optional `## Notes` section holds prose. The parser ignores
everything outside the `## Work` table.

### Status meanings

| Status | Meaning |
|--------|---------|
| `todo` | Real, not started. No lane owns it. |
| `doing` | A lane is actively working it. |
| `blocked` | Cannot proceed; Notes says why. |
| `done` | Landed on master. `PR` names the PR where there is one. |
| `dropped` | Decided against, or superseded. Notes says why. |

## Rules

1. **Every lane that works an item updates its row in the same PR.** Status,
   Lane and PR change with the work, not afterwards.
2. **New work gets a row before, or with, its first PR.** Take the next free
   `W-NNN`.
3. **Rows are never deleted.** Finished work is `done`; abandoned work is
   `dropped`. IDs therefore stay valid forever.
4. Keep rows to real items. Do not add speculative or aspirational entries;
   ideas under discussion are `todo` with `discussion` in Notes.
5. `docs/WORK.md` records status only. Design stays in `docs/design/`, bugs in
   `BUGS.md`; a row links to them in Notes rather than copying them.

The reviewer checks that the PR's row is current.
