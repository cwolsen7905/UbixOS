/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * UbixFSSession implementation.  See UbixFSSession.h.
 *
 * Wraps the portable C core: image -> ubfs_pool_open -> ubfs_dsl_open, then a
 * selected dataset's object set + POSIX fs layer.  Mutations commit via
 * ubfs_dsl_sync_dataset + ubfs_dsl_sync and then reopen the dataset so the
 * in-memory tree always reflects committed (on-disk) state.
 */
#import "UbixFSSession.h"

#include "ubfs_vdev_image.h"
#include <ubfs_dsl.h>
#include <ubfs_fs.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

NSErrorDomain const UbixFSErrorDomain = @"UbixFSErrorDomain";

typedef NS_ERROR_ENUM(UbixFSErrorDomain, UbixFSError){
    UbixFSErrorOpenFailed = 1,
    UbixFSErrorNoPool = 2,
    UbixFSErrorNoDataset = 3,
    UbixFSErrorNoPath = 4,
    UbixFSErrorReadOnly = 5,
    UbixFSErrorIO = 6,
};

@implementation UbixFSSession
{
	ubfs_image_t *_img; /* heap: io.ctx points back at it, must not move */
	ubfs_pool_t *_pool;
	ubfs_dsl_t _dsl;
	BOOL _dslOpen;

	/* currently selected dataset */
	ubfs_dmu_os_t _os;
	ubfs_fs_t _fs;
	uint64_t _dsObj;
	NSString *_openDataset;
	BOOL _dsSelected;
}

/* ── error helper ───────────────────────────────────────────────────────────*/
static NSError *mkerr(UbixFSError code, NSString *msg)
{
	return [NSError errorWithDomain:UbixFSErrorDomain code:code userInfo:@{NSLocalizedDescriptionKey : msg}];
}

static void set_err(NSError **out, UbixFSError code, NSString *msg)
{
	if (out)
		*out = mkerr(code, msg);
}

/* ── lifecycle ──────────────────────────────────────────────────────────────*/
- (nullable instancetype)initWithImagePath:(NSString *)path readOnly:(BOOL)readOnly error:(NSError **)error
{
	if (!(self = [super init]))
		return nil;

	_img = calloc(1, sizeof(*_img));
	if (!_img)
	{
		set_err(error, UbixFSErrorOpenFailed, @"out of memory");
		return nil;
	}

	int r = ubfs_image_open(_img, path.fileSystemRepresentation, readOnly ? 0 : 1);
	if (r == -1)
	{
		set_err(error, UbixFSErrorOpenFailed, [NSString stringWithFormat:@"cannot open %@", path]);
		free(_img);
		_img = NULL;
		return nil;
	}
	if (r < 0)
	{
		set_err(error, UbixFSErrorNoPool, [NSString stringWithFormat:@"no UbixFS pool found in %@", path]);
		ubfs_image_close(_img);
		free(_img);
		_img = NULL;
		return nil;
	}

	if (ubfs_pool_open(&_img->io, &_pool) < 0 || ubfs_dsl_open(_pool, &_dsl) < 0)
	{
		set_err(error, UbixFSErrorNoPool, @"pool/DSL open failed");
		ubfs_image_close(_img);
		free(_img);
		_img = NULL;
		return nil;
	}
	_dslOpen = YES;
	_readOnly = readOnly;
	_poolBaseOffset = _img->base;
	return self;
}

- (void)close
{
	if (_pool)
	{
		ubfs_pool_close(_pool);
		_pool = NULL;
	}
	if (_img)
	{
		ubfs_image_close(_img);
		free(_img);
		_img = NULL;
	}
	_dslOpen = NO;
	_dsSelected = NO;
}

- (void)dealloc
{
	[self close];
}

