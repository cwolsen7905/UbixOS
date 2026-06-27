/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * filetest.c — FAT file create/write/read/append test for UbixOS
 *
 * Uses POSIX fd syscalls (open/write/read/close) — the same path TCC uses.
 * The FILE* stdio layer (fwrite/fprintf) is broken for files because
 * sys_fwrite is not in the syscall table; this program avoids it entirely.
 *
 * Run:
 *   filetest
 */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define TESTFILE "/filetest.txt"

static const char line1[] = "line1: hello from filetest\n";
static const char line2[] = "line2: second line\n";
static const char line3[] = "line3: appended\n";

static int
test_create_write(void)
{
    int fd, n;

    printf("[1] open(%s, O_WRONLY|O_CREAT)...\n", TESTFILE);
    fd = open(TESTFILE, O_WRONLY | O_CREAT, 0644);
    if (fd < 0) {
        printf("    FAIL: open returned %d\n", fd);
        return 0;
    }
    printf("    OK: fd=%d\n", fd);

    n = write(fd, line1, sizeof(line1) - 1);
    printf("    write line1: %d bytes\n", n);

    n = write(fd, line2, sizeof(line2) - 1);
    printf("    write line2: %d bytes\n", n);

    n = close(fd);
    printf("    close: %d\n", n);
    return 1;
}

static int
test_read_back(const char *tag, const char *expected, int expected_len)
{
    int fd, n;
    char buf[256];

    printf("[%s] open(%s, O_RDONLY)...\n", tag, TESTFILE);
    fd = open(TESTFILE, O_RDONLY, 0);
    if (fd < 0) {
        printf("    FAIL: open returned %d\n", fd);
        return 0;
    }
    printf("    OK: fd=%d\n", fd);

    memset(buf, 0, sizeof(buf));
    n = read(fd, buf, sizeof(buf) - 1);
    printf("    read: %d bytes\n", n);
    close(fd);

    if (n != expected_len) {
        printf("    FAIL: expected %d bytes, got %d\n", expected_len, n);
        return 0;
    }

    if (memcmp(buf, expected, expected_len) != 0) {
        printf("    FAIL: content mismatch\n");
        printf("    got:      [%s]\n", buf);
        printf("    expected: [%s]\n", expected);
        return 0;
    }

    printf("    OK: content matches\n");
    return 1;
}

static int
test_append(void)
{
    int fd, n;

    printf("[3] open(%s, O_WRONLY|O_APPEND)...\n", TESTFILE);
    fd = open(TESTFILE, O_WRONLY | O_APPEND, 0);
    if (fd < 0) {
        printf("    FAIL: open returned %d\n", fd);
        return 0;
    }
    printf("    OK: fd=%d\n", fd);

    n = write(fd, line3, sizeof(line3) - 1);
    printf("    write line3: %d bytes\n", n);

    n = close(fd);
    printf("    close: %d\n", n);
    return 1;
}

int
main(void)
{
    char expected_after_write[128];
    char expected_after_append[256];
    int after_write_len, after_append_len;
    int pass = 0, fail = 0;

    /* build expected strings without sprintf */
    memcpy(expected_after_write, line1, sizeof(line1) - 1);
    memcpy(expected_after_write + sizeof(line1) - 1, line2, sizeof(line2) - 1);
    after_write_len = (sizeof(line1) - 1) + (sizeof(line2) - 1);

    memcpy(expected_after_append, expected_after_write, after_write_len);
    memcpy(expected_after_append + after_write_len, line3, sizeof(line3) - 1);
    after_append_len = after_write_len + (sizeof(line3) - 1);

    printf("=== UbixOS FAT file I/O test ===\n");

    /* test 1: create + write */
    if (test_create_write()) {
        pass++;
        /* test 2: read back after write */
        if (test_read_back("2", expected_after_write, after_write_len))
            pass++;
        else
            fail++;
    } else {
        fail += 2;
    }

    /* test 3: append */
    if (test_append()) {
        pass++;
        /* test 4: read back after append */
        if (test_read_back("4", expected_after_append, after_append_len))
            pass++;
        else
            fail++;
    } else {
        fail += 2;
    }

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
