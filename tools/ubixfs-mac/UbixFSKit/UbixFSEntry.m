/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * UbixFSEntry implementation.  See UbixFSEntry.h.
 */
#import "UbixFSEntry.h"

@implementation UbixFSEntry

- (instancetype)initWithName:(NSString *)name
                        kind:(UbixFSEntryKind)kind
                        size:(uint64_t)size
                        mode:(uint32_t)mode
                         uid:(uint32_t)uid
                         gid:(uint32_t)gid
                       mtime:(uint64_t)mtime
{
	if ((self = [super init]))
	{
		_name = [name copy];
		_kind = kind;
		_size = size;
		_mode = mode;
		_uid = uid;
		_gid = gid;
		_mtime = mtime;
	}
	return self;
}

- (BOOL)isDirectory
{
	return _kind == UbixFSEntryKindDir;
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<UbixFSEntry %@ %@ %llu bytes 0%o>",
	                                  self.isDirectory ? @"dir" : @"file",
	                                  _name,
	                                  (unsigned long long)_size,
	                                  _mode];
}

@end
