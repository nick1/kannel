/* ==================================================================== 
 * The Kannel Software License, Version 1.0 
 * 
 * Copyright (c) 2001-2003 Kannel Group  
 * Copyright (c) 1998-2001 WapIT Ltd.   
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 * 
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in 
 *    the documentation and/or other materials provided with the 
 *    distribution. 
 * 
 * 3. The end-user documentation included with the redistribution, 
 *    if any, must include the following acknowledgment: 
 *       "This product includes software developed by the 
 *        Kannel Group (http://www.kannel.org/)." 
 *    Alternately, this acknowledgment may appear in the software itself, 
 *    if and wherever such third-party acknowledgments normally appear. 
 * 
 * 4. The names "Kannel" and "Kannel Group" must not be used to 
 *    endorse or promote products derived from this software without 
 *    prior written permission. For written permission, please  
 *    contact org@kannel.org. 
 * 
 * 5. Products derived from this software may not be called "Kannel", 
 *    nor may "Kannel" appear in their name, without prior written 
 *    permission of the Kannel Group. 
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED.  IN NO EVENT SHALL THE KANNEL GROUP OR ITS CONTRIBUTORS 
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,  
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT  
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR  
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,  
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE  
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,  
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 * ==================================================================== 
 * 
 * This software consists of voluntary contributions made by many 
 * individuals on behalf of the Kannel Group.  For more information on  
 * the Kannel Group, please see <http://www.kannel.org/>. 
 * 
 * Portions of this software are based upon software originally written at  
 * WapIT Ltd., Helsinki, Finland for the Kannel project.  
 */ 

/*
 * msg.c - manipulate messages
 *
 * This file contains implementations of the functions that create, destroy,
 * pack, and unpack messages.
 *
 * Lars Wirzenius <liw@wapit.com>
 */

#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "msg.h"
#include "gwlib/gwlib.h"

/**********************************************************************
 * Prototypes for private functions.
 */

static void append_integer(Octstr *os, long i);
static void append_string(Octstr *os, Octstr *field);

static int parse_integer(long *i, Octstr *packed, int *off);
static int parse_string(Octstr **os, Octstr *packed, int *off);

static char *type_as_str(Msg *msg);


/**********************************************************************
 * Implementations of the exported functions.
 */

Msg *msg_create_real(enum msg_type type, const char *file, long line,
                     const char *func)
{
    Msg *msg;

    msg = gw_malloc_trace(sizeof(Msg), file, line, func);

    msg->type = type;
#define INTEGER(name) p->name = MSG_PARAM_UNDEFINED;
#define OCTSTR(name) p->name = NULL
#define MSG(type, stmt) { struct type *p = &msg->type; stmt }
#include "msg-decl.h"

    return msg;
}

Msg *msg_duplicate(Msg *msg)
{
    Msg *new;

    new = msg_create(msg->type);

#define INTEGER(name) p->name = q->name
#define OCTSTR(name) \
    if (q->name == NULL) p->name = NULL; \
    else p->name = octstr_duplicate(q->name);
#define MSG(type, stmt) { \
    struct type *p = &new->type; \
    struct type *q = &msg->type; \
    stmt }
#include "msg-decl.h"

    return new;
}

void msg_destroy(Msg *msg)
{
    if (msg == NULL)
        return;

#define INTEGER(name) p->name = 0
#define OCTSTR(name) octstr_destroy(p->name)
#define MSG(type, stmt) { struct type *p = &msg->type; stmt }
#include "msg-decl.h"

    gw_free(msg);
}

void msg_destroy_item(void *msg)
{
    msg_destroy(msg);
}

void msg_dump(Msg *msg, int level)
{
    debug("gw.msg", 0, "%*sMsg object at %p:", level, "", (void *) msg);
    debug("gw.msg", 0, "%*s type: %s", level, "", type_as_str(msg));
#define INTEGER(name) \
    debug("gw.msg", 0, "%*s %s.%s: %ld", level, "", t, #name, (long) p->name)
#define OCTSTR(name) \
    debug("gw.msg", 0, "%*s %s.%s:", level, "", t, #name); \
    octstr_dump(p->name, level + 1)
#define MSG(tt, stmt) \
    if (tt == msg->type) \
        { char *t = #tt; struct tt *p = &msg->tt; stmt }
#include "msg-decl.h"
    debug("gw.msg", 0, "Msg object ends.");
}


enum msg_type msg_type(Msg *msg)
{
    return msg->type;
}

Octstr *msg_pack(Msg *msg)
{
    Octstr *os;

    os = octstr_create("");
    append_integer(os, msg->type);

#define INTEGER(name) append_integer(os, p->name)
#define OCTSTR(name) append_string(os, p->name)
#define MSG(type, stmt) \
    case type: { struct type *p = &msg->type; stmt } break;

    switch (msg->type) {
#include "msg-decl.h"
    default:
        panic(0, "Internal error: unknown message type %d",
              msg->type);
    }

    return os;
}


Msg *msg_unpack_real(Octstr *os, const char *file, long line, const char *func)
{
    Msg *msg;
    int off;
    long i;

    msg = msg_create_real(0, file, line, func);
    if (msg == NULL)
        goto error;

    off = 0;

    if (parse_integer(&i, os, &off) == -1)
        goto error;
    msg->type = i;

#define INTEGER(name) \
    if (parse_integer(&(p->name), os, &off) == -1) goto error
#define OCTSTR(name) \
    if (parse_string(&(p->name), os, &off) == -1) goto error
#define MSG(type, stmt) \
    case type: { struct type *p = &(msg->type); stmt } break;

    switch (msg->type) {
#include "msg-decl.h"
    default:
        panic(0, "Internal error: unknown message type: %d",
              msg->type);
    }

    return msg;

error:
    error(0, "Msg packet was invalid.");
    msg_destroy(msg);
    return NULL;
}


/**********************************************************************
 * Implementations of private functions.
 */


static void append_integer(Octstr *os, long i)
{
    unsigned char buf[4];

    encode_network_long(buf, i);
    octstr_append_data(os, buf, 4);
}

static void append_string(Octstr *os, Octstr *field)
{
    if (field == NULL)
        append_integer(os, -1);
    else {
        append_integer(os, octstr_len(field));
        octstr_insert(os, field, octstr_len(os));
    }
}


static int parse_integer(long *i, Octstr *packed, int *off)
{
    unsigned char buf[4];

    gw_assert(*off >= 0);
    if (*off + 4 > octstr_len(packed)) {
        error(0, "Packet too short while unpacking Msg.");
        return -1;
    }

    octstr_get_many_chars(buf, packed, *off, 4);
    *i = decode_network_long(buf);
    *off += 4;
    return 0;
}


static int parse_string(Octstr **os, Octstr *packed, int *off)
{
    long len;

    if (parse_integer(&len, packed, off) == -1)
        return -1;

    if (len == -1) {
        *os = NULL;
        return 0;
    }

    /* XXX check that len is ok */

    *os = octstr_copy(packed, *off, len);
    if (*os == NULL)
        return -1;
    *off += len;

    return 0;
}


static char *type_as_str(Msg *msg)
{
    switch (msg->type) {
#define MSG(t, stmt) case t: return #t;
#include "msg-decl.h"
    default:
        return "unknown type";
    }
}
