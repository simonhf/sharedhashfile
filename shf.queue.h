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

#ifndef __SHF_QUEUE_H__
#define __SHF_QUEUE_H__

#include <stdint.h>

extern uint32_t   shf_queue_new_item (SHF * shf, uint32_t data_size                );
extern uint32_t   shf_queue_new_name (SHF * shf, const char * key, uint32_t key_len);
extern uint32_t   shf_queue_get_name (SHF * shf, const char * key, uint32_t key_len);
extern void       shf_queue_push_head(SHF * shf, uint32_t uid_head, uint32_t uid   );
extern void     * shf_queue_pull_tail(SHF * shf, uint32_t uid_head                 ); /* sets both shf_uid & shf_addr */
extern void     * shf_queue_take_item(SHF * shf, uint32_t uid_head, uint32_t uid   );

#endif /* __SHF_QUEUE_H__ */