/* ── datasets ───────────────────────────────────────────────────────────────*/
static int collect_dataset(void *arg, const char *name, uint64_t ds_obj)
{
	(void)ds_obj;
	NSMutableArray<NSString *> *arr = (__bridge NSMutableArray<NSString *> *)arg;
	[arr addObject:[NSString stringWithUTF8String:name]];
	return 0;
}

- (nullable NSArray<NSString *> *)datasetsWithError:(NSError **)error
{
	if (!_dslOpen)
	{
		set_err(error, UbixFSErrorNoPool, @"session is closed");
		return nil;
	}
	NSMutableArray<NSString *> *arr = [NSMutableArray array];
	if (ubfs_dsl_foreach(&_dsl, collect_dataset, (__bridge void *)arr) < 0)
	{
		set_err(error, UbixFSErrorIO, @"dataset enumeration failed");
		return nil;
	}
	return arr;
}

- (BOOL)openDataset:(NSString *)name error:(NSError **)error
{
	if (!_dslOpen)
	{
		set_err(error, UbixFSErrorNoPool, @"session is closed");
		return NO;
	}
	ubfs_dataset_phys_t dp;
	uint64_t obj;
	if (ubfs_dsl_lookup(&_dsl, name.UTF8String, &obj) < 0 || ubfs_dsl_open_dataset(&_dsl, obj, &dp, &_os) < 0)
	{
		set_err(error, UbixFSErrorNoDataset, [NSString stringWithFormat:@"no dataset '%@'", name]);
		return NO;
	}
	ubfs_fs_init(&_fs, &_os, (uint64_t)time(NULL));
	_dsObj = obj;
	_openDataset = [name copy];
	_dsSelected = YES;
	return YES;
}

/* After a mutation, persist and reopen so the in-memory tree == on-disk state. */
- (BOOL)commitAndReopen:(NSError **)error
{
	if (ubfs_dsl_sync_dataset(&_dsl, _dsObj, &_os) < 0 || ubfs_dsl_sync(&_dsl) < 0)
	{
		set_err(error, UbixFSErrorIO, @"commit failed");
		return NO;
	}
	if (ubfs_dsl_open(_pool, &_dsl) < 0)
	{
		set_err(error, UbixFSErrorIO, @"reopen failed");
		return NO;
	}
	ubfs_dataset_phys_t dp;
	if (ubfs_dsl_open_dataset(&_dsl, _dsObj, &dp, &_os) < 0)
	{
		set_err(error, UbixFSErrorIO, @"dataset reopen failed");
		return NO;
	}
	ubfs_fs_init(&_fs, &_os, (uint64_t)time(NULL));
	return YES;
}

- (BOOL)ensureDataset:(NSError **)error
{
	if (_dsSelected)
		return YES;
	set_err(error, UbixFSErrorNoDataset, @"no dataset selected (call openDataset:)");
	return NO;
}

- (BOOL)ensureWritable:(NSError **)error
{
	if (!_readOnly)
		return YES;
	set_err(error, UbixFSErrorReadOnly, @"session is read-only");
	return NO;
}

/* ── reads ──────────────────────────────────────────────────────────────────*/
static UbixFSEntryKind kind_from_dt(uint8_t dt)
{
	if (dt == UBFS_DT_DIR)
		return UbixFSEntryKindDir;
	if (dt == UBFS_DT_LNK)
		return UbixFSEntryKindSymlink;
	return UbixFSEntryKindFile;
}

static UbixFSEntryKind kind_from_mode(uint64_t mode)
{
	switch (mode & UBFS_S_IFMT)
	{
		case UBFS_S_IFDIR:
			return UbixFSEntryKindDir;
		case UBFS_S_IFLNK:
			return UbixFSEntryKindSymlink;
		default:
			return UbixFSEntryKindFile;
	}
}

struct readdir_ctx
{
	__unsafe_unretained NSMutableArray<UbixFSEntry *> *arr;
	ubfs_fs_t *fs;
};

