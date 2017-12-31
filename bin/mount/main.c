#include <stdio.h>
#include <unistd.h>

void usage();

int main(int argc,char * const argv[]) {
  int ch = 0x0;
  while ((ch = getopt(argc, argv, "adlF:fo:prwt:uv")) != -1) {
    printf("[%i]\n",ch);
    switch (ch) {
      default:
         usage();
      }
    argc -= optind;
    argv += optind;
    }
  return(0x0);
  }

void usage() {

        (void)printf("%s\n%s\n%s\n",
"usage: mount [-adflpruvw] [-F fstab] [-o options] [-t ufs | external_type]",
"       mount [-dfpruvw] special | node",
"       mount [-dfpruvw] [-o options] [-t ufs | external_type] special node");
        exit(1);
}

