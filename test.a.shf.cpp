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

int
main(/* int argc,char **argv */)
{
    plan_tests(45);

    SHF_ASSERT(NULL != setlocale(LC_NUMERIC, ""), "setlocale(): %u: ", errno);

    char  testShfName[256];
    char  testShfFolder[] = "/dev/shm";
    pid_t pid             = getpid();
    SHF_SNPRINTF(1, testShfName, "test-%05u", pid);

    SharedHashFile * shf = new SharedHashFile;
    ok(              shf                                                    , "c++: new SharedHashFile returned object as expected"            );
    ok(0          == shf->IsAttached    (                                  ), "c++: ->IsAttached()               not attached      as expected");
    ok(0          == shf->AttachExisting(testShfFolder, testShfName        ), "c++: ->AttachExisting() fails for non-existing file as expected");
    ok(0          == shf->IsAttached    (                                  ), "c++: ->IsAttached()               not attached      as expected");
    ok(0          != shf->Attach        (testShfFolder, testShfName, 1     ), "c++: ->Attach()         works for non-existing file as expected");
    ok(1          == shf->IsAttached    (                                  ), "c++: ->IsAttached()                   attached      as expected");
                     shf->MakeHash      (         "key" , sizeof("key") - 1)                                                                    ;
    ok(0          == shf->GetKeyValCopy (                                  ), "c++: ->GetKeyValCopy() could not find unput key as expected"    );
    ok(0          == shf->DelKeyVal     (                                  ), "c++: ->DelKeyVal()     could not find unput key as expected"    );
    uint32_t  uid =  shf->PutKeyVal     (         "val" , 3                )                                                                    ;
    ok(       uid != SHF_UID_NONE                                           , "c++: ->PutKeyVal()                      put key as expected"    );
    ok(1          == shf->GetKeyValCopy (                                  ), "c++: ->GetKeyValCopy() could     find   put key as expected"    );
    ok(3          == shf_val_len                                            , "c++: shf_val_len                                as expected"    );
    ok(0          == memcmp             (shf_val, "val" , 3                ), "c++: shf_val                                    as expected"    );
    ok(1          == shf->DelUidVal     (uid                               ), "c++: ->DelUidVal()     could     find   put key as expected"    );
    ok(0          == shf->GetKeyValCopy (                                  ), "c++: ->GetKeyValCopy() could not find   del key as expected"    );
    ok(0          == shf->DelUidVal     (uid                               ), "c++: ->DelUidVal()     could not find   del key as expected"    );
              uid =  shf->PutKeyVal     (         "val2", 4                )                                                                    ;
    ok(       uid != SHF_UID_NONE                                           , "c++: ->PutKeyVal()                    reput key as expected"    );
    ok(1          == shf->GetUidValCopy (uid                               ), "c++: ->GetUidValCopy() could     find reput key as expected"    );
    ok(4          == shf_val_len                                            , "c++: shf_val_len                                as expected"    );
    ok(0          == memcmp             (shf_val, "val2", 4                ), "c++: shf_val                                    as expected"    );
    ok(1          == shf->DelKeyVal     (                                  ), "c++: ->DelKeyVal()     could     find reput key as expected"    );

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

    uint32_t testKeys = 100000;
    shf->SetDataNeedFactor(250);

    {
        shf->DebugVerbosityLess();
        double testStartTime = shf_get_time_in_seconds();
        for (uint32_t i = 0; i < testKeys; i++) {
            shf->MakeHash (SHF_CAST(const char *, &i), sizeof(i));
            shf->PutKeyVal(SHF_CAST(const char *, &i), sizeof(i));
        }
        double testElapsedTime = shf_get_time_in_seconds() - testStartTime;
        ok(1, "c++: put expected number of              keys // estimate %'.0f keys per second", testKeys / testElapsedTime);
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
        ok(0 == keys_found, "c++: got expected number of non-existing keys // estimate %'.0f keys per second", testKeys / testElapsedTime);
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
        ok(testKeys == keys_found, "c++: got expected number of     existing keys // estimate %'.0f keys per second", testKeys / testElapsedTime);
        shf->DebugVerbosityMore();
    }

    ok(0 == shf->DebugGetGarbage(), "c++: graceful growth cleans up after itself as expected");

    {
        shf->DebugVerbosityLess();
        double testStartTime = shf_get_time_in_seconds();
        uint32_t keys_found = 0;
        for (uint32_t i = 0; i < testKeys; i++) {
                          shf->MakeHash (SHF_CAST(const char *, &i), sizeof(i));
            keys_found += shf->DelKeyVal();
        }
        double testElapsedTime = shf_get_time_in_seconds() - testStartTime;
        ok(testKeys == keys_found, "c++: del expected number of     existing keys // estimate %'.0f keys per second", testKeys / testElapsedTime);
        shf->DebugVerbosityMore();
    }

    ok(0 != shf->DebugGetGarbage(), "c++: del does not    clean  up after itself as expected");

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

    ok(1 == shf->IsAttached(), "c++: ->IsAttached()     attached as expected");
            shf->Detach    ();
    ok(0 == shf->IsAttached(), "c++: ->IsAttached() not attached as expected");

    delete  shf;

    char test_du_folder[256]; SHF_SNPRINTF(1, test_du_folder, "du -h -d 0 %s/%s.shf ; rm -rf %s/%s.shf/", testShfFolder, testShfName, testShfFolder, testShfName);
    fprintf(stderr, "test: shf size before deletion: %s\n", shf_backticks(test_du_folder)); // todo: change this to auto delete mechanism

    return exit_status();
}
