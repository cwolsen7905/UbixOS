/*****************************************************************************************
 Copyright (c) 2002-2004, 2016 The UbixOS Project
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

 $Id: hd.h 102 2016-01-12 03:59:34Z reddawg $

 *****************************************************************************************/

#ifndef _HD_H_
#define _HD_H_

#include <sys/types.h>
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

/* ATA Uhm? */
#define ATA_IDENTIFY_COMMAND_SET_SUPPORTED1_48BIT_ENABLE 0x0400
#define ATA_IDENTIFY_SECTOR_LARGER_THEN_512_ENABLE       0x0100

/* ATA register defines */
#define ATA_DATA                        0       /* (RW) data */

#define ATA_FEATURE                     1       /* (W) feature */
#define         ATA_F_DMA               0x01    /* enable DMA */
#define         ATA_F_OVL               0x02    /* enable overlap */

#define ATA_COUNT                       2       /* (W) sector count */

#define ATA_SECTOR                      3       /* (RW) sector # */
#define ATA_CYL_LSB                     4       /* (RW) cylinder# LSB */
#define ATA_CYL_MSB                     5       /* (RW) cylinder# MSB */
#define ATA_DRIVE                       6       /* (W) Sector/Drive/Head */
#define         ATA_D_LBA               0x40    /* use LBA addressing */
#define         ATA_D_IBM               0xa0    /* 512 byte sectors, ECC */

#define ATA_COMMAND                     7       /* (W) command */

#define ATA_ERROR                       8       /* (R) error */
#define         ATA_E_ILI               0x01    /* illegal length */
#define         ATA_E_NM                0x02    /* no media */
#define         ATA_E_ABORT             0x04    /* command aborted */
#define         ATA_E_MCR               0x08    /* media change request */
#define         ATA_E_IDNF              0x10    /* ID not found */
#define         ATA_E_MC                0x20    /* media changed */
#define         ATA_E_UNC               0x40    /* uncorrectable data */
#define         ATA_E_ICRC              0x80    /* UDMA crc error */
#define         ATA_E_ATAPI_SENSE_MASK  0xf0    /* ATAPI sense key mask */

#define ATA_IREASON                     9       /* (R) interrupt reason */
#define         ATA_I_CMD               0x01    /* cmd (1) | data (0) */
#define         ATA_I_IN                0x02    /* read (1) | write (0) */
#define         ATA_I_RELEASE           0x04    /* released bus (1) */
#define         ATA_I_TAGMASK           0xf8    /* tag mask */

#define ATA_STATUS                      10      /* (R) status */
#define ATA_ALTSTAT                     11      /* (R) alternate status */
#define         ATA_S_ERROR             0x01    /* error */
#define         ATA_S_INDEX             0x02    /* index */
#define         ATA_S_CORR              0x04    /* data corrected */
#define         ATA_S_DRQ               0x08    /* data request */
#define         ATA_S_DSC               0x10    /* drive seek completed */
#define         ATA_S_SERVICE           0x10    /* drive needs service */
#define         ATA_S_DWF               0x20    /* drive write fault */
#define         ATA_S_DMA               0x20    /* DMA ready */
#define         ATA_S_READY             0x40    /* drive ready */
#define         ATA_S_BUSY              0x80    /* busy */

#define ATA_CONTROL                     12      /* (W) control */

#define ATA_CTLOFFSET                   0x206   /* control register offset */
#define ATA_PCCARD_CTLOFFSET            0x0e    /* do for PCCARD devices */
#define ATA_PC98_CTLOFFSET              0x10c   /* do for PC98 devices */
#define         ATA_A_IDS               0x02    /* disable interrupts */
#define         ATA_A_RESET             0x04    /* RESET controller */
#ifdef  ATA_LEGACY_SUPPORT
#define         ATA_A_4BIT              0x08    /* 4 head bits: obsolete 1996 */
#else
#define         ATA_A_4BIT              0x00
#endif
#define         ATA_A_HOB               0x80    /* High Order Byte enable */

