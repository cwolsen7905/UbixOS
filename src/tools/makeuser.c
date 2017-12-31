#include <stdio.h>
#include <stdlib.h>

struct passwd {
  char username[32];
  char passwd[32];
  int  uid;
  int  gid;
  char shell[128];
  char realname[256];
  char path[256];
  };

int main(int argc,char **argv) {
  int i = 0x0;
  struct passwd *password = 0x0;
  FILE *out;
  password = (struct passwd *)malloc(4096);
  out = fopen("./userdb","wbb");
  sprintf(password[0].username,"root");
  sprintf(password[0].passwd,"user");
  sprintf(password[0].shell,"sys:/bin/shell");
  sprintf(password[0].realname,"Root User");
  sprintf(password[0].path,"root");
  password[0].uid = 0;
  password[0].gid = 0;
  sprintf(password[1].username,"guest");
  sprintf(password[1].passwd,"user");
  sprintf(password[1].shell,"sys:/bin/shell");
  sprintf(password[1].realname,"Guest User");
  sprintf(password[1].path,"guest");
  password[1].uid = 1;
  password[1].gid = 1;
  sprintf(password[2].username,"reddawg");
  sprintf(password[2].passwd,"temp123");
  sprintf(password[2].shell,"sys:/bin/shell");
  sprintf(password[2].realname,"Christopher");
  sprintf(password[2].path,"reddawg");
  password[2].uid = 1000;
  password[2].gid = 1000;
  fwrite(password,4096,1,out);
  fclose(out);
  for (i=0;i<3;i++) {
    printf("User:  [%s]\n",password[i].username);
    printf("Pass:  [%s]\n",password[i].passwd);
    printf("UID:   [%i]\n",password[i].uid);
    printf("GID:   [%i]\n",password[i].gid);
    printf("Shell: [%s]\n",password[i].shell);
    printf("Name:  [%s]\n",password[i].realname);
    printf("Path:  [%s]\n",password[i].path);
    }
  return(0);
  }
