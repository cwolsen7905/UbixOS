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

 $Id: hd.c 130 2016-01-14 15:45:44Z reddawg $

 *****************************************************************************************/

#include <pci/hd.h>
#include <sys/io.h>
#include <sys/device.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <devfs/devfs.h>
#include <string.h>
#include <fs/common/gpt.h>

static const uuid_t freebsd_ufs_uuid = GPT_ENT_TYPE_FREEBSD_UFS;

int initHardDisk() {
  int i = 0x0;
  int x = 0x0;
  int minor = 0x0;
  struct device_interface *devInfo = 0x0;
  struct device_interface *devInfo2 = 0x0;
  struct dos_partition *d = 0x0;
  struct driveInfo *hdd = 0x0;
  struct driveInfo *hdd2 = 0x0;
  char *data = 0x0;
  char *data2 = 0x0;
  char name[16];
  struct bsd_disklabel *bsdd = 0x0;

  //GPT
  char *secbuf = 0x0;

  hdd = (struct driveInfo *) kmalloc( sizeof(struct driveInfo) );
  hdd->ata_identify = (struct ata_identify_data *) kmalloc( sizeof(struct ata_identify_data) );
  hdd->hdPort = 0x1F0;
  hdd->hdDev = 0x40;
  hdd->parOffset = 0x0;
  hdd->part = 0x2;

  /* Alloc memory for device structure and set it up correctly */
  devInfo = (struct device_interface *) kmalloc( sizeof(struct device_interface) );
  devInfo->read = (void *) &hdRead;
  devInfo->write = (void *) &hdWrite;
  devInfo->reset = (void *) &hdReset;
  devInfo->init = (void *) &hdInit;
  devInfo->ioctl = (void *) &hdIoctl;
  devInfo->stop = (void *) &hdStop;
  devInfo->start = (void *) &hdStart;
  devInfo->standby = (void *) &hdStandby;
  devInfo->info = hdd;

  devInfo->major = 0x1;

  data = (char *) kmalloc( 512 );
  d = (struct dos_partition *) (data + 0x1BE);

  data2 = (char *) kmalloc( 512 );
  bsdd = (struct bsd_disklabel *) data2;

  if ( device_add( 0, 'c', devInfo ) == 0x0 ) {
    kprintf( "ad0 - Start: [0x0], Size: [0x%x/0x%X]\n", hdd->hdSize, hdd->hdSize * 512 );
    devfs_makeNode( "ad0", 'b', 0x1, 0x0 );
    hdRead( devInfo->info, data, 0x0, 0x1 );

    if ( d[0].dp_type == 0xEE ) {
      // MrOlsen (2016-01-14) DEBUG: This was just to help debug
      //kprintf( "%s - Type: [0x%X - %s], Start: [0x%X], Size: [0x%X]\n", name, d[0].dp_type, (d[0].dp_type >= 0 && d[0].dp_type <= 255) ? part_types[d[0].dp_type] : "Unknown", d[0].dp_start, d[0].dp_size );

      secbuf = (char *)kmalloc(65536);

      if (gptread(&freebsd_ufs_uuid, devInfo, secbuf) == -1) {
        kprintf("%s: unable to load GPT\n", "KERNEL");
        //return (-1);
      }
      else {
          devInfo2 = (struct device_interface *) kmalloc( sizeof(struct device_interface) );
          hdd2 = (struct driveInfo *) kmalloc( sizeof(struct driveInfo) );
          memcpy( devInfo2, devInfo, sizeof(struct device_interface) );
          memcpy( hdd2, hdd, sizeof(struct driveInfo) );
          //hdd2->parOffset = d[i].dp_start;
          devInfo2->info = hdd2;
          minor++;

      if (gptfind(&freebsd_ufs_uuid, devInfo2, hdd->part) == -1) {
      //MrOlsen (2016-01-11) FIX: I am using "1" as partition 1
       kprintf("%s: no UFS partition was found\n", "KERNEL");
       //return (-1);
      }

//MrOlsen (2016-01-14) DEBUG: This was just debugging code
//kprintf("[%i - %i]\n", hdd->parOffset, hdd2->parOffset);

          if ( device_add( minor, 'c', devInfo2 ) == 0x0 ) {
            sprintf( name, "ad0p%i", hdd->part);
            kprintf( "%s - Type: [0x%X - %s], Start: [%i], Offset: [%i], Size: [%i]\n", name, d[0].dp_type, (d[0].dp_type >= 0 && d[0].dp_type <= 255) ? part_types[d[0].dp_type] : "Unknown", hdd2->lba_start, hdd2->parOffset, hdd2->lba_end - hdd2->lba_start);
            devfs_makeNode( name, 'c', 0x1, minor );
}
      }

    }
    else {

      for ( i = 0x0; i < 0x4; i++ ) {

        //MrOlsen (2016-01-14) DEBUG: This was just debugging code
        //kprintf( "Type: 0x%X\n", d[i].dp_type );

        if ( d[i].dp_type == 0xEE )
          kprintf( "Motherfucker Jones! We're GPT.... Lick My Nuts Now Bitch!\n" );

        if ( d[i].dp_type != 0x0 ) {
          devInfo2 = (struct device_interface *) kmalloc( sizeof(struct device_interface) );
          hdd2 = (struct driveInfo *) kmalloc( sizeof(struct driveInfo) );
          memcpy( devInfo2, devInfo, sizeof(struct device_interface) );
          memcpy( hdd2, hdd, sizeof(struct driveInfo) );
          hdd2->parOffset = d[i].dp_start;
          devInfo2->info = hdd2;
          minor++;

          if ( device_add( minor, 'c', devInfo2 ) == 0x0 ) {
            sprintf( name, "ad0s%i", i + 1 );
            kprintf( "%s - Type: [0x%X - %s], Start: [0x%X], Size: [0x%X]\n", name, d[i].dp_type, (d[i].dp_type >= 0 && d[i].dp_type <= 255) ? part_types[d[i].dp_type] : "Unknown", d[i].dp_start, d[i].dp_size );
            devfs_makeNode( name, 'c', 0x1, minor );

            if ( d[i].dp_type == 0xA5 ) {
              //Why do i need to add 1?
              hdRead( devInfo->info, data2, d[i].dp_start + 1, 0x1 );

              for ( x = 0; x < bsdd->d_npartitions; x++ ) {
                if ( bsdd->d_partitions[x].p_size > 0 ) {
                  sprintf( name, "ad0s%i%c", i + 1, 'a' + x );
                  //New Nodes
                  devInfo2 = (struct device_interface *) kmalloc( sizeof(struct device_interface) );
                  hdd2 = (struct driveInfo *) kmalloc( sizeof(struct driveInfo) );
                  memcpy( devInfo2, devInfo, sizeof(struct device_interface) );
                  memcpy( hdd2, hdd, sizeof(struct driveInfo) );
                  //hdd2->parOffset = d[i].dp_start + bsdd->d_partitions[x].p_offset;
                  hdd2->parOffset = bsdd->d_partitions[x].p_offset;
                  devInfo2->info = hdd2;
                  minor++;
                  device_add( minor, 'c', devInfo2 );
                  devfs_makeNode( name, 'c', 0x1, minor );
                  kprintf( "%s - Type: [%s], Start: [0x%X], Size: [0x%X], MM: [%i:%i]\n", name, fstypenames[bsdd->d_partitions[x].p_fstype], bsdd->d_partitions[x].p_offset, bsdd->d_partitions[x].p_size, devInfo->major, minor );
                }

              }

            }

          }

        }

      }
    }
  }
  kfree( data );
  return (0x0);
}