static int readdir_cb(void *arg, const char *name, uint64_t obj, uint8_t type)
{
	struct readdir_ctx *c = (struct readdir_ctx *)arg;
	ubfs_inode_t in;
	UbixFSEntry *e;

	if (ubfs_fs_getattr(c->fs, obj, &in) != 0)
		return 0; /* skip unreadable entry rather than abort the listing */
	e = [[UbixFSEntry alloc] initWithName:[NSString stringWithUTF8String:name]
	                                 kind:kind_from_dt(type)
	                                 size:in.size
	                                 mode:(uint32_t)(in.mode & 07777)
	                                  uid:(uint32_t)in.uid
	                                  gid:(uint32_t)in.gid
	                                mtime:in.mtime];
	[c->arr addObject:e];
	return 0;
}

- (nullable NSArray<UbixFSEntry *> *)readDirectoryAtPath:(NSString *)path error:(NSError **)error
{
	if (![self ensureDataset:error])
		return nil;
	uint64_t obj;
	ubfs_inode_t in;
	if (ubfs_fs_lookup(&_fs, path.fileSystemRepresentation, &obj) < 0 || ubfs_fs_getattr(&_fs, obj, &in) < 0)
	{
		set_err(error, UbixFSErrorNoPath, [NSString stringWithFormat:@"no such path: %@", path]);
		return nil;
	}
	if ((in.mode & UBFS_S_IFMT) != UBFS_S_IFDIR)
	{
		set_err(error, UbixFSErrorNoPath, [NSString stringWithFormat:@"not a directory: %@", path]);
		return nil;
	}
	NSMutableArray<UbixFSEntry *> *arr = [NSMutableArray array];
	struct readdir_ctx ctx = {arr, &_fs};
	ubfs_fs_readdir(&_fs, obj, readdir_cb, &ctx);
	return arr;
}

- (nullable UbixFSEntry *)attributesOfItemAtPath:(NSString *)path error:(NSError **)error
{
	if (![self ensureDataset:error])
		return nil;
	uint64_t obj;
	ubfs_inode_t in;
	if (ubfs_fs_lookup(&_fs, path.fileSystemRepresentation, &obj) < 0 || ubfs_fs_getattr(&_fs, obj, &in) < 0)
	{
		set_err(error, UbixFSErrorNoPath, [NSString stringWithFormat:@"no such path: %@", path]);
		return nil;
	}
	NSString *name = path.lastPathComponent ?: path;
	return [[UbixFSEntry alloc] initWithName:name
	                                    kind:kind_from_mode(in.mode)
	                                    size:in.size
	                                    mode:(uint32_t)(in.mode & 07777)
	                                     uid:(uint32_t)in.uid
	                                     gid:(uint32_t)in.gid
	                                   mtime:in.mtime];
}

- (nullable NSData *)readFileAtPath:(NSString *)path error:(NSError **)error
{
	if (![self ensureDataset:error])
		return nil;
	uint64_t obj;
	ubfs_inode_t in;
	if (ubfs_fs_lookup(&_fs, path.fileSystemRepresentation, &obj) < 0 || ubfs_fs_getattr(&_fs, obj, &in) < 0)
	{
		set_err(error, UbixFSErrorNoPath, [NSString stringWithFormat:@"no such file: %@", path]);
		return nil;
	}
	NSMutableData *data = [NSMutableData dataWithLength:(NSUInteger)in.size];
	uint8_t *p = data.mutableBytes;
	uint64_t off = 0;
	while (off < in.size)
	{
		uint64_t want = in.size - off;
		if (want > (1u << 20))
			want = (1u << 20);
		int got = ubfs_fs_read(&_fs, obj, off, p + off, want);
		if (got <= 0)
		{
			set_err(error, UbixFSErrorIO, [NSString stringWithFormat:@"read error at offset %llu", off]);
			return nil;
		}
		off += (uint64_t)got;
	}
	return data;
}

