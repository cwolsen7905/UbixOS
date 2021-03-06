/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
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
#include <unistd.h>
#include <sys/sys.h>
#include <string.h>
#include <sys/sched.h>

struct passwd {
  char username[32];
  char password[32];
  int  uid;
  int  gid;
  char shell[128];
  char realname[256];
  char path[256];
  };

static char *pgets(char *string) {
  int count=0,ch=0;
  while (1) {
    ch = fgetc(stdin);
    if(ch == 10) {
      printf("\n");
      break;
      }
    else if(ch == 8 && count > 0) count-=2;
    else if(ch == 0) count--;
    else string[count] = ch;
    if (ch != 8) printf("*");
    count++;
    }
  string[count] = '\0';
  return(string);
  }

static char *argv_shell[2] = { "shell", NULL, }; // ARGV For Initial Proccess
static char *envp_shell[6] = { "HOME=/", "PWD=/", "PATH=/bin:/sbin:/usr/bin:/usr/sbin", "USER=root", "GROUP=admin", NULL, }; //ENVP For Initial Proccess

static char *envp_init[11] = {
    "HOST=Dev.uBixOS.com",
    "TERM=xterm",
    "SHELL=/bin/sh",
    "HOME=/",
    "PWD=/",
    "PATH=/bin:/sbin:/usr/bin:/usr/sbin",
    "USER=root",
    "LOGNAME=root",
    "GROUP=admin",
    "LD_LIBRARY_PATH=/lib:/usr/lib",
    NULL, }; //ENVP For Initial Proccess

  
int main(int argc, char **argv, char **env) {
  FILE *fd;
  int shellPid = 0,i = 0x0;
  char userName[32];
  char passWord[32];
  char *data2 = 0x0;
  struct passwd *data = 0x0;
  int users = 0;


  if ((getuid() != 0x0) && (getgid() != 0x0)) {
    printf("This Application Must Be Run As Root.\n");
    exit(-1);
    }

  if ((data = (struct passwd *)malloc(0x1000)) == 0x0) {
    printf("Malloc Failed\n");
    exit(0x1);
    }

  fd = fopen("sys:/etc/userdb","r");
  if (fd->fd == 0x0) {
    printf("Missing User Database.\n");
    exit(0x1);
    }
     
  users = fread(data,0x1000,0x1,fd) / sizeof(struct passwd);


  fclose(fd);

  if ((data2 = (char *)malloc(384)) == 0x0) {
    printf("Malloc Failed\n");
    exit(0x1);
    }

  login:
  printf("\nUbixOS/IA-32 (devel.ubixos.com) (console)");
  getUsername:
  printf("\n\nLogin: ");
  gets((char *)&userName);

  if (userName[0] == '\0')
    goto getUsername;

  printf("Password: ");
  pgets((char *)&passWord);


  for (i=0x0;i<users;i++) {
    if (0x0 == strcmp(userName,data[i].username)) {
      if (0x0 == strcmp(passWord,data[i].password)) {
        shellPid = fork();

        if (shellPid == 0x0) {
          if (setuid(data[i].uid) != 0x0) {
            printf("Set UID Failed\n");
            }

          if (setgid(data[i].gid) != 0x0) {
            printf("Set GID Failed\n");
          }

          fd = 0x0;
          fd = fopen("sys:/etc/motd","r");

          if (fd == 0x0) {
            printf("No MOTD");
            }
          else {
            fread(data2,384,1,fd);
            printf("%s\n",data2);
            }
          fclose(fd);          
          //chdir(data[i].path);
          chdir("sys:/bin/");
          execve(data[i].shell,argv_shell,envp_init);
          printf("Error: Problem Starting Shell\n");
          exit(-1);
          }
        else {
          while (pidStatus(shellPid) == shellPid) {
            sched_yield();
            }
          goto login;
          }
        }
      }
    }
  printf("Login Incorrect!\n");
  goto login;      
  return(0x0);
  }

