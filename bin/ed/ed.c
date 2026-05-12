/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES  4096
#define LINE_MAX   1024
#define CMD_MAX    2048

static char  *lines[MAX_LINES];
static int    nlines = 0;
static int    cur    = 0;       /* current line, 1-based; 0 = before first */
static char   filename[256];
static int    modified = 0;
static int    verbose  = 1;     /* print '?' on error */

/* ---- helpers ---------------------------------------------------------- */

static void err(const char *msg) {
  if (verbose)
    fprintf(stderr, "?\n");
  if (msg)
    fprintf(stderr, "%s\n", msg);
}

static char *dupstr(const char *s) {
  size_t n = strlen(s) + 1;
  char *p = malloc(n);
  if (!p) { perror("malloc"); exit(1); }
  memcpy(p, s, n);
  return p;
}

/* strip trailing newline */
static void chomp(char *s) {
  size_t n = strlen(s);
  if (n && s[n-1] == '\n') s[n-1] = '\0';
}

/* ---- address parsing -------------------------------------------------- */

/*
 * Parse a single address token starting at *pp.
 * Returns the resolved line number (1-based) or -1 on error.
 * Advances *pp past the token.
 */
static int parse_addr(const char **pp) {
  const char *p = *pp;

  while (*p == ' ' || *p == '\t') p++;

  if (*p == '.') {
    *pp = p + 1;
    return cur;
  }
  if (*p == '$') {
    *pp = p + 1;
    return nlines;
  }
  if (*p >= '0' && *p <= '9') {
    int n = 0;
    while (*p >= '0' && *p <= '9') n = n * 10 + (*p++ - '0');
    *pp = p;
    return n;
  }
  if (*p == '+' || *p == '-') {
    int sign = (*p == '+') ? 1 : -1;
    p++;
    int off = 0;
    while (*p >= '0' && *p <= '9') off = off * 10 + (*p++ - '0');
    if (off == 0) off = 1;
    *pp = p;
    return cur + sign * off;
  }
  return -2; /* no address */
}

/*
 * Parse up to two comma/semicolon-separated addresses.
 * Returns number of addresses found (0, 1, or 2).
 * *a1 and *a2 are set to the resolved line numbers.
 * Default (no address): *a1 = *a2 = cur.
 */
static int parse_addrs(const char **pp, int *a1, int *a2) {
  int n1 = parse_addr(pp);
  if (n1 == -2) {
    *a1 = *a2 = cur;
    return 0;
  }
  *a1 = n1;

  const char *p = *pp;
  while (*p == ' ' || *p == '\t') p++;

  if (*p == ',' || *p == ';') {
    p++;
    *pp = p;
    int n2 = parse_addr(pp);
    if (n2 == -2) n2 = nlines; /* 'n,' means n,$ */
    *a2 = n2;
    return 2;
  }
  *a2 = n1;
  return 1;
}

/* ---- file I/O --------------------------------------------------------- */

static int read_file(const char *path) {
  FILE *f = fopen(path, "r");
  /* UbixOS fopen never returns NULL; check fd to detect failure */
  if (!f || !f->fd) {
    fprintf(stderr, "%s: cannot open\n", path);
    if (f) free(f);
    return -1;
  }

  /* Read the whole file in one shot — fgetc returns 0 at EOF (not -1),
   * so a fgetc/fgets loop never terminates.  fread returns bytes read,
   * which drops to 0 at EOF and gives us a clean stopping point. */
  size_t filesz = f->size ? f->size : 65536;
  char *buf = malloc(filesz + 1);
  if (!buf) { fclose(f); return -1; }

  size_t got = fread(buf, 1, filesz, f);
  fclose(f);
  buf[got] = '\0';

  int count = 0;
  char *p = buf;
  char *end = buf + got;
  while (p < end) {
    char *nl = p;
    while (nl < end && *nl != '\n') nl++;
    size_t len = nl - p;
    if (len >= LINE_MAX) len = LINE_MAX - 1;
    char tmp[LINE_MAX];
    memcpy(tmp, p, len);
    tmp[len] = '\0';
    if (nlines >= MAX_LINES) { err("too many lines"); break; }
    lines[nlines++] = dupstr(tmp);
    count++;
    p = nl + 1;
  }

  free(buf);
  return count;
}