int hdStandby() {
  return (0x0);
}

int hdStart() {
  return (0x0);
}

int hdStop() {
  return (0x0);
}

int hdIoctl() {
  return (0x0);
}

int hdReset() {
  return (0x0);
}

int hdInit( struct device_node *dev ) {
  uInt8 retVal = 0x0;
  long counter = 0x0;
  uInt16 *tmp = 0x0;
  struct driveInfo *hdd = dev->devInfo->info;

  for ( counter = 1000000; counter >= 0; counter-- ) {
    retVal = inportByte( hdd->hdPort + ATA_COMMAND ) & ATA_S_BUSY;

    if ( !retVal )
      goto ready;

  }

  kprintf( "Error Initializing Drive\n" );
  return (1);

  ready:

  outportByte( hdd->hdPort + ATA_DRIVE, hdd->hdDev );
  outportByte( hdd->hdPort + ATA_COMMAND, ATA_IDENTIFY );

  for ( counter = 1000000; counter >= 0; counter-- ) {
    retVal = inportByte( hdd->hdPort + ATA_COMMAND );
    if ( (retVal & 1) != 0x0 ) {
      kprintf( "Error Drive Not Available\n" );
      return (1);
    }
    if ( (retVal & 8) != 0x0 ) {
      goto go;
    }
  }

  kprintf( "Time Out Waiting On Drive\n" );
  return (1);

  go:

  tmp = (uInt16 *) hdd->ata_identify;

  for ( counter = 0; counter < 256; counter++ ) {
    tmp[counter] = inportWord( hdd->hdPort + ATA_DATA );
  }

  retVal = tmp[0x2F] & 0xFF;

  switch ( retVal ) {
  case 0:
    hdd->hdShift = 0;
    hdd->hdMulti = 1;
    break;
  case 2:
    hdd->hdShift = 1;
    hdd->hdMulti = retVal;
    break;
  case 4:
    hdd->hdShift = 2;
    hdd->hdMulti = retVal;
    break;
  case 8:
    hdd->hdShift = 3;
    hdd->hdMulti = retVal;
    break;
  case 16:
    hdd->hdShift = 4;
    hdd->hdMulti = retVal;
    break;
  case 32:
    hdd->hdShift = 5;
    hdd->hdMulti = retVal;
    break;
  case 64:
    hdd->hdShift = 6;
    hdd->hdMulti = retVal;
    break;
  case 128:
    hdd->hdShift = 7;
    hdd->hdMulti = retVal;
    break;
  default:
    kprintf( "Error BLOCK Mode Unavailable: [%x]\n", retVal );
    return (1);
  }

  if ( hdd->ata_identify->command_set_enabled1 & ATA_IDENTIFY_COMMAND_SET_SUPPORTED1_48BIT_ENABLE ) {
    hdd->lba_high = hdd->ata_identify->max_48bit_lba[7] << 24;
    hdd->lba_high |= hdd->ata_identify->max_48bit_lba[6] << 16;
    hdd->lba_high |= hdd->ata_identify->max_48bit_lba[5] << 8;
    hdd->lba_high |= hdd->ata_identify->max_48bit_lba[4];

    hdd->lba_low = hdd->ata_identify->max_48bit_lba[3] << 24;
    hdd->lba_low |= hdd->ata_identify->max_48bit_lba[2] << 16;
    hdd->lba_low |= hdd->ata_identify->max_48bit_lba[1] << 8;
    hdd->lba_low |= hdd->ata_identify->max_48bit_lba[0];
  }
  else {
    hdd->lba_high = 0;
    hdd->lba_low = hdd->ata_identify->total_num_sectors[3] << 24;
    hdd->lba_low |= hdd->ata_identify->total_num_sectors[2] << 16;
    hdd->lba_low |= hdd->ata_identify->total_num_sectors[1] << 8;
    hdd->lba_low |= hdd->ata_identify->total_num_sectors[0];
  }

  // If the ATA device reports its sector size (bit 12 of Word 106), then use that instead.
  if ( hdd->ata_identify->physical_logical_sector_info & ATA_IDENTIFY_SECTOR_LARGER_THEN_512_ENABLE ) {
    hdd->sector_size = hdd->ata_identify->words_per_logical_sector[3] << 24;
    hdd->sector_size |= hdd->ata_identify->words_per_logical_sector[2] << 16;
    hdd->sector_size |= hdd->ata_identify->words_per_logical_sector[1] << 8;
    hdd->sector_size |= hdd->ata_identify->words_per_logical_sector[0];
  }
  else {
    // Default the sector size to 512 bytes
    hdd->sector_size = 512;
  }

  kprintf( "LBA [0x%X - 0x%X], LBA_HIGH: %i, LBA_LOW: %i, SECTOR_SIZE: %i\n", hdd->ata_identify->command_set_enabled1, hdd->ata_identify->command_set_enabled1 & ATA_IDENTIFY_COMMAND_SET_SUPPORTED1_48BIT_ENABLE, hdd->lba_high, hdd->lba_low, hdd->sector_size );

  outportByte( hdd->hdPort + hdSecCount, retVal );
  outportByte( hdd->hdPort + hdHead, hdd->hdDev );
  outportByte( hdd->hdPort + hdCmd, 0xC6 );

  hdd->hdMask = retVal;
  //hdd->hdSize = (hdd->hdSector[0x7B] * 256 * 256 * 256) + (hdd->hdSector[0x7A] * 256 * 256) + (hdd->hdSector[0x79] * 256) + hdd->hdSector[0x78];

  // MrOlsen (2016-01-11) NOTE: Must phase out hdSize
  hdd->hdSize = hdd->lba_low + hdd->lba_high;
  hdd->hdEnable = 1;

  kprintf( "Drive: [0x%X/0x%X], Size: [%i Sectors/%i MB]\n", hdd->hdPort, hdd->hdDev, hdd->hdSize, hdd->hdSize / 2048 );
  dev->devInfo->size = hdd->hdSize * 512;
  dev->devInfo->initialized = 0x1;
  return (0x0);
}

