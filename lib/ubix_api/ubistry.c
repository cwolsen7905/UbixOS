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
 *
 * ubistry.c — client wrappers for the ubistry registry (part of ubix_api).
 * Each call round-trips a request to the /bin/ubistry daemon over MPI; GET/ENUM
 * wait for a reply on a per-process reply mailbox, SET/DEL are fire-and-forget.
 * All calls are best-effort: if the daemon is unreachable they return an error
 * so the caller can fall back to defaults rather than hang.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <sys/mpi.h>
#include <ubistry/ubistry.h>

/* Yields to wait for a reply before giving up (daemon down → fall back). */
#define UB_WAIT_SPINS 200000

static char g_reply[UB_MBOX_MAX];
static int g_reply_ready = 0;

/**
 * Lazily create this process's reply mailbox ("ubistry.r.<pid>") and return it.
 */
static const char *reply_mbox(void)
{
	if (!g_reply_ready)
	{
		snprintf(g_reply, sizeof(g_reply), "ubistry.r.%d", (int)getpid());
		mpi_createMbox(g_reply); /* harmless if it already exists */
		g_reply_ready = 1;
	}
	return (g_reply);
}

/**
 * Wait for a reply with the wanted header on our reply mailbox.
 * @return 0 on success, -1 if the daemon never answers.
 */
static int wait_reply(uint32_t want, mpi_message_t *out)
{
	int spins = 0;

	for (;;)
	{
		if (mpi_fetchMessage(g_reply, out) == 0)
		{
			if (out->header == want)
				return (0);
			/* stale/unexpected reply — keep waiting */
		}
		else
		{
			if (++spins > UB_WAIT_SPINS)
				return (-1);
			sched_yield();
		}
	}
}

int ubistry_get_str(const char *path, char *buf, int len)
{
	mpi_message_t msg, rep;
	struct ub_query_req *q = (struct ub_query_req *)msg.data;
	struct ub_value_rsp *r = (struct ub_value_rsp *)rep.data;

	if (path == NULL || buf == NULL || len <= 0)
		return (-1);

	reply_mbox();
	memset(&msg, 0, sizeof(msg));
	snprintf(q->reply_mbox, sizeof(q->reply_mbox), "%s", g_reply);
	snprintf(q->path, sizeof(q->path), "%s", path);
	msg.header = UB_MSG_GET;
	/* If the registry daemon's mailbox doesn't exist (e.g. no ubistry running),
	 * the post fails — bail immediately instead of spinning UB_WAIT_SPINS for a
	 * reply that will never come.  Callers fall back to their defaults. */
	if (mpi_postMessage(UBISTRY_MBOX, UB_MSG_GET, &msg) != 0)
		return (-1);

	if (wait_reply(UB_MSG_VALUE, &rep) != 0 || !r->ok)
		return (-1);
	snprintf(buf, (size_t)len, "%s", r->value);
	return (0);
}

int ubistry_get_int(const char *path, int *out)
{
	char buf[UB_VAL_MAX];

	if (ubistry_get_str(path, buf, (int)sizeof(buf)) != 0)
		return (-1);
	if (out != NULL)
	{
		if (strcmp(buf, "true") == 0)
			*out = 1;
		else if (strcmp(buf, "false") == 0)
			*out = 0;
		else
			*out = atoi(buf);
	}
	return (0);
}

/**
 * Post a fire-and-forget SET with the given type and value text.
 */
static int set_raw(const char *path, ub_type_t type, const char *value)
{
	mpi_message_t msg;
	struct ub_set_req *s = (struct ub_set_req *)msg.data;

	if (path == NULL)
		return (-1);
	memset(&msg, 0, sizeof(msg));
	s->type = (uint8_t)type;
	snprintf(s->path, sizeof(s->path), "%s", path);
	snprintf(s->value, sizeof(s->value), "%s", value ? value : "");
	msg.header = UB_MSG_SET;
	mpi_postMessage(UBISTRY_MBOX, UB_MSG_SET, &msg);
	return (0);
}

int ubistry_set_str(const char *path, const char *val)
{
	return (set_raw(path, UB_STR, val));
}

int ubistry_set_int(const char *path, int val)
{
	char buf[24];

	snprintf(buf, sizeof(buf), "%d", val);
	return (set_raw(path, UB_INT, buf));
}

int ubistry_enum(const char *path, char *names_buf, int len)
{
	mpi_message_t msg, rep;
	struct ub_query_req *q = (struct ub_query_req *)msg.data;
	struct ub_children_rsp *r = (struct ub_children_rsp *)rep.data;

	if (path == NULL || names_buf == NULL || len <= 0)
		return (-1);

	reply_mbox();
	memset(&msg, 0, sizeof(msg));
	snprintf(q->reply_mbox, sizeof(q->reply_mbox), "%s", g_reply);
	snprintf(q->path, sizeof(q->path), "%s", path);
	msg.header = UB_MSG_ENUM;
	if (mpi_postMessage(UBISTRY_MBOX, UB_MSG_ENUM, &msg) != 0)
		return (-1); /* no registry daemon — don't spin waiting for a reply */

	if (wait_reply(UB_MSG_CHILDREN, &rep) != 0)
		return (-1);
	snprintf(names_buf, (size_t)len, "%s", r->names);
	return (r->count);
}

int ubistry_del(const char *path)
{
	mpi_message_t msg;
	struct ub_del_req *d = (struct ub_del_req *)msg.data;

	if (path == NULL)
		return (-1);
	memset(&msg, 0, sizeof(msg));
	snprintf(d->path, sizeof(d->path), "%s", path);
	msg.header = UB_MSG_DEL;
	mpi_postMessage(UBISTRY_MBOX, UB_MSG_DEL, &msg);
	return (0);
}

int ubistry_get_for(const char *user, const char *key, char *buf, int len)
{
	char path[UB_PATH_MAX];

	if (key == NULL)
		return (-1);
	if (user != NULL && user[0] != '\0')
	{
		snprintf(path, sizeof(path), "/users/%s/%s", user, key);
		if (ubistry_get_str(path, buf, len) == 0)
			return (0);
	}
	snprintf(path, sizeof(path), "/%s", key);
	return (ubistry_get_str(path, buf, len));
}

int ubistry_get_for_int(const char *user, const char *key, int *out)
{
	char buf[UB_VAL_MAX];

	if (ubistry_get_for(user, key, buf, (int)sizeof(buf)) != 0)
		return (-1);
	if (out != NULL)
	{
		if (strcmp(buf, "true") == 0)
			*out = 1;
		else if (strcmp(buf, "false") == 0)
			*out = 0;
		else
			*out = atoi(buf);
	}
	return (0);
}

int ubistry_set_user(const char *user, const char *key, const char *val)
{
	char path[UB_PATH_MAX];

	if (user == NULL || user[0] == '\0' || key == NULL)
		return (-1);
	snprintf(path, sizeof(path), "/users/%s/%s", user, key);
	return (ubistry_set_str(path, val));
}

int ubistry_set_user_int(const char *user, const char *key, int val)
{
	char b[24];

	snprintf(b, sizeof(b), "%d", val);
	return (ubistry_set_user(user, key, b));
}
