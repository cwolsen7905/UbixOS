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
#include <unistd.h>
#include <dirent.h>
#include <sys/sys.h>
#include <sys/sched.h>
#include <sys/mpi.h>

#define INITD_PATH    "sys:/etc/init.d"
#define INITD_MAXSVCS 64
#define INITD_PATHMAX 256

static char *argv_login[2] = { "login", NULL, };
static char *envp_login[6] = { "HOME=/", "PWD=/", "PATH=/bin:/sbin:/usr/bin:/usr/sbin", "USER=root", "GROUP=admin", NULL, };

/*
 * Scan INITD_PATH for service definition files.  Each file contains one
 * line: the full VFS path of the binary to exec.  Files are started in
 * lexicographic order, so a numeric prefix (10-logd, 20-views …) controls
 * the launch sequence.  Lines beginning with '#' and empty lines are
 * skipped.  Missing or unreadable files are silently ignored.
 */
static void
start_initd_services(void)
{
  DIR           *d;
  struct dirent *ent;
  char          *names[INITD_MAXSVCS];
  int            nnames = 0, i, j;
  char           path[INITD_PATHMAX];
  char           execpath[INITD_PATHMAX];
  char          *tmp;
  FILE          *f;
  char          *argv_svc[2];

  d = opendir(INITD_PATH);
  if (d == NULL)
    return;

  while ((ent = readdir(d)) != NULL && nnames < INITD_MAXSVCS) {
    if (ent->d_name[0] == '.')
      continue;
    names[nnames] = strdup(ent->d_name);
    if (names[nnames] != NULL)
      nnames++;
  }
  closedir(d);

  /* bubble sort — numeric prefixes (10-, 20-, …) set launch order */
  for (i = 0; i < nnames - 1; i++) {
    for (j = i + 1; j < nnames; j++) {
      if (strcmp(names[i], names[j]) > 0) {
        tmp = names[i]; names[i] = names[j]; names[j] = tmp;
      }
    }
  }

  for (i = 0; i < nnames; i++) {
    snprintf(path, sizeof(path), "%s/%s", INITD_PATH, names[i]);
    free(names[i]);

    f = fopen(path, "r");
    if (f == NULL)
      continue;
    if (fgets(execpath, sizeof(execpath), f) == NULL) {
      fclose(f);
      continue;
    }
    fclose(f);

    execpath[strcspn(execpath, "\r\n")] = '\0';
    if (execpath[0] == '\0' || execpath[0] == '#')
      continue;

    printf("init: starting %s\n", execpath);
    fflush(stdout);

    if (fork() == 0) {
      argv_svc[0] = execpath;
      argv_svc[1] = NULL;
      execve(execpath, argv_svc, envp_login);
      printf("init: failed to exec %s\n", execpath);
      exit(0);
    }
  }
}

int main(int argc,char **argv, char **envp) {
  int i=0x0;
  mpi_message_t myMsg;


  /* Create a mailbox for this task */
  if (mpi_createMbox("init") != 0x0) {
    printf("Error: Failed to creating mail box: init\n");
    exit(0x1);
  }

  /* Make sure we have superuser permissions if not exit */
  if ((getuid() != 0x0) || (getgid() != 0x0)) {
    printf("Error: This program must be run by root.\n");
    exit(0x1);
    }

  printf("Initializing UbixOS\n");

  start_initd_services();

  /* Start TTYD */
  #ifdef _IGNORE
  i = fork();

  printf("Forked: %i", i);

  if (0x0 == i) {
    printf("Starting TTYD\n");
    execve("sys:/bin/ttyd", argv_login, envp_login);
    printf("Error: Could not start TTYD\n");
    exit(0x0);
  }
  #endif

  #ifdef _IGNORE
  i = fork();
  if (0x0 == i) {
    printf("Starting Ubix Registry (ubistry)\n");
    exec("sys:/bin/ubistry",0x0);
    printf("Error: Error Starting ubistry\n");
    exit(0x0);
  }

  /*
  while (pidStatus(i) > 0x0) {
    sched_yield();
  } 
  */
  #endif

  /* Idle loop — terminals are managed by ttyd (etc/init.d/30-ttyd). */
  for (;;) {
    if (mpi_fetchMessage("init", &myMsg) == 0x0) {
      /* reserved for future init control messages */
    }
    sched_yield();
  }

  return(0x0);
  }

/***
 END
 ***/

