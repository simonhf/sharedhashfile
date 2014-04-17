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

#define _GNU_SOURCE   /* See feature_test_macros(7) */
#include <sys/mman.h> /* for mremap() */
#include <string.h>   /* for memcmp() */

#include "shf.private.h"
#include "shf.h"
#include "shf.queue.h"
#include "tap.h"

int main(void)
{
    plan_tests(32);

    char  test_shf_name[256];
    char  test_shf_folder[] = "/dev/shm";
    pid_t pid               = getpid();
    SHF_SNPRINTF(1, test_shf_name, "test-%05u", pid);

                    shf_init            ();
    SHF    * shf =  shf_attach_existing (test_shf_folder, test_shf_name); ok(NULL == shf, "c: shf_attach_existing() fails for non-existing file as expected");
             shf =  shf_attach          (test_shf_folder, test_shf_name); ok(NULL != shf, "c: shf_attach()          works for non-existing file as expected");
                    SHF_MAKE_HASH       (         "key"    );
    ok(0         == shf_get_key_val_copy(shf               ), "c: shf_get_key_val_copy() could not find unput key as expected");
    ok(0         == shf_del_key_val     (shf               ), "c: shf_del_key_val()      could not find unput key as expected");
    uint32_t uid =  shf_put_key_val     (shf    , "val" , 3)                                                                   ;
    ok(      uid != SHF_UID_NONE                            , "c: shf_put_val()                           put key as expected");
    ok(1         == shf_get_key_val_copy(shf               ), "c: shf_get_key_val_copy() could     find   put key as expected");
    ok(3         == shf_val_len                             , "c: shf_val_len                                     as expected");
    ok(0         == memcmp              (shf_val, "val" , 3), "c: shf_val                                         as expected");
    ok(1         == shf_del_uid_val     (shf,uid           ), "c: shf_del_uid_val()      could     find   put key as expected");
    ok(0         == shf_get_key_val_copy(shf               ), "c: shf_get_key_val_copy() could not find   del key as expected");
    ok(0         == shf_del_uid_val     (shf,uid           ), "c: shf_del_uid_val()      could not find   del key as expected");
             uid =  shf_put_key_val     (shf    , "val2", 4)                                                                   ;
    ok(      uid != SHF_UID_NONE                            , "c: shf_put_key_val()                     reput key as expected");
    ok(1         == shf_get_uid_val_copy(shf,uid           ), "c: shf_get_uid_val_copy() could     find reput key as expected");
    ok(4         == shf_val_len                             , "c: shf_val_len                                     as expected");
    ok(0         == memcmp              (shf_val, "val2", 4), "c: shf_val                                         as expected");
    ok(1         == shf_del_key_val     (shf               ), "c: shf_del_key_val()      could     find reput key as expected");

    uint32_t uid_queue_unused =  shf_queue_new_name(shf, SHF_CONST_STR_AND_SIZE("queue-unused")); /* e.g. queue names created by process a */
    uint32_t uid_queue_a2b    =  shf_queue_new_name(shf, SHF_CONST_STR_AND_SIZE("queue-a2b"   ));
    uint32_t uid_queue_b2a    =  shf_queue_new_name(shf, SHF_CONST_STR_AND_SIZE("queue-b2a"   ));
    ok(      uid_queue_unused == shf_queue_get_name(shf, SHF_CONST_STR_AND_SIZE("queue-unused")), "c: shf_queue_get_name('queue-unused') returned uid as expected");  /* e.g. queue names got by process b */
    ok(      uid_queue_a2b    == shf_queue_get_name(shf, SHF_CONST_STR_AND_SIZE("queue-a2b"   )), "c: shf_queue_get_name('queue-a2b'   ) returned uid as expected");
    ok(      uid_queue_b2a    == shf_queue_get_name(shf, SHF_CONST_STR_AND_SIZE("queue-b2a"   )), "c: shf_queue_get_name('queue-b2a'   ) returned uid as expected");

    uint32_t test_pull_items;
    uint32_t test_queue_items = 10;
    uint32_t test_queue_item_data_size = 4096;
    for (uint32_t i = 0; i < test_queue_items; i++) { /* e.g. queue items created & queued in unused queue by process a */
        uid = shf_queue_new_item (shf, test_queue_item_data_size);
              shf_queue_push_head(shf, uid_queue_unused, uid);
    }

    test_pull_items = 0;
    while(NULL != shf_queue_pull_tail(shf, uid_queue_unused         )) { /* e.g. items transferred from unused to a2b queue by process a */
                  shf_queue_push_head(shf, uid_queue_a2b   , shf_uid);
                  SHF_CAST(uint32_t *, shf_addr)[0] = test_pull_items;
                  test_pull_items ++;
    }
    ok(test_queue_items == test_pull_items, "c: pulled & pushed items from unused to a2b    as expected");

    test_pull_items = 0;
    while(NULL != shf_queue_pull_tail(shf, uid_queue_a2b         )) { /* e.g. items transferred from a2b to b2a queue by process b */
                  shf_queue_push_head(shf, uid_queue_b2a, shf_uid);
                  SHF_ASSERT(test_pull_items == SHF_CAST(uint32_t *, shf_addr)[0], "INTERNAL: test expected %u but got %u", test_pull_items, SHF_CAST(uint32_t *, shf_addr)[0]);
                  test_pull_items ++;
    }
    ok(test_queue_items == test_pull_items, "c: pulled & pushed items from a2b    to b2a    as expected");

    test_pull_items = 0;
    while(NULL != shf_queue_pull_tail(shf, uid_queue_b2a            )) { /* e.g. items transferred from b2a to unused queue by process a */
                  shf_queue_push_head(shf, uid_queue_unused, shf_uid);
                  SHF_ASSERT(test_pull_items == SHF_CAST(uint32_t *, shf_addr)[0], "INTERNAL: test expected %u but got %u", test_pull_items, SHF_CAST(uint32_t *, shf_addr)[0]);
                  test_pull_items ++;
    }
    ok(test_queue_items == test_pull_items, "c: pulled & pushed items from b2a    to unused as expected");

    uint32_t test_keys = 250000;
    shf_set_data_need_factor(250);

    {
        shf_debug_verbosity_less();
        double test_start_time = shf_get_time_in_seconds();
        for (uint32_t i = 0; i < test_keys; i++) {
            shf_make_hash(SHF_CAST(const char *, &i), sizeof(i));
            shf_put_key_val(shf, SHF_CAST(const char *, &i), sizeof(i));
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(1, "c: put expected number of              keys // estimate %.0f keys per second", test_keys / test_elapsed_time);
        shf_debug_verbosity_more();
    }

    {
        shf_debug_verbosity_less();
        double test_start_time = shf_get_time_in_seconds();
        uint32_t keys_found = 0;
        for (uint32_t i = (test_keys * 2); i < (test_keys * 3); i++) {
            shf_make_hash(SHF_CAST(const char *, &i), sizeof(i));
            keys_found += shf_get_key_val_copy(shf);
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(0 == keys_found, "c: got expected number of non-existing keys // estimate %.0f keys per second", test_keys / test_elapsed_time);
        shf_debug_verbosity_more();
    }

    {
        shf_debug_verbosity_less();
        double test_start_time = shf_get_time_in_seconds();
        uint32_t keys_found = 0;
        for (uint32_t i = 0; i < test_keys; i++) {
            shf_make_hash(SHF_CAST(const char *, &i), sizeof(i));
            keys_found += shf_get_key_val_copy(shf);
            SHF_ASSERT(sizeof(i) == shf_val_len, "INTERNAL: expected shf_val_len to be %lu but got %u\n", sizeof(i), shf_val_len);
            SHF_ASSERT(0 == memcmp(&i, shf_val, sizeof(i)), "INTERNAL: unexpected shf_val\n");
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(test_keys == keys_found, "c: got expected number of     existing keys // estimate %.0f keys per second", test_keys / test_elapsed_time);
        shf_debug_verbosity_more();
    }

    ok(0 == shf_debug_get_garbage(shf), "c: graceful growth cleans up after itself as expected");

    {
        shf_debug_verbosity_less();
        double test_start_time = shf_get_time_in_seconds();
        uint32_t keys_found = 0;
        for (uint32_t i = 0; i < test_keys; i++) {
            shf_make_hash(SHF_CAST(const char *, &i), sizeof(i));
            keys_found += shf_del_key_val(shf);
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(test_keys == keys_found, "c: del expected number of     existing keys // estimate %.0f keys per second", test_keys / test_elapsed_time);
        shf_debug_verbosity_more();
    }

    ok(0 != shf_debug_get_garbage(shf), "c: del does not    clean  up after itself as expected");

    {
        shf_debug_verbosity_less();
        double test_start_time = shf_get_time_in_seconds();
        for (uint32_t i = 0; i < test_keys; i++) {
            uid = shf_queue_new_item (shf, test_queue_item_data_size);
                  shf_queue_push_head(shf, uid_queue_unused, uid);
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(1, "c: created expected number of new queue items // estimate %.0f keys per second", test_keys / test_elapsed_time);
        shf_debug_verbosity_more();
    }

    {
        shf_debug_verbosity_less();
        double test_start_time = shf_get_time_in_seconds();
        test_pull_items = 0;
        while(NULL != shf_queue_pull_tail(shf, uid_queue_unused         )) {
                      shf_queue_push_head(shf, uid_queue_a2b   , shf_uid);
                      test_pull_items ++;
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(test_queue_items + test_keys == test_pull_items, "c: moved   expected number of new queue items // estimate %.0f keys per second", test_keys / test_elapsed_time);
        shf_debug_verbosity_more();
    }

    {
        shf_debug_verbosity_less();
        double test_start_time = shf_get_time_in_seconds();
        test_pull_items = 0;
        while(NULL != shf_queue_pull_tail(shf, uid_queue_a2b         )) {
                      shf_queue_push_head(shf, uid_queue_b2a, shf_uid);
                      test_pull_items ++;
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(test_queue_items + test_keys == test_pull_items, "c: moved   expected number of new queue items // estimate %.0f keys per second", test_keys / test_elapsed_time);
        shf_debug_verbosity_more();
    }

    {
        shf_debug_verbosity_less();
        double test_start_time = shf_get_time_in_seconds();
        test_pull_items = 0;
        while(NULL != shf_queue_pull_tail(shf, uid_queue_b2a            )) {
                      shf_queue_push_head(shf, uid_queue_unused, shf_uid);
                      test_pull_items ++;
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(test_queue_items + test_keys == test_pull_items, "c: moved   expected number of new queue items // estimate %.0f keys per second", test_keys / test_elapsed_time);
        shf_debug_verbosity_more();
    }

    return exit_status();
} /* main() */