- (nullable NSString *)readSymlinkAtPath:(NSString *)path error:(NSError **)error
{
	if (![self ensureDataset:error])
		return nil;
	uint64_t obj;
	if (ubfs_fs_lookup(&_fs, path.fileSystemRepresentation, &obj) < 0)
	{
		set_err(error, UbixFSErrorNoPath, [NSString stringWithFormat:@"no such link: %@", path]);
		return nil;
	}
	char buf[1024];
	if (ubfs_fs_readlink(&_fs, obj, buf, sizeof(buf)) < 0)
	{
		set_err(error, UbixFSErrorIO, @"readlink failed");
		return nil;
	}
	return [NSString stringWithUTF8String:buf];
}

/* ── writes ─────────────────────────────────────────────────────────────────*/

/* Create `path` and any missing parent directories (mkdir -p), no commit. */
- (BOOL)makeDirs:(NSString *)path mode:(uint32_t)mode
{
	NSArray<NSString *> *parts = [path pathComponents];
	NSString *cur = @"";
	for (NSString *comp in parts)
	{
		if ([comp isEqualToString:@"/"] || comp.length == 0)
			continue;
		cur = [cur stringByAppendingFormat:@"/%@", comp];
		uint64_t obj;
		if (ubfs_fs_lookup(&_fs, cur.fileSystemRepresentation, &obj) != 0)
			if (ubfs_fs_mkdir(&_fs, cur.fileSystemRepresentation, mode, 0, 0, &obj) < 0)
				return NO;
	}
	return YES;
}

- (BOOL)writeData:(NSData *)data toPath:(NSString *)path mode:(uint32_t)mode error:(NSError **)error
{
	if (![self ensureDataset:error] || ![self ensureWritable:error])
		return NO;

	NSString *parent = [path stringByDeletingLastPathComponent];
	if (parent.length > 1 && ![self makeDirs:parent mode:0755])
	{
		set_err(error, UbixFSErrorIO, [NSString stringWithFormat:@"cannot create parents of %@", path]);
		return NO;
	}

	const char *cpath = path.fileSystemRepresentation;
	uint64_t obj;
	/* Overwrite semantics: drop any existing file first, then recreate. */
	if (ubfs_fs_lookup(&_fs, cpath, &obj) == 0)
		ubfs_fs_unlink(&_fs, cpath);
	if (ubfs_fs_create(&_fs, cpath, mode, 0, 0, &obj) < 0)
	{
		set_err(error, UbixFSErrorIO, [NSString stringWithFormat:@"cannot create %@", path]);
		return NO;
	}

	const uint8_t *bytes = data.bytes;
	uint64_t len = data.length;
	uint64_t off = 0;
	while (off < len)
	{
		uint64_t want = len - off;
		if (want > (1u << 20))
			want = (1u << 20);
		if (ubfs_fs_write(&_fs, obj, off, bytes + off, want) < 0)
		{
			set_err(error, UbixFSErrorIO, [NSString stringWithFormat:@"write error at offset %llu", off]);
			return NO;
		}
		off += want;
	}
	return [self commitAndReopen:error];
}

- (BOOL)createDirectoryAtPath:(NSString *)path mode:(uint32_t)mode error:(NSError **)error
{
	if (![self ensureDataset:error] || ![self ensureWritable:error])
		return NO;
	if (![self makeDirs:path mode:mode])
	{
		set_err(error, UbixFSErrorIO, [NSString stringWithFormat:@"cannot create %@", path]);
		return NO;
	}
	return [self commitAndReopen:error];
}

- (BOOL)removeItemAtPath:(NSString *)path error:(NSError **)error
{
	if (![self ensureDataset:error] || ![self ensureWritable:error])
		return NO;
	if (ubfs_fs_unlink(&_fs, path.fileSystemRepresentation) < 0)
	{
		set_err(error, UbixFSErrorIO, [NSString stringWithFormat:@"cannot remove %@", path]);
		return NO;
	}
	return [self commitAndReopen:error];
}

@end
