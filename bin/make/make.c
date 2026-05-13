/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
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

/*
 * Minimal POSIX-subset make for UbixOS.
 *
 * Supports:
 *   VAR = value        (recursively expanded)
 *   VAR := value       (simply expanded)
 *   target: deps       (explicit rules)
 *   .PHONY: targets
 *   Suffix rules: .c.o  .S.o
 *   Automatic variables in recipes: $@  $<  $^
 *   include file
 *   -f makefile  -n (dry-run)  -s (silent)
 *
 * Does NOT support: pattern rules (%), order-only deps, archives,
 * multi-line variable assignments, $(shell ...), conditionals.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

/* ------------------------------------------------------------------ limits */
#define MAX_VARS     256
#define MAX_RULES    512
#define MAX_DEPS     64
#define MAX_RECIPES  64
#define MAX_PHONY    64
#define LINE_MAX_LEN 4096

/* ------------------------------------------------------------------ types */
typedef struct {
    char *name;
    char *value;
    int   simple;   /* 1 = := (already expanded at assignment time) */
} Var;

typedef struct {
    char *target;
    char *deps[MAX_DEPS];
    int   ndeps;
    char *recipes[MAX_RECIPES];
    int   nrecipes;
    int   is_suffix; /* .c.o style */
} Rule;

/* ------------------------------------------------------------------ globals */
static Var   vars[MAX_VARS];
static int   nvars;
static Rule  rules[MAX_RULES];
static int   nrules;
static char *phony[MAX_PHONY];
static int   nphony;
static int   dry_run;
static int   silent;
static char *first_goal;   /* first non-special target in the file */

/* ------------------------------------------------------------------ var ops */
static char *var_get(const char *name) {
    int i;
    for (i = 0; i < nvars; i++)
        if (strcmp(vars[i].name, name) == 0)
            return vars[i].value;
    return getenv(name);
}

static void var_set(const char *name, const char *value, int simple) {
    int i;
    for (i = 0; i < nvars; i++) {
        if (strcmp(vars[i].name, name) == 0) {
            free(vars[i].value);
            vars[i].value = strdup(value);
            vars[i].simple = simple;
            return;
        }
    }
    if (nvars >= MAX_VARS) { fprintf(stderr, "make: too many variables\n"); exit(1); }
    vars[nvars].name   = strdup(name);
    vars[nvars].value  = strdup(value);
    vars[nvars].simple = simple;
    nvars++;
}

/* Expand $(VAR) and ${VAR} references in src into buf (max buflen bytes). */
static void expand(const char *src, char *buf, int buflen) {
    const char *p = src;
    char *out = buf;
    char *end = buf + buflen - 1;

    while (*p && out < end) {
        if (p[0] == '$' && p[1] == '(') {
            const char *close = strchr(p + 2, ')');
            if (!close) { *out++ = *p++; continue; }
            char vname[256];
            int  vlen = (int)(close - (p + 2));
            if (vlen >= (int)sizeof(vname)) vlen = (int)sizeof(vname) - 1;
            memcpy(vname, p + 2, vlen);
            vname[vlen] = '\0';
            const char *val = var_get(vname);
            if (val) {
                int room = (int)(end - out);
                int vl   = (int)strlen(val);
                if (vl > room) vl = room;
                memcpy(out, val, vl);
                out += vl;
            }
            p = close + 1;
        } else if (p[0] == '$' && p[1] == '{') {
            const char *close = strchr(p + 2, '}');
            if (!close) { *out++ = *p++; continue; }
            char vname[256];
            int  vlen = (int)(close - (p + 2));
            if (vlen >= (int)sizeof(vname)) vlen = (int)sizeof(vname) - 1;
            memcpy(vname, p + 2, vlen);
            vname[vlen] = '\0';
            const char *val = var_get(vname);
            if (val) {
                int room = (int)(end - out);
                int vl   = (int)strlen(val);
                if (vl > room) vl = room;
                memcpy(out, val, vl);
                out += vl;
            }
            p = close + 1;
        } else {
            *out++ = *p++;
        }
    }
    *out = '\0';
}