/* ATA Commands */
#define ATA_IDENTIFY                   0xEC
#define ATA_CHECK_POWER_MODE           0xE5
#define ATA_STANDBY                    0xE2
#define ATA_STANDBY_IMMED              0xE0
#define ATA_IDLE_IMMED                 0xE1
#define ATA_IDLE                       0xE3
#define ATA_FLUSH_CACHE                0xE7
#define ATA_FLUSH_CACHE_EXT            0xEA
#define ATA_READ_DMA_EXT               0x25
#define ATA_READ_DMA                   0xC8
#define ATA_READ_SECTORS_EXT           0x24
#define ATA_READ_SECTORS               0x20
#define ATA_WRITE_DMA_EXT              0x35
#define ATA_WRITE_DMA                  0xCA
#define ATA_WRITE_SECTORS_EXT          0x34
#define ATA_WRITE_SECTORS              0x30
#define ATA_WRITE_UNCORRECTABLE        0x45
#define ATA_READ_VERIFY_SECTORS        0x40
#define ATA_READ_VERIFY_SECTORS_EXT    0x42
#define ATA_READ_BUFFER                0xE4
#define ATA_WRITE_BUFFER               0xE8
#define ATA_EXECUTE_DEVICE_DIAG        0x90
#define ATA_SET_FEATURES               0xEF
#define ATA_SMART                      0xB0
#define ATA_PACKET_IDENTIFY            0xA1
#define ATA_PACKET                     0xA0
#define ATA_READ_FPDMA                 0x60
#define ATA_WRITE_FPDMA                0x61
#define ATA_READ_LOG_EXT               0x2F
#define ATA_NOP                        0x00
#define ATA_DEVICE_RESET               0x08
#define ATA_MEDIA_EJECT                0xED
#define ATA_SECURITY_UNLOCK            0xF2
#define ATA_SECURITY_FREEZE_LOCK       0xF5
#define ATA_DATA_SET_MANAGEMENT        0x06
#define ATA_DOWNLOAD_MICROCODE         0x92
#define ATA_WRITE_STREAM_DMA_EXT       0x3A
#define ATA_READ_LOG_DMA_EXT           0x47
#define ATA_READ_STREAM_DMA_EXT        0x2A
#define ATA_WRITE_DMA_FUA              0x3D
#define ATA_WRITE_LOG_DMA_EXT          0x57
#define ATA_READ_DMA_QUEUED            0xC7
#define ATA_READ_DMA_QUEUED_EXT        0x26
#define ATA_WRITE_DMA_QUEUED           0xCC
#define ATA_WRITE_DMA_QUEUED_EXT       0x36
#define ATA_WRITE_DMA_QUEUED_FUA_EXT   0x3E
#define ATA_READ_MULTIPLE              0xC4
#define ATA_READ_MULTIPLE_EXT          0x29
#define ATA_WRITE_MULTIPLE             0xC5
#define ATA_WRITE_MULTIPLE_EXT         0x39
#define ATA_WRITE_MULTIPLE_FUA_EXT     0xCE

struct driveInfo {
  struct driveDiskLabel *diskLabel;
  struct ata_identify_data *ata_identify;
  u_int32_t lba_high;
  u_int32_t lba_low;
  u_int32_t sector_size;
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
  int  part;
  long lba_start;
  long lba_end;
};

int initHardDisk();
int hdWrite( struct driveInfo *hdd, void *, uInt32, uInt32 );
int hdRead( struct driveInfo *hdd, void *, uInt32, uInt32 );
int hdReset();
int hdIoctl();
int hdStart();
int hdStop();
int hdStandby();
int hdInit( struct device_node *dev );

