/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
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

/*
 * ubistry — the UbixOS registry.  A hierarchical, path-addressed tree of typed
 * nodes served by the /bin/ubistry daemon over MPI and persisted to
 * /var/db/ubistry.db.  This header is the public contract shared by the daemon
 * and its clients: the MPI mailbox, the wire protocol, the value types, and the
 * client-library API.
 *
 * Paths look like "/views/startmenu/0/label"; "/" is the root.  A node is
 * either a container (has children) or a leaf carrying a typed scalar.
 */

#ifndef _UBISTRY_UBISTRY_H
#define _UBISTRY_UBISTRY_H

#include <stdint.h>

#define UBISTRY_MBOX "ubistry"
#define UBISTRY_DB "/var/db/ubistry.db"

/* Size caps — chosen so every request/response fits the 248-byte MPI payload. */
#define UB_MBOX_MAX 64   /* reply mailbox name */
#define UB_PATH_MAX 120  /* node path */
#define UB_VAL_MAX 120   /* scalar value as text */
#define UB_NAME_MAX 64   /* single path component / node name */
#define UB_NAMES_MAX 224 /* ENUM reply: '\n'-joined child names */

/* Value types.  UB_CONTAINER nodes hold children rather than a scalar. */
typedef enum
{
	UB_CONTAINER = 0,
	UB_STR = 1,
	UB_INT = 2,
	UB_BOOL = 3
} ub_type_t;

/* MPI message headers (mpi_message_t.header). */
#define UB_MSG_GET 0x101      /* client -> daemon: read a value */
#define UB_MSG_VALUE 0x102    /* daemon -> client: value reply */
#define UB_MSG_SET 0x103      /* client -> daemon: write a value (no reply) */
#define UB_MSG_ENUM 0x104     /* client -> daemon: list children */
#define UB_MSG_CHILDREN 0x105 /* daemon -> client: children reply */
#define UB_MSG_DEL 0x106      /* client -> daemon: delete a subtree (no reply) */

/* UB_MSG_GET / UB_MSG_ENUM request payload. */
struct ub_query_req
{
	char reply_mbox[UB_MBOX_MAX];
	char path[UB_PATH_MAX];
	uint32_t seq; /* client request id, echoed in the reply for correlation */
};

/* UB_MSG_VALUE reply payload. */
struct ub_value_rsp
{
	int32_t ok;   /* 1 = found, 0 = not found */
	uint8_t type; /* ub_type_t of the value */
	char value[UB_VAL_MAX];
	uint32_t seq; /* echoes ub_query_req.seq so the client matches reply↔request */
};

/* UB_MSG_SET request payload. */
struct ub_set_req
{
	uint8_t type; /* ub_type_t */
	char path[UB_PATH_MAX];
	char value[UB_VAL_MAX];
};

/* UB_MSG_CHILDREN reply payload. */
struct ub_children_rsp
{
	int32_t count;     /* number of children (may exceed what fits in names) */
	uint8_t truncated; /* 1 = names[] does not list every child */
	char names[UB_NAMES_MAX];
	uint32_t seq; /* echoes ub_query_req.seq so the client matches reply↔request */
};

/* UB_MSG_DEL request payload. */
struct ub_del_req
{
	char path[UB_PATH_MAX];
};

/*
 * Client library (lib/ubix_api).  Each call round-trips through the daemon over
 * MPI; all are best-effort and return a negative value if the daemon is not
 * reachable so callers can fall back to defaults.
 */
#ifdef __cplusplus
extern "C"
{
#endif

	/* Read a string value into buf.  @return 0 on success, -1 if missing/error. */
	int ubistry_get_str(const char *path, char *buf, int len);

	/* Read an integer (or bool) value.  @return 0 on success, -1 if missing/error. */
	int ubistry_get_int(const char *path, int *out);

	/* Create/update a value.  @return 0 on success, -1 on error. */
	int ubistry_set_str(const char *path, const char *val);
	int ubistry_set_int(const char *path, int val);

	/*
	 * List the child node names of a container into names_buf as a '\n'-separated
	 * list.  @return the child count (>=0), or -1 on error.
	 */
	int ubistry_enum(const char *path, char *names_buf, int len);

	/* Delete a node and its subtree.  @return 0 on success, -1 on error. */
	int ubistry_del(const char *path);

	/*
	 * Layered (system/user) access.  Settings live at a bare "key" path
	 * (e.g. "views/desktop/mode") as the machine-wide default; a user's
	 * override is stored at "/users/<name>/<key>".  The get_for() calls
	 * resolve user-first then fall back to the system default; set_user()
	 * writes the per-user override.  Pass a NULL/empty user to use the system
	 * layer only.  key has NO leading slash.
	 */
	int ubistry_get_for(const char *user, const char *key, char *buf, int len);
	int ubistry_get_for_int(const char *user, const char *key, int *out);
	int ubistry_set_user(const char *user, const char *key, const char *val);
	int ubistry_set_user_int(const char *user, const char *key, int val);

#ifdef __cplusplus
}
#endif

#endif /* _UBISTRY_UBISTRY_H */
