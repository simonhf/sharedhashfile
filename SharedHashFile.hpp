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
    bool     AttachExisting(const char * path, const char * name);
    bool     Attach(const char * path, const char * name);
    void     MakeHash(const char * key, uint32_t key_len);
    bool     GetCopyViaKey();
    uint32_t PutVal(const char * val, uint32_t val_len);
    bool     DelKey();
    uint64_t DebugGetBytesMarkedAsDeleted();

private:
    SHF * shf;
};

#endif /* __SHAREDHASHFILE_HPP__ */
