/*****************************************************************************************
 Copyright (c) 2002-2004 The UbixOS Project
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are
 permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this list of
 conditions, the following disclaimer and the list of authors.  Redistributions in binary
 form must reproduce the above copyright notice, this list of conditions, the following
 disclaimer and the list of authors in the documentation and/or other materials provided
 with the distribution. Neither the name of the UbixOS Project nor the names of its
 contributors may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 $Id$

*****************************************************************************************/

#ifndef _HD_H
#define _HD_H

#include <ubixfs/ubixfs.h>

#define hdData     0x0
#define hdError    0x1
#define hdSecCount 0x2
#define hdSecNum   0x3
#define hdCylLow   0x4
#define hdCylHi    0x5
#define hdHead     0x6
#define hdStat     0x7
#define hdCmd      0x7


struct driveInfo {
  struct driveDiskLabel *diskLabel;
  char hdSector[512];
  char hdEnable;
  char hdDev;
  char hdFlags;
  char hdShift;
  long hdMask;
  long hdMulti;
  long hdPort;
  long hdSize;
  long hdCalc;
  long parOffset;
  };

int initHardDisk();
void hdWrite(struct driveInfo *hdd,void *,uInt32,uInt32);
void hdRead(struct driveInfo *hdd,void *,uInt32,uInt32);
int hdReset();
int hdIoctl();
int hdStart();
int hdStop();
int hdStandby();
int hdInit(struct device_node *dev);

struct dos_partition {
  unsigned char   dp_flag;        /* bootstrap flags */
  unsigned char   dp_shd;         /* starting head */
  unsigned char   dp_ssect;       /* starting sector */
  unsigned char   dp_scyl;        /* starting cylinder */
  unsigned char   dp_type;         /* partition type */
  unsigned char   dp_ehd;         /* end head */
  unsigned char   dp_esect;       /* end sector */
  unsigned char   dp_ecyl;        /* end cylinder */
  uInt32       dp_start;       /* absolute starting sector number */
  uInt32       dp_size;        /* partition size in sectors */
  };

#define MAXPARTITIONS   8

struct bsd_disklabel {
        u_int32_t d_magic;              /* the magic number */
        u_int16_t d_type;               /* drive type */
        u_int16_t d_subtype;            /* controller/d_type specific */
        char      d_typename[16];       /* type name, e.g. "eagle" */

        char      d_packname[16];       /* pack identifier */

                        /* disk geometry: */
        u_int32_t d_secsize;            /* # of bytes per sector */
        u_int32_t d_nsectors;           /* # of data sectors per track */
        u_int32_t d_ntracks;            /* # of tracks per cylinder */
        u_int32_t d_ncylinders;         /* # of data cylinders per unit */
        u_int32_t d_secpercyl;          /* # of data sectors per cylinder */
        u_int32_t d_secperunit;         /* # of data sectors per unit */

        /*
         * Spares (bad sector replacements) below are not counted in
         * d_nsectors or d_secpercyl.  Spare sectors are assumed to
         * be physical sectors which occupy space at the end of each
         * track and/or cylinder.
         */
        u_int16_t d_sparespertrack;     /* # of spare sectors per track */
        u_int16_t d_sparespercyl;       /* # of spare sectors per cylinder */
        /*
         * Alternate cylinders include maintenance, replacement, configuration
         * description areas, etc.
         */
        u_int32_t d_acylinders;         /* # of alt. cylinders per unit */

                        /* hardware characteristics: */
        /*
         * d_interleave, d_trackskew and d_cylskew describe perturbations
         * in the media format used to compensate for a slow controller.
         * Interleave is physical sector interleave, set up by the
         * formatter or controller when formatting.  When interleaving is
         * in use, logically adjacent sectors are not physically
         * contiguous, but instead are separated by some number of
         * sectors.  It is specified as the ratio of physical sectors
         * traversed per logical sector.  Thus an interleave of 1:1
         * implies contiguous layout, while 2:1 implies that logical
         * sector 0 is separated by one sector from logical sector 1.
         * d_trackskew is the offset of sector 0 on track N relative to
         * sector 0 on track N-1 on the same cylinder.  Finally, d_cylskew
         * is the offset of sector 0 on cylinder N relative to sector 0
         * on cylinder N-1.
         */
        u_int16_t d_rpm;                /* rotational speed */
        u_int16_t d_interleave;         /* hardware sector interleave */
        u_int16_t d_trackskew;          /* sector 0 skew, per track */
        u_int16_t d_cylskew;            /* sector 0 skew, per cylinder */
        u_int32_t d_headswitch;         /* head switch time, usec */
        u_int32_t d_trkseek;            /* track-to-track seek, usec */
        u_int32_t d_flags;              /* generic flags */
#define NDDATA 5
        u_int32_t d_drivedata[NDDATA];  /* drive-type specific information */
#define NSPARE 5
        u_int32_t d_spare[NSPARE];      /* reserved for future use */
        u_int32_t d_magic2;             /* the magic number (again) */
        u_int16_t d_checksum;           /* xor of data incl. partitions */

                        /* filesystem and partition information: */
        u_int16_t d_npartitions;        /* number of partitions in following */
        u_int32_t d_bbsize;             /* size of boot area at sn0, bytes */
        u_int32_t d_sbsize;             /* max size of fs superblock, bytes */
        struct partition {              /* the partition table */
                u_int32_t p_size;       /* number of sectors in partition */
                u_int32_t p_offset;     /* starting sector */
                u_int32_t p_fsize;      /* filesystem basic fragment size */
                u_int8_t p_fstype;      /* filesystem type, see below */
                u_int8_t p_frag;        /* filesystem fragments per block */
                u_int16_t p_cpg;        /* filesystem cylinders per group */
        } d_partitions[MAXPARTITIONS];  /* actually may be more */
};

static const char *fstypenames[] = {
        "unused",
        "swap",
        "Version 6",
        "Version 7",
        "System V",
        "4.1BSD",
        "Eighth Edition",
        "4.2BSD",
        "MSDOS",
        "4.4LFS",
        "unknown",
        "HPFS",
        "ISO9660",
        "boot",
        "vinum",
        "raid",
        "?",
        "?",
        "?",
        "?",
        "jfs",
        NULL
};

#endif

/***
 $Log$
 Revision 1.2  2006/10/09 02:58:05  reddawg
 Fixing UFS

 Revision 1.1.1.1  2006/06/01 12:46:14  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:50  reddawg
 no message

 Revision 1.7  2004/08/15 00:33:02  reddawg
 Wow the ide driver works again

 Revision 1.6  2004/07/21 10:02:09  reddawg
 devfs: renamed functions
 device system: renamed functions
 fdc: fixed a few potential bugs and cleaned up some unused variables
 strol: fixed definition
 endtask: made it print out freepage debug info
 kmalloc: fixed a huge memory leak we had some unhandled descriptor insertion so some descriptors were lost
 ld: fixed a pointer conversion
 file: cleaned up a few unused variables
 sched: broke task deletion
 kprintf: fixed ogPrintf definition

 Revision 1.5  2004/05/21 15:05:07  reddawg
 Cleaned up


 END
 ***/
