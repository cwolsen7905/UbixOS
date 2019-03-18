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

int main(int argc, char **argv) {
  FILE *fd;
  struct ubixDiskLabel *d = (struct ubixDiskLabel *) malloc(512);
  int i = 0x0;
  char buf[256];

  printf("Ubix Disk Label Editor Version 1.0\n");
  printf("(c) 2004 Ubix Corp              \n\n");

  if (argc >= 2) {
    printf("Drive Info (%s):\n", argv[1]);
    fd = fopen(argv[1], "rb");
  }
  else {
    printf("Drive Info (hd0):\n");
    fd = fopen("hd0@devfs", "rb");
  }
  fseek(fd, 512, 0);
  fread(d, 512, 1, fd);

  if (argc >= 3) {
    i = atoi(argv[2]);
    printf("d->partitions[%i].p_size   = %i, ", i, d->partitions[i].p_size);
    printf("New Value: ");
    gets((char *) &buf);
    d->partitions[i].p_size = atoi(buf);
    printf("d->partitions[%i].p_offset = %i, ", i, d->partitions[i].p_offset);
    printf("New Value: ");
    gets((char *) &buf);
    d->partitions[i].p_offset = atoi(buf);
    printf("d->partitions[%i].p_fstype = %i, ", i, d->partitions[i].p_fstype);
    printf("New Value: ");
    gets((char *) &buf);
    d->partitions[i].p_fstype = atoi(buf);
    printf("d->partitions[%i].p_bsize  = %i, ", i, d->partitions[i].p_bsize);
    printf("New Value: ");
    gets((char *) &buf);
    d->partitions[i].p_bsize = atoi(buf);
    printf("\n");
    printf("d->partitions[%i].p_size   = %i\n", i, d->partitions[i].p_size);
    printf("d->partitions[%i].p_offset = %i\n", i, d->partitions[i].p_offset);
    printf("d->partitions[%i].p_fstype = %i\n", i, d->partitions[i].p_fstype);
    printf("d->partitions[%i].p_bsize  = %i\n", i, d->partitions[i].p_bsize);
    fseek(fd, 512, 0);
    fwrite(d, 512, 1, fd);
  }
  else {
    for (i = 0; i < 4; i++) {
      if (d->partitions[i].p_fstype != 0x0) {
        printf("d->partitions[%i].p_size   = %i\n", i, d->partitions[i].p_size);
        printf("d->partitions[%i].p_offset = %i\n", i, d->partitions[i].p_offset);
        printf("d->partitions[%i].p_fstype = 0x%X\n", i, d->partitions[i].p_fstype);
        printf("d->partitions[%i].p_bsize  = 0x%X\n", i, d->partitions[i].p_bsize);
      }
    }
  }

  fclose(fd);

  return (0);
}

/***
 $Log: main.c,v $
 Revision 1.1.1.1  2006/06/01 12:46:09  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:28  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:13:58  reddawg
 no message

 Revision 1.3  2004/06/01 01:30:43  reddawg
 No more warnings and organized make files

 Revision 1.2  2004/05/24 13:42:29  reddawg
 Clean Up


 END
 ***/
