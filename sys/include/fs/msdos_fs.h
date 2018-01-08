#ifndef _FS_MSDOS_FS_H
#define _FS_MSDOS_FS_H

/*
 * MS-DOS file system in-core superblock data
 */

struct msdos_sb_info {
    unsigned short cluster_size; /* sectors/cluster */
    unsigned char fats, fat_bits; /* number of FATs, FAT bits (12 or 16) */
    unsigned short fat_start, fat_length; /* FAT start & length (sec.) */
    unsigned short dir_start, dir_entries; /* root dir start & entries */
    unsigned short data_start; /* first data sector */
    unsigned long clusters; /* number of clusters */
    uid_t fs_uid;
    gid_t fs_gid;
    int quiet; /* fake successful chmods and chowns */
    unsigned short fs_umask;
    unsigned char name_check; /* r = relaxed, n = normal, s = strict */
    unsigned char conversion; /* b = binary, t = text, a = auto */
    struct wait_queue *fat_wait;
    int fat_lock;
    int prev_free; /* previously returned free cluster number */
    int free_clusters; /* -1 if undefined */
};

/*
 * MS-DOS file system inode data in memory
 */

struct msdos_inode_info {
	int i_start;	/* first cluster or 0 */
	int i_attrs;	/* unused attribute bits */
	int i_busy;	/* file is either deleted but still open, or
			   inconsistent (mkdir) */
	struct inode *i_depend; /* pointer to inode that depends on the
				   current inode */
	struct inode *i_old;	/* pointer to the old inode this inode
				   depends on */
	int i_binary;	/* file contains non-text data */
};

#endif
