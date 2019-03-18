/* e2fs.h */
#ifndef E2FS_H
#define E2FS_H

/*
 * This code is a complete rewrite of the read-only part of E2FS
 * library, done to reduce code size and memory requirements, and
 * also increase speed in my particular conditions.
 *
 * So I need to include this for the common structure description:
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/include/linux/minix_fs.h
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 * And I also have to point to the complete e2fsprogs suite
 * at: http://sourceforge.net/projects/e2fsprogs/
 * or: http://e2fsprogs.sourceforge.net
 * by Theodore Ts'o (http://thunk.org/tytso/) (tytso@mit.edu).
 *
 * This rewrite is based on the structure found in e2fsprogs-1.26.
 *
 * Note that some parts of e2fsprogs may be available under LGPL,
 * my added work is only available under the GPL - not LGPL.
 */

/* Gujin is a bootloader, it loads a Linux kernel from cold boot or DOS.
 * Copyright (C) 1999-2003 Etienne Lorrain, fingerprint (2D3AF3EA):
 *   2471 DF64 9DEE 41D4 C8DB 9667 E448 FF8C 2D3A F3EA
 * E-Mail: etienne.lorrain@masroudeau.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
typedef unsigned char		__u8;
typedef short			__s16;
typedef unsigned short		__u16;
typedef int			__s32;
typedef unsigned int		__u32;

/*
 * Where the master copy of the superblock is located, and how big
 * superblocks are supposed to be.  We define SUPERBLOCK_SIZE because
 * the size of the superblock structure is not necessarily trustworthy
 * (some versions have the padding set up so that the superblock is
 * 1032 bytes long).
 */
#define SUPERBLOCK_OFFSET	1024
#define SUPERBLOCK_SIZE		1024
#define EXT2_MIN_BLOCK_SIZE	1024
#define EXT2_MAX_BLOCK_SIZE	4096

/*
 * Special inodes numbers
 */
#define	EXT2_BAD_INO		 1	/* Bad blocks inode */
#define EXT2_ROOT_INO		 2	/* Root inode */
#define EXT2_ACL_IDX_INO	 3	/* ACL inode */
#define EXT2_ACL_DATA_INO	 4	/* ACL inode */
#define EXT2_BOOT_LOADER_INO	 5	/* Boot loader inode */
#define EXT2_UNDEL_DIR_INO	 6	/* Undelete directory inode */
#define EXT2_RESIZE_INO		 7	/* Reserved group descriptors inode */
#define EXT2_JOURNAL_INO	 8	/* Journal inode */

/*
 * First non-reserved inode for old ext2 filesystems
 */
#define EXT2_GOOD_OLD_FIRST_INO	11

/*
 * The second extended file system magic number
 */
#define EXT2_SUPER_MAGIC	0xEF53

/*
 * Revision levels
 */
#define EXT2_GOOD_OLD_REV	0	/* The good old (original) format */
#define EXT2_DYNAMIC_REV	1	/* V2 format w/ dynamic inode sizes */

#define EXT2_CURRENT_REV	EXT2_GOOD_OLD_REV
#define EXT2_MAX_SUPP_REV	EXT2_DYNAMIC_REV

#define EXT2_GOOD_OLD_INODE_SIZE 128

#define EXT2_NAME_LEN 255

/*
 * Structure of the super block
 */
typedef struct {
    __u32	s_inodes_count;		/* Inodes count */
    __u32	s_blocks_count;		/* Blocks count */
    __u32	s_r_blocks_count;	/* Reserved blocks count */
    __u32	s_free_blocks_count;	/* Free blocks count */
    __u32	s_free_inodes_count;	/* Free inodes count */
    __u32	s_first_data_block;	/* First Data Block */
    __u32	s_log_block_size;	/* Block size */
    __s32	s_log_frag_size;	/* Fragment size */
    __u32	s_blocks_per_group;	/* # Blocks per group */
    __u32	s_frags_per_group;	/* # Fragments per group */
    __u32	s_inodes_per_group;	/* # Inodes per group */
    __u32	s_mtime;		/* Mount time */
    __u32	s_wtime;		/* Write time */
    __u16	s_mnt_count;		/* Mount count */
    __s16	s_max_mnt_count;	/* Maximal mount count */
    __u16	s_magic;		/* Magic signature */
    __u16	s_state;		/* File system state */
    __u16	s_errors;		/* Behaviour when detecting errors */
    __u16	s_minor_rev_level;	/* minor revision level */
    __u32	s_lastcheck;		/* time of last check */
    __u32	s_checkinterval;	/* max. time between checks */
    __u32	s_creator_os;		/* OS */
    __u32	s_rev_level;		/* Revision level */
    __u16	s_def_resuid;		/* Default uid for reserved blocks */
    __u16	s_def_resgid;		/* Default gid for reserved blocks */
    /*
     * These fields are for EXT2_DYNAMIC_REV superblocks only.
     *
     * Note: the difference between the compatible feature set and
     * the incompatible feature set is that if there is a bit set
     * in the incompatible feature set that the kernel doesn't
     * know about, it should refuse to mount the filesystem.
     *
     * e2fsck's requirements are more strict; if it doesn't know
     * about a feature in either the compatible or incompatible
     * feature set, it must abort and not try to meddle with
     * things it doesn't understand...
     */
    __u32	s_first_ino;		/* First non-reserved inode */
    __u16	s_inode_size;		/* size of inode structure */
    __u16	s_block_group_nr;	/* block group # of this superblock */
    struct feature_compat_str {
	unsigned	dir_prealloc	: 1;
	unsigned	imagic_inode	: 1;
	unsigned	has_journal	: 1;
	unsigned	ext_attr	: 1;
	unsigned	resize_inode	: 1;
	unsigned	dir_index	: 1;
	unsigned	reserved	: 26;
	} __attribute__ ((packed)) s_feature_compat; /* compatible feature set */
    struct feature_incompat_str {
	unsigned	compression	: 1;
	unsigned	filetype	: 1;
	unsigned	recover		: 1;
	unsigned	journal_dev	: 1;
	unsigned	reserved	: 28;
	} __attribute__ ((packed)) s_feature_incompat; /* incompatible feature set */
    struct feature_ro_compat_str {
	unsigned	sparse_super	: 1;
	unsigned	large_file	: 1;
	unsigned	btree_dir	: 1;
	unsigned	reserved	: 27;
	} __attribute__ ((packed)) s_feature_ro_compat; /* readonly-compatible feature set */
    __u8	s_uuid[16];		/* 128-bit uuid for volume */
    char	s_volume_name[16];	/* volume name */
    char	s_last_mounted[64];	/* directory where last mounted */
    __u32	s_algorithm_usage_bitmap; /* For compression */
    /*
     * Performance hints.  Directory preallocation should only
     * happen if the EXT2_FEATURE_COMPAT_DIR_PREALLOC flag is on.
     */
    __u8	s_prealloc_blocks;	/* Nr of blocks to try to preallocate*/
    __u8	s_prealloc_dir_blocks;	/* Nr to preallocate for dirs */
    __u16	s_padding1;
    /*
     * Journaling support valid if EXT2_FEATURE_COMPAT_HAS_JOURNAL set.
     */
    __u8	s_journal_uuid[16];	/* uuid of journal superblock */
    __u32	s_journal_inum;		/* inode number of journal file */
    __u32	s_journal_dev;		/* device number of journal file */
    __u32	s_last_orphan;		/* start of list of inodes to delete */

    __u32	s_reserved[197];	/* Padding to the end of the block */
    } E2FS_super_block;

