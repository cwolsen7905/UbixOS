#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>



int main(int argc, char **argv) {
  struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) != -1)
      printf("cur: %i, max: %i\n", rl.rlim_cur, rl.rlim_max);

  return(0);
}
