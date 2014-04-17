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

#ifndef __SHF_DEFINES_H__
#define __SHF_DEFINES_H__

#include <stdint.h>

extern int32_t shf_debug_disabled;

#define SHF_CAST(TYPE, PTR) ((TYPE)(uintptr_t)(PTR))
#define SHF_ASSERT(CONDITION,ARGS...) if (!(CONDITION)) { fprintf(stderr, "ERROR: assertion @ line %u of file %s: ", __LINE__, __FILE__); fprintf(stderr, ARGS); if (0 != errno) { perror(NULL); } else { fprintf(stderr, "\n"); } exit(EXIT_FAILURE); }
#define SHF_UNUSE(ARGUMENT) (void)(ARGUMENT)

#ifdef SHF_DEBUG_VERSION
#define SHF_DEBUG(ARGS...)      if (shf_debug_disabled > 0) { } else { fprintf(stderr, "%s: debug: ", __FILE__); fprintf(stderr, ARGS); }
#define SHF_DEBUG_FILE(ARGS...) \
    shf_debug_file = fopen("/tmp/debug.shf", "a"); SHF_ASSERT(NULL != shf_debug_file, "fopen(): %u: ", errno); \
    fprintf(shf_debug_file, ARGS); \
    fclose(shf_debug_file)

#else
#define SHF_DEBUG(ARGS...)
#define SHF_DEBUG_FILE(ARGS...)
#endif

#define SHF_SNPRINTF(DEBUG, BUFFER, FORMAT, ...) { \
        int shf_snprintf_value = snprintf(BUFFER, sizeof(BUFFER), FORMAT, __VA_ARGS__); \
        SHF_ASSERT(shf_snprintf_value >= 0                            , "%d=snprintf() too small!", shf_snprintf_value); \
        SHF_ASSERT(shf_snprintf_value <= SHF_CAST(int, sizeof(BUFFER)), "%d=snprintf() too big!"  , shf_snprintf_value); \
        if (DEBUG) { SHF_DEBUG("- SHF_SNPRINTF() // '%s'\n", BUFFER); } \
    }

#define SHF_MOD_PAGE(BYTES) ((((BYTES - 1) / SHF_SIZE_PAGE) + 1) * SHF_SIZE_PAGE)

#define SHF_MAKE_HASH(KEY) shf_make_hash(KEY, sizeof(KEY) - 1)
#define SHF_CONST_STR_AND_SIZE(KEY) KEY, (sizeof(KEY) - 1)

#define SHF_U08_AT(BASE, OFFSET)                                        SHF_CAST(uint8_t *, BASE)[OFFSET]
#define SHF_U32_AT(BASE, OFFSET)                 SHF_CAST(uint32_t *, &(SHF_CAST(uint8_t *, BASE)[OFFSET]))[0]
#define SHF_MEM_AT(BASE, OFFSET, FROM_LEN, FROM)               memcpy(&(SHF_CAST(uint8_t *, BASE)[OFFSET]), &(SHF_CAST(uint8_t *, FROM)[0]), FROM_LEN)
#define SHF_CMP_AT(BASE, OFFSET, FROM_LEN, FROM)               memcmp(&(SHF_CAST(uint8_t *, BASE)[OFFSET]), &(SHF_CAST(uint8_t *, FROM)[0]), FROM_LEN)

#endif /* __SHF_DEFINES_H__ */