struct dos_partition {
  unsigned char dp_flag; /* bootstrap flags */
  unsigned char dp_shd; /* starting head */
  unsigned char dp_ssect; /* starting sector */
  unsigned char dp_scyl; /* starting cylinder */
  unsigned char dp_type; /* partition type */
  unsigned char dp_ehd; /* end head */
  unsigned char dp_esect; /* end sector */
  unsigned char dp_ecyl; /* end cylinder */
  uInt32 dp_start; /* absolute starting sector number */
  uInt32 dp_size; /* partition size in sectors */
};

#define MAXPARTITIONS   8

struct bsd_disklabel {
  u_int32_t d_magic; /* the magic number */
  u_int16_t d_type; /* drive type */
  u_int16_t d_subtype; /* controller/d_type specific */
  char d_typename[16]; /* type name, e.g. "eagle" */

  char d_packname[16]; /* pack identifier */

  /* disk geometry: */
  u_int32_t d_secsize; /* # of bytes per sector */
  u_int32_t d_nsectors; /* # of data sectors per track */
  u_int32_t d_ntracks; /* # of tracks per cylinder */
  u_int32_t d_ncylinders; /* # of data cylinders per unit */
  u_int32_t d_secpercyl; /* # of data sectors per cylinder */
  u_int32_t d_secperunit; /* # of data sectors per unit */

  /*
   * Spares (bad sector replacements) below are not counted in
   * d_nsectors or d_secpercyl.  Spare sectors are assumed to
   * be physical sectors which occupy space at the end of each
   * track and/or cylinder.
   */
  u_int16_t d_sparespertrack; /* # of spare sectors per track */
  u_int16_t d_sparespercyl; /* # of spare sectors per cylinder */
  /*
   * Alternate cylinders include maintenance, replacement, configuration
   * description areas, etc.
   */
  u_int32_t d_acylinders; /* # of alt. cylinders per unit */

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
  u_int16_t d_rpm; /* rotational speed */
  u_int16_t d_interleave; /* hardware sector interleave */
  u_int16_t d_trackskew; /* sector 0 skew, per track */
  u_int16_t d_cylskew; /* sector 0 skew, per cylinder */
  u_int32_t d_headswitch; /* head switch time, usec */
  u_int32_t d_trkseek; /* track-to-track seek, usec */
  u_int32_t d_flags; /* generic flags */
#define NDDATA 5
  u_int32_t d_drivedata[NDDATA]; /* drive-type specific information */
#define NSPARE 5
  u_int32_t d_spare[NSPARE]; /* reserved for future use */
  u_int32_t d_magic2; /* the magic number (again) */
  u_int16_t d_checksum; /* xor of data incl. partitions */

  /* filesystem and partition information: */
  u_int16_t d_npartitions; /* number of partitions in following */
  u_int32_t d_bbsize; /* size of boot area at sn0, bytes */
  u_int32_t d_sbsize; /* max size of fs superblock, bytes */
  struct partition { /* the partition table */
    u_int32_t p_size; /* number of sectors in partition */
    u_int32_t p_offset; /* starting sector */
    u_int32_t p_fsize; /* filesystem basic fragment size */
    u_int8_t p_fstype; /* filesystem type, see below */
    u_int8_t p_frag; /* filesystem fragments per block */
    u_int16_t p_cpg; /* filesystem cylinders per group */
  } d_partitions[MAXPARTITIONS]; /* actually may be more */
};

static const char *fstypenames[] = { "unused", "swap", "Version 6", "Version 7", "System V", "4.1BSD", "Eighth Edition", "4.2BSD", "MSDOS", "4.4LFS", "unknown", "HPFS", "ISO9660", "boot", "vinum", "raid", "?", "?", "?", "?", "jfs", NULL };

/**
 * @name ATA_IDENTIFY_DEVICE_FIELD_LENGTHS
 *
 * The following constants define the number of bytes contained in various
 * fields found in the IDENTIFY DEVICE data structure.
 */
