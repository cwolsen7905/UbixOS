# ed — Line Editor

A minimal line-oriented text editor for UbixOS, modelled after POSIX `ed`.

## Usage

```
ed [file]
```

If a filename is given, it is loaded into the buffer and the line count is printed.
`ed` then reads commands from standard input, one per line, displaying a `*` prompt.

## Addresses

Most commands accept an address or address range specifying which lines to act on.

| Address | Meaning |
|---------|---------|
| `n` | Line number *n* (1-based) |
| `.` | Current line |
| `$` | Last line in buffer |
| `+n` | *n* lines after current (default: `+1`) |
| `-n` | *n* lines before current (default: `-1`) |
| `a1,a2` | Lines *a1* through *a2* (inclusive) |
| `a1;a2` | Same as comma form |

When no address is given, each command uses its own default (usually the current line).

## Commands

### Navigation and printing

| Command | Default addr | Description |
|---------|-------------|-------------|
| `p` | `.` | Print addressed line(s) |
| `n` | `.` | Print addressed line(s) with line numbers |
| `l` | `.` | Print addressed line(s), marking end-of-line with `$` |
| `=` | `$` | Print the addressed line number (or total line count when no address given) |

Entering just a line address (e.g. `5`, `.`, `$`) prints that line and makes it current.

### Editing

| Command | Default addr | Description |
|---------|-------------|-------------|
| `a` | `.` | Append lines after addressed line; enter text, end with `.` on its own line |
| `i` | `.` | Insert lines before addressed line; enter text, end with `.` on its own line |
| `c` | `.` | Change (replace) addressed lines; enter text, end with `.` on its own line |
| `d` | `.` | Delete addressed lines |
| `j` | `.,+1` | Join addressed lines into one |
| `m dest` | `.` | Move addressed lines to after line *dest* |
| `s/pat/rep/` | `.` | Replace first occurrence of literal *pat* with *rep* |
| `s/pat/rep/g` | `.` | Replace all occurrences (global flag) |

The substitute delimiter can be any character (shown as `/` above).

### File operations

| Command | Description |
|---------|-------------|
| `e file` | Edit *file* — clears buffer and loads file (warns if unsaved changes) |
| `E file` | Edit *file* unconditionally (discards unsaved changes) |
| `r file` | Read *file* and insert its contents after the addressed line |
| `w [file]` | Write buffer to *file* (defaults to current filename) |
| `f [file]` | Print or set the current filename |

### Quitting

| Command | Description |
|---------|-------------|
| `q` | Quit (warns if there are unsaved changes) |
| `Q` | Quit unconditionally |

## Examples

Create a new file:

```
ed hello.txt
a
Hello, world!
.
w
q
```

Edit an existing file, change line 3, save:

```
ed /etc/motd
3p
3c
Welcome to UbixOS.
.
w
q
```

Substitute on a range:

```
1,$s/foo/bar/g
w
q
```

Print the whole file with line numbers:

```
1,$n
```

## Limits

| Item | Limit |
|------|-------|
| Lines per buffer | 4096 |
| Characters per line | 1023 |
| Command line length | 2047 |

## Notes

- Substitute uses **literal string matching**, not regular expressions.
- `?` is printed to stderr on any error; a descriptive message follows when available.
- The editor tracks a *modified* flag; `q` and `e` refuse to proceed if there are unsaved
  changes (use `Q` or `E` to override).
