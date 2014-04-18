/*
 * ============================================================================
 * Copyright (c) 2014 Hardy-Francis Enterprises Inc.
 * This file is part of SharedHashFile.
 *
 * SharedHashFile is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * SharedHashFile is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see www.gnu.org/licenses/.
 * ----------------------------------------------------------------------------
 * To use SharedHashFile in a closed-source product, commercial licenses are
 * available; email office [@] sharedhashfile [.] com for more information.
 * ============================================================================
 */

#include <string.h> /* for memset() */

#include "shf.private.h"
#include "shf.h"
#include "shf.queue.h"

typedef struct SHF_QUEUE_ITEM {
#ifdef SHF_DEBUG_VERSION
    /* todo: consider adding debug magic number */
#endif
    volatile uint32_t uid_last;
    volatile uint32_t uid_next;
             uint32_t data_used; /* data[] used          */
             uint32_t data_size; /* data[] maximum size  */
             char     data[]   ;
} __attribute__((packed)) SHF_QUEUE_ITEM;

static uint32_t shf_queue_win = 0;
static uint64_t shf_queue_key = 0;

static __thread SHF_QUEUE_ITEM * shf_queue_item      = NULL;
static __thread uint32_t         shf_queue_item_size = 0;

static uint32_t /* SHF_UID; SHF_UID_NONE means something went wrong */
shf_queue_new_item_internal(
    SHF      * shf      ,
    uint32_t   data_size,
    uint32_t   force_win) /* forces item to be placed in a particular window */ // todo: is this overkill?
{
    shf_debug_verbosity_less();

#ifdef SHF_DEBUG_VERSION
    uint32_t queue_key_old = shf_queue_key;
#endif
    uint32_t win;
    do {
        shf_make_hash(SHF_CAST(const char *, &shf_queue_key), sizeof(shf_queue_key));
        shf_queue_key ++;
        win = shf_hash.u16[0] % SHF_WINS_PER_SHF;
    } while (force_win && win != shf_queue_win);

    uint32_t item_and_data_size = sizeof(SHF_QUEUE_ITEM) + data_size;
    if (item_and_data_size > shf_queue_item_size) {
        shf_queue_item_size = item_and_data_size;
        shf_queue_item = realloc(shf_queue_item, shf_queue_item_size);
        SHF_ASSERT(NULL != shf_queue_item, "ERROR: malloc(%u) failed", shf_queue_item_size);
        memset(shf_queue_item, 0, shf_queue_item_size);
               shf_queue_item->uid_last  = SHF_UID_NONE;
               shf_queue_item->uid_next  = SHF_UID_NONE;
    }
    shf_queue_item->data_size = data_size;
    uint32_t uid = shf_put_key_val(shf, SHF_CAST(const char *, shf_queue_item), item_and_data_size);

    shf_debug_verbosity_more();

    SHF_DEBUG("%s(shf=?){} // uid 0x%08x; found win %u by burning %lu queue keys\n", __FUNCTION__, uid, shf_queue_win, shf_queue_key - queue_key_old);

    shf_queue_win ++;
    shf_queue_win = shf_queue_win >= SHF_WINS_PER_SHF ? 0 : shf_queue_win;

    return uid;
} /* shf_queue_new_item() */

uint32_t /* SHF_UID; SHF_UID_NONE means something went wrong */
shf_queue_new_item(
    SHF      * shf      ,
    uint32_t   data_size)
{
    return shf_queue_new_item_internal(shf, data_size, 0 /* do not force win */);
} /* shf_queue_new_item() */

