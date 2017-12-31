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
  FILE *in;
  password = (struct passwd *)malloc(4096);
  in = fopen("./userdb","rb");
  fread(password,4096,1,in);
  fclose(in);
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