#define ATA_IDENTIFY_SERIAL_NUMBER_LEN        20
#define ATA_IDENTIFY_MODEL_NUMBER_LEN         40
#define ATA_IDENTIFY_FW_REVISION_LEN          8
#define ATA_IDENTIFY_48_LBA_LEN               8
#define ATA_IDENTIFY_MEDIA_SERIAL_NUMBER_LEN  30
#define ATA_IDENTIFY_WWN_LEN                  8

struct ata_identify_data {
  u_int16_t general_config_bits;                             // word  00
  u_int16_t obsolete0;                                       // word  01 (num cylinders)
  u_int16_t vendor_specific_config_bits;                     // word  02
  u_int16_t obsolete1;                                       // word  03 (num heads)
  u_int16_t retired1[2];                                     // words 04-05
  u_int16_t obsolete2;                                       // word  06 (sectors / track)
  u_int16_t reserved_for_compact_flash1[2];                  // words 07-08
  u_int16_t retired0;                                        // word  09
  u_int8_t serial_number[ATA_IDENTIFY_SERIAL_NUMBER_LEN];   // word 10-19
  u_int16_t retired2[2];                                     // words 20-21
  u_int16_t obsolete4;                                       // word  22
  u_int8_t firmware_revision[ATA_IDENTIFY_FW_REVISION_LEN]; // words 23-26
  u_int8_t model_number[ATA_IDENTIFY_MODEL_NUMBER_LEN];     // words 27-46
  u_int16_t max_sectors_per_multiple;                        // word  47
  u_int16_t reserved0;                                       // word  48
  u_int16_t capabilities1;                                   // word  49
  u_int16_t capabilities2;                                   // word  50
  u_int16_t obsolete5[2];                                    // words 51-52
  u_int16_t validity_bits;                                   // word  53
  u_int16_t obsolete6[5];                                    // words 54-58 Used to be:
  // current cylinders,
  // current heads,
  // current sectors/Track,
  // current capacity
  u_int16_t current_max_sectors_per_multiple;                // word  59
  u_int8_t total_num_sectors[4];                            // words 60-61
  u_int16_t obsolete7;                                       // word  62
  u_int16_t multi_word_dma_mode;                             // word  63
  u_int16_t pio_modes_supported;                             // word  64
  u_int16_t min_multiword_dma_transfer_cycle;                // word  65
  u_int16_t rec_min_multiword_dma_transfer_cycle;            // word  66
  u_int16_t min_pio_transfer_no_flow_ctrl;                   // word  67
  u_int16_t min_pio_transfer_with_flow_ctrl;                 // word  68
  u_int16_t additional_supported;                            // word  69
  u_int16_t reserved1;                                       // word  70
  u_int16_t reserved2[4];                                    // words 71-74
  u_int16_t queue_depth;                                     // word  75
  u_int16_t serial_ata_capabilities;                         // word  76
  u_int16_t serial_ata_reserved;                             // word  77
  u_int16_t serial_ata_features_supported;                   // word  78
  u_int16_t serial_ata_features_enabled;                     // word  79
  u_int16_t major_version_number;                            // word  80
  u_int16_t minor_version_number;                            // word  81
  u_int16_t command_set_supported0;                          // word  82
  u_int16_t command_set_supported1;                          // word  83
  u_int16_t command_set_supported_extention;                 // word  84
  u_int16_t command_set_enabled0;                            // word  85
  u_int16_t command_set_enabled1;                            // word  86
  u_int16_t command_set_default;                             // word  87
  u_int16_t ultra_dma_mode;                                  // word  88
  u_int16_t security_erase_completion_time;                  // word  89
  u_int16_t enhanced_security_erase_time;                    // word  90
  u_int16_t current_power_mgmt_value;                        // word  91
  u_int16_t master_password_revision;                        // word  92
  u_int16_t hardware_reset_result;                           // word  93
  u_int16_t current_acoustic_management_value;               // word  94
  u_int16_t stream_min_request_size;                         // word  95
  u_int16_t stream_transfer_time;                            // word  96
  u_int16_t stream_access_latency;                           // word  97
  u_int16_t stream_performance_granularity[2];               // words 98-99
  u_int8_t max_48bit_lba[ATA_IDENTIFY_48_LBA_LEN];          // words 100-103
  u_int16_t streaming_transfer_time;                         // word  104
  u_int16_t max_lba_range_entry_blocks;                      // word  105
  u_int16_t physical_logical_sector_info;                    // word  106
  u_int16_t acoustic_test_interseek_delay;                   // word  107
  u_int8_t world_wide_name[ATA_IDENTIFY_WWN_LEN];           // words 108-111
  u_int8_t reserved_for_wwn_extention[ATA_IDENTIFY_WWN_LEN];           // words 112-115
  u_int16_t reserved4;                                       // word  116
  u_int8_t words_per_logical_sector[4];                     // words 117-118
  u_int16_t command_set_supported2;                          // word  119
  u_int16_t reserved5[7];                                    // words 120-126
  u_int16_t removable_media_status;                          // word  127
  u_int16_t security_status;                                 // word  128
  u_int16_t vendor_specific1[31];                            // words 129-159
  u_int16_t cfa_power_mode1;                                 // word  160
  u_int16_t reserved_for_compact_flash2[7];                  // words 161-167
  u_int16_t device_nominal_form_factor;                      // word  168
  u_int16_t data_set_management;                             // word  169
  u_int16_t reserved_for_compact_flash3[6];                  // words 170-175
  u_int16_t current_media_serial_number[ATA_IDENTIFY_MEDIA_SERIAL_NUMBER_LEN];                  //words 176-205
  u_int16_t reserved6[3];                                    // words 206-208
  u_int16_t logical_sector_alignment;                        // words 209
  u_int16_t reserved7[7];                                    // words 210-216
  u_int16_t nominal_media_rotation_rate;                     // word  217
  u_int16_t reserved8[16];                                   // words 218-233
  u_int16_t min_num_blocks_per_microcode;                    // word  234
  u_int16_t max_num_blocks_per_microcode;                    // word  235
  u_int16_t reserved9[19];                                   // words 236-254
  u_int16_t integrity_word;                                  // word  255
};

