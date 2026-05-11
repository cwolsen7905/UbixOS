/*
 * filetest.c — FAT file create/write/append/read test for UbixOS
 *
 * Compile inside UbixOS with:
 *   tcc -o /bin/filetest /src/filetest.c
 *
 * Then run:
 *   filetest
 */
#include <stdio.h>
#include <string.h>

#define TESTFILE "/filetest.txt"

static int
test_create_write(void)
{
    FILE *f;
    int n;

    printf("[1] Creating %s for write...\n", TESTFILE);
    f = fopen(TESTFILE, "w");
    if (!f) {
        printf("    FAIL: fopen(w) returned NULL\n");
        return 0;
    }
    printf("    OK: fopen(w) succeeded\n");

    n = fprintf(f, "line1: hello from filetest\n");
    printf("    fprintf wrote %d bytes\n", n);

    n = fwrite("line2: binary write\n", 1, 20, f);
    printf("    fwrite wrote %d bytes\n", n);

    if (fclose(f) != 0) {
        printf("    FAIL: fclose failed\n");
        return 0;
    }
    printf("    OK: fclose succeeded\n");
    return 1;
}

static int
test_read_back(const char *expected, int expected_len)
{
    FILE *f;
    char buf[256];
    int n;

    printf("[2] Reading back %s...\n", TESTFILE);
    f = fopen(TESTFILE, "r");
    if (!f) {
        printf("    FAIL: fopen(r) returned NULL\n");
        return 0;
    }
    printf("    OK: fopen(r) succeeded\n");

    memset(buf, 0, sizeof(buf));
    n = fread(buf, 1, sizeof(buf) - 1, f);
    printf("    fread got %d bytes\n", n);

    if (n != expected_len) {
        printf("    FAIL: expected %d bytes, got %d\n", expected_len, n);
        fclose(f);
        return 0;
    }

    if (memcmp(buf, expected, expected_len) != 0) {
        printf("    FAIL: content mismatch\n");
        printf("    got: [%s]\n", buf);
        fclose(f);
        return 0;
    }

    printf("    OK: content matches\n");
    fclose(f);
    return 1;
}

static int
test_append(void)
{
    FILE *f;
    int n;

    printf("[3] Appending to %s...\n", TESTFILE);
    f = fopen(TESTFILE, "a");
    if (!f) {
        printf("    FAIL: fopen(a) returned NULL\n");
        return 0;
    }
    printf("    OK: fopen(a) succeeded\n");

    n = fprintf(f, "line3: appended\n");
    printf("    fprintf wrote %d bytes\n", n);

    if (fclose(f) != 0) {
        printf("    FAIL: fclose failed\n");
        return 0;
    }
    printf("    OK: fclose succeeded\n");
    return 1;
}

int
main(void)
{
    static const char first_content[] =
        "line1: hello from filetest\n"
        "line2: binary write\n";
    static const char full_content[] =
        "line1: hello from filetest\n"
        "line2: binary write\n"
        "line3: appended\n";
    int pass = 0, fail = 0;

    printf("=== UbixOS FAT file test ===\n");

    if (test_create_write()) {
        pass++;
        if (test_read_back(first_content, sizeof(first_content) - 1))
            pass++;
        else
            fail++;
    } else {
        fail++;
        fail++;  /* skip read-back */
    }

    if (test_append()) {
        pass++;
        printf("[4] Reading back after append...\n");
        if (test_read_back(full_content, sizeof(full_content) - 1))
            pass++;
        else
            fail++;
    } else {
        fail++;
        fail++;
    }

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
