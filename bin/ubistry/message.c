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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mpi.h>
#include <api/ubix.h>
#include <ubistry/ubistry.h>
#include "db.h"
#include "persist.h"
#include "message.h"

int ubistry_init_mbox(const char *name)
{
	if (mpi_createMbox(name) != 0)
	{
		ulogf(ULOG_ERR, "ubistry: error creating mailbox [%s]", name);
		return (-1);
	}
	return (0);
}

/**
 * Handle UB_MSG_GET: look up a value and reply UB_MSG_VALUE to reply_mbox.
 */
static void handle_get(struct ub_query_req *q)
{
	mpi_message_t rmsg;
	struct ub_value_rsp *r = (struct ub_value_rsp *)rmsg.data;
	ub_type_t t = UB_STR;
	char val[UB_VAL_MAX];

	memset(&rmsg, 0, sizeof(rmsg));
	if (ub_get(q->path, &t, val, (int)sizeof(val)) == 0)
	{
		r->ok = 1;
		r->type = (uint8_t)t;
		snprintf(r->value, sizeof(r->value), "%s", val);
	}
	else
	{
		r->ok = 0;
		r->type = (uint8_t)UB_STR;
		r->value[0] = '\0';
	}
	rmsg.header = UB_MSG_VALUE;
	if (q->reply_mbox[0] != '\0')
		mpi_postMessage(q->reply_mbox, UB_MSG_VALUE, &rmsg);
}

/**
 * Handle UB_MSG_ENUM: list children and reply UB_MSG_CHILDREN to reply_mbox.
 */
static void handle_enum(struct ub_query_req *q)
{
	mpi_message_t rmsg;
	struct ub_children_rsp *r = (struct ub_children_rsp *)rmsg.data;
	int trunc = 0;

	memset(&rmsg, 0, sizeof(rmsg));
	r->count = ub_enum(q->path, r->names, (int)sizeof(r->names), &trunc);
	r->truncated = (uint8_t)trunc;
	rmsg.header = UB_MSG_CHILDREN;
	if (q->reply_mbox[0] != '\0')
		mpi_postMessage(q->reply_mbox, UB_MSG_CHILDREN, &rmsg);
}

void ubistry_process_messages(void)
{
	mpi_message_t msg;

	while (mpi_fetchMessage(UBISTRY_MBOX, &msg) == 0)
	{
		switch (msg.header)
		{
			case UB_MSG_GET:
				handle_get((struct ub_query_req *)msg.data);
				break;
			case UB_MSG_ENUM:
				handle_enum((struct ub_query_req *)msg.data);
				break;
			case UB_MSG_SET:
			{
				struct ub_set_req *s = (struct ub_set_req *)msg.data;
				if (ub_set(s->path, (ub_type_t)s->type, s->value) == 0)
					persist_mark_dirty();
				break;
			}
			case UB_MSG_DEL:
			{
				struct ub_del_req *d = (struct ub_del_req *)msg.data;
				if (ub_delete(d->path) == 0)
					persist_mark_dirty();
				break;
			}
			default:
				/* Unknown command — ignore. */
				break;
		}
	}
}
