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

#include <fs/vfs/mount.h>
#include <fs/vfs/vfs.h>
#include <ubixos/vitals.h>
#include <ubixos/kpanic.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <string.h>
#include <sys/bus.h>
#include <sys/descrip.h> /* g_device_find hook (path B) */

/************************************************************************

 Function: mount(int driveId,int partition,int fsType,char *mountPoint,char *perms)

 Description: mount adds a mount point and returns 0 if successful 1 if it fails

 Notes:

 ************************************************************************/
int vfs_mount( int major, int minor, int partition, int vfsType, char *mountPoint, char *perms ) {
  struct vfs_mountPoint *mp = 0x0;
  struct ubx_device *device = 0x0;

  /* If an exact mount already exists at this path, treat as success (no-op).
   * Matches FreeBSD behaviour: fstab entries for kernel-pre-mounted FSes
   * (devfs, procfs) succeed silently rather than double-mounting. */
  {
    struct vfs_mountPoint *existing = vfs_findMount(mountPoint);
    if (existing != NULL && strcmp(existing->mountPoint, mountPoint) == 0) {
      kprintf("vfs_mount: %s already mounted, skipping\n", mountPoint);
      return (0x0);
    }
  }

  /* Allocate Memory For Mount Point */
  if ( (mp = (struct vfs_mountPoint *) kmalloc( sizeof(struct vfs_mountPoint) )) == NULL ) {
    kprintf( "vfs_mount: failed to allocate mp\n" );
    return (0x1);
  }

  /* Validate filesystem type before touching the mount list */
  mp->fs = (struct fileSystem *) vfs_findFS( vfsType );
  if ( mp->fs == 0x0 ) {
    kprintf( "File System Type: %i Not Found\n", vfsType );
    kfree( mp );
    return (0x1);
  }

  /* Copy Mount Point Into Buffer */
  snprintf( mp->mountPoint, sizeof(mp->mountPoint), "%s", mountPoint );

  /* Set Pointer To Physical Drive (via the device-lookup hook; NULL on arches
   * without the device model, e.g. a deviceless procfs/devfs mount). */
  device = g_device_find ? (struct ubx_device *)g_device_find( major, minor ) : NULL;

  /* Set Up Mp Defaults */
  mp->device = device;
  mp->partition = partition;
  mp->perms = (perms && perms[0] == 'r' && perms[1] == 'w') ? 'w' : 'r';

  /* Add Mountpoint; if it fails free and return */
  if ( vfs_addMount( mp ) != 0x0 ) {
    kfree( mp );
    return (0x1);
  }

  /* Initialize The File System; if it fails remove from list and free */
  if ( mp->fs->vfsInitFS( mp ) == 0x0 ) {
    if ( systemVitals->mountPoints == mp )
      systemVitals->mountPoints = mp->next;
    if ( mp->prev )
      mp->prev->next = mp->next;
    if ( mp->next )
      mp->next->prev = mp->prev;
    kfree( mp );
    return (0x1);
  }
  /* Return */
  return (0x0);
}

/************************************************************************

 Function: vfs_addMount(struct vfs_mountPoint *mp)

 Description: This function adds a mount point to the system

 Notes:

 ************************************************************************/
int vfs_addMount( struct vfs_mountPoint *mp ) {

  /* If There Are No Existing Mounts Make It The First */
  if ( systemVitals->mountPoints == 0x0 ) {
    mp->prev = 0x0;
    mp->next = 0x0;
    systemVitals->mountPoints = mp;
  }
  else {
    mp->next = systemVitals->mountPoints;
    systemVitals->mountPoints->prev = mp;
    mp->prev = 0x0;
    systemVitals->mountPoints = mp;
  }
  /* Return */
  return (0x0);
}

/************************************************************************

 Function: vfs_findMount(char *mountPoint)

 Description: This function finds a particular mount point

 Notes:

 ************************************************************************/
/*
 * vfs_findMount — longest-prefix mount lookup.
 *
 * Given a POSIX path like "/dev/tty" or "/bin/ls", walk the mount table and
 * return the entry whose mountPoint is the longest prefix of path.  The root
 * mount "/" always matches as a fallback.  A non-root mount must be followed
 * by '/' or end-of-string to avoid false matches (e.g. "/developer" must not
 * match a "/dev" mount).
 */
struct vfs_mountPoint *vfs_findMount( const char *path ) {
  struct vfs_mountPoint *tmpMp = 0x0;
  struct vfs_mountPoint *best  = NULL;
  size_t best_len = 0;
  size_t path_len;

  if (path == NULL)
    return NULL;

  path_len = strlen(path);

  for ( tmpMp = systemVitals->mountPoints; tmpMp; tmpMp = tmpMp->next ) {
    size_t mlen = strlen(tmpMp->mountPoint);
    if (mlen > path_len)
      continue;
    if (strncmp(path, tmpMp->mountPoint, mlen) != 0)
      continue;
    /* Boundary check: root "/" matches everything; otherwise the next
     * character must be '/' or '\0'. */
    if (mlen > 1 && path[mlen] != '/' && path[mlen] != '\0')
      continue;
    if (mlen > best_len) {
      best     = tmpMp;
      best_len = mlen;
    }
  }
  return best;
}

/*
 * vfs_umount — remove a mount point by exact path.
 *
 * Returns 0 on success, -1 if no mount with that path exists.
 * Does not call a per-FS cleanup hook (none registered yet).
 */
int vfs_umount(const char *path)
{
	struct vfs_mountPoint *mp;

	if (path == NULL)
		return (-1);

	mp = vfs_findMount(path);
	if (mp == NULL || strcmp(mp->mountPoint, path) != 0)
		return (-1);

	if (mp->prev)
		mp->prev->next = mp->next;
	else
		systemVitals->mountPoints = mp->next;
	if (mp->next)
		mp->next->prev = mp->prev;

	kfree(mp);
	return (0);
}

/***
 END
 ***/

