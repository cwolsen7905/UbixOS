/*-
 * Copyright (c) 2010 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: releng/10.2/sys/boot/common/gpt.h 213136 2010-09-24 19:49:12Z pjd $
 */

#ifndef _GPT_H_
#define	_GPT_H_

#include <sys/uuid.h>
#include <sys/device.h>
#include <lib/string.h>

#define bcopy(src, dst, len)    memcpy((dst), (src), (len))
#define bzero(buf, size)        memset((buf), 0, (size))
#define bcmp(b1, b2, len)       (memcmp((b1), (b2), (len)) != 0)

//MrOlsen (2016-01-11) NOTE: Fix This Temm Place To Store These Defines
#define        DEV_BSHIFT      9               /* log2(DEV_BSIZE) */
#define        DEV_BSIZE       (1<<DEV_BSHIFT)

struct gpt_hdr {
  char hdr_sig[8];
#define GPT_HDR_SIG             "EFI PART"
  uint32_t hdr_revision;
#define GPT_HDR_REVISION        0x00010000
  uint32_t hdr_size;
  uint32_t hdr_crc_self;
  uint32_t __reserved;
  u_int64_t hdr_lba_self;
  u_int64_t hdr_lba_alt;
  u_int64_t hdr_lba_start;
  u_int64_t hdr_lba_end;
  struct uuid hdr_uuid;
  u_int64_t hdr_lba_table;
  uint32_t hdr_entries;
  uint32_t hdr_entsz;
  uint32_t hdr_crc_table;
  /*
   * The header as defined in the EFI spec is not a multiple of 8 bytes
   * and given that the alignment requirement is on an 8 byte boundary,
   * padding will happen. We make the padding explicit so that we can
   * correct the value returned by sizeof() when we put the size of the
   * header in field hdr_size, or otherwise use offsetof().
   */
  uint32_t padding;
};

struct gpt_ent {
  struct uuid ent_type;
  struct uuid ent_uuid;
  u_int64_t ent_lba_start;
  u_int64_t ent_lba_end;
  u_int64_t ent_attr;
#define GPT_ENT_ATTR_PLATFORM           (1ULL << 0)
#define GPT_ENT_ATTR_BOOTME             (1ULL << 59)
#define GPT_ENT_ATTR_BOOTONCE           (1ULL << 58)
#define GPT_ENT_ATTR_BOOTFAILED         (1ULL << 57)
  u_int16_t ent_name[36]; /* UTF-16. */
};

#define GPT_ENT_TYPE_UNUSED             \
        {0x00000000,0x0000,0x0000,0x00,0x00,{0x00,0x00,0x00,0x00,0x00,0x00}}
#define GPT_ENT_TYPE_EFI                \
        {0xc12a7328,0xf81f,0x11d2,0xba,0x4b,{0x00,0xa0,0xc9,0x3e,0xc9,0x3b}}
#define GPT_ENT_TYPE_MBR                \
        {0x024dee41,0x33e7,0x11d3,0x9d,0x69,{0x00,0x08,0xc7,0x81,0xf3,0x9f}}
#define GPT_ENT_TYPE_FREEBSD            \
        {0x516e7cb4,0x6ecf,0x11d6,0x8f,0xf8,{0x00,0x02,0x2d,0x09,0x71,0x2b}}
#define GPT_ENT_TYPE_FREEBSD_BOOT       \
        {0x83bd6b9d,0x7f41,0x11dc,0xbe,0x0b,{0x00,0x15,0x60,0xb8,0x4f,0x0f}}
#define GPT_ENT_TYPE_FREEBSD_NANDFS     \
        {0x74ba7dd9,0xa689,0x11e1,0xbd,0x04,{0x00,0xe0,0x81,0x28,0x6a,0xcf}}
#define GPT_ENT_TYPE_FREEBSD_SWAP       \
        {0x516e7cb5,0x6ecf,0x11d6,0x8f,0xf8,{0x00,0x02,0x2d,0x09,0x71,0x2b}}
#define GPT_ENT_TYPE_FREEBSD_UFS        \
        {0x516e7cb6,0x6ecf,0x11d6,0x8f,0xf8,{0x00,0x02,0x2d,0x09,0x71,0x2b}}
#define GPT_ENT_TYPE_FREEBSD_VINUM      \
        {0x516e7cb8,0x6ecf,0x11d6,0x8f,0xf8,{0x00,0x02,0x2d,0x09,0x71,0x2b}}
#define GPT_ENT_TYPE_FREEBSD_ZFS        \
        {0x516e7cba,0x6ecf,0x11d6,0x8f,0xf8,{0x00,0x02,0x2d,0x09,0x71,0x2b}}
#define GPT_ENT_TYPE_PREP_BOOT          \
        {0x9e1a2d38,0xc612,0x4316,0xaa,0x26,{0x8b,0x49,0x52,0x1e,0x5a,0x8b}}


/*

 #include <drv.h>
 */

 int gptread(const uuid_t *uuid, struct device_interface *devInfo, char *buf);
 int gptfind(const uuid_t *uuid, struct device_interface *devInfo, int part);
 void gptbootfailed(struct device_interface *devInfo);

#endif	/* !_GPT_H_ */
