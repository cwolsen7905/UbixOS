/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * UbixFSEntry — an immutable directory-entry DTO crossing the C↔Swift boundary.
 * One per child of a directory; carries only what the browser shows.  A typed
 * value object (not a dictionary) so the seam stays validated and self-describing.
 */
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(uint8_t, UbixFSEntryKind) {
	UbixFSEntryKindFile = 0,
	UbixFSEntryKindDir = 1,
	UbixFSEntryKindSymlink = 2,
};

@interface UbixFSEntry : NSObject

@property(nonatomic, readonly, copy) NSString *name;
@property(nonatomic, readonly) UbixFSEntryKind kind;
@property(nonatomic, readonly) uint64_t size; /* logical size in bytes */
@property(nonatomic, readonly) uint32_t mode; /* POSIX permission bits (& 07777) */
@property(nonatomic, readonly) uint32_t uid;
@property(nonatomic, readonly) uint32_t gid;
@property(nonatomic, readonly) uint64_t mtime; /* seconds since the epoch */

- (instancetype)initWithName:(NSString *)name
                        kind:(UbixFSEntryKind)kind
                        size:(uint64_t)size
                        mode:(uint32_t)mode
                         uid:(uint32_t)uid
                         gid:(uint32_t)gid
                       mtime:(uint64_t)mtime NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, readonly) BOOL isDirectory;

@end

NS_ASSUME_NONNULL_END