int hdWrite( struct driveInfo *hdd, void *baseAddr, uInt32 startSector, uInt32 sectorCount ) {
  long counter = 0x0;
  long retVal = 0x0;
  short transactionCount = 0x0;
  short *tmp = (short *) baseAddr;
  //startSector += hdd->parOffset;
  startSector += hdd->lba_start;

  if ( hdd->hdEnable == 0x0 ) {
    kprintf( "Invalid Drive\n" );
    return(1);
  }
  if ( (sectorCount >> hdd->hdShift) == 0x0 ) {
    hdd->hdCalc = sectorCount; /* hdd->hdMask; */
    transactionCount = 1;
  }
  else {
    hdd->hdCalc = hdd->hdMulti;
    transactionCount = sectorCount >> hdd->hdShift;
  }
  for ( ; transactionCount > 0; transactionCount-- ) {
    for ( counter = 1000000; counter >= 0; counter-- ) {
      retVal = inportByte( hdd->hdPort + hdStat ) & 0x80;
      if ( !retVal )
        goto ready;
    }
    kprintf( "Time Out Waiting On Drive\n" );
    return(1);
    ready: outportByte( hdd->hdPort + hdSecCount, hdd->hdCalc );
    outportByte( hdd->hdPort + hdSecNum, (startSector & 0xFF) );
    retVal = startSector >> 8;
    outportByte( hdd->hdPort + hdCylLow, (retVal & 0xFF) );
    retVal >>= 8;
    outportByte( hdd->hdPort + hdCylHi, (retVal & 0xFF) );
    retVal >>= 8;
    retVal &= 0x0F;
    retVal |= (hdd->hdDev | 0xA0); //Test As Per TJ
    outportByte( hdd->hdPort + hdHead, (retVal & 0xFF) );
    if ( hdd->hdShift > 0 )
      outportByte( hdd->hdPort + hdCmd, 0xC5 );
    else
      outportByte( hdd->hdPort + hdCmd, 0x30 );
    for ( counter = 1000000; counter >= 0; counter-- ) {
      retVal = inportByte( hdd->hdPort + hdStat );
      if ( (retVal & 1) != 0x0 ) {
        kprintf( "HD Write Error\n" );
        return(1);
      }
      if ( (retVal & 8) != 0x0 ) {
        goto go;
      }
    }
    kprintf( "Time Out Waiting On Drive\n" );
    return(1);
    go: for ( counter = 0; counter < (hdd->hdCalc << 8); counter++ ) {
      outportWord( hdd->hdPort + hdData, (short) tmp[counter] );
    }
    tmp += (counter + 0);
    startSector += hdd->hdCalc;
  }
  return(0);
}

