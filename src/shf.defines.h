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
#include <string.h> /* for strerror() */
#include <syslog.h> /* for LOG_INFO etc */

__attribute__((weak)) __thread int32_t   shf_debug_disabled = 0;
__attribute__((weak)) __thread int32_t   shf_snprintf_value = 0;
#ifdef SHF_DEBUG_VERSION
__attribute__((weak)) __thread FILE    * shf_debug_file     = 0;
#endif

extern void   shf_log(char * prefix, const char * format_type, int line, const char * file, const char * strerror, const char * eol, int priority, const char * format_user, ...);
extern char * shf_log_prefix_get(void);

#define SHF_CAST(TYPE, PTR)                    ((TYPE)(uintptr_t)(PTR))
#define SHF_UNUSE(ARGUMENT)                    (void)(ARGUMENT)
#define SHF_ASSERT(CONDITION,ARGS...)          if (!(CONDITION)) { shf_log(shf_log_prefix_get(), "%05u:%s: ERROR: assertion: ", __LINE__, __FILE__, strerror(errno), "\n", LOG_INFO, ARGS); exit(EXIT_FAILURE); }
#define SHF_ASSERT_INTERNAL(CONDITION,ARGS...) if (!(CONDITION)) { shf_log(shf_log_prefix_get(), "%05u:%s: ERROR: assertion: ", __LINE__, __FILE__, NULL           , "\n", LOG_INFO, ARGS); exit(EXIT_FAILURE); }
#define SHF_WARNING(ARGS...)                                     { shf_log(shf_log_prefix_get(), "%05u:%s: WARNING: "         , __LINE__, __FILE__, NULL           , NULL, LOG_INFO, ARGS); }
#define SHF_PLAIN(ARGS...)                                       { shf_log(SHF_CAST(char *, ""), ""                           , __LINE__, __FILE__, NULL           , NULL, LOG_INFO, ARGS); }

#ifdef SHF_DEBUG_VERSION
#define SHF_DEBUG(ARGS...)                     if (shf_debug_disabled > 0) { } else { shf_log(shf_log_prefix_get(), "%05u:%s: debug: ", __LINE__, __FILE__, NULL, NULL, LOG_INFO, ARGS); }
#define SHF_DEBUG_FILE(ARGS...) \
    shf_debug_file = fopen("/tmp/debug.shf", "a"); SHF_ASSERT(NULL != shf_debug_file, "fopen(): %u: ", errno); \
    fprintf(shf_debug_file, ARGS); \
    fclose(shf_debug_file)

#else
#define SHF_DEBUG(ARGS...)
#define SHF_DEBUG_FILE(ARGS...)
#endif

#define SHF_SYSLOG_ASSERT(CONDITION,ARGS...)          if (!(CONDITION)) { shf_log(shf_log_prefix_get(), "%05u:%s: ERROR: assertion: ", __LINE__, __FILE__, strerror(errno), "\n", LOG_CRIT   , ARGS); exit(EXIT_FAILURE); }
#define SHF_SYSLOG_ASSERT_INTERNAL(CONDITION,ARGS...) if (!(CONDITION)) { shf_log(shf_log_prefix_get(), "%05u:%s: ERROR: assertion: ", __LINE__, __FILE__, NULL           , "\n", LOG_CRIT   , ARGS); exit(EXIT_FAILURE); }
#define SHF_SYSLOG_WARNING(ARGS...)                                     { shf_log(shf_log_prefix_get(), "%05u:%s: WARNING: "         , __LINE__, __FILE__, NULL           , NULL, LOG_WARNING, ARGS); }
#if 0 /* enable this to debug e.g. shf_log mechanism which has to use a different logging mechanism to avoid recursion */
#define SHF_SYSLOG_DEBUG(ARGS...)                                       { shf_log(shf_log_prefix_get(), "%05u:%s: debug: "           , __LINE__, __FILE__, NULL           , NULL, LOG_DEBUG  , ARGS); }
#else
#define SHF_SYSLOG_DEBUG(ARGS...)
#endif

#define SHF_SNPRINTF(DEBUG, BUFFER, FORMAT, ...) { \
        shf_snprintf_value = snprintf(BUFFER, sizeof(BUFFER), FORMAT, __VA_ARGS__); \
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