#define SHF_QUEUE_GET_UID_VAL_ADDR(ADDR,UID) \
    shf_debug_verbosity_less(); \
    SHF_QUEUE_ITEM * ADDR = shf_get_uid_val_addr(shf, UID); \
    SHF_ASSERT(NULL != ADDR, "ERROR: could not find addr for " #UID ": 0x%08x", UID); \
    shf_debug_verbosity_more();

void
shf_queue_put_item(
    SHF        * shf     ,
    uint32_t     uid_item,
    const char * data    ,
    uint32_t     data_len)
{
    SHF_DEBUG("%s(shf=?, uid_item=0x%08x, data='%.*s', data_len=%u){}\n", __FUNCTION__, uid_item, data_len, data, data_len);

    shf_debug_verbosity_less();

    SHF_QUEUE_GET_UID_VAL_ADDR(item, uid_item);
    SHF_ASSERT(SHF_UID_NONE == item->uid_next, "ERROR: expected uid_item 0x%08x to have ->uid_next SHF_UID_NONE", uid_item);
    SHF_ASSERT(SHF_UID_NONE == item->uid_last, "ERROR: expected uid_item 0x%08x to have ->uid_last SHF_UID_NONE", uid_item);

    shf_debug_verbosity_more();

    SHF_ASSERT(item->data_size >= data_len, "ERROR: tried to put %u bytes into item with only %u bytes", data_len, item->data_size);
    item->data_used = data_len;
    memcpy(&item->data[0], data, data_len);
} /* shf_queue_put_item() */

typedef struct SHF_QUEUE_NAME {
    uint32_t uid;
} __attribute__((packed)) SHF_QUEUE_NAME;

typedef struct SHF_QUEUE_NAME_ITEM {
    SHF_LOCK       lock;
    SHF_QUEUE_ITEM tail;
    uint32_t       name_len;
    char           name[];
} __attribute__((packed)) SHF_QUEUE_NAME_ITEM;

uint32_t /* SHF_UID; SHF_UID_NONE means something went wrong */
shf_queue_new_name(
    SHF        * shf     ,
    const char * name    ,
    uint32_t     name_len)
{
    shf_debug_verbosity_less();

    uint32_t              name_item_len  = sizeof(SHF_QUEUE_NAME_ITEM) + name_len;
    SHF_QUEUE_NAME        queue_name;
                          queue_name.uid = shf_queue_new_item_internal(shf, name_item_len, 1 /* force win */);
    SHF_QUEUE_ITEM      * item           = shf_get_uid_val_addr(shf, queue_name.uid);
    SHF_QUEUE_NAME_ITEM * name_item      = SHF_CAST(SHF_QUEUE_NAME_ITEM *, &item->data[0]);
    SHF_ASSERT(name_item_len == item->data_size, "INTERNAL: expected item->data_size %u but got %u", name_item_len, item->data_size);
            name_item->tail.uid_last = SHF_UID_NONE;
            name_item->tail.uid_next = SHF_UID_NONE;
            name_item->name_len      = name_len;
    memcpy(&name_item->name[0], name, name_len); /* copy queue name */

    shf_make_hash(name, name_len);
    shf_put_key_val(shf, SHF_CAST(const char *, &queue_name), sizeof(queue_name));

    shf_debug_verbosity_more();

    SHF_DEBUG("%s(shf=?, name='%.*s', name_len=%u){} // uid 0x%08x\n", __FUNCTION__, name_len, name, name_len, queue_name.uid);

    return queue_name.uid;
} /* shf_queue_new_name() */

uint32_t /* SHF_UID; SHF_UID_NONE means something went wrong */
shf_queue_get_name(
    SHF        * shf     ,
    const char * name    ,
    uint32_t     name_len)
{
    shf_debug_verbosity_less();

    shf_make_hash(name, name_len);
    SHF_QUEUE_NAME * queue_name = shf_get_key_val_addr(shf); SHF_ASSERT(NULL != queue_name, "ERROR: could not find addr for queue: %.*s", name_len, name);

    shf_debug_verbosity_more();

    SHF_DEBUG("%s(shf=?, name='%.*s', name_len=%u){} // return queue_uid=0x%08x\n", __FUNCTION__, name_len, name, name_len, queue_name->uid);

    return queue_name->uid;
} /* shf_queue_get_name() */

// before:                  tail     last              head
// before:                  ----     ----              ----
// before: SHF_UID_NONE <-- last <-- last          <-- last
// before:                  next --> next -->          next --> SHF_UID_NONE

// after :                  tail     next     item     head
// after :                  ----     ----     ----     ----
// after : SHF_UID_NONE <-- last <-- last <-- last <-- last
// after :                  next --> next --> next --> next --> SHF_UID_NONE

void
shf_queue_push_head( /* note: safe before shf_freeze() if single threaded access */
    SHF      * shf     ,
    uint32_t   uid_head,
    uint32_t   uid_item)
{
#ifdef SHF_DEBUG_VERSION
    const char * debug = "";
#endif

    SHF_QUEUE_GET_UID_VAL_ADDR(item, uid_item);
    SHF_ASSERT(SHF_UID_NONE == item->uid_next, "ERROR: expected uid_item 0x%08x to have ->uid_next SHF_UID_NONE", uid_item);
    SHF_ASSERT(SHF_UID_NONE == item->uid_last, "ERROR: expected uid_item 0x%08x to have ->uid_last SHF_UID_NONE", uid_item);

    SHF_QUEUE_GET_UID_VAL_ADDR(head, uid_head);
    SHF_QUEUE_NAME_ITEM * name_item = SHF_CAST(SHF_QUEUE_NAME_ITEM *, &head->data[0]);
    shf_debug_verbosity_less(); SHF_LOCK_WRITER(&name_item->lock); shf_debug_verbosity_more();
    SHF_ASSERT(SHF_UID_NONE == head->uid_next, "ERROR: expected uid_head 0x%08x to have ->uid_next SHF_UID_NONE", uid_head);

    if (SHF_UID_NONE == head->uid_last) {
#ifdef SHF_DEBUG_VERSION
        debug = " // pushed           to empty queue ";
#endif
        SHF_QUEUE_ITEM * tail = &name_item->tail;
        SHF_ASSERT(SHF_UID_NONE == tail->uid_last, "ERROR: expected tail (in uid_head 0x%08x) to have ->uid_last SHF_UID_NONE", uid_head);
        SHF_ASSERT(SHF_UID_NONE == tail->uid_next, "ERROR: expected tail (in uid_head 0x%08x) to have ->uid_next SHF_UID_NONE", uid_head);
        head->uid_last = uid_item;
        item->uid_next = uid_head;
        item->uid_last = uid_head; /* tail has same uid as head */
        tail->uid_next = uid_item;
    }
    else {
#ifdef SHF_DEBUG_VERSION
        debug = " // pushed       to non-empty queue ";
#endif
        SHF_QUEUE_GET_UID_VAL_ADDR(last, head->uid_last);
        head->uid_last =       uid_item;
        item->uid_next =       uid_head;
        item->uid_last = head->uid_last;
        last->uid_next =       uid_item;
    }

    shf_debug_verbosity_less(); SHF_UNLOCK_WRITER(&name_item->lock); shf_debug_verbosity_more();

    SHF_DEBUG("%s(shf=?, uid_head=0x%08x, uid=0x%08x){}%s'%.*s'\n", __FUNCTION__, uid_head, uid_item, debug, name_item->name_len, &name_item->name[0]);
} /* shf_queue_push_head() */

// before:                  tail     item     next     head
// before:                  ----     ----     ----     ----
// before: SHF_UID_NONE <-- last <-- last <-- last <-- last
// before:                  next --> next --> next --> next --> SHF_UID_NONE

// after :                  tail              next     head
// after :                  ----              ----     ----
// after : SHF_UID_NONE <-- last          <-- last <-- last
// after :                  next -->          next --> next --> SHF_UID_NONE

void * /* address of data_size memory requested by shf_queue_new_item() */
shf_queue_pull_tail( /* note: safe before shf_freeze() if single threaded access */
    SHF      * shf     ,
    uint32_t   uid_head)
{
    SHF_QUEUE_ITEM * result_item = NULL;

#ifdef SHF_DEBUG_VERSION
    const char * debug = "";
#endif

    shf_uid           = SHF_UID_NONE;
    shf_item_addr     = NULL;
    shf_item_addr_len = 0;

    SHF_QUEUE_GET_UID_VAL_ADDR(head, uid_head);
    SHF_QUEUE_NAME_ITEM * name_item = SHF_CAST(SHF_QUEUE_NAME_ITEM *, &head->data[0]);
    shf_debug_verbosity_less(); SHF_LOCK_WRITER(&name_item->lock); shf_debug_verbosity_more();
    SHF_ASSERT(SHF_UID_NONE == head->uid_next, "ERROR: expected uid_head 0x%08x to have ->uid_next SHF_UID_NONE", uid_head);

    SHF_QUEUE_ITEM * tail = &name_item->tail;
    SHF_ASSERT(SHF_UID_NONE == tail->uid_last, "ERROR: expected tail (in uid_head 0x%08x) to have ->uid_last SHF_UID_NONE", uid_head);
    if (SHF_UID_NONE == tail->uid_next) { /* queue empty */
#ifdef SHF_DEBUG_VERSION
        debug = "pulled 0 of 0  items from queue ";
#endif
    }
    else {
        SHF_QUEUE_GET_UID_VAL_ADDR(item, tail->uid_next);
        SHF_QUEUE_GET_UID_VAL_ADDR(next, item->uid_next);
        result_item       =  item;
        shf_item_addr     = &item->data[0];
        shf_item_addr_len =  item->data_size;
        shf_uid           =  tail->uid_next;
        if (item->uid_next == uid_head) {
#ifdef SHF_DEBUG_VERSION
            debug = "pulled 1 of 1  item  from queue ";
#endif
            tail->uid_next = SHF_UID_NONE;
            next->uid_last = SHF_UID_NONE; /* aka head-> */
        }
        else {
#ifdef SHF_DEBUG_VERSION
            debug = "pulled 1 of 2+ items from queue ";
#endif
            tail->uid_next = item->uid_next;
            next->uid_last =       uid_head; /* tail has same uid as head */
        }
        item->uid_next = SHF_UID_NONE;
        item->uid_last = SHF_UID_NONE;
    }

    shf_debug_verbosity_less(); SHF_UNLOCK_WRITER(&name_item->lock); shf_debug_verbosity_more();

    SHF_DEBUG("%s(shf=?, uid_head=0x%08x                ){} // %s'%.*s'; shf_uid=0x%08x\n", __FUNCTION__, uid_head, debug, name_item->name_len, &name_item->name[0], shf_uid);
    return result_item ? &result_item->data[0] : NULL;
} /* shf_queue_pull_tail() */

// before:                  tail     last     item     next     head
// before:                  ----     ----     ----     ----     ----
// before: SHF_UID_NONE <-- last <-- last <-- last <-- last <-- last
// before:                  next --> next --> next --> next --> next --> SHF_UID_NONE

// after :                  tail     next              next     head
// after :                  ----     ----              ----     ----
// after : SHF_UID_NONE <-- last <-- last          <-- last <-- last
// after :                  next     next -->          next --> next --> SHF_UID_NONE

void * /* remove a particular item from a particular queue */
shf_queue_take_item( /* note: safe before shf_freeze() if single threaded access */
    SHF      * shf     ,
    uint32_t   uid_head,
    uint32_t   uid_item)
{
    SHF_UNUSE(shf);
    SHF_UNUSE(uid_head);
    SHF_UNUSE(uid_item);
    SHF_DEBUG("%s(shf=?, uid_head=0x%08x, uid_item=0x%08x){}\n", __FUNCTION__, uid_head, uid_item);
    SHF_ASSERT(0, "INTERNAL: todo: implement %s()", __FUNCTION__);
    return NULL;
} /* shf_queue_take_item() */