/*
 * Structure of an inode on the disk
 */
typedef struct {
    struct i_mode_str {
#if 0
	struct {
	    unsigned short execute	: 1;
	    unsigned short write	: 1;
	    unsigned short read		: 1;
	    } __attribute__ ((packed)) other, group, user;
#else
	unsigned short execute_other	: 1;
	unsigned short write_other	: 1;
	unsigned short read_other	: 1;
	unsigned short execute_group	: 1;
	unsigned short write_group	: 1;
	unsigned short read_group	: 1;
	unsigned short execute_user	: 1;
	unsigned short write_user	: 1;
	unsigned short read_user	: 1;
#endif
	unsigned short vtx		: 1;
	unsigned short gid		: 1;
	unsigned short uid		: 1;
	enum {
	    fmt_fifo = 1,
	    fmt_chr = 2,
	    fmt_dir = 4,
	    fmt_blk = 6,
	    fmt_reg = 8,
	    fmt_lnk = 10,
	    fmt_sock = 12
	    } fmt : 4;
	} __attribute__ ((packed)) i_mode;	/* File mode */
    __u16	i_uid;		/* Low 16 bits of Owner Uid */
    __u32	i_size;		/* Size in bytes */
    __u32	i_atime;	/* Access time */
    __u32	i_ctime;	/* Creation time */
    __u32	i_mtime;	/* Modification time */
    __u32	i_dtime;	/* Deletion Time */
    __u16	i_gid;		/* Low 16 bits of Group Id */
    __u16	i_links_count;	/* Links count */
    __u32	i_blocks;	/* Blocks count */
    __u32	i_flags;	/* File flags */
    /* OS dependent 1: */
    __u32	i_translator;	/* Only HURD, else reserved */
    /* end OS dependent 1 */
    __u32	i_direct_block[12];
    __u32	i_simple_indirect_block;
    __u32	i_double_indirect_block;
    __u32	i_triple_indirect_block;
    __u32	i_generation;	/* File version (for NFS) */
    __u32	i_file_acl;	/* File ACL */
    __u32	i_size_high;	/* was i_dir_acl, i.e. Directory ACL */
    __u32	i_faddr;	/* Fragment address */
    /* OS dependent 2: */
    __u8	i_frag;		/* Fragment number */
    __u8	i_fsize;	/* Fragment size */
    __u16	i_mode_high;	/* Only HURD, else padding */
    /* Following is reserved for masix: */
    __u16	i_uid_high;
    __u16	i_gid_high;
    __u32	i_author;	/* Only HURD, else reserved */
    /* end OS dependent 2 */
    } __attribute__ ((packed)) E2FS_inode;

typedef struct {
    __u32	bg_block_bitmap;	/* Blocks bitmap block */
    __u32	bg_inode_bitmap;	/* Inodes bitmap block */
    __u32	bg_inode_table;		/* Inodes table block */
    __u16	bg_free_blocks_count;	/* Free blocks count */
    __u16	bg_free_inodes_count;	/* Free inodes count */
    __u16	bg_used_dirs_count;	/* Directories count */
    __u16	bg_pad;
    __u32	bg_reserved[3];
    } E2FS_group_desc;

typedef struct {
    __u32	inode;			/* Inode number */
    __u16	rec_len;		/* Directory entry length */
    __u8	name_len;		/* Name length */
    enum E2FS_file_type {
	e2fs_ft_unknown		= 0,
	e2fs_ft_reg_file	= 1,
	e2fs_ft_dir		= 2,
	e2fs_ft_chrdev		= 3,
	e2fs_ft_blkdev		= 4,
	e2fs_ft_fifo		= 5,
	e2fs_ft_sock		= 6,
	e2fs_ft_symlink		= 7
	} file_type	: 8;
    char	name[EXT2_NAME_LEN];	/* File name (variable array) */
    } E2FS_dir_entry;

#endif /* E2FS_H */

