#ifndef _FS_PIPE_FS_H
#define _FS_PIPE_FS_H

struct pipe_inode_info {
	struct wait_queue *wait;
	char *base;
	unsigned int start;
	unsigned int len;
	unsigned int lock;
	unsigned int rd_openers;
	unsigned int wr_openers;
	unsigned int readers;
	unsigned int writers;
};

#endif