int hdRead( struct driveInfo *hdd, void *baseAddr, uInt32 startSector, uInt32 sectorCount ) {
  long counter = 0x0;
  long retVal = 0x0;
  short transactionCount = 0x0;
  short *tmp = (short *) baseAddr;
  //startSector += hdd->parOffset;
  startSector += hdd->lba_start;

  if ( hdd->hdEnable == 0x0 ) {
    kprintf( "Invalid Drive\n" );
    return(1);
  }
  if ( (sectorCount >> hdd->hdShift) == 0x0 ) {
    hdd->hdCalc = sectorCount; /* hdd->hdMask); */
    transactionCount = 1;
  }
  else {
    hdd->hdCalc = hdd->hdMulti;
    transactionCount = sectorCount >> hdd->hdShift;
  }
  for ( ; transactionCount > 0; transactionCount-- ) {
    for ( counter = 1000000; counter >= 0; counter-- ) {
      retVal = inportByte( hdd->hdPort + hdStat ) & 0x80;
      if ( !retVal )
        goto ready;
    }
    kprintf( "Time Out Waiting On Drive\n" );
    return(1);
    ready: outportByte( hdd->hdPort + hdSecCount, hdd->hdCalc );
    outportByte( hdd->hdPort + hdSecNum, (startSector & 0xFF) );
    retVal = startSector >> 8;
    outportByte( hdd->hdPort + hdCylLow, (retVal & 0xFF) );
    retVal >>= 8;
    outportByte( hdd->hdPort + hdCylHi, (retVal & 0xFF) );
    retVal >>= 8;
    retVal &= 0x0F;
    retVal |= (hdd->hdDev | 0xA0); //Test as per TJ
    //retVal |= hdd->hdDev; //retVal |= (hdd->hdDev | 0xA0); //Test as per TJ
    outportByte( hdd->hdPort + hdHead, (retVal & 0xFF) );
    if ( hdd->hdShift > 0 )
      outportByte( hdd->hdPort + hdCmd, 0xC4 );
    else
      outportByte( hdd->hdPort + hdCmd, 0x20 );
    for ( counter = 1000000; counter >= 0; counter-- ) {
      retVal = inportByte( hdd->hdPort + hdStat );
      if ( (retVal & 1) != 0x0 ) {
        kprintf( "HD Read Error: [%i:0x%X:%i]\n", counter, (uInt32) baseAddr, startSector );
        return(1);
      }
      if ( (retVal & 8) != 0x0 ) {
        goto go;
      }
    }
    kprintf( "Error: Time Out Waiting On Drive\n" );
    return(1);
    go: for ( counter = 0; counter < (hdd->hdCalc << 8); counter++ ) {
      tmp[counter] = inportWord( hdd->hdPort + hdData );
    }
    tmp += (counter + 0);
    startSector += hdd->hdCalc;
  }
  return (0);
}

