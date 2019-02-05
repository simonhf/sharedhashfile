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

#ifndef __SHAREDHASHFILE_HPP__
#define __SHAREDHASHFILE_HPP__

extern "C" {
#include <shf.private.h>
#include <shf.h>
}

class SharedHashFile {
public:
    SharedHashFile();
    ~SharedHashFile();
    void       Detach            ();
    bool       AttachExisting    (const char * path, const char * name);
    bool       Attach            (const char * path, const char * name, uint32_t delete_upon_process_exit);
    bool       IsAttached        ();
    void       MakeHash          (const char * key, uint32_t key_len);
    bool       GetKeyValCopy     ();
    bool       GetUidValCopy     (uint32_t uid);
    uint32_t   PutKeyVal         (const char * val, uint32_t val_len);
    bool       DelKeyVal         ();
    bool       DelUidVal         (uint32_t uid);
    int        UpdKeyVal         ();
    int        UpdUidVal         (uint32_t uid);
    int        UpdCallbackCopy   (const char * val, uint32_t val_len);
    char     * Del               ();
    uint64_t   DebugGetGarbage   ();
    void       DebugVerbosityLess();
    void       DebugVerbosityMore();
    void       SetDataNeedFactor (uint32_t data_needed_factor);
    void       SetIsLockable     (uint32_t is_lockable);
    void       SetIsFixedLen     (uint32_t fixed_key_len, uint32_t fixed_val_len);
    void     * QNew              (uint32_t shf_qs, uint32_t shf_q_items, uint32_t shf_q_item_size, uint32_t qids_nolock_max);
    void     * QGet              ();
    void       QDel              ();
    uint32_t   QNewName          (const char * name, uint32_t name_len);
    uint32_t   QGetName          (const char * name, uint32_t name_len);
    void       QPushHead         (uint32_t qid, uint32_t qiid);
    uint32_t   QPullTail         (uint32_t qid                                            ); /* sets shf_qiid & shf_qiid_addr & shf_qiid_addr_len */
    uint32_t   QTakeItem         (uint32_t qid                                            ); /* sets shf_qiid & shf_qiid_addr & shf_qiid_addr_len */
    uint32_t   QPushHeadPullTail (uint32_t push_qid, uint32_t push_qiid, uint32_t pull_qid); /* sets shf_qiid & shf_qiid_addr & shf_qiid_addr_len */
    void       QFlush            (uint32_t pull_qid);
    bool       QIsReady          ();
    void       RaceInit          (const char * name, uint32_t name_len                 );
    void       RaceStart         (const char * name, uint32_t name_len, uint32_t horses);

private:
    SHF      * shf;
    uint32_t   isAttached;
};

#endif /* __SHAREDHASHFILE_HPP__ */
