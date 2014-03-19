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

#ifndef __SHF_H__
#define __SHF_H__

#include <stdint.h>

#include "shf.defines.h"

typedef enum SHF_KEY_TYPES {
    SHF_KEY_TYPE_KEY_IS_UID     , /*  0: no key, UID accesses directly <- todo */
    SHF_KEY_TYPE_KEY_AT_UID     , /*  1:    key is value at UID        <- todo */
    SHF_KEY_TYPE_KEY_IS_U32     , /*  2:    key is U32 number          <- todo */
    SHF_KEY_TYPE_KEY_IS_U64     , /*  3:    key is U64 number          <- todo */
    SHF_KEY_TYPE_KEY_IS_STR08   , /*  4:    key is  8bit length string <- todo */
    SHF_KEY_TYPE_KEY_IS_STR16   , /*  5:    key is 16bit length string <- todo */
    SHF_KEY_TYPE_KEY_IS_STR32   , /*  6:    key is 32bit length string */
    SHF_KEY_TYPE_KEY_IS_DELETED , /*  7:    0xFF == deleted            */
} SHF_KEY_TYPES;

typedef enum SHF_VAL_TYPES {
    SHF_VAL_TYPE_UNUSED         =     0,
    SHF_VAL_TYPE_VAL_AT_UID     , /*  1:    val is value at UID            <- todo */
    SHF_VAL_TYPE_VAL_IS_U32     , /*  2:    val is U32 number              <- todo */
    SHF_VAL_TYPE_VAL_IS_U64     , /*  3:    val is U64 number              <- todo */
    SHF_VAL_TYPE_VAL_IS_SHM     , /*  4:    val is mmap() at 64bit pointer <- todo */
    SHF_KEY_TYPE_VAL_IS_STR08   , /*  5:    val is  8bit length string     <- todo */
    SHF_KEY_TYPE_VAL_IS_STR16   , /*  6:    val is 16bit length string     <- todo */
    SHF_KEY_TYPE_VAL_IS_STR32   , /*  7:    val is 32bit length string     */
    SHF_KEY_TYPE_VAL_IS_UNUSED08, /*  8:    val is yet to be defined       */
    SHF_KEY_TYPE_VAL_IS_UNUSED09, /*  9:    val is yet to be defined       */
    SHF_KEY_TYPE_VAL_IS_UNUSED10, /* 10:    val is yet to be defined       */
    SHF_KEY_TYPE_VAL_IS_UNUSED11, /* 11:    val is yet to be defined       */
    SHF_KEY_TYPE_VAL_IS_UNUSED12, /* 12:    val is yet to be defined       */
    SHF_KEY_TYPE_VAL_IS_UNUSED13, /* 13:    val is yet to be defined       */
    SHF_KEY_TYPE_VAL_IS_UNUSED14, /* 14:    val is yet to be defined       */
    SHF_KEY_TYPE_VAL_IS_UNUSED15, /* 15:    val is yet to be defined       */
} SHF_VAL_TYPES;

typedef union SHF_DATA_TYPE {
    struct {
        SHF_KEY_TYPES key_type : 3; /*  7 types of keys */
        SHF_VAL_TYPES val_type : 4; /* 16 types of values */
        uint8_t       extended : 1; /* is key,value data extended? */
    } __attribute__((packed)) as_type;
    uint8_t as_u08;
} __attribute__((packed)) SHF_DATA_TYPE;

#define SHF_DATA_TYPE_DELETED (0xff)
#define SHF_UID_NONE          (UINT32_MAX)

extern double     shf_get_time_in_seconds(void);
extern void       shf_init(void);
extern void       shf_detach(SHF * shf);
extern SHF      * shf_attach_existing(const char * path, const char * name);
extern SHF      * shf_attach(const char * path, const char * name);
extern void       shf_make_hash(const char * key, uint32_t key_len);
extern uint32_t   shf_put_val(SHF * shf, const char * val, uint32_t val_len);
extern int        shf_get_copy_via_key(SHF * shf);
extern int        shf_del_key(SHF * shf);
extern void       shf_del(SHF * shf);

#endif /* __SHF_H__ */
