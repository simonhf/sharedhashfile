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
    isAttached = 0;
}

SharedHashFile::~SharedHashFile()
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    if (isAttached) {
        Detach();
    }
}

void
SharedHashFile::Detach()
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    shf_detach(shf);
    shf        = NULL;
    isAttached = 0;
}

bool
SharedHashFile::AttachExisting(
    const char * path, /* e.g. '/dev/shm' */
    const char * name) /* e.g. 'myshf'    */
{
    SHF_DEBUG("%s(path='%s', name='%s')\n", __FUNCTION__, path, name);
    shf = shf_attach_existing(path, name);
    isAttached = shf ? 1 : 0;
    return shf;
}

bool
SharedHashFile::Attach(
    const char * path                    , /* e.g. '/dev/shm' */
    const char * name                    , /* e.g. 'myshf'    */
    uint32_t     delete_upon_process_exit) /* 0 means do nothing, 1 means delete shf when calling process exits */
{
    SHF_DEBUG("%s(path='%s', name='%s')\n", __FUNCTION__, path, name);
    shf = shf_attach(path, name, delete_upon_process_exit);
    isAttached = shf ? 1 : 0;
    return shf;
}

bool
SharedHashFile::IsAttached()
{
    SHF_DEBUG("%s() // isAttached=%u\n", __FUNCTION__, isAttached);
    return isAttached;
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

int
SharedHashFile::UpdKeyVal()
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_upd_key_val(shf);
}

int
SharedHashFile::UpdUidVal(uint32_t uid)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_upd_uid_val(shf, uid);
}

int
SharedHashFile::UpdCallbackCopy(const char * val, uint32_t val_len)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_upd_callback_copy(val, val_len);
}

char *
SharedHashFile::Del()
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    char * hint = shf_del(shf);
    shf        = NULL;
    isAttached = 0;
    return hint;
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

void
SharedHashFile::SetIsLockable(uint32_t is_lockable)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    shf_set_is_lockable(shf, is_lockable);
}

void
SharedHashFile::SetIsFixedLen(uint32_t fixed_key_len, uint32_t fixed_val_len)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    shf_set_is_fixed_len(shf, fixed_key_len, fixed_val_len);
}

void *
SharedHashFile::QNew(uint32_t shf_qs, uint32_t shf_q_items, uint32_t shf_q_item_size, uint32_t qids_nolock_max)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_q_new(shf, shf_qs, shf_q_items, shf_q_item_size, qids_nolock_max);
}

void *
SharedHashFile::QGet()
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_q_get(shf);
}

void
SharedHashFile::QDel()
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_q_del(shf);
}

uint32_t
SharedHashFile::QNewName(const char * name, uint32_t name_len)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_q_new_name(shf, name, name_len);
}

uint32_t
SharedHashFile::QGetName(const char * name, uint32_t name_len)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_q_get_name(shf, name, name_len);
}

void
SharedHashFile::QPushHead(uint32_t qid, uint32_t qiid)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_q_push_head(shf, qid, qiid);
}

uint32_t
SharedHashFile::QPullTail(uint32_t qid) /* sets shf_qiid & shf_qiid_addr & shf_qiid_addr_len */
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_q_pull_tail(shf, qid);
}

uint32_t
SharedHashFile::QTakeItem(uint32_t qid) /* sets shf_qiid & shf_qiid_addr & shf_qiid_addr_len */
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_q_take_item(shf, qid);
}

uint32_t
SharedHashFile::QPushHeadPullTail(uint32_t push_qid, uint32_t push_qiid, uint32_t pull_qid) { /* sets shf_qiid & shf_qiid_addr & shf_qiid_addr_len */
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_q_push_head_pull_tail(shf, push_qid, push_qiid, pull_qid);
}

void
SharedHashFile::QFlush(uint32_t pull_qid) {
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_q_flush(shf, pull_qid);
}

bool
SharedHashFile::QIsReady() {
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_q_is_ready(shf);
}

void
SharedHashFile::RaceInit(const char * name, uint32_t name_len)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_race_init(shf, name, name_len);
}

void
SharedHashFile::RaceStart(const char * name, uint32_t name_len, uint32_t horses)
{
    SHF_DEBUG("%s()\n", __FUNCTION__);
    return shf_race_start(shf, name, name_len, horses);
}
