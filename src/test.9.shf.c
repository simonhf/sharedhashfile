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
#include <locale.h>   /* for setlocale() */

#include "shf.private.h"
#include "shf.h"
#include "tap.h"

static int
upd_callback_test(const char * val, uint32_t val_len) /* callback for shf_upd_*_val() */
{
    int result = 0;
    SHF_DEBUG("%s(val=%.*s, val_len=%u){} // return %u; testing custom SHF update callback\n", __FUNCTION__, 3, val, val_len, result);
    char temp = val[0];
    SHF_CAST(char *, val)[          0] = val[val_len - 1];
    SHF_CAST(char *, val)[val_len - 1] = temp;
    return result;
} /* upd_callback_test() */

int main(void)
{
    // Tell tap (test anything protocol) how many tests we expect to run.
    plan_tests(75);

    // To enable ```%'.0f``` in sprintf() instead of boring ```%.0f```.
    SHF_ASSERT(NULL != setlocale(LC_NUMERIC, ""), "setlocale(): %u: ", errno);

    { // start of non-fixed length tests

        // Create a shared hash file in ```/dev/shm``` shared memory and call it a unique (because we use the pid) name so that it cannot conflict with any other tests.
        char  test_shf_name[256];
        char  test_shf_folder[] = "/dev/shm";
        pid_t pid               = getpid();
        SHF_SNPRINTF(1, test_shf_name, "test-%05u", pid);

        // ## Functional tests: Key value
        // Create a new shared hash file.
        uint32_t bit =  1; /* delete shf when calling process exits */
                        shf_init             ();
        SHF    * shf =  shf_attach_existing  (test_shf_folder, test_shf_name     ); ok(NULL == shf, "c: attach                 : shf_attach_existing()   could not find file      as expected");
                 shf =  shf_attach           (test_shf_folder, test_shf_name, bit); ok(NULL != shf, "c: attach                 : shf_attach()            could     make file      as expected");
                        shf_set_is_lockable  (shf, 0                             ); /* single threaded test; no need to lock */

        // Functional tests to exercise the API.
                        SHF_MAKE_HASH        (         "key"    )   /* non-existing    xxx key */                                                             ;
        ok(0         == shf_get_key_val_copy (shf               ), "c: non-existing    xxx key: shf_get_key_val_copy()  could not find unput key as expected");
        ok(0         == shf_upd_key_val      (shf               ), "c: non-existing    xxx key: shf_upd_key_val()       could not find unput key as expected");
        ok(0         == shf_del_key_val      (shf               ), "c: non-existing    xxx key: shf_del_key_val()       could not find unput key as expected");
        uint32_t uid =  shf_put_key_val      (shf    , "val" , 3)   /*     existing    get key */                                                             ;
        ok(      uid != SHF_UID_NONE                             , "c:     existing    get key: shf_put_key_val()                        put key as expected");
        ok(1         == shf_get_key_val_copy (shf               ), "c:     existing    get key: shf_get_key_val_copy()  could     find   put key as expected");
        ok(3         == shf_val_len                              , "c:     existing    get key: shf_val_len                                      as expected");
        ok(0         == memcmp               (shf_val, "val" , 3), "c:     existing    get key: shf_val                                          as expected");
        ok(0         == shf_upd_callback_copy(         "upvZ", 4), "c: bad val size    upd key: shf_upd_callback_copy() could         preset val as expected");
        ok(3         == shf_upd_key_val      (shf               ), "c: bad val size    upd key: shf_upd_key_val()       callback error 4 upd key as expected");
        ok(3         == shf_upd_uid_val      (shf,uid           ), "c: bad val size    upd uid: shf_upd_uid_val()       callback error 4 upd uid as expected");
        ok(0         == shf_upd_callback_copy(         "up1" , 3), "c: copy   callback upd key: shf_upd_callback_copy() could         preset val as expected");
        ok(1         == shf_upd_key_val      (shf               ), "c: copy   callback upd key: shf_upd_uid_val()       callback works 4 upd key as expected");
        ok(1         == shf_get_key_val_copy (shf               ), "c: copy   callback upd key: shf_get_key_val_copy()  could     find   upd key as expected");
        ok(3         == shf_val_len                              , "c: copy   callback upd key: shf_val_len                                      as expected");
        ok(0         == memcmp               (shf_val, "up1" , 3), "c: copy   callback upd key: shf_val                                          as expected");
        ok(0         == shf_upd_callback_copy(         "up2" , 3), "c: copy   callback upd uid: shf_upd_callback_copy() could         preset val as expected");
        ok(1         == shf_upd_uid_val      (shf,uid           ), "c: copy   callback upd uid: shf_upd_uid_val()       callback works 4 upd uid as expected");
        ok(1         == shf_get_key_val_copy (shf               ), "c: copy   callback upd uid: shf_get_key_val_copy()  could     find   upd key as expected");
        ok(3         == shf_val_len                              , "c: copy   callback upd uid: shf_val_len                                      as expected");
        ok(0         == memcmp               (shf_val, "up2" , 3), "c: copy   callback upd uid: shf_val                                          as expected");
                        shf_upd_callback_set (upd_callback_test )   /* custom callback upd key */                                                             ;
        ok(1         == shf_upd_key_val      (shf               ), "c: custom callback upd key: shf_upd_uid_val()       callback works 4 upd key as expected");
        ok(1         == shf_get_key_val_copy (shf               ), "c: custom callback upd key: shf_get_key_val_copy()  could     find   upd key as expected");
        ok(3         == shf_val_len                              , "c: custom callback upd key: shf_val_len                                      as expected");
        ok(0         == memcmp               (shf_val, "2pu" , 3), "c: custom callback upd key: shf_val                                          as expected");
                        shf_upd_callback_set (upd_callback_test )   /* custom callback upd uid */                                                             ;
        ok(1         == shf_upd_uid_val      (shf,uid           ), "c: custom callback upd uid: shf_upd_uid_val()       callback works 4 upd uid as expected");
        ok(1         == shf_get_key_val_copy (shf               ), "c: custom callback upd uid: shf_get_key_val_copy()  could     find   upd key as expected");
        ok(3         == shf_val_len                              , "c: custom callback upd uid: shf_val_len                                      as expected");
        ok(0         == memcmp               (shf_val, "up2" , 3), "c: custom callback upd uid: shf_val                                          as expected");
        ok(1         == shf_del_uid_val      (shf,uid           ), "c:     existing    del uid: shf_del_uid_val()       could     find   put key as expected");
        ok(0         == shf_get_key_val_copy (shf               ), "c:     existing    del uid: shf_get_key_val_copy()  could not find   del key as expected");
        ok(0         == shf_del_uid_val      (shf,uid           ), "c: non-existing    del uid: shf_del_uid_val()       could not find   del key as expected");
                 uid =  shf_put_key_val      (shf    , "val2", 4)   /* reput / reuse key / uid */                                                             ;
        ok(      uid != SHF_UID_NONE                             , "c: reput / reuse key / uid: shf_put_key_val()                      reput key as expected");
        ok(1         == shf_get_uid_val_copy (shf,uid           ), "c: reput / reuse key / uid: shf_get_uid_val_copy()  could     find reput key as expected");
        ok(4         == shf_val_len                              , "c: reput / reuse key / uid: shf_val_len                                      as expected");
        ok(0         == memcmp               (shf_val, "val2", 4), "c: reput / reuse key / uid: shf_val                                          as expected");
        ok(1         == shf_del_key_val      (shf               ), "c: reput / reuse key / uid: shf_del_key_val()       could     find reput key as expected");

        // ## Functional tests: IPC queues
        // Create a some queue elements and a bunch of queues using the exiting test shared hash file from the tests above.
        uint32_t test_pull_items  = 0;
        uint32_t test_qs          = 3;
        uint32_t test_q_items     = 10;
        uint32_t test_q_item_size = 4096;
        ok(      0               == shf_q_is_ready(shf                                            ), "c: shf_q_is_ready()           not ready    as expected");
        ok(      NULL            != shf_q_new     (shf, test_qs, test_q_items, test_q_item_size, 1), "c: shf_q_new()                returned     as expected"); /* e.g. q items created  by process a */
        ok(      1               == shf_q_is_ready(shf                                            ), "c: shf_q_is_ready()               ready    as expected");
        uint32_t test_qid_free    = shf_q_new_name(shf, SHF_CONST_STR_AND_SIZE("qid-free")        );                                                            /* e.g. q names set qids by process a */
        uint32_t test_qid_a2b     = shf_q_new_name(shf, SHF_CONST_STR_AND_SIZE("qid-a2b" )        );
        uint32_t test_qid_b2a     = shf_q_new_name(shf, SHF_CONST_STR_AND_SIZE("qid-b2a" )        );
        ok(      test_qid_free   == shf_q_get_name(shf, SHF_CONST_STR_AND_SIZE("qid-free")        ), "c: shf_q_get_name('qid-free') returned qid as expected"); /* e.g. q names get qids by process b */
        ok(      test_qid_a2b    == shf_q_get_name(shf, SHF_CONST_STR_AND_SIZE("qid-a2b" )        ), "c: shf_q_get_name('qid-a2b' ) returned qid as expected");
        ok(      test_qid_b2a    == shf_q_get_name(shf, SHF_CONST_STR_AND_SIZE("qid-b2a" )        ), "c: shf_q_get_name('qid-b2a' ) returned qid as expected");

        // Functional test to move all the queue elements from queue ```qid-free``` to ```qid-a2b```.
        test_pull_items = 0;
        while(SHF_QIID_NONE != shf_q_pull_tail(shf, test_qid_free          )) {                                                                                        /* e.g. q items from unused to a2b q by process a */
                               shf_q_push_head(shf, test_qid_a2b , shf_qiid);
                               SHF_CAST(uint32_t *, shf_qiid_addr)[0] = test_pull_items; /* store q item # in item */
                               test_pull_items ++;
        }
        ok(test_q_items == test_pull_items, "c: pulled & pushed items from free to a2b  as expected");

        test_pull_items = 0;
        while(SHF_QIID_NONE != shf_q_pull_tail(shf, test_qid_a2b          )) {                                                                                         /* e.g. q items from a2b to b2a queue by process b */
                               shf_q_push_head(shf, test_qid_b2a, shf_qiid);
                               SHF_ASSERT(test_pull_items == SHF_CAST(uint32_t *, shf_qiid_addr)[0], "INTERNAL: test expected q item %u but got %u", test_pull_items, SHF_CAST(uint32_t *, shf_qiid_addr)[0]);
                               test_pull_items ++;
        }
        ok(test_q_items == test_pull_items, "c: pulled & pushed items from a2b  to b2a  as expected");

        test_pull_items = 0;
        while(SHF_QIID_NONE != shf_q_pull_tail(shf, test_qid_b2a           )) {                                                                                        /* e.g. q items from a2b to b2a queue by process b */
                               shf_q_push_head(shf, test_qid_free, shf_qiid);
                               SHF_ASSERT(test_pull_items == SHF_CAST(uint32_t *, shf_qiid_addr)[0], "INTERNAL: test expected q item %u but got %u", test_pull_items, SHF_CAST(uint32_t *, shf_qiid_addr)[0]);
                               test_pull_items ++;
        }
        ok(test_q_items == test_pull_items, "c: pulled & pushed items from a2b  to free as expected");

        test_q_items = 100000;
        shf_set_data_need_factor(1);

        {
            double test_start_time = shf_get_time_in_seconds();
                       shf_debug_verbosity_less();
            ok(   1 == shf_q_is_ready          (shf                                              ), "c: shf_q_is_ready()     ready as expected");
                       shf_q_del               (shf                                              );
            ok(   0 == shf_q_is_ready          (shf                                              ), "c: shf_q_is_ready() not ready as expected");
            ok(NULL != shf_q_new               (shf, test_qs, test_q_items, test_q_item_size, 100), "c: shf_q_new()      returned  as expected");
            ok(   1 == shf_q_is_ready          (shf                                              ), "c: shf_q_is_ready() ready     as expected");
                       shf_debug_verbosity_more();
            double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
            ok(1, "c: created expected number of new queue items // estimate %'.0f q items per second", test_q_items / test_elapsed_time);
        }

        {
            shf_debug_verbosity_less();
            double test_start_time = shf_get_time_in_seconds();
            test_pull_items = 0;
            while(SHF_QIID_NONE != shf_q_pull_tail(shf, test_qid_free          )) {
                                   shf_q_push_head(shf, test_qid_a2b , shf_qiid);
                                   test_pull_items ++;
            }
            double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
            ok(test_q_items == test_pull_items, "c: moved   expected number of new queue items // estimate %'.0f q items per second using 2 functions", test_q_items / test_elapsed_time);
            shf_debug_verbosity_more();
        }

        {
            shf_debug_verbosity_less();
            double test_start_time = shf_get_time_in_seconds();
            test_pull_items = 0;
            while(SHF_QIID_NONE != shf_q_pull_tail(shf, test_qid_a2b         )) {
                                   shf_q_push_head(shf, test_qid_b2a, shf_qiid);
                                   test_pull_items ++;
            }
            double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
            ok(test_q_items == test_pull_items, "c: moved   expected number of new queue items // estimate %'.0f q items per second using 2 functions", test_q_items / test_elapsed_time);
            shf_debug_verbosity_more();
        }

        {
            shf_debug_verbosity_less();
            double test_start_time = shf_get_time_in_seconds();
            test_pull_items = 0;
            shf_qiid = SHF_QIID_NONE;
            while(SHF_QIID_NONE != shf_q_push_head_pull_tail(shf, test_qid_free, shf_qiid, test_qid_b2a)) {
                                   test_pull_items ++;
            }
            double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
            ok(test_q_items == test_pull_items, "c: moved   expected number of new queue items // estimate %'.0f q items per second using 1 function", test_q_items / test_elapsed_time);
            shf_debug_verbosity_more();
        }

        ok(1, "c: shf_del() // size before deletion: %s", shf_del(shf));

    } // end of non-fixed length tests

    for (uint32_t fixed_len = 0; fixed_len < 2; fixed_len++) {

        const char * test_hint = fixed_len ? "with    fixed length key,values" : "without fixed length key,values";

        // Create a shared hash file in ```/dev/shm``` shared memory and call it a unique (because we use the pid) name so that it cannot conflict with any other tests.
        char  test_shf_name[256];
        char  test_shf_folder[] = "/dev/shm";
        pid_t pid               = getpid();
        SHF_SNPRINTF(1, test_shf_name, "test-%05u-fixed-len-%u", pid, fixed_len);

        // Create a new shared hash file.
                        shf_init            ();
        SHF    * shf =  shf_attach_existing (test_shf_folder, test_shf_name                                  ); ok(NULL == shf, "c: %s: shf_attach_existing() fails for non-existing file as expected", test_hint);
                 shf =  shf_attach          (test_shf_folder, test_shf_name, 1 /* delete upon process exit */); ok(NULL != shf, "c: %s: shf_attach()          works for non-existing file as expected", test_hint);
                        shf_set_is_lockable (shf, 0); /* single threaded test; no need to lock */

        if (fixed_len) {
            shf_set_is_fixed_len(shf, sizeof(uint32_t), sizeof(uint32_t));
        }

        uint32_t test_keys = 100000;
        shf_set_data_need_factor(250);

        {
            shf_debug_verbosity_less();
            char command[256]; SHF_SNPRINTF(1, command, "ps -o rss -p %u | perl -lane 'print if(m~[0-9]~);'", getpid());
            int32_t rss_size_before = atoi(shf_backticks(command));
            double test_start_time = shf_get_time_in_seconds();
            for (uint32_t i = 0; i < test_keys; i++) {
                shf_make_hash(SHF_CAST(const char *, &i), sizeof(i));
                shf_put_key_val(shf, SHF_CAST(const char *, &i), sizeof(i));
            }
            double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
            int32_t rss_size_after = atoi(shf_backticks(command));
            ok(1, "c: %s: put expected number of              keys // estimate %'.0f keys per second, %'uKB RAM", test_hint, test_keys / test_elapsed_time, rss_size_after - rss_size_before);
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
            ok(0 == keys_found, "c: %s: got expected number of non-existing keys // estimate %'.0f keys per second", test_hint, test_keys / test_elapsed_time);
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
            ok(test_keys == keys_found, "c: %s: got expected number of     existing keys // estimate %'.0f keys per second", test_hint, test_keys / test_elapsed_time);
            shf_debug_verbosity_more();
        }

        ok(0 == shf_debug_get_garbage(shf), "c: %s: graceful growth cleans up after itself as expected", test_hint);

        {
            shf_debug_verbosity_less();
            double test_start_time = shf_get_time_in_seconds();
            uint32_t keys_found = 0;
            for (uint32_t i = 0; i < test_keys; i++) {
                              shf_make_hash(SHF_CAST(const char *, &i), sizeof(i));
                keys_found += shf_del_key_val(shf);
            }
            double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
            ok(test_keys == keys_found, "c: %s: del expected number of     existing keys // estimate %'.0f keys per second", test_hint, test_keys / test_elapsed_time);
            shf_debug_verbosity_more();
        }

        ok(0 != shf_debug_get_garbage(shf), "c: %s: del does not    clean  up after itself as expected", test_hint);

        ok(1, "c: %s: shf_del() // size before deletion: %s", test_hint, shf_del(shf));

    } /* for (fixed_len ...)*/

    ok(1, "c: test still alive");

    return exit_status();
} /* main() */
