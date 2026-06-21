/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * objc_test — exercises the UbixFSSession facade end to end (open, enumerate,
 * read, write-back, mkdir, remove) against a real pool image.  Linking this
 * proves the Obj-C bridge + the write→commit→reopen path before any SwiftUI.
 *
 *   objc_test <image>     (image must be read-write; created by run_test.sh)
 */
#import "UbixFSSession.h"
#import <Foundation/Foundation.h>

static int g_fail = 0;
#define CHECK(cond, msg)                                                                                               \
	do                                                                                                             \
	{                                                                                                              \
		if (cond)                                                                                              \
		{                                                                                                      \
			printf("  ok   %s\n", msg);                                                                    \
		}                                                                                                      \
		else                                                                                                   \
		{                                                                                                      \
			printf("  FAIL %s\n", msg);                                                                    \
			g_fail = 1;                                                                                    \
		}                                                                                                      \
	} while (0)

int main(int argc, const char **argv)
{
	@autoreleasepool
	{
		if (argc < 2)
		{
			fprintf(stderr, "usage: %s <image>\n", argv[0]);
			return 2;
		}
		NSString *path = [NSString stringWithUTF8String:argv[1]];
		NSError *err = nil;

		UbixFSSession *s = [[UbixFSSession alloc] initWithImagePath:path readOnly:NO error:&err];
		CHECK(s != nil, "open image read-write");
		if (!s)
		{
			NSLog(@"open error: %@", err);
			return 1;
		}
		printf("  pool base offset = %llu\n", (unsigned long long)s.poolBaseOffset);

		NSArray<NSString *> *ds = [s datasetsWithError:&err];
		CHECK(ds.count >= 1, "enumerate datasets");
		printf("  datasets: %s\n", [[ds componentsJoinedByString:@", "] UTF8String]);

		CHECK([s openDataset:@"root" error:&err], "open 'root' dataset");

		NSArray<UbixFSEntry *> *root = [s readDirectoryAtPath:@"/" error:&err];
		CHECK(root != nil, "list /");
		for (UbixFSEntry *e in root)
			printf("    %s %-12s %llu bytes\n",
			       e.isDirectory ? "d" : "-",
			       e.name.UTF8String,
			       (unsigned long long)e.size);

		/* Write-back round trip. */
		NSData *payload = [@"written via UbixFSSession\n" dataUsingEncoding:NSUTF8StringEncoding];
		CHECK([s writeData:payload toPath:@"/etc/greeting" mode:0644 error:&err], "write /etc/greeting");
		NSData *back = [s readFileAtPath:@"/etc/greeting" error:&err];
		CHECK(back != nil && [back isEqualToData:payload], "read back /etc/greeting matches");

		/* mkdir + verify it appears. */
		CHECK([s createDirectoryAtPath:@"/var/log" mode:0755 error:&err], "mkdir -p /var/log");
		UbixFSEntry *vl = [s attributesOfItemAtPath:@"/var/log" error:&err];
		CHECK(vl != nil && vl.isDirectory, "stat /var/log is a directory");

		/* remove. */
		CHECK([s removeItemAtPath:@"/etc/greeting" error:&err], "remove /etc/greeting");
		UbixFSEntry *gone = [s attributesOfItemAtPath:@"/etc/greeting" error:&err];
		CHECK(gone == nil, "/etc/greeting is gone after remove");

		[s close];
		printf(g_fail ? "\nOBJC_TEST FAILED\n" : "\nOBJC_TEST PASS\n");
		return g_fail;
	}
}
