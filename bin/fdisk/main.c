/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sys.h>
#include <string.h>

#include "ubixfs.h"

struct dos_partition {
  unsigned char dp_flag; /* bootstrap flags */
  unsigned char dp_shd; /* starting head */
  unsigned char dp_ssect; /* starting sector */
  unsigned char dp_scyl; /* starting cylinder */
  unsigned char dp_type; /* partition type */
  unsigned char dp_ehd; /* end head */
  unsigned char dp_esect; /* end sector */
  unsigned char dp_ecyl; /* end cylinder */
  uint32_t dp_start; /* absolute starting sector number */
  uint32_t dp_size; /* partition size in sectors */
};

int main(int argc, char **argv) {
  FILE *fd;
  FILE *mbr;
  struct dos_partition *d = 0x0;
  char *data = (char *) malloc(512);
  int i = 0x0;
  char buf[256];

  d = (struct dos_partition *) (data + 0x1BE);

  printf("Ubix Disk Editor Version 1.0\n");
  printf("(c) 2004 Ubix Corp        \n\n");

  if (argc >= 2) {
    printf("Drive Info (%s):\n", argv[1]);
    fd = fopen(argv[1], "rb");
  }
  else {
    printf("Drive Info (ad0):\n");
    fd = fopen("devfs:ad0", "rb");
  }
  if (fd->size == 0x0) {
    printf("Invalid Device\n");
    exit(0x1);
  }

  fseek(fd, 0, 0);
  fread(data, 512, 1, fd);

  if (argc >= 3) {
    i = atoi(argv[2]);
    if (i == 0) {
      mbr = fopen("sys:mrb", "rb");
      fseek(mbr, 0, 0);
      fread(data, 512, 1, mbr);
      printf("Installing Ubix MBR\n");
    }
    else {
      i--;
      printf("d[%i].dp_type   = %i, ", i, d[i].dp_type);
      printf("New Value: ");
      gets((char *) &buf);
      d[i].dp_type = atoi(buf);
      printf("d[%i].dp_start: %i, ", i, d[i].dp_start);
      printf("New Value: ");
      gets((char *) &buf);
      d[i].dp_start = atoi(buf);
      printf("d[%i].dp_size: %i, ", i, d[i].dp_size);
      printf("New Value: ");
      gets((char *) &buf);
      d[i].dp_size = atoi(buf);
      printf("d[%i].dp_type:  0x%X\n", i, d[i].dp_type);
      printf("d[%i].dp_start: %i\n", i, d[i].dp_start);
      printf("d[%i].dp_size:  %i\n", i, d[i].dp_size);
    }
    fseek(fd, 0, 0);
    fwrite(data, 512, 1, fd);
  }
  else {
    printf("Partition Table:\n");
    for (i = 0; i < 4; i++) {
      if (d[i].dp_type != 0x0) {
        printf("d[%i].dp_type: 0x%X\n", i, d[i].dp_type);
        printf("d[%i].dp_start: %i\n", i, d[i].dp_start);
        printf("d[%i].dp_size:  %i\n", i, d[i].dp_size);
      }
    }
  }

  fclose(fd);

  return (0);
}

/***
 $Log: main.c,v $
 Revision 1.2  2006/10/12 15:00:26  reddawg
 More changes

 Revision 1.1.1.1  2006/06/01 12:46:09  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:28  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:13:58  reddawg
 no message

 Revision 1.9  2004/08/15 00:33:02  reddawg
 Wow the ide driver works again

 Revision 1.8  2004/06/01 01:30:43  reddawg
 No more warnings and organized make files

 Revision 1.7  2004/05/24 13:54:52  reddawg
 Clean Up


 END
 ***/
