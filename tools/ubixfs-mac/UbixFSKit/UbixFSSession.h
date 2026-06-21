/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * UbixFSSession — the Obj-C facade over the portable UbixFS core (lib/ubixfs_core)
 * + the host vdev shim (ubfs_vdev_image).  One session == one opened pool image.
 * This is the entire surface Swift consumes; no C struct or pointer leaks across
 * the boundary.  See docs/design/ubixfs-mac-browser-plan.md.
 *
 * Threading: a session is NOT thread-safe; drive it from a single (e.g. a
 * dedicated background) queue.  The underlying core keeps an in-memory object
 * cache per opened dataset.
 */
#import <Foundation/Foundation.h>
#import "UbixFSEntry.h"

NS_ASSUME_NONNULL_BEGIN

extern NSErrorDomain const UbixFSErrorDomain;

@interface UbixFSSession : NSObject

/**
 * Open a pool image (raw pool, or a full disk image with an MBR UbixFS
 * partition).  Read-only unless `readOnly` is NO.
 * @return a session, or nil with *error set (UbixFSErrorDomain).
 */
- (nullable instancetype)initWithImagePath:(NSString *)path
                                  readOnly:(BOOL)readOnly
                                     error:(NSError **)error NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, readonly, getter=isReadOnly) BOOL readOnly;
@property(nonatomic, readonly) uint64_t poolBaseOffset; /* byte offset within the image */

/** All dataset names in the pool. */
- (nullable NSArray<NSString *> *)datasetsWithError:(NSError **)error;

/** Select the dataset subsequent path ops act on.  @return YES on success. */
- (BOOL)openDataset:(NSString *)name error:(NSError **)error;

/** List `path` (absolute, within the open dataset).  @return entries, or nil. */
- (nullable NSArray<UbixFSEntry *> *)readDirectoryAtPath:(NSString *)path error:(NSError **)error;

/** Stat a single path.  @return its entry (name = last component), or nil. */
- (nullable UbixFSEntry *)attributesOfItemAtPath:(NSString *)path error:(NSError **)error;

/** Read a whole file out (copy-out).  @return its bytes, or nil. */
- (nullable NSData *)readFileAtPath:(NSString *)path error:(NSError **)error;

/** Read a symlink's target.  @return the target string, or nil. */
- (nullable NSString *)readSymlinkAtPath:(NSString *)path error:(NSError **)error;

/**
 * Write `data` to `path` (copy-in), creating it (and missing parent dirs) and
 * committing the pool.  Requires a read-write session.  @return YES on success.
 */
- (BOOL)writeData:(NSData *)data toPath:(NSString *)path mode:(uint32_t)mode error:(NSError **)error;

/** Create a directory (and missing parents) and commit.  Requires read-write. */
- (BOOL)createDirectoryAtPath:(NSString *)path mode:(uint32_t)mode error:(NSError **)error;

/** Remove a file / empty directory and commit.  Requires read-write. */
- (BOOL)removeItemAtPath:(NSString *)path error:(NSError **)error;

/** Close the image (also called on dealloc). */
- (void)close;

@end

NS_ASSUME_NONNULL_END