/* ------------------------------------------------------------------ rule ops */
static Rule *rule_find(const char *target) {
    int i;
    for (i = 0; i < nrules; i++)
        if (!rules[i].is_suffix && strcmp(rules[i].target, target) == 0)
            return &rules[i];
    return NULL;
}

static Rule *suffix_find(const char *from, const char *to) {
    char key[64];
    snprintf(key, sizeof(key), "%s%s", from, to);
    int i;
    for (i = 0; i < nrules; i++)
        if (rules[i].is_suffix && strcmp(rules[i].target, key) == 0)
            return &rules[i];
    return NULL;
}

static int is_phony(const char *t) {
    int i;
    for (i = 0; i < nphony; i++)
        if (strcmp(phony[i], t) == 0)
            return 1;
    return 0;
}

/* ------------------------------------------------------------------ mtime */
static time_t file_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) < 0)
        return (time_t)-1;
    return st.st_mtime;
}

/* ------------------------------------------------------------------ exec */
static int run_recipe(const char *recipe, const char *target,
                      const char *first_dep, const char *all_deps) {
    /* expand automatic variables: $@ $< $^ */
    char step1[LINE_MAX_LEN], step2[LINE_MAX_LEN];
    const char *p = recipe;
    char *out = step1;
    char *e   = step1 + sizeof(step1) - 1;
    int   echo = 1;

    if (*p == '@') { echo = 0; p++; }
    if (*p == '-') { p++; } /* ignore errors — we still print them */

    while (*p && out < e) {
        if (*p == '$' && *(p+1) == '@') {
            int l = strlen(target); if (l > e - out) l = e - out;
            memcpy(out, target, l); out += l; p += 2;
        } else if (*p == '$' && *(p+1) == '<') {
            const char *s = first_dep ? first_dep : "";
            int l = strlen(s); if (l > e - out) l = e - out;
            memcpy(out, s, l); out += l; p += 2;
        } else if (*p == '$' && *(p+1) == '^') {
            const char *s = all_deps ? all_deps : "";
            int l = strlen(s); if (l > e - out) l = e - out;
            memcpy(out, s, l); out += l; p += 2;
        } else {
            *out++ = *p++;
        }
    }
    *out = '\0';

    expand(step1, step2, sizeof(step2));

    if (!silent && echo)
        printf("%s\n", step2);

    if (dry_run)
        return 0;

    pid_t pid = fork();
    if (pid < 0) { perror("make: fork"); return 1; }
    if (pid == 0) {
        char *argv[] = { "shell", "-c", step2, NULL };
        execve("sys:/bin/shell", argv, environ);
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return 1;
}

/* ------------------------------------------------------------------ build */
/* visited bitmap: index into rules[] */
static int built[MAX_RULES];

static int build(const char *target) {
    int i;

    /* find explicit rule */
    Rule *r = rule_find(target);

    /* if no explicit rule, try suffix rules (.c.o, .S.o) */
    if (!r) {
        const char *suffixes[] = { ".c", ".S", ".cc", ".cpp", NULL };
        const char *dot = strrchr(target, '.');
        if (dot) {
            const char **sf;
            for (sf = suffixes; *sf; sf++) {
                Rule *sr = suffix_find(*sf, dot);
                if (!sr) continue;
                /* synthesize source filename */
                char src[512];
                int  plen = (int)(dot - target);
                memcpy(src, target, plen);
                strcpy(src + plen, *sf);
                if (file_mtime(src) == (time_t)-1)
                    continue; /* source doesn't exist */
                /* build with this suffix rule */
                if (!silent)
                    ; /* recipe will echo */
                char all_deps[512];
                snprintf(all_deps, sizeof(all_deps), "%s", src);
                for (i = 0; i < sr->nrecipes; i++) {
                    int ret = run_recipe(sr->recipes[i], target, src, all_deps);
                    if (ret != 0) {
                        fprintf(stderr, "make: *** [%s] Error %d\n", target, ret);
                        return 1;
                    }
                }
                return 0;
            }
        }
        /* no rule and no suffix match — check if file exists */
        if (file_mtime(target) != (time_t)-1)
            return 0; /* exists, nothing to do */
        fprintf(stderr, "make: *** No rule to make target '%s'.\n", target);
        return 1;
    }

    /* mark visited (index in rules[]) to avoid cycles */
    int ridx = (int)(r - rules);
    if (built[ridx])
        return 0;
    built[ridx] = 1;

    /* build deps first */
    for (i = 0; i < r->ndeps; i++) {
        char edep[LINE_MAX_LEN];
        expand(r->deps[i], edep, sizeof(edep));
        if (build(edep) != 0)
            return 1;
    }

    /* decide whether to run recipes */
    int phony_target = is_phony(target);
    int need_build   = phony_target;

    if (!need_build) {
        time_t tmtime = file_mtime(target);
        if (tmtime == (time_t)-1) {
            need_build = 1;
        } else {
            for (i = 0; i < r->ndeps; i++) {
                char edep[LINE_MAX_LEN];
                expand(r->deps[i], edep, sizeof(edep));
                time_t dmtime = file_mtime(edep);
                if (dmtime == (time_t)-1 || dmtime > tmtime) {
                    need_build = 1;
                    break;
                }
            }
        }
    }

    if (!need_build) {
        if (r->nrecipes > 0 && !silent)
            printf("make: '%s' is up to date.\n", target);
        return 0;
    }

    /* build all-deps string for $^ */
    char all_deps[LINE_MAX_LEN];
    all_deps[0] = '\0';
    for (i = 0; i < r->ndeps; i++) {
        char edep[LINE_MAX_LEN];
        expand(r->deps[i], edep, sizeof(edep));
        if (i) strncat(all_deps, " ", sizeof(all_deps) - strlen(all_deps) - 1);
        strncat(all_deps, edep, sizeof(all_deps) - strlen(all_deps) - 1);
    }

    const char *first_dep = r->ndeps > 0 ? r->deps[0] : NULL;
    char efirst[LINE_MAX_LEN];
    if (first_dep) { expand(first_dep, efirst, sizeof(efirst)); first_dep = efirst; }

    for (i = 0; i < r->nrecipes; i++) {
        int ret = run_recipe(r->recipes[i], target, first_dep, all_deps);
        if (ret != 0) {
            fprintf(stderr, "make: *** [%s] Error %d\n", target, ret);
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ parse */
static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r'))
        *--e = '\0';
    return s;
}

static void parse_file(const char *path);

static void parse_line(char *line, Rule **cur) {
    /* skip comments */
    char *hash = strchr(line, '#');
    if (hash) *hash = '\0';

    /* recipe line */
    if (line[0] == '\t') {
        if (*cur && (*cur)->nrecipes < MAX_RECIPES) {
            char *r = trim(line + 1);
            if (*r)
                (*cur)->recipes[(*cur)->nrecipes++] = strdup(r);
        }
        return;
    }

    char *s = trim(line);
    if (!*s) return;

    /* include */
    if (strncmp(s, "include", 7) == 0 && (s[7] == ' ' || s[7] == '\t')) {
        char path[512];
        expand(trim(s + 7), path, sizeof(path));
        parse_file(path);
        *cur = NULL;
        return;
    }

    /* .PHONY */
    if (strncmp(s, ".PHONY", 6) == 0 && s[6] == ':') {
        char *rest = trim(s + 7);
        char *tok = strtok(rest, " \t");
        while (tok && nphony < MAX_PHONY) {
            phony[nphony++] = strdup(tok);
            tok = strtok(NULL, " \t");
        }
        *cur = NULL;
        return;
    }

    /* variable assignment: name = val  or  name := val */
    char *eq = strchr(s, '=');
    if (eq) {
        /* make sure there's no ':' before '=' that would make it a rule */
        char *colon = strchr(s, ':');
        int simple = 0;
        if (eq > s && eq[-1] == ':') { simple = 1; eq--; }
        if (!colon || colon > eq) {
            char name[256];
            int nlen = (int)(eq - s);
            if (nlen >= (int)sizeof(name)) nlen = (int)sizeof(name) - 1;
            memcpy(name, s, nlen);
            name[nlen] = '\0';
            /* trim name */
            char *nt = name + strlen(name);
            while (nt > name && (nt[-1] == ' ' || nt[-1] == '\t')) *--nt = '\0';

            char *val = trim(eq + (simple ? 2 : 1));
            if (simple) {
                char eval[LINE_MAX_LEN];
                expand(val, eval, sizeof(eval));
                var_set(name, eval, 1);
            } else {
                var_set(name, val, 0);
            }
            *cur = NULL;
            return;
        }
    }

    /* rule: target: deps */
    char *colon = strchr(s, ':');
    if (colon) {
        if (nrules >= MAX_RULES) { fprintf(stderr, "make: too many rules\n"); exit(1); }
        Rule *r = &rules[nrules++];
        memset(r, 0, sizeof(*r));

        /* target */
        char tgt[512];
        int tlen = (int)(colon - s);
        if (tlen >= (int)sizeof(tgt)) tlen = (int)sizeof(tgt) - 1;
        memcpy(tgt, s, tlen);
        tgt[tlen] = '\0';
        char *ttrim = trim(tgt);

        /* detect suffix rules: starts with '.' and has no '/' */
        if (ttrim[0] == '.' && strchr(ttrim + 1, '.') && !strchr(ttrim, '/'))
            r->is_suffix = 1;

        r->target = strdup(ttrim);

        /* record first non-special, non-suffix goal */
        if (!first_goal && !r->is_suffix && ttrim[0] != '.') {
            /* skip .PHONY etc handled above — but here it's a plain rule */
            first_goal = r->target;
        }

        /* deps */
        char *rest = trim(colon + 1);
        char *tok = strtok(rest, " \t");
        while (tok && r->ndeps < MAX_DEPS) {
            r->deps[r->ndeps++] = strdup(tok);
            tok = strtok(NULL, " \t");
        }

        *cur = r;
        return;
    }

    /* unrecognised — ignore */
    *cur = NULL;
}

static void parse_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "make: cannot open '%s'\n", path);
        return;
    }
    Rule *cur = NULL;
    char line[LINE_MAX_LEN];
    while (fgets(line, sizeof(line), f))
        parse_line(line, &cur);
    fclose(f);
}

