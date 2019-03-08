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

extern "C" {
#include <string.h> /* for memcmp() */
#include <locale.h> /* for setlocale() */

#include <tap.h>
#include <shf.defines.h>
}

#include <SharedHashFile.hpp>

static uint32_t
upd_callback_test(const char * val, uint32_t val_len) /* callback for ->Upd*Val() */
{
    int result = 0;
    SHF_DEBUG("%s(val=%.*s, val_len=%u){} // return %u; testing custom SHF update callback\n", __FUNCTION__, 3, val, val_len, result);
    char temp = val[0];
    SHF_CAST(char *, val)[          0] = val[val_len - 1];
    SHF_CAST(char *, val)[val_len - 1] = temp;
    return result;
} /* upd_callback_test() */

int
main(/* int argc,char **argv */)
{
    plan_tests(130);

    SHF_ASSERT(NULL != setlocale(LC_NUMERIC, ""), "setlocale(): %u: ", errno);

    { // start of non-fixed length tests

        char  testShfName[256];
        char  testShfFolder[] = "/dev/shm";
        pid_t pid             = getpid();
        SHF_SNPRINTF(1, testShfName, "test-%05u", pid);

        uint32_t bit = 1; /* delete shf when calling process exits */
        uint32_t uid;

        SharedHashFile                          * shf = new SharedHashFile;
        ok(                                       shf                                                     , "c++:                          op 1: new SharedHashFile            object         as expected");
        ok(0                                   == shf->IsAttached     (                                  ), "c++: attach                 : op 1: ->IsAttached()      is    not attached       as expected");
        ok(0                                   == shf->AttachExisting (testShfFolder, testShfName        ), "c++: attach                 : op 1: ->AttachExisting()  could not find file      as expected");
        ok(0                                   == shf->IsAttached     (                                  ), "c++: attach                 : op 1: ->IsAttached()      is    not attached       as expected");
        ok(0                                   != shf->Attach         (testShfFolder, testShfName, bit   ), "c++: attach                 : op 1: ->Attach()          could     make file      as expected");
        ok(1                                   == shf->IsAttached     (                                  ), "c++: attach                 : op 1: ->IsAttached()      is        attached       as expected");
                                                  shf->SetIsLockable  (0                                 )     /* single threaded test; no need to lock */                                                 ;
                                                  shf->MakeHash       (         "key" , sizeof("key") - 1)     /* non-existing    xxx key: op 1 */                                                         ;
        ok(SHF_RET_KEY_NONE                    == shf->GetKeyValCopy  (                                  ), "c++: non-existing    xxx key: op 1: ->GetKeyValCopy()   could not find unput key as expected");
        ok(shf_uid                             == SHF_UID_NONE                                            , "c++: non-existing    xxx key: op 1: shf_uid                                unset as expected");
        ok(SHF_RET_KEY_NONE                    == shf->UpdKeyVal      (                                  ), "c++: non-existing    xxx key: op 1: ->UpdKeyVal()       could not find unput key as expected");
        ok(SHF_RET_KEY_NONE                    == shf->DelKeyVal      (                                  ), "c++: non-existing    xxx key: op 1: ->DelKeyVal()       could not find unput key as expected");
           uid                                  = shf->PutKeyVal      (         "val" , 3                )     /*     existing    xxx key: op 2 */                                                         ;
        ok(uid                                 != SHF_UID_NONE                                            , "c++:     existing    xxx key: op 2: ->PutKeyVal()                        put key as expected");
        ok(SHF_RET_KEY_FOUND                   == shf->GetUidValCopy  (uid                               ), "c++:     existing    uid key: op 2: ->GetUidValCopy()   could     find   put key as expected");
        ok(3                                   == shf_val_len                                             , "c++:     existing    uid key: op 2: shf_val_len                                  as expected");
        ok(0 /* matches */                     == memcmp              (shf_val, "val" , 3                ), "c++:     existing    uid key: op 2: shf_val                                      as expected");
        ok(shf_uid                             != SHF_UID_NONE                                            , "c++:     existing    uid key: op 3: shf_uid                                  set as expected");
        ok(SHF_RET_KEY_FOUND                   == shf->GetKeyValCopy  (                                  ), "c++:     existing    get key: op 3: ->GetKeyValCopy()   could     find   put key as expected");
        ok(3                                   == shf_val_len                                             , "c++:     existing    get key: op 3: shf_val_len                                  as expected");
        ok(0 /* matches */                     == memcmp              (shf_val, "val" , 3                ), "c++:     existing    get key: op 3: shf_val                                      as expected");
        ok(shf_uid                             == uid                                                     , "c++:     existing    get key: op 3: shf_uid                                  set as expected");
        ok(SHF_RET_KEY_FOUND                   == shf->GetKeyKeyCopy  (                                  ), "c++:     existing    get key: op 4: ->GetKeyValCopy()   could     find   put key as expected");
        ok(3                                   == shf_key_len                                             , "c++:     existing    get key: op 4: shf_key_len                                  as expected");
        ok(0 /* matches */                     == memcmp              (shf_key, "key" , 3                ), "c++:     existing    get key: op 4: shf_key                                      as expected");
        ok(SHF_RET_KEY_FOUND                   == shf->GetUidKeyCopy  (uid                               ), "c++:     existing    get key: op 5: ->GetKeyValCopy()   could     find   put key as expected");
        ok(3                                   == shf_key_len                                             , "c++:     existing    get key: op 5: shf_key_len                                  as expected");
        ok(0 /* matches */                     == memcmp              (shf_key, "key" , 3                ), "c++:     existing    get key: op 5: shf_key                                      as expected");
        ok(SHF_RET_OK                          == shf->UpdCallbackCopy(         "upvZ", 4                ), "c++: bad val size    upd key: op 1: ->UpdCallbackCopy() could         preset val as expected");
        ok(SHF_RET_KEY_FOUND + SHF_RET_BAD_CB  == shf->UpdKeyVal      (                                  ), "c++: bad val size    upd key: op 1: ->UpdKeyVal()       callback error 4 upd key as expected");
        ok(SHF_RET_KEY_FOUND + SHF_RET_BAD_CB  == shf->UpdUidVal      (uid                               ), "c++: bad val size    upd uid: op 1: ->UpdUidVal()       callback error 4 upd uid as expected");
        ok(SHF_RET_OK                          == shf->UpdCallbackCopy(         "up1" , 3                ), "c++: copy   callback upd key: op 1: ->UpdCallbackCopy() could         preset val as expected");
        ok(SHF_RET_KEY_FOUND                   == shf->UpdKeyVal      (                                  ), "c++: copy   callback upd key: op 1: ->UpdUidVal()       callback works 4 upd key as expected");
        ok(SHF_RET_KEY_FOUND                   == shf->GetKeyValCopy  (                                  ), "c++: copy   callback upd key: op 1: ->GetKeyValCopy()   could     find   upd key as expected");
        ok(3                                   == shf_val_len                                             , "c++: copy   callback upd key: op 1: shf_val_len                                  as expected");
        ok(0 /* matches */                     == memcmp              (shf_val, "up1" , 3                ), "c++: copy   callback upd key: op 1: shf_val                                      as expected");
        ok(SHF_RET_OK                          == shf->UpdCallbackCopy(         "up2" , 3                ), "c++: copy   callback upd uid: op 2: ->UpdCallbackCopy() could         preset val as expected");
        ok(SHF_RET_KEY_FOUND                   == shf->UpdUidVal      (uid                               ), "c++: copy   callback upd uid: op 2: ->UpdUidVal()       callback works 4 upd uid as expected");
        ok(SHF_RET_KEY_FOUND                   == shf->GetKeyValCopy  (                                  ), "c++: copy   callback upd uid: op 2: ->GetKeyValCopy()   could     find   upd key as expected");
        ok(3                                   == shf_val_len                                             , "c++: copy   callback upd uid: op 2: shf_val_len                                  as expected");
        ok(0 /* matches */                     == memcmp              (shf_val, "up2" , 3                ), "c++: copy   callback upd uid: op 2: shf_val                                      as expected");
                                                  shf_upd_callback_set(upd_callback_test                 )     /* custom callback upd key  op 1 */                                                         ;
        ok(SHF_RET_KEY_FOUND                   == shf->UpdKeyVal      (                                  ), "c++: custom callback upd key: op 1: ->UpdUidVal()       callback works 4 upd key as expected");
        ok(SHF_RET_KEY_FOUND                   == shf->GetKeyValCopy  (                                  ), "c++: custom callback upd key: op 1: ->GetKeyValCopy()   could     find   upd key as expected");
        ok(3                                   == shf_val_len                                             , "c++: custom callback upd key: op 1: shf_val_len                                  as expected");
        ok(0 /* matches */                     == memcmp              (shf_val, "2pu" , 3                ), "c++: custom callback upd key: op 1: shf_val                                      as expected");
                                                  shf_upd_callback_set(upd_callback_test                 )     /* custom callback upd uid  op 2 */                                                         ;
        ok(SHF_RET_KEY_FOUND                   == shf->UpdUidVal      (uid                               ), "c++: custom callback upd uid: op 2: ->UpdUidVal()       callback works 4 upd uid as expected");
        ok(SHF_RET_KEY_FOUND                   == shf->GetKeyValCopy  (                                  ), "c++: custom callback upd uid: op 2: ->GetKeyValCopy()   could     find   upd key as expected");
        ok(3                                   == shf_val_len                                             , "c++: custom callback upd uid: op 2: shf_val_len                                  as expected");
        ok(0 /* matches */                     == memcmp              (shf_val, "up2" , 3                ), "c++: custom callback upd uid: op 2: shf_val                                      as expected");

        ok(SHF_RET_KEY_FOUND                   == shf->DelUidVal      (uid                               ), "c++:     existing    del uid: op 1: ->DelUidVal()       could     find   put key as expected");
        ok(SHF_RET_KEY_NONE                    == shf->GetKeyValCopy  (                                  ), "c++:     existing    del uid: op 1: ->GetKeyValCopy()   could not find   del key as expected");
        ok(SHF_RET_KEY_NONE                    == shf->DelUidVal      (uid                               ), "c++: non-existing    del uid: op 1: ->DelUidVal()       could not find   del key as expected");
           uid                                  = shf->PutKeyVal      (         "val2", 4                )     /* reput / reuse key / uid  op 1 */                                                         ;
        ok(uid                                 != SHF_UID_NONE                                            , "c++: reput / reuse key / uid: op 1: ->PutKeyVal()                      reput key as expected");
        ok(SHF_RET_KEY_FOUND                   == shf->GetUidValCopy  (uid                               ), "c++: reput / reuse key / uid: op 1: ->GetUidValCopy()   could     find reput key as expected");
        ok(4                                   == shf_val_len                                             , "c++: reput / reuse key / uid: op 1: shf_val_len                                  as expected");
        ok(0 /* matches */                     == memcmp              (shf_val, "val2", 4                ), "c++: reput / reuse key / uid: op 1: shf_val                                      as expected");
        ok(SHF_RET_KEY_FOUND                   == shf->DelKeyVal      (                                  ), "c++: reput / reuse key / uid: op 1: ->DelKeyVal()       could     find reput key as expected");
           uid                                  = shf->PutKeyVal      (         "val" , 3                )     /* bad-existing    add key: op 1 */                                                             ;
        ok(uid                                 != SHF_UID_NONE                                            , "c++: bad-existing    add key: op 1: ->PutKeyVal()                      reput key as expected");
        ok(SHF_RET_KEY_FOUND + SHF_RET_BAD_VAL == shf->AddKeyVal      (         123                      ), "c++: bad-existing    add key: op 1: ->AddKeyVal()       could not use    add key as expected");
        ok(shf_uid                             != SHF_UID_NONE                                            , "c++: bad-existing    add key: op 1: shf_uid                                  set as expected");
        ok(SHF_RET_KEY_FOUND                   == shf->DelKeyVal      (                                  ), "c++: bad-existing    add key: op 1: ->DelKeyVal()       could     find reput key as expected");
        ok(SHF_RET_KEY_NONE                    == shf->GetKeyValCopy  (                                  ), "c++: non-existing    add key: op 1: ->GetKeyValCopy()   could not find unadd key as expected");
        ok(shf_uid                             == SHF_UID_NONE                                            , "c++: non-existing    add key: op 1: shf_uid                                unset as expected");
        ok(SHF_RET_KEY_FOUND                   == shf->AddKeyVal      (         123                      ), "c++: non-existing    add key: op 2: ->AddKeyVal()       could     create add key as expected"); uid = shf_uid;
        ok(shf_uid                             != SHF_UID_NONE                                            , "c++: non-existing    add key: op 2: shf_uid                                  set as expected");
        ok(123                                 == shf_val_long                                            , "c++: non-existing    add key: op 2: shf_val_long                             set as expected");
        ok(SHF_RET_KEY_FOUND                   == shf->GetKeyValCopy  (                                  ), "c++: non-existing    add key: op 2: ->GetKeyValCopy()   could     find   add key as expected");
        ok(shf_uid                             != SHF_UID_NONE                                            , "c++: non-existing    add key: op 2: shf_uid                                  set as expected");
        ok(shf_uid                             == uid                                                     , "c++: non-existing    add key: op 2: shf_uid is uid and                       set as expected");
        ok(8                                   == shf_val_len                                             , "c++: non-existing    add key: op 2: shf_val_len                                  as expected");
        ok(123                                 == SHF_CAST(long *, shf_val)[0]                            , "c++: non-existing    add key: op 2: shf_val                                      as expected");
        ok(SHF_RET_KEY_FOUND                   == shf->AddKeyVal      (         123                      ), "c++:     existing    add key: op 3: ->AddKeyVal()       could            add key as expected"); uid = shf_uid;
        ok(shf_uid                             != SHF_UID_NONE                                            , "c++:     existing    add key: op 3: uid found   and                          set as expected");
        ok(246                                 == shf_val_long                                            , "c++:     existing    add key: op 3: shf_val_long                             set as expected");
        ok(SHF_RET_KEY_FOUND                   == shf->GetKeyValCopy  (                                  ), "c++:     existing    add key: op 3: ->GetKeyValCopy()   could     find   add key as expected");
        ok(shf_uid                             != SHF_UID_NONE                                            , "c++:     existing    add key: op 3: shf_uid                                  set as expected");
        ok(shf_uid                             == uid                                                     , "c++:     existing    add key: op 3: shf_uid is uid and                       set as expected");
        ok(8                                   == shf_val_len                                             , "c++:     existing    add key: op 3: shf_val_len                                  as expected");
        ok(246                                 == SHF_CAST(long *, shf_val)[0]                            , "c++:     existing    add key: op 3: shf_val                                      as expected");
        ok(SHF_RET_KEY_FOUND                   == shf->AddKeyVal      (        -146                      ), "c++:     existing    add key: op 4: ->AddKeyVal()       could            add key as expected"); uid = shf_uid;
        ok(shf_uid                             != SHF_UID_NONE                                            , "c++:     existing    add key: op 4: uid found   and                          set as expected");
        ok(100                                 == shf_val_long                                            , "c++:     existing    add key: op 4: shf_val_long                             set as expected");
        ok(SHF_RET_KEY_FOUND                   == shf->GetKeyValCopy  (                                  ), "c++:     existing    add key: op 4: ->GetKeyValCopy()   could     find   add key as expected");
        ok(shf_uid                             != SHF_UID_NONE                                            , "c++:     existing    add key: op 4: shf_uid                                  set as expected");
        ok(shf_uid                             == uid                                                     , "c++:     existing    add key: op 4: shf_uid is uid and                       set as expected");
        ok(8                                   == shf_val_len                                             , "c++:     existing    add key: op 4: shf_val_len                                  as expected");
        ok(100                                 == SHF_CAST(long *, shf_val)[0]                            , "c++:     existing    add key: op 4: shf_val                                      as expected");
        ok(SHF_RET_KEY_FOUND                   == shf->AddKeyVal      (        -100                      ), "c++:     existing    add key: op 5: ->AddKeyVal()       could del after  add key as expected");
        ok(shf_uid                             == SHF_UID_NONE                                            , "c++:     existing    add key: op 5: shf_uid deleted and                    unset as expected");
        ok(0                                   == shf_val_long                                            , "c++:     existing    add key: op 5: shf_val_long                             set as expected");
        ok(SHF_RET_KEY_NONE                    == shf->GetKeyValCopy  (                                  ), "c++:     existing    add key: op 5: ->GetKeyValCopy()   could not find   add key as expected");
        ok(shf_uid                             == SHF_UID_NONE                                            , "c++:     existing    add key: op 5: shf_uid                                unset as expected");

        uint32_t testPullItems  = 0;
        uint32_t testQs         = 3;
        uint32_t testQItems     = 10;
        uint32_t testQItemSize  = 4096;
        ok(      0             == shf->QIsReady(                                    ), "c++: ->QIsReady() not ready as expected");
        ok(      NULL          != shf->QNew    (testQs, testQItems, testQItemSize, 1), "c++: ->QNew()     returned  as expected");              /* e.g. q items created  by process a */
        ok(      1             == shf->QIsReady(                                    ), "c++: ->QIsReady()     ready as expected");
        uint32_t testQidFree    = shf->QNewName(SHF_CONST_STR_AND_SIZE("qid-free")  );                                                          /* e.g. q names set qids by process a */
        uint32_t testQidA2b     = shf->QNewName(SHF_CONST_STR_AND_SIZE("qid-a2b" )  );
        uint32_t testQidB2a     = shf->QNewName(SHF_CONST_STR_AND_SIZE("qid-b2a" )  );
        ok(      testQidFree   == shf->QGetName(SHF_CONST_STR_AND_SIZE("qid-free")  ), "c++: ->QGetName('qid-free') returned qid as expected"); /* e.g. q names get qids by process b */
        ok(      testQidA2b    == shf->QGetName(SHF_CONST_STR_AND_SIZE("qid-a2b" )  ), "c++: ->QGetName('qid-a2b' ) returned qid as expected");
        ok(      testQidB2a    == shf->QGetName(SHF_CONST_STR_AND_SIZE("qid-b2a" )  ), "c++: ->QGetName('qid-b2a' ) returned qid as expected");

        testPullItems = 0;
        while(SHF_QIID_NONE != shf->QPullTail(testQidFree          )) {                                                                       /* e.g. q items from unused to a2b q by process a */
                               shf->QPushHead(testQidA2b , shf_qiid);
                               SHF_CAST(uint32_t *, shf_qiid_addr)[0] = testPullItems; /* store q item # in item */
                               testPullItems ++;
        }
        ok(testQItems == testPullItems, "c++: pulled & pushed items from free to a2b  as expected");

        testPullItems = 0;
        while(SHF_QIID_NONE != shf->QPullTail(testQidA2b          )) {                                                                        /* e.g. q items from a2b to b2a queue by process b */
                               shf->QPushHead(testQidB2a, shf_qiid);
                               SHF_ASSERT(testPullItems == SHF_CAST(uint32_t *, shf_qiid_addr)[0], "INTERNAL: test expected q item %u but got %u", testPullItems, SHF_CAST(uint32_t *, shf_qiid_addr)[0]);
                               testPullItems ++;
        }
        ok(testQItems == testPullItems, "c++: pulled & pushed items from a2b  to b2a  as expected");

        testPullItems = 0;
        while(SHF_QIID_NONE != shf->QPullTail(testQidB2a           )) {                                                                       /* e.g. q items from b2a to free queue by process a */
                               shf->QPushHead(testQidFree, shf_qiid);
                               SHF_ASSERT(testPullItems == SHF_CAST(uint32_t *, shf_qiid_addr)[0], "INTERNAL: test expected q item %u but got %u", testPullItems, SHF_CAST(uint32_t *, shf_qiid_addr)[0]);
                               testPullItems ++;
        }
        ok(testQItems == testPullItems, "c++: pulled & pushed items from a2b  to free as expected");

        testQItems = 100000;
        shf->SetDataNeedFactor(1);

        {
            double testStartTime = shf_get_time_in_seconds();
                       shf->DebugVerbosityLess();
            ok(   1 == shf->QIsReady          (                                      ), "c++: ->QIsReady()     ready as expected");
                       shf->QDel              (                                      );
            ok(   0 == shf->QIsReady          (                                      ), "c++: ->QIsReady() not ready as expected");
            ok(NULL != shf->QNew              (testQs, testQItems, testQItemSize, 100), "c++: ->QNew()     returned  as expected");
            ok(   1 == shf->QIsReady          (                                      ), "c++: ->QIsReady()     ready as expected");
                       shf->DebugVerbosityMore();
            double testElapsedTime = shf_get_time_in_seconds() - testStartTime;
            ok(1, "c++: created expected number of new queue items // estimate %'.0f q items per second", testQItems / testElapsedTime);
        }

        {
            shf->DebugVerbosityLess();
            double testStartTime = shf_get_time_in_seconds();
            testPullItems = 0;
            while(SHF_QIID_NONE != shf->QPullTail(testQidFree          )) {
                                   shf->QPushHead(testQidA2b , shf_qiid);
                                   testPullItems ++;
            }
            double testElapsedTime = shf_get_time_in_seconds() - testStartTime;
            ok(testQItems == testPullItems, "c++: moved   expected number of new queue items // estimate %'.0f q items per second using 2 functions", testQItems / testElapsedTime);
            shf->DebugVerbosityMore();
        }

        {
            shf->DebugVerbosityLess();
            double testStartTime = shf_get_time_in_seconds();
            testPullItems = 0;
            while(SHF_QIID_NONE != shf->QPullTail(testQidA2b         )) {
                                   shf->QPushHead(testQidB2a, shf_qiid);
                                   testPullItems ++;
            }
            double testElapsedTime = shf_get_time_in_seconds() - testStartTime;
            ok(testQItems == testPullItems, "c++: moved   expected number of new queue items // estimate %'.0f q items per second using 2 functions", testQItems / testElapsedTime);
            shf->DebugVerbosityMore();
        }

        {
            shf->DebugVerbosityLess();
            double testStartTime = shf_get_time_in_seconds();
            testPullItems = 0;
            shf_qiid = SHF_QIID_NONE;
            while(SHF_QIID_NONE != shf->QPushHeadPullTail(testQidFree, shf_qiid, testQidB2a)) {
                                   testPullItems ++;
            }
            double testElapsedTime = shf_get_time_in_seconds() - testStartTime;
            ok(testQItems == testPullItems, "c++: moved   expected number of new queue items // estimate %'.0f q items per second using 1 function", testQItems / testElapsedTime);
            shf->DebugVerbosityMore();
        }

        ok(1, "c++: ->Del() // size before deletion: %s", shf->Del());

        delete shf;

    } // end of non-fixed length tests

    for (uint32_t fixedLen = 0; fixedLen < 2; fixedLen++) {

        const char * testHint = fixedLen ? "with    fixed length key,values" : "without fixed length key,values";

        char  testShfName[256];
        char  testShfFolder[] = "/dev/shm";
        pid_t pid             = getpid();
        SHF_SNPRINTF(1, testShfName, "test-%05u-fixed-len-%u", pid, fixedLen);

        SharedHashFile * shf = new SharedHashFile;
        ok(0          == shf->AttachExisting(testShfFolder, testShfName        ), "c++: %s: ->AttachExisting() fails for non-existing file as expected", testHint);
        ok(0          == shf->IsAttached    (                                  ), "c++: %s: ->IsAttached()               not attached      as expected", testHint);
        ok(0          != shf->Attach        (testShfFolder, testShfName, 1     ), "c++: %s: ->Attach()         works for non-existing file as expected", testHint);
        ok(1          == shf->IsAttached    (                                  ), "c++: %s: ->IsAttached()                   attached      as expected", testHint);
                         shf->SetIsLockable (0                                 ); /* single threaded test; no need to lock */
        if (fixedLen) {  shf->SetIsFixedLen (sizeof(uint32_t), sizeof(uint32_t)); }

        uint32_t testKeys = 100000;
        shf->SetDataNeedFactor(250);

        {
            shf->DebugVerbosityLess();
            char command[256]; SHF_SNPRINTF(1, command, "ps -o rss -p %u | perl -lane 'print if(m~[0-9]~);'", getpid());
            int32_t rss_size_before = atoi(shf_backticks(command));
            double testStartTime = shf_get_time_in_seconds();
            for (uint32_t i = 0; i < testKeys; i++) {
                shf->MakeHash (SHF_CAST(const char *, &i), sizeof(i));
                shf->PutKeyVal(SHF_CAST(const char *, &i), sizeof(i));
            }
            double testElapsedTime = shf_get_time_in_seconds() - testStartTime;
            int32_t rss_size_after = atoi(shf_backticks(command));
            ok(1, "c++: %s: put expected number of              keys // estimate %'.0f keys per second, %'uKB RAM", testHint, testKeys / testElapsedTime, rss_size_after - rss_size_before);
            shf->DebugVerbosityMore();
        }

        {
            shf->DebugVerbosityLess();
            double testStartTime = shf_get_time_in_seconds();
            uint32_t keys_found = 0;
            for (uint32_t i = (testKeys * 2); i < (testKeys * 3); i++) {
                              shf->MakeHash     (SHF_CAST(const char *, &i), sizeof(i));
                keys_found += shf->GetKeyValCopy();
            }
            double testElapsedTime = shf_get_time_in_seconds() - testStartTime;
            ok(0 == keys_found, "c++: %s: got expected number of non-existing keys // estimate %'.0f keys per second", testHint, testKeys / testElapsedTime);
            shf->DebugVerbosityMore();
        }

        {
            shf->DebugVerbosityLess();
            double testStartTime = shf_get_time_in_seconds();
            uint32_t keys_found = 0;
            for (uint32_t i = 0; i < testKeys; i++) {
                              shf->MakeHash     (SHF_CAST(const char *, &i), sizeof(i));
                keys_found += shf->GetKeyValCopy();
                SHF_ASSERT(sizeof(i) == shf_val_len, "INTERNAL: expected shf_val_len to be %lu but got %u\n", sizeof(i), shf_val_len);
                SHF_ASSERT(0 == memcmp(&i, shf_val, sizeof(i)), "INTERNAL: unexpected shf_val\n");
            }
            double testElapsedTime = shf_get_time_in_seconds() - testStartTime;
            ok(testKeys == keys_found, "c++: %s: got expected number of     existing keys // estimate %'.0f keys per second", testHint, testKeys / testElapsedTime);
            shf->DebugVerbosityMore();
        }

        ok(0 == shf->DebugGetGarbage(), "c++: %s: graceful growth cleans up after itself as expected", testHint);

        {
            shf->DebugVerbosityLess();
            double testStartTime = shf_get_time_in_seconds();
            uint32_t keys_found = 0;
            for (uint32_t i = 0; i < testKeys; i++) {
                              shf->MakeHash (SHF_CAST(const char *, &i), sizeof(i));
                keys_found += shf->DelKeyVal();
            }
            double testElapsedTime = shf_get_time_in_seconds() - testStartTime;
            ok(testKeys == keys_found, "c++: %s: del expected number of     existing keys // estimate %'.0f keys per second", testHint, testKeys / testElapsedTime);
            shf->DebugVerbosityMore();
        }

        ok(0 != shf->DebugGetGarbage(), "c++: %s: del does not    clean  up after itself as expected", testHint);

        ok(1, "c++: %s: ->Del() // size before deletion: %s", testHint, shf->Del());

        delete shf;

    } /* for (fixed_len ...)*/

    ok(1, "c++: test still alive");

    return exit_status();
}
