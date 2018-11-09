#ifndef _BUF_H
#define _BUF_H

#include <sys/dir.h>

typedef struct buf
{
	union
	{
		char			b__data[BLOCK_SIZE];					/* ordinary user data */
		direct		b__dir[NR_DIR_ENTRIES];				/* directory block */
		zone1_t		b__v1_ind[V1_INDIRECTS];			/* V1 indirect block */
		zone_t		b__v2_ind[V2_INDIRECTS];			/* V2 indirect block */
		d1_inode		b__v1_ino[V1_INODES_PER_BLOCK];	/* V1 inode block */
		d2_inode		b__v2_ino[V2_INODES_PER_BLOCK];	/* V2 inode block */
		bitchunk_t	b__bitmap[BITMAP_CHUNKS];			/* bit map block */
	} b;

	/**
	 * Header portion of the buffer
	 */
	struct buf	*b_next;		/* used to link all free bufs in a chain */
	struct buf	*b_prev;		/* used to link all free bufs the other way */
	struct buf	*b_hash;		/* used to link all bufs on hash chains */
	block_t		b_blocknr;	/* block number of its (minor) device */
	dev_t			b_dev;		/* major | minor device where block resides */
	char			b_dirt;		/* CLEAN or DIRTY */
	char			b_count;		/* number of users of this buffer */
} buf[NR_BUFFS];

#endif /* _BUF_H */