/***
 $Log: hd.c,v $
 Revision 1.5  2006/10/12 15:00:26  reddawg
 More changes

 Revision 1.4  2006/10/10 14:14:01  reddawg
 UFS Reading

 Revision 1.3  2006/10/09 02:58:05  reddawg
 Fixing UFS

 Revision 1.2  2006/10/06 15:48:01  reddawg
 Starting to make ubixos work with UFS2

 Revision 1.1.1.1  2006/06/01 12:46:16  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:24:34  reddawg
 no message

 Revision 1.16  2004/08/26 22:51:19  reddawg
 TCA touched me :( i think he likes men....


 sched.h:        kTask_t added parentPid
 endtask.c:     fixed term back to parentPid
 exec.c:          cleaned warnings
 fork.c:            fixed term to childPid
 sched.c:         clean up for dead tasks
 systemtask.c: clean up dead tasks
 kmalloc.c:       cleaned up warnings
 udp.c:            cleaned up warnings
 bot.c:             cleaned up warnings
 shell.c:           cleaned up warnings
 tcpdump.c:     took a dump
 hd.c:             cleaned up warnings
 ubixfs.c:        stopped prning debug info

 Revision 1.15  2004/08/15 00:33:02  reddawg
 Wow the ide driver works again

 Revision 1.14  2004/08/14 21:56:44  reddawg
 Added initialized byte to the device system to make it easy to add child devices which use parent hardware.

 Revision 1.13  2004/08/02 11:43:17  reddawg
 Fixens

 Revision 1.12  2004/07/21 10:02:09  reddawg
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

 Revision 1.11  2004/05/19 23:36:52  reddawg
 Bug Fixes

 Revision 1.10  2004/05/19 15:20:06  reddawg
 Fixed reference problems due to changes in drive subsystem

 Revision 1.9  2004/05/19 15:07:59  reddawg
 Typo defInfo should of been devInfo

 Revision 1.7  2004/05/19 04:07:43  reddawg
 kmalloc(size,pid) no more it is no kmalloc(size); the way it should of been

 Revision 1.6  2004/04/28 21:10:40  reddawg
 Lots Of changes to make it work with existing os

 Revision 1.5  2004/04/28 02:37:34  reddawg
 More updates for using the new driver subsystem

 Revision 1.4  2004/04/28 02:22:54  reddawg
 This is a fiarly large commit but we are starting to use new driver model
 all around

 Revision 1.3  2004/04/27 21:05:19  reddawg
 Updating drivers to use new model

 Revision 1.2  2004/04/26 22:22:33  reddawg
 DevFS now uses correct size of device

 Revision 1.1.1.1  2004/04/15 12:07:16  reddawg
 UbixOS v1.0

 Revision 1.12  2004/04/13 16:36:33  reddawg
 Changed our copyright, it is all now under a BSD-Style license

 END
 ***/