/* ------------------------------------------------------------------ main */
int main(int argc, char *argv[]) {
    const char *makefile = NULL;
    char *goals[64];
    int   ngoals = 0;
    int   i;

    /* seed built-in variables */
    var_set("CC",     "cc",   0);
    var_set("CXX",    "c++",  0);
    var_set("AR",     "ar",   0);
    var_set("RANLIB", "ranlib", 0);
    var_set("RM",     "rm -f", 0);
    var_set("CFLAGS", "",     0);
    var_set("LDFLAGS","",     0);

    /* parse argv */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) { dry_run = 1; }
        else if (strcmp(argv[i], "-s") == 0) { silent = 1; }
        else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) { makefile = argv[++i]; }
        else if (argv[i][0] != '-') {
            /* VAR=val override or goal */
            char *eq = strchr(argv[i], '=');
            if (eq) {
                char name[256];
                int nlen = (int)(eq - argv[i]);
                if (nlen >= (int)sizeof(name)) nlen = (int)sizeof(name) - 1;
                memcpy(name, argv[i], nlen);
                name[nlen] = '\0';
                var_set(name, eq + 1, 1);
            } else {
                if (ngoals < 64) goals[ngoals++] = argv[i];
            }
        }
    }

    if (!makefile) {
        FILE *_probe;
        if ((_probe = fopen("Makefile", "r")) != NULL)  { fclose(_probe); makefile = "Makefile"; }
        else if ((_probe = fopen("makefile", "r")) != NULL) { fclose(_probe); makefile = "makefile"; }
        else { fprintf(stderr, "make: no Makefile found\n"); return 1; }
    }

    parse_file(makefile);

    if (ngoals == 0) {
        if (!first_goal) { fprintf(stderr, "make: no targets\n"); return 1; }
        goals[0]  = first_goal;
        ngoals    = 1;
    }

    int ret = 0;
    for (i = 0; i < ngoals; i++) {
        memset(built, 0, sizeof(built));
        ret |= build(goals[i]);
    }
    return ret;
}