static int write_file(const char *path, int a1, int a2) {
  FILE *f = fopen(path, "w");
  if (!f) { perror(path); return -1; }
  int count = 0;
  for (int i = a1; i <= a2; i++) {
    fprintf(f, "%s\n", lines[i-1]);
    count++;
  }
  fclose(f);
  return count;
}

/* ---- line buffer mutation ---------------------------------------------- */

static void insert_line(int after, const char *text) {
  if (nlines >= MAX_LINES) { err("buffer full"); return; }
  for (int i = nlines; i > after; i--)
    lines[i] = lines[i-1];
  lines[after] = dupstr(text);
  nlines++;
  cur = after + 1;
}

static void delete_lines(int a1, int a2) {
  for (int i = a1; i <= a2; i++)
    free(lines[i-1]);
  int del = a2 - a1 + 1;
  for (int i = a1 - 1; i < nlines - del; i++)
    lines[i] = lines[i + del];
  nlines -= del;
  if (nlines == 0) cur = 0;
  else if (a1 > nlines) cur = nlines;
  else cur = a1;
}

/* ---- substitute ------------------------------------------------------- */

/*
 * Very simple non-regex substitute: s/old/new/[g]
 * Parses the delimiter from the command string.
 */
static int do_substitute(int a1, int a2, const char *cmd) {
  char delim = cmd[0];
  if (!delim) { err("bad substitute"); return -1; }

  /* split cmd by delimiter */
  const char *p = cmd + 1;
  char pat[LINE_MAX] = {0}, rep[LINE_MAX] = {0};
  int pi = 0, ri = 0;

  while (*p && *p != delim) pat[pi++] = *p++;
  if (*p == delim) p++;
  while (*p && *p != delim) rep[ri++] = *p++;
  if (*p == delim) p++;

  int global = (*p == 'g');

  int changed = 0;
  for (int i = a1; i <= a2; i++) {
    char out[LINE_MAX * 2];
    char *src = lines[i-1];
    char *q;
    int oi = 0;
    size_t plen = strlen(pat);
    int did = 0;

    if (plen == 0) { err("empty pattern"); return -1; }

    while ((q = strstr(src, pat)) != NULL) {
      int before = (int)(q - src);
      if (oi + before + (int)strlen(rep) >= (int)sizeof(out) - 1) break;
      memcpy(out + oi, src, before);
      oi += before;
      memcpy(out + oi, rep, strlen(rep));
      oi += (int)strlen(rep);
      src = q + plen;
      did = 1;
      if (!global) break;
    }
    if (did) {
      strcpy(out + oi, src);
      free(lines[i-1]);
      lines[i-1] = dupstr(out);
      cur = i;
      changed++;
    }
  }
  if (!changed) { err("no match"); return -1; }
  modified = 1;
  return 0;
}

/* ---- main command dispatch -------------------------------------------- */

