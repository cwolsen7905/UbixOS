/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
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

#ifndef _AUTHD_H
#define _AUTHD_H

/*
 * authd IPC protocol.
 *
 * All authentication requests are sent to the "authd" MPI mailbox.
 * The caller supplies a reply_mbox name; authd posts the response there.
 *
 * Both structs must fit within mpi_message_t.data[248].
 *   auth_request:  32+32+32 = 96 bytes
 *   auth_response: 4+4+4+80+128 = 220 bytes
 */

#define AUTHD_MBOX        "authd"

#define AUTHD_MSG_REQUEST  1
#define AUTHD_MSG_RESPONSE 2

#define AUTH_MBOX_MAX     32
#define AUTH_USER_MAX     32
#define AUTH_PASS_MAX     32
#define AUTH_SHELL_MAX    80
#define AUTH_HOME_MAX     128

struct auth_request {
    char reply_mbox[AUTH_MBOX_MAX]; /* mailbox authd should reply to */
    char username[AUTH_USER_MAX];
    char password[AUTH_PASS_MAX];
};

struct auth_response {
    int  ok;                        /* 1 = authenticated, 0 = rejected */
    int  uid;
    int  gid;
    char shell[AUTH_SHELL_MAX];
    char home[AUTH_HOME_MAX];
};

#endif
