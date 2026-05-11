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
 * syscheck — runtime test for mkdir, rmdir, getenv, setenv, unsetenv, environ.
 * Run this binary on the live UbixOS image.  Each test prints PASS or FAIL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int passed = 0;
static int failed = 0;

#define PASS(name) do { printf("  PASS  %s\n", name); passed++; } while(0)
#define FAIL(name, why) do { printf("  FAIL  %s (%s)\n", name, why); failed++; } while(0)

/* ------------------------------------------------------------------ */
static void test_environ(void) {
  printf("\n[environ]\n");

  /* The kernel passes PATH/HOME/USER in envp — at least one should exist */
  if (environ != 0x0)
    PASS("environ pointer non-null");
  else
    FAIL("environ pointer non-null", "environ is NULL");

  if (environ && environ[0] != 0x0)
    PASS("environ[0] non-null");
  else
    FAIL("environ[0] non-null", "no entries");
}

/* ------------------------------------------------------------------ */
static void test_getenv(void) {
  const char *v;

  printf("\n[getenv]\n");

  v = getenv("PATH");
  if (v != 0x0)
    PASS("getenv(PATH) found");
  else
    FAIL("getenv(PATH) found", "returned NULL");

  v = getenv("HOME");
  if (v != 0x0)
    PASS("getenv(HOME) found");
  else
    FAIL("getenv(HOME) found", "returned NULL");

  v = getenv("_DEFINITELY_NOT_SET_XYZ_");
  if (v == 0x0)
    PASS("getenv(missing) returns NULL");
  else
    FAIL("getenv(missing) returns NULL", "returned non-NULL");
}

/* ------------------------------------------------------------------ */
static void test_setenv(void) {
  const char *v;

  printf("\n[setenv / unsetenv]\n");

  if (setenv("SYSCHECK_VAR", "hello", 1) == 0)
    PASS("setenv new variable");
  else
    FAIL("setenv new variable", "returned -1");

  v = getenv("SYSCHECK_VAR");
  if (v != 0x0 && strcmp(v, "hello") == 0)
    PASS("getenv after setenv");
  else
    FAIL("getenv after setenv", v ? v : "NULL");

  /* overwrite=0 must not replace */
  setenv("SYSCHECK_VAR", "world", 0);
  v = getenv("SYSCHECK_VAR");
  if (v != 0x0 && strcmp(v, "hello") == 0)
    PASS("setenv overwrite=0 preserves value");
  else
    FAIL("setenv overwrite=0 preserves value", v ? v : "NULL");

  /* overwrite=1 must replace */
  setenv("SYSCHECK_VAR", "world", 1);
  v = getenv("SYSCHECK_VAR");
  if (v != 0x0 && strcmp(v, "world") == 0)
    PASS("setenv overwrite=1 replaces value");
  else
    FAIL("setenv overwrite=1 replaces value", v ? v : "NULL");

  if (unsetenv("SYSCHECK_VAR") == 0)
    PASS("unsetenv returns 0");
  else
    FAIL("unsetenv returns 0", "returned -1");

  v = getenv("SYSCHECK_VAR");
  if (v == 0x0)
    PASS("getenv after unsetenv returns NULL");
  else
    FAIL("getenv after unsetenv returns NULL", v);
}

/* ------------------------------------------------------------------ */
static void test_mkdir_rmdir(void) {
  printf("\n[mkdir / rmdir]\n");

  /* Use a path under sys: which is our mounted FAT volume */
  const char *dir = "sys:/tmp_syscheck";

  if (mkdir(dir, 0755) == 0)
    PASS("mkdir creates directory");
  else
    FAIL("mkdir creates directory", "returned -1");

  /* Try to open the directory to confirm it exists */
  FILE *f = fopen(dir, "rb");
  if (f != 0x0) {
    PASS("directory visible via fopen");
    fclose(f);
  } else {
    /* fopen on a directory may not be supported — just note it */
    printf("  NOTE  directory fopen not supported (expected on FAT)\n");
  }

  if (rmdir(dir) == 0)
    PASS("rmdir removes directory");
  else
    FAIL("rmdir removes directory", "returned -1");

  /* Confirm it's gone */
  f = fopen(dir, "rb");
  if (f == 0x0)
    PASS("directory gone after rmdir");
  else {
    FAIL("directory gone after rmdir", "still accessible");
    fclose(f);
  }
}

/* ------------------------------------------------------------------ */
int main(int argc, char *argv[]) {
  printf("syscheck — UbixOS syscall verification\n");
  printf("========================================\n");

  test_environ();
  test_getenv();
  test_setenv();
  test_mkdir_rmdir();

  printf("\n========================================\n");
  printf("Results: %d passed, %d failed\n", passed, failed);
  return (failed > 0) ? 1 : 0;
}