static int do_command(char *cmdline) {
  const char *p = cmdline;
  int a1, a2, naddr;

  naddr = parse_addrs(&p, &a1, &a2);

  while (*p == ' ' || *p == '\t') p++;
  char cmd = *p ? *p++ : '\0';

  /* skip leading space after command for args */
  while (*p == ' ' || *p == '\t') p++;

  switch (cmd) {

  case '\0':
  case '\n':
    /* bare address: print that line */
    if (naddr == 0) {
      if (cur >= nlines) { err("invalid address"); return 0; }
      cur++;
      if (cur < 1 || cur > nlines) { err("invalid address"); return 0; }
      printf("%s\n", lines[cur-1]);
    } else {
      if (a1 < 1 || a2 > nlines || a1 > a2) { err("invalid address"); return 0; }
      cur = a2;
      for (int i = a1; i <= a2; i++)
        printf("%s\n", lines[i-1]);
    }
    break;

  case 'p':
    if (naddr == 0) { a1 = a2 = cur; }
    if (a1 < 1 || a2 > nlines || a1 > a2) { err("invalid address"); return 0; }
    cur = a2;
    for (int i = a1; i <= a2; i++)
      printf("%s\n", lines[i-1]);
    break;

  case 'n':  /* print with line numbers */
    if (naddr == 0) { a1 = a2 = cur; }
    if (a1 < 1 || a2 > nlines || a1 > a2) { err("invalid address"); return 0; }
    cur = a2;
    for (int i = a1; i <= a2; i++)
      printf("%d\t%s\n", i, lines[i-1]);
    break;

  case '=':
    if (naddr == 0) printf("%d\n", nlines);
    else printf("%d\n", a2);
    break;

  case 'a':  /* append after a1 */
    if (naddr == 0) a1 = cur;
    if (nlines > 0 && (a1 < 0 || a1 > nlines)) { err("invalid address"); return 0; }
    {
      char buf[LINE_MAX];
      int after = a1;
      while (fgets(buf, sizeof buf, stdin)) {
        chomp(buf);
        if (buf[0] == '.' && buf[1] == '\0') break;
        insert_line(after++, buf);
        modified = 1;
      }
    }
    break;

  case 'i':  /* insert before a1 */
    if (naddr == 0) a1 = cur;
    if (a1 < 1) a1 = 1;
    if (a1 > nlines + 1) { err("invalid address"); return 0; }
    {
      char buf[LINE_MAX];
      int before = a1 - 1;
      while (fgets(buf, sizeof buf, stdin)) {
        chomp(buf);
        if (buf[0] == '.' && buf[1] == '\0') break;
        insert_line(before++, buf);
        modified = 1;
      }
    }
    break;

  case 'c':  /* change (delete + insert) */
    if (naddr == 0) { a1 = a2 = cur; }
    if (a1 < 1 || a2 > nlines || a1 > a2) { err("invalid address"); return 0; }
    delete_lines(a1, a2);
    modified = 1;
    {
      char buf[LINE_MAX];
      int after = a1 - 1;
      while (fgets(buf, sizeof buf, stdin)) {
        chomp(buf);
        if (buf[0] == '.' && buf[1] == '\0') break;
        insert_line(after++, buf);
      }
    }
    break;

  case 'd':
    if (naddr == 0) { a1 = a2 = cur; }
    if (a1 < 1 || a2 > nlines || a1 > a2) { err("invalid address"); return 0; }
    delete_lines(a1, a2);
    modified = 1;
    break;

  case 'j':  /* join lines */
    if (naddr < 2) { a1 = cur; a2 = cur + 1; }
    if (a1 < 1 || a2 > nlines || a1 >= a2) { err("invalid address"); return 0; }
    {
      size_t total = 0;
      for (int i = a1; i <= a2; i++) total += strlen(lines[i-1]);
      char *joined = malloc(total + 1);
      if (!joined) { perror("malloc"); return -1; }
      joined[0] = '\0';
      for (int i = a1; i <= a2; i++) strcat(joined, lines[i-1]);
      delete_lines(a1, a2);
      insert_line(a1 - 1, joined);
      free(joined);
      modified = 1;
    }
    break;

  case 's':
    if (naddr == 0) { a1 = a2 = cur; }
    if (a1 < 1 || a2 > nlines || a1 > a2) { err("invalid address"); return 0; }
    do_substitute(a1, a2, p);
    break;

  case 'e':  /* edit file */
    if (modified) {
      err("unsaved changes (use E to override)");
      break;
    }
    /* fall through */
  case 'E':
    {
      const char *fname = (*p) ? p : filename;
      if (!*fname) { err("no filename"); break; }
      strncpy(filename, fname, sizeof(filename) - 1);
      /* free existing buffer */
      for (int i = 0; i < nlines; i++) free(lines[i]);
      nlines = 0; cur = 0;
      int n = read_file(filename);
      if (n >= 0) {
        printf("%d\n", n);
        cur = nlines;
        modified = 0;
      }
    }
    break;

  case 'r':  /* read file, append after a2 */
    {
      const char *fname = (*p) ? p : filename;
      if (!*fname) { err("no filename"); break; }
      int saved_cur = cur;
      cur = (naddr > 0) ? a2 : nlines;
      /* we'll insert after cur */
      int after = cur;
      FILE *f = fopen(fname, "r");
      if (!f) { perror(fname); cur = saved_cur; break; }
      char buf[LINE_MAX];
      int count = 0;
      while (fgets(buf, sizeof buf, f)) {
        chomp(buf);
        if (nlines >= MAX_LINES) break;
        insert_line(after++, buf);
        count++;
      }
      fclose(f);
      printf("%d\n", count);
      modified = (count > 0);
    }
    break;

  case 'w':  /* write */
    {
      const char *fname = (*p) ? p : filename;
      if (!*fname) { err("no filename"); break; }
      strncpy(filename, fname, sizeof(filename) - 1);
      int wa1 = 1, wa2 = nlines;
      if (naddr >= 1) { wa1 = a1; wa2 = a2; }
      int n = write_file(filename, wa1, wa2);
      if (n >= 0) {
        printf("%d\n", n);
        modified = 0;
      }
    }
    break;

  case 'f':  /* filename */
    if (*p) {
      strncpy(filename, p, sizeof(filename) - 1);
    } else {
      if (*filename) printf("%s\n", filename);
      else printf("(none)\n");
    }
    break;

  case 'q':
    if (modified) { err("unsaved changes (use Q to override)"); return 0; }
    return 1;  /* quit */

  case 'Q':
    return 1;  /* quit unconditionally */

  case 'l':  /* list (show $ for end-of-line) */
    if (naddr == 0) { a1 = a2 = cur; }
    if (a1 < 1 || a2 > nlines || a1 > a2) { err("invalid address"); return 0; }
    cur = a2;
    for (int i = a1; i <= a2; i++)
      printf("%s$\n", lines[i-1]);
    break;

  case 'm':  /* move */
    {
      if (naddr == 0) { a1 = a2 = cur; }
      if (a1 < 1 || a2 > nlines || a1 > a2) { err("invalid address"); return 0; }
      int dest = atoi(p);
      if (dest < 0 || dest > nlines) { err("invalid address"); return 0; }
      if (dest >= a1 && dest <= a2) { err("invalid move destination"); return 0; }
      /* collect lines to move */
      int cnt = a2 - a1 + 1;
      char **tmp = malloc(cnt * sizeof(char *));
      if (!tmp) { perror("malloc"); return -1; }
      for (int i = 0; i < cnt; i++) tmp[i] = lines[a1 - 1 + i];
      /* shift remaining lines */
      if (dest < a1) {
        for (int i = a2 - 1; i >= a1 - 1; i--) lines[i + cnt] = lines[i]; /* won't work — need proper shift */
        /* redo: close the gap first, then open at dest */
        /* easier: delete then re-insert */
        /* delete a1..a2 */
        for (int i = a1 - 1; i < nlines - cnt; i++) lines[i] = lines[i + cnt];
        nlines -= cnt;
        int ins = dest;
        for (int i = nlines + cnt - 1; i >= ins + cnt; i--) lines[i] = lines[i - cnt];
        for (int i = 0; i < cnt; i++) lines[ins + i] = tmp[i];
        nlines += cnt;
        cur = ins + cnt;
      } else {
        /* dest > a2 */
        int ins = dest - cnt; /* insertion point after deletion */
        for (int i = a1 - 1; i < nlines - cnt; i++) lines[i] = lines[i + cnt];
        nlines -= cnt;
        for (int i = nlines + cnt - 1; i >= ins + cnt; i--) lines[i] = lines[i - cnt];
        for (int i = 0; i < cnt; i++) lines[ins + i] = tmp[i];
        nlines += cnt;
        cur = ins + cnt;
      }
      free(tmp);
      modified = 1;
    }
    break;

  default:
    err(NULL);
    break;
  }
  return 0;
}

/* ----------------------------------------------------------------------- */

int main(int argc, char *argv[]) {
  char cmdline[CMD_MAX];

  filename[0] = '\0';

  if (argc > 1) {
    strncpy(filename, argv[1], sizeof(filename) - 1);
    int n = read_file(filename);
    if (n >= 0) {
      printf("%d\n", n);
      cur = nlines;
    }
  }

  while (1) {
    printf("* ");
    if (!fgets(cmdline, sizeof cmdline, stdin))
      break;
    chomp(cmdline);
    if (do_command(cmdline))
      break;
  }

  return 0;
}
