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

#include <SharedHashFile.hpp>

extern "C" {
#include <shf.defines.h>
}

SharedHashFile::SharedHashFile()
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    shf_init();
}

SharedHashFile::~SharedHashFile()
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    // todo
}

bool
SharedHashFile::AttachExisting(
    const char * path, /* e.g. '/dev/shm' */
    const char * name) /* e.g. 'myshf'    */
{
    SHF_DEBUG("%s(path='%s', name='%s')\n", __FUNCTION__, path, name);
    shf = shf_attach_existing(path, name);
    return shf;
}

bool
SharedHashFile::Attach(
    const char * path, /* e.g. '/dev/shm' */
    const char * name) /* e.g. 'myshf'    */
{
    SHF_DEBUG("%s(path='%s', name='%s')\n", __FUNCTION__, path, name);
    shf = shf_attach(path, name);
    return shf;
}

void
SharedHashFile::MakeHash( // todo: warn in docs that ::MakeHash works __thread globally & not at the object/class level
    const char * key    ,
    uint32_t     key_len)
{
    SHF_DEBUG("%s(key=?, key_len=%u)\n", __FUNCTION__, key_len);
    shf_make_hash(key, key_len);
}

bool
SharedHashFile::GetKeyValCopy()
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_get_key_val_copy(shf);
}

bool
SharedHashFile::GetUidValCopy(uint32_t uid)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_get_uid_val_copy(shf, uid);
}

uint32_t
SharedHashFile::PutKeyVal(
    const char * val    ,
    uint32_t     val_len)
{
    SHF_DEBUG("%s(val=?, val_len=%u)\n", __FUNCTION__, val_len);
    return shf_put_key_val(shf, val, val_len);
}

bool
SharedHashFile::DelKeyVal()
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_del_key_val(shf);
}

bool
SharedHashFile::DelUidVal(uint32_t uid)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_del_uid_val(shf, uid);
}

uint64_t
SharedHashFile::DebugGetGarbage()
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_debug_get_garbage(shf);
}

void
SharedHashFile::DebugVerbosityLess()
{
    shf_debug_verbosity_less();
}

void
SharedHashFile::DebugVerbosityMore()
{
    shf_debug_verbosity_more();
}

void
SharedHashFile::SetDataNeedFactor(uint32_t data_needed_factor)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    shf_set_data_need_factor(data_needed_factor);
}

uint32_t
SharedHashFile::QueueNewItem(uint32_t data_size)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_queue_new_item(shf, data_size);
}

uint32_t
SharedHashFile::QueueNewName(const char * key, uint32_t key_len)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_queue_new_name(shf, key, key_len);
}

uint32_t
SharedHashFile::QueueGetName(const char * key, uint32_t key_len)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_queue_get_name (shf, key, key_len);
}

void
SharedHashFile::QueuePushHead(uint32_t uid_head, uint32_t uid)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    shf_queue_push_head(shf, uid_head, uid);
}

void *
SharedHashFile::QueuePullTail(uint32_t uid_head)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_queue_pull_tail(shf, uid_head);
}

void *
SharedHashFile::QueueTakeItem(uint32_t uid_head, uint32_t uid)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_queue_take_item(shf, uid_head, uid);
}
