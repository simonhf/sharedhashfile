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

static uint32_t
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
    plan_tests(201);

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
        SHF    * shf  = shf_attach_existing  (test_shf_folder, test_shf_name     ); ok(NULL == shf, "c: attach                 : shf_attach_existing()   could not find file      as expected");
                 shf  = shf_attach           (test_shf_folder, test_shf_name, bit); ok(NULL != shf, "c: attach                 : shf_attach()            could     make file      as expected");
                        shf_set_is_lockable  (shf, 0                             ); /* single threaded test; no need to lock */

        // Functional tests to exercise the API.
        uint32_t uid;
                                                  SHF_MAKE_HASH        (         "key"    )   /* non-existing    xxx key: op 1 */                                                             ;
        ok(SHF_RET_KEY_NONE                    == shf_get_key_val_copy (shf               ), "c: non-existing    xxx key: op 1: shf_get_key_val_copy()  could not find unput key as expected");
        ok(shf_uid                             == SHF_UID_NONE                             , "c: non-existing    xxx key: op 1: shf_uid                                    unset as expected");
        ok(SHF_RET_KEY_NONE                    == shf_upd_key_val      (shf               ), "c: non-existing    xxx key: op 1: shf_upd_key_val()       could not find unput key as expected");
        ok(SHF_RET_KEY_NONE                    == shf_del_key_val      (shf               ), "c: non-existing    xxx key: op 1: shf_del_key_val()       could not find unput key as expected");
        ok(SHF_RET_KEY_PUT                     == shf_put_key_val      (shf    , "val" , 3), "c:     existing    xxx key: op 2: shf_put_key_val()                        put key as expected"); uid = shf_uid;
        ok(uid                                 != SHF_UID_NONE                             , "c:     existing    xxx key: op 2: shf_put_key_val()                        put key as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_uid_val_copy (shf,uid           ), "c:     existing    uid key: op 2: shf_get_uid_val_copy()  could     find   put key as expected");
        ok(3                                   == shf_val_len                              , "c:     existing    uid key: op 2: shf_val_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_val, "val" , 3), "c:     existing    uid key: op 2: shf_val                                          as expected");
        ok(shf_uid                             != SHF_UID_NONE                             , "c:     existing    uid key: op 3: shf_uid                                      set as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_key_val_copy (shf               ), "c:     existing    get key: op 3: shf_get_key_val_copy()  could     find   put key as expected");
        ok(3                                   == shf_val_len                              , "c:     existing    get key: op 3: shf_val_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_val, "val" , 3), "c:     existing    get key: op 3: shf_val                                          as expected");
        ok(shf_uid                             == uid                                      , "c:     existing    get key: op 3: shf_uid                                      set as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_key_key_copy (shf               ), "c:     existing    get key: op 4: shf_get_key_key_copy()  could     find   put key as expected");
        ok(3                                   == shf_key_len                              , "c:     existing    get key: op 4: shf_key_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_key, "key" , 3), "c:     existing    get key: op 4: shf_key                                          as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_uid_key_copy (shf,uid           ), "c:     existing    get key: op 5: shf_get_uid_key_copy()  could     find   put key as expected");
        ok(3                                   == shf_key_len                              , "c:     existing    get key: op 5: shf_key_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_key, "key" , 3), "c:     existing    get key: op 5: shf_key                                          as expected");
        ok(SHF_RET_OK                          == shf_upd_callback_copy(         "upvZ", 4), "c: bad val size    upd key: op 1: shf_upd_callback_copy() could         preset val as expected");
        ok(SHF_RET_KEY_FOUND + SHF_RET_BAD_CB  == shf_upd_key_val      (shf               ), "c: bad val size    upd key: op 1: shf_upd_key_val()       callback error 4 upd key as expected");
        ok(SHF_RET_KEY_FOUND + SHF_RET_BAD_CB  == shf_upd_uid_val      (shf,uid           ), "c: bad val size    upd uid: op 1: shf_upd_uid_val()       callback error 4 upd uid as expected");
        ok(SHF_RET_OK                          == shf_upd_callback_copy(         "up1" , 3), "c: copy   callback upd key: op 1: shf_upd_callback_copy() could         preset val as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_upd_key_val      (shf               ), "c: copy   callback upd key: op 1: shf_upd_uid_val()       callback works 4 upd key as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_key_val_copy (shf               ), "c: copy   callback upd key: op 1: shf_get_key_val_copy()  could     find   upd key as expected");
        ok(3                                   == shf_val_len                              , "c: copy   callback upd key: op 1: shf_val_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_val, "up1" , 3), "c: copy   callback upd key: op 1: shf_val                                          as expected");
        ok(SHF_RET_OK                          == shf_upd_callback_copy(         "up2" , 3), "c: copy   callback upd uid: op 2: shf_upd_callback_copy() could         preset val as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_upd_uid_val      (shf,uid           ), "c: copy   callback upd uid: op 2: shf_upd_uid_val()       callback works 4 upd uid as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_key_val_copy (shf               ), "c: copy   callback upd uid: op 2: shf_get_key_val_copy()  could     find   upd key as expected");
        ok(3                                   == shf_val_len                              , "c: copy   callback upd uid: op 2: shf_val_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_val, "up2" , 3), "c: copy   callback upd uid: op 2: shf_val                                          as expected");
                                                  shf_upd_callback_set (upd_callback_test )   /* custom callback upd key: op 1 */                                                             ;
        ok(SHF_RET_KEY_FOUND                   == shf_upd_key_val      (shf               ), "c: custom callback upd key: op 1: shf_upd_uid_val()       callback works 4 upd key as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_key_val_copy (shf               ), "c: custom callback upd key: op 1: shf_get_key_val_copy()  could     find   upd key as expected");
        ok(3                                   == shf_val_len                              , "c: custom callback upd key: op 1: shf_val_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_val, "2pu" , 3), "c: custom callback upd key: op 1: shf_val                                          as expected");
                                                  shf_upd_callback_set (upd_callback_test )   /* custom callback upd uid: op 2 */                                                             ;
        ok(SHF_RET_KEY_FOUND                   == shf_upd_uid_val      (shf,uid           ), "c: custom callback upd uid: op 2: shf_upd_uid_val()       callback works 4 upd uid as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_key_val_copy (shf               ), "c: custom callback upd uid: op 2: shf_get_key_val_copy()  could     find   upd key as expected");
        ok(3                                   == shf_val_len                              , "c: custom callback upd uid: op 2: shf_val_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_val, "up2" , 3), "c: custom callback upd uid: op 2: shf_val                                          as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_del_uid_val      (shf,uid           ), "c:     existing    del uid: op 1: shf_del_uid_val()       could     find   put key as expected");
        ok(SHF_RET_KEY_NONE                    == shf_get_key_val_copy (shf               ), "c:     existing    del uid: op 1: shf_get_key_val_copy()  could not find   del key as expected");
        ok(SHF_RET_KEY_NONE                    == shf_del_uid_val      (shf,uid           ), "c: non-existing    del uid: op 1: shf_del_uid_val()       could not find   del key as expected");
        ok(SHF_RET_KEY_PUT                     == shf_put_key_val      (shf    , "val2", 4), "c: reput / reuse key / uid: op 1: shf_put_key_val()                      reput key as expected"); uid = shf_uid;
        ok(uid                                 != SHF_UID_NONE                             , "c: reput / reuse key / uid: op 1: shf_put_key_val()                      reput key as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_uid_val_copy (shf,uid           ), "c: reput / reuse key / uid: op 1: shf_get_uid_val_copy()  could     find reput key as expected");
        ok(4                                   == shf_val_len                              , "c: reput / reuse key / uid: op 1: shf_val_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_val, "val2", 4), "c: reput / reuse key / uid: op 1: shf_val                                          as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_del_key_val      (shf               ), "c: reput / reuse key / uid: op 1: shf_del_key_val()       could     find reput key as expected");
        ok(SHF_RET_KEY_PUT                     == shf_put_key_val      (shf    , "val" , 3), "c: bad-existing    add key: op 1: shf_put_key_val()                      reput key as expected"); uid = shf_uid;
        ok(uid                                 != SHF_UID_NONE                             , "c: bad-existing    add key: op 1: shf_put_key_val()                      reput key as expected");
        ok(SHF_RET_KEY_FOUND + SHF_RET_BAD_VAL == shf_add_key_val      (shf    , 123      ), "c: bad-existing    add key: op 1: shf_add_key_val()       could not use    add key as expected");
        ok(SHF_RET_KEY_FOUND + SHF_RET_BAD_VAL == shf_add_uid_val      (shf,uid, 123      ), "c: bad-existing    add key: op 1: shf_add_uid_val()       could not use    add uid as expected");
        ok(shf_uid                             != SHF_UID_NONE                             , "c: bad-existing    add key: op 1: shf_uid                                      set as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_del_key_val      (shf               ), "c: bad-existing    add key: op 1: shf_del_key_val()       could     find reput key as expected");
        ok(SHF_RET_KEY_NONE                    == shf_get_key_val_copy (shf               ), "c: non-existing    add key: op 1: shf_get_key_val_copy()  could not find unadd key as expected");
        ok(shf_uid                             == SHF_UID_NONE                             , "c: non-existing    add key: op 1: shf_uid                                    unset as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_add_key_val      (shf    , 123      ), "c: non-existing    add key: op 2: shf_add_key_val()       could     create add key as expected"); uid = shf_uid;
        ok(shf_uid                             != SHF_UID_NONE                             , "c: non-existing    add key: op 2: shf_uid                                      set as expected");
        ok(123                                 == shf_val_long                             , "c: non-existing    add key: op 2: shf_val_long                                 set as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_key_val_copy (shf               ), "c: non-existing    add key: op 2: shf_get_key_val_copy()  could     find   add key as expected");
        ok(shf_uid                             != SHF_UID_NONE                             , "c: non-existing    add key: op 2: shf_uid                                      set as expected");
        ok(shf_uid                             == uid                                      , "c: non-existing    add key: op 2: shf_uid is uid and                           set as expected");
        ok(8                                   == shf_val_len                              , "c: non-existing    add key: op 2: shf_val_len                                      as expected");
        ok(123                                 == SHF_CAST(long *, shf_val)[0]             , "c: non-existing    add key: op 2: shf_val                                          as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_add_key_val      (shf    , 123      ), "c:     existing    add key: op 3: shf_add_key_val()       could            add key as expected"); uid = shf_uid;
        ok(shf_uid                             != SHF_UID_NONE                             , "c:     existing    add key: op 3: uid found   and                              set as expected");
        ok(246                                 == shf_val_long                             , "c:     existing    add key: op 3: shf_val_long                                 set as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_key_val_copy (shf               ), "c:     existing    add key: op 3: shf_get_key_val_copy()  could     find   add key as expected");
        ok(shf_uid                             != SHF_UID_NONE                             , "c:     existing    add key: op 3: shf_uid                                      set as expected");
        ok(shf_uid                             == uid                                      , "c:     existing    add key: op 3: shf_uid is uid and                           set as expected");
        ok(8                                   == shf_val_len                              , "c:     existing    add key: op 3: shf_val_len                                      as expected");
        ok(246                                 == SHF_CAST(long *, shf_val)[0]             , "c:     existing    add key: op 3: shf_val                                          as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_add_key_val      (shf    ,-146      ), "c:     existing    add key: op 4: shf_add_key_val()       could            add key as expected"); uid = shf_uid;
        ok(shf_uid                             != SHF_UID_NONE                             , "c:     existing    add key: op 4: uid found   and                              set as expected");
        ok(100                                 == shf_val_long                             , "c:     existing    add key: op 4: shf_val_long                                 set as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_key_val_copy (shf               ), "c:     existing    add key: op 4: shf_get_key_val_copy()  could     find   add key as expected");
        ok(shf_uid                             != SHF_UID_NONE                             , "c:     existing    add key: op 4: shf_uid                                      set as expected");
        ok(shf_uid                             == uid                                      , "c:     existing    add key: op 4: shf_uid is uid and                           set as expected");
        ok(8                                   == shf_val_len                              , "c:     existing    add key: op 4: shf_val_len                                      as expected");
        ok(100                                 == SHF_CAST(long *, shf_val)[0]             , "c:     existing    add key: op 4: shf_val                                          as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_add_key_val      (shf    ,-100      ), "c:     existing    add key: op 5: shf_add_key_val()       could del after  add key as expected");
        ok(shf_uid                             == SHF_UID_NONE                             , "c:     existing    add key: op 5: shf_uid deleted and                        unset as expected");
        ok(0                                   == shf_val_long                             , "c:     existing    add key: op 5: shf_val_long                                 set as expected");
        ok(SHF_RET_KEY_NONE                    == shf_get_key_val_copy (shf               ), "c:     existing    add key: op 5: shf_get_key_val_copy()  could not find   add key as expected");
        ok(shf_uid                             == SHF_UID_NONE                             , "c:     existing    add key: op 5: shf_uid                                    unset as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_add_key_val      (shf    , 111      ), "c:     existing    add key: op 6: shf_add_key_val()       could            add key as expected"); uid = shf_uid;
        ok(SHF_RET_KEY_FOUND                   == shf_add_uid_val      (shf,uid,-111      ), "c:     existing    add key: op 6: shf_add_uid_val()       could del after  add uid as expected");
        ok(shf_uid                             == SHF_UID_NONE                             , "c:     existing    add key: op 6: shf_uid deleted and                        unset as expected");
        ok(0                                   == shf_val_long                             , "c:     existing    add key: op 6: shf_val_long                                 set as expected");
        ok(SHF_RET_KEY_NONE                    == shf_get_key_val_copy (shf               ), "c:     existing    add key: op 6: shf_get_key_val_copy()  could not find   add key as expected");
        ok(shf_uid                             == SHF_UID_NONE                             , "c:     existing    add key: op 6: shf_uid                                    unset as expected");

        // Use own hash -- in this example hard-coded SHA256() -- instead of shf_make_hash() function.
        uint32_t h_foo[] = {0x2c26b46b, 0x68ffc68f, 0xf99b453c, 0x1d304134, 0x13422d70, 0x6483bfa0, 0xf98a5e88, 0x6266e7ae}; /* SHA256("foo") */
        uint32_t h_bar[] = {0xfcde2b2e, 0xdba56bf4, 0x08601fb7, 0x21fe9b5c, 0x338d10ee, 0x429ea04f, 0xae5511b6, 0x8fbf8fb9}; /* SHA256("bar") */

        shf_hash_key_len = strlen("foo") ; /* these lines instead of shf_make_hash() */
        shf_hash_key     =        "foo"  ;
        shf_hash.u32[0]  =       h_foo[0]; /* $ perl -e 'use Digest::SHA; $k=q[foo]; printf qq[- key %s has SHA256 hash: %s\n], $k, Digest::SHA::sha256_hex($k);' */
        shf_hash.u32[1]  =       h_foo[1]; /* - key foo has SHA256 hash: 2c26b46b68ffc68ff99b453c1d30413413422d706483bfa0f98a5e886266e7ae */
        shf_hash.u32[2]  =       h_foo[2];
        shf_hash.u32[3]  =       h_foo[3]; /* part of SHA256 hash unused: 13422d706483bfa0f98a5e886266e7ae */
        ok(SHF_RET_KEY_PUT                     == shf_put_key_val      (shf    , "val" , 3), "c: own hash   foo  xxx key: op 1: shf_put_key_val()                        put key as expected"); uid = shf_uid;
        ok(uid                                 != SHF_UID_NONE                             , "c: own hash   foo  xxx key: op 1: shf_put_key_val()                        put key as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_uid_val_copy (shf,uid           ), "c: own hash   foo  uid key: op 1: shf_get_uid_val_copy()  could     find   put key as expected");
        ok(3                                   == shf_val_len                              , "c: own hash   foo  uid key: op 1: shf_val_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_val, "val" , 3), "c: own hash   foo  uid key: op 1: shf_val                                          as expected");
        ok(shf_uid                             != SHF_UID_NONE                             , "c: own hash   foo  uid key: op 2: shf_uid                                      set as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_key_val_copy (shf               ), "c: own hash   foo  get key: op 2: shf_get_key_val_copy()  could     find   put key as expected");
        ok(3                                   == shf_val_len                              , "c: own hash   foo  get key: op 2: shf_val_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_val, "val" , 3), "c: own hash   foo  get key: op 2: shf_val                                          as expected");
        ok(shf_uid                             == uid                                      , "c: own hash   foo  get key: op 2: shf_uid                                      set as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_key_key_copy (shf               ), "c: own hash   foo  get key: op 3: shf_get_key_key_copy()  could     find   put key as expected");
        ok(shf_hash_key_len                    == shf_key_len                              , "c: own hash   foo  get key: op 3: shf_key_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_key, "foo" , 3), "c: own hash   foo  get key: op 3: shf_key                                          as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_uid_key_copy (shf,uid           ), "c: own hash   foo  get key: op 4: shf_get_uid_key_copy()  could     find   put key as expected");
        ok(shf_hash_key_len                    == shf_key_len                              , "c: own hash   foo  get key: op 4: shf_key_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_key, "foo" , 3), "c: own hash   foo  get key: op 4: shf_key                                          as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_del_key_val      (shf               ), "c: own hash   foo  del key: op 5: shf_del_key_val()       could     find   put key as expected");

        shf_hash_key_len = strlen("bar") ; /* these lines instead of shf_make_hash() */
        shf_hash_key     =        "bar"  ;
        shf_hash.u32[0]  =       h_bar[0]; /* $ perl -e 'use Digest::SHA; $k=q[bar]; printf qq[- key %s has SHA256 hash: %s\n], $k, Digest::SHA::sha256_hex($k);' */
        shf_hash.u32[1]  =       h_bar[1]; /* - key bar has SHA256 hash: fcde2b2edba56bf408601fb721fe9b5c338d10ee429ea04fae5511b68fbf8fb9 */
        shf_hash.u32[2]  =       h_bar[2];
        shf_hash.u32[3]  =       h_bar[3]; /* part of SHA256 hash unused: 338d10ee429ea04fae5511b68fbf8fb9 */
        ok(SHF_RET_KEY_PUT                     == shf_put_key_val      (shf    , "val" , 3), "c: own hash   bar  xxx key: op 1: shf_put_key_val()                        put key as expected"); uid = shf_uid;
        ok(uid                                 != SHF_UID_NONE                             , "c: own hash   bar  xxx key: op 1: shf_put_key_val()                        put key as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_uid_val_copy (shf,uid           ), "c: own hash   bar  uid key: op 1: shf_get_uid_val_copy()  could     find   put key as expected");
        ok(3                                   == shf_val_len                              , "c: own hash   bar  uid key: op 1: shf_val_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_val, "val" , 3), "c: own hash   bar  uid key: op 1: shf_val                                          as expected");
        ok(shf_uid                             != SHF_UID_NONE                             , "c: own hash   bar  uid key: op 2: shf_uid                                      set as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_key_val_copy (shf               ), "c: own hash   bar  get key: op 2: shf_get_key_val_copy()  could     find   put key as expected");
        ok(3                                   == shf_val_len                              , "c: own hash   bar  get key: op 2: shf_val_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_val, "val" , 3), "c: own hash   bar  get key: op 2: shf_val                                          as expected");
        ok(shf_uid                             == uid                                      , "c: own hash   bar  get key: op 2: shf_uid                                      set as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_key_key_copy (shf               ), "c: own hash   bar  get key: op 3: shf_get_key_key_copy()  could     find   put key as expected");
        ok(shf_hash_key_len                    == shf_key_len                              , "c: own hash   bar  get key: op 3: shf_key_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_key, "bar" , 3), "c: own hash   bar  get key: op 3: shf_key                                          as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_uid_key_copy (shf,uid           ), "c: own hash   bar  get key: op 4: shf_get_uid_key_copy()  could     find   put key as expected");
        ok(shf_hash_key_len                    == shf_key_len                              , "c: own hash   bar  get key: op 4: shf_key_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_key, "bar" , 3), "c: own hash   bar  get key: op 4: shf_key                                          as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_del_key_val      (shf               ), "c: own hash   bar  del key: op 5: shf_del_key_val()       could     find   put key as expected");

        shf_hash_key_len = 32                         ; /* these lines instead of shf_make_hash() */
        shf_hash_key     = SHF_CAST(char *, &h_foo[0]);
        shf_hash.u32[0]  =                   h_foo[0] ; /* $ perl -e 'use Digest::SHA; $k=q[foo]; printf qq[- key %s has SHA256 hash: %s\n], $k, Digest::SHA::sha256_hex($k);' */
        shf_hash.u32[1]  =                   h_foo[1] ; /* - key foo has SHA256 hash: 2c26b46b68ffc68ff99b453c1d30413413422d706483bfa0f98a5e886266e7ae */
        shf_hash.u32[2]  =                   h_foo[2] ;
        shf_hash.u32[3]  =                   h_foo[3] ; /* part of SHA256 hash unused: 13422d706483bfa0f98a5e886266e7ae */
        ok(SHF_RET_KEY_PUT                     == shf_put_key_val      (shf    , "val" , 3), "c: own hash h_foo  xxx key: op 1: shf_put_key_val()                        put key as expected"); uid = shf_uid;
        ok(uid                                 != SHF_UID_NONE                             , "c: own hash h_foo  xxx key: op 1: shf_put_key_val()                        put key as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_uid_val_copy (shf,uid           ), "c: own hash h_foo  uid key: op 1: shf_get_uid_val_copy()  could     find   put key as expected");
        ok(3                                   == shf_val_len                              , "c: own hash h_foo  uid key: op 1: shf_val_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_val, "val" , 3), "c: own hash h_foo  uid key: op 1: shf_val                                          as expected");
        ok(shf_uid                             != SHF_UID_NONE                             , "c: own hash h_foo  uid key: op 2: shf_uid                                      set as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_key_val_copy (shf               ), "c: own hash h_foo  get key: op 2: shf_get_key_val_copy()  could     find   put key as expected");
        ok(3                                   == shf_val_len                              , "c: own hash h_foo  get key: op 2: shf_val_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_val, "val" , 3), "c: own hash h_foo  get key: op 2: shf_val                                          as expected");
        ok(shf_uid                             == uid                                      , "c: own hash h_foo  get key: op 2: shf_uid                                      set as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_key_key_copy (shf               ), "c: own hash h_foo  get key: op 3: shf_get_key_key_copy()  could     find   put key as expected");
        ok(shf_hash_key_len                    == shf_key_len                              , "c: own hash h_foo  get key: op 3: shf_key_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_key, h_foo ,32), "c: own hash h_foo  get key: op 3: shf_key                                          as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_uid_key_copy (shf,uid           ), "c: own hash h_foo  get key: op 4: shf_get_uid_key_copy()  could     find   put key as expected");
        ok(shf_hash_key_len                    == shf_key_len                              , "c: own hash h_foo  get key: op 4: shf_key_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_key, h_foo ,32), "c: own hash h_foo  get key: op 4: shf_key                                          as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_del_key_val      (shf               ), "c: own hash h_foo  del key: op 5: shf_del_key_val()       could     find   put key as expected");

        shf_hash_key_len = 32                         ; /* these lines instead of shf_make_hash() */
        shf_hash_key     = SHF_CAST(char *, &h_bar[0]);
        shf_hash.u32[0]  =                   h_bar[0] ; /* $ perl -e 'use Digest::SHA; $k=q[bar]; printf qq[- key %s has SHA256 hash: %s\n], $k, Digest::SHA::sha256_hex($k);' */
        shf_hash.u32[1]  =                   h_bar[1] ; /* - key bar has SHA256 hash: fcde2b2edba56bf408601fb721fe9b5c338d10ee429ea04fae5511b68fbf8fb9 */
        shf_hash.u32[2]  =                   h_bar[2] ;
        shf_hash.u32[3]  =                   h_bar[3] ; /* part of SHA256 hash unused: 338d10ee429ea04fae5511b68fbf8fb9 */
        ok(SHF_RET_KEY_PUT                     == shf_put_key_val      (shf    , "val" , 3), "c: own hash h_bar  xxx key: op 1: shf_put_key_val()                        put key as expected"); uid = shf_uid;
        ok(uid                                 != SHF_UID_NONE                             , "c: own hash h_bar  xxx key: op 1: shf_put_key_val()                        put key as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_uid_val_copy (shf,uid           ), "c: own hash h_bar  uid key: op 1: shf_get_uid_val_copy()  could     find   put key as expected");
        ok(3                                   == shf_val_len                              , "c: own hash h_bar  uid key: op 1: shf_val_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_val, "val" , 3), "c: own hash h_bar  uid key: op 1: shf_val                                          as expected");
        ok(shf_uid                             != SHF_UID_NONE                             , "c: own hash h_bar  uid key: op 2: shf_uid                                      set as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_key_val_copy (shf               ), "c: own hash h_bar  get key: op 2: shf_get_key_val_copy()  could     find   put key as expected");
        ok(3                                   == shf_val_len                              , "c: own hash h_bar  get key: op 2: shf_val_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_val, "val" , 3), "c: own hash h_bar  get key: op 2: shf_val                                          as expected");
        ok(shf_uid                             == uid                                      , "c: own hash h_bar  get key: op 2: shf_uid                                      set as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_key_key_copy (shf               ), "c: own hash h_bar  get key: op 3: shf_get_key_key_copy()  could     find   put key as expected");
        ok(shf_hash_key_len                    == shf_key_len                              , "c: own hash h_bar  get key: op 3: shf_key_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_key, h_bar ,32), "c: own hash h_bar  get key: op 3: shf_key                                          as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_get_uid_key_copy (shf,uid           ), "c: own hash h_bar  get key: op 4: shf_get_uid_key_copy()  could     find   put key as expected");
        ok(shf_hash_key_len                    == shf_key_len                              , "c: own hash h_bar  get key: op 4: shf_key_len                                      as expected");
        ok(0 /* matches */                     == memcmp               (shf_key, h_bar ,32), "c: own hash h_bar  get key: op 4: shf_key                                          as expected");
        ok(SHF_RET_KEY_FOUND                   == shf_del_key_val      (shf               ), "c: own hash h_bar  del key: op 5: shf_del_key_val()       could     find   put key as expected");

        uint32_t win          = 0;
        uint32_t tab          = 0;
        uint32_t tabs_visited = 0;
        uint64_t tabs_len     = 0;
        uint32_t refs_visited = 0;
        uint32_t refs_used    = 0;
        //debug double   t1           = shf_get_time_in_seconds();
        do {
            shf_tab_copy_iterate(shf, &win, &tab);
            tabs_len     += shf_tab_len;
            tabs_visited ++;
            for(uint32_t row = 0; row < SHF_ROWS_PER_TAB; row ++) {
                for(uint32_t ref = 0; ref < SHF_REFS_PER_ROW; ref ++) {
                    refs_visited ++;
                    if (0 == shf_tab->row[row].ref[ref].pos) {
                        /* come here if ref UNused */
                    }
                    else {
                        /* come here if ref used */
                        refs_used ++;
                    }
                }
            }
        } while((win > 0) || (tab > 0));
        //debug double t2 = shf_get_time_in_seconds();
        //debug printf("- %'u=tabs_visited %'lu=tabs_len %'u=refs_visited %'u=refs_used in %f seconds\n", tabs_visited, tabs_len, refs_visited, refs_used, t2 - t1);
        ok(0 == refs_used, "c: search via                     shf_tab_copy_iterate()  could not find       key as expected");

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
                keys_found += (SHF_RET_KEY_FOUND == shf_get_key_val_copy(shf)) ? 1 : 0;
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
                keys_found += (SHF_RET_KEY_FOUND == shf_get_key_val_copy(shf)) ? 1 : 0;
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
                keys_found += (SHF_RET_KEY_FOUND == shf_del_key_val(shf)) ? 1 : 0;
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
