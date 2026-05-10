/*
 * hello.c — TCC smoke test for UbixOS
 *
 * Compile inside UbixOS with:
 *   tcc -o /bin/hello /src/hello.c
 *
 * Then run:
 *   hello
 */
#include <stdio.h>

int main(void)
{
    printf("Hello from TCC on UbixOS!\n");
    printf("TCC is working correctly.\n");
    return 0;
}