/*
 * A list of partition types, probably outdated.
 */
static const char *const part_types[256] = {
  [0x00] = "unused",
  [0x01] = "Primary DOS with 12 bit FAT",
  [0x02] = "XENIX / file system",
  [0x03] = "XENIX /usr file system",
  [0x04] = "Primary DOS with 16 bit FAT (< 32MB)",
  [0x05] = "Extended DOS",
  [0x06] = "Primary DOS, 16 bit FAT (>= 32MB)",
  [0x07] = "NTFS, OS/2 HPFS, QNX-2 (16 bit) or Advanced UNIX",
  [0x08] = "AIX file system or SplitDrive",
  [0x09] = "AIX boot partition or Coherent",
  [0x0A] = "OS/2 Boot Manager, OPUS or Coherent swap",
  [0x0B] = "DOS or Windows 95 with 32 bit FAT",
  [0x0C] = "DOS or Windows 95 with 32 bit FAT (LBA)",
  [0x0E] = "Primary 'big' DOS (>= 32MB, LBA)",
  [0x0F] = "Extended DOS (LBA)",
  [0x10] = "OPUS",
  [0x11] = "OS/2 BM: hidden DOS with 12-bit FAT",
  [0x12] = "Compaq diagnostics",
  [0x14] = "OS/2 BM: hidden DOS with 16-bit FAT (< 32MB)",
  [0x16] = "OS/2 BM: hidden DOS with 16-bit FAT (>= 32MB)",
  [0x17] = "OS/2 BM: hidden IFS (e.g. HPFS)",
  [0x18] = "AST Windows swapfile",
  [0x1b] = "ASUS Recovery partition (NTFS)",
  [0x24] = "NEC DOS",
  [0x3C] = "PartitionMagic recovery",
  [0x39] = "plan9",
  [0x40] = "VENIX 286",
  [0x41] = "Linux/MINIX (sharing disk with DRDOS)",
  [0x42] = "SFS or Linux swap (sharing disk with DRDOS)",
  [0x43] = "Linux native (sharing disk with DRDOS)",
  [0x4D] = "QNX 4.2 Primary",
  [0x4E] = "QNX 4.2 Secondary",
  [0x4F] = "QNX 4.2 Tertiary",
  [0x50] = "DM (disk manager)",
  [0x51] = "DM6 Aux1 (or Novell)",
  [0x52] = "CP/M or Microport SysV/AT",
  [0x53] = "DM6 Aux3",
  [0x54] = "DM6",
  [0x55] = "EZ-Drive (disk manager)",
  [0x56] = "Golden Bow (disk manager)",
  [0x5c] = "Priam Edisk (disk manager)", /* according to S. Widlake */
  [0x61] = "SpeedStor",
  [0x63] = "System V/386 (such as ISC UNIX), GNU HURD or Mach",
  [0x64] = "Novell Netware/286 2.xx",
  [0x65] = "Novell Netware/386 3.xx",
  [0x70] = "DiskSecure Multi-Boot",
  [0x75] = "PCIX",
  [0x77] = "QNX4.x",
  [0x78] = "QNX4.x 2nd part",
  [0x79] = "QNX4.x 3rd part",
  [0x80] = "Minix until 1.4a",
  [0x81] = "Minix since 1.4b, early Linux partition or Mitac disk manager",
  [0x82] = "Linux swap or Solaris x86",
  [0x83] = "Linux native",
  [0x84] = "OS/2 hidden C: drive",
  [0x85] = "Linux extended",
  [0x86] = "NTFS volume set??",
  [0x87] = "NTFS volume set??",
  [0x93] = "Amoeba file system",
  [0x94] = "Amoeba bad block table",
  [0x9F] = "BSD/OS",
  [0xA0] = "Suspend to Disk",
  [0xA5] = "FreeBSD/NetBSD/386BSD",
  [0xA6] = "OpenBSD",
  [0xA7] = "NeXTSTEP",
  [0xA9] = "NetBSD",
  [0xAC] = "IBM JFS",
  [0xAF] = "HFS+",
  [0xB7] = "BSDI BSD/386 file system",
  [0xB8] = "BSDI BSD/386 swap",
  [0xBE] = "Solaris x86 boot",
  [0xBF] = "Solaris x86 (new)",
  [0xC1] = "DRDOS/sec with 12-bit FAT",
  [0xC4] = "DRDOS/sec with 16-bit FAT (< 32MB)",
  [0xC6] = "DRDOS/sec with 16-bit FAT (>= 32MB)",
  [0xC7] = "Syrinx",
  [0xDB] = "CP/M, Concurrent CP/M, Concurrent DOS or CTOS",
  [0xDE] = "DELL Utilities - FAT filesystem",
  [0xE1] = "DOS access or SpeedStor with 12-bit FAT extended partition",
  [0xE3] = "DOS R/O or SpeedStor",
  [0xE4] = "SpeedStor with 16-bit FAT extended partition < 1024 cyl.",
  [0xEB] = "BeOS file system",
  [0xEE] = "EFI GPT",
  [0xEF] = "EFI System Partition",
  [0xF1] = "SpeedStor",
  [0xF2] = "DOS 3.3+ Secondary",
  [0xF4] = "SpeedStor large partition",
  [0xFB] = "VMware VMFS",
  [0xFE] = "SpeedStor >1024 cyl. or LANstep",
  [0xFF] = "Xenix bad blocks table",
};

#endif

/***
 $Log: hd.h,v $
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
