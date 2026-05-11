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

#include <stdlib.h>
#include <string.h>

extern char **environ;

static int _env_count(void) {
  int n = 0;
  if (environ)
    while (environ[n]) n++;
  return n;
}

int setenv(const char *name, const char *value, int overwrite) {
  size_t namelen, vallen, n, i;
  char *entry, **newenv;

  if (name == 0x0 || *name == '\0' || strchr(name, '=') != 0x0)
    return -1;

  namelen = strlen(name);
  vallen  = strlen(value);

  /* Check if name already exists */
  if (environ) {
    for (i = 0; environ[i] != 0x0; i++) {
      if (strncmp(environ[i], name, namelen) == 0 && environ[i][namelen] == '=') {
        if (!overwrite)
          return 0;
        /* Replace in-place with a fresh allocation */
        entry = (char *)malloc(namelen + 1 + vallen + 1);
        if (!entry) return -1;
        strcpy(entry, name);
        entry[namelen] = '=';
        strcpy(entry + namelen + 1, value);
        environ[i] = entry;
        return 0;
      }
    }
  }

  /* Append: reallocate environ array */
  n = (size_t)_env_count();
  newenv = (char **)malloc((n + 2) * sizeof(char *));
  if (!newenv) return -1;

  if (environ)
    for (i = 0; i < n; i++)
      newenv[i] = environ[i];

  entry = (char *)malloc(namelen + 1 + vallen + 1);
  if (!entry) { free(newenv); return -1; }
  strcpy(entry, name);
  entry[namelen] = '=';
  strcpy(entry + namelen + 1, value);

  newenv[n]     = entry;
  newenv[n + 1] = 0x0;
  environ = newenv;
  return 0;
}

int unsetenv(const char *name) {
  size_t namelen;
  size_t i, j;

  if (name == 0x0 || *name == '\0' || strchr(name, '=') != 0x0)
    return -1;

  if (!environ) return 0;

  namelen = strlen(name);
  for (i = 0; environ[i] != 0x0; i++) {
    if (strncmp(environ[i], name, namelen) == 0 && environ[i][namelen] == '=') {
      for (j = i; environ[j] != 0x0; j++)
        environ[j] = environ[j + 1];
      return 0;
    }
  }
  return 0;
}
