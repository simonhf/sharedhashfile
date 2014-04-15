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

#include <tap.h>
#include <shf.defines.h>
}

#include <SharedHashFile.hpp>

int
main(/* int argc,char **argv */)
{
    plan_tests(23);

    char  testShfName[256];
    char  testShfFolder[] = "/dev/shm";
    pid_t pid             = getpid();
    SHF_SNPRINTF(1, testShfName, "test-%05u", pid);

    SharedHashFile * shf = new SharedHashFile;
    ok(              shf                                                    , "c++: new SharedHashFile returned object as expected"            );
    ok(0          == shf->AttachExisting(testShfFolder, testShfName        ), "c++: ->AttachExisting() fails for non-existing file as expected");
    ok(0          != shf->Attach        (testShfFolder, testShfName        ), "c++: ->Attach()         works for non-existing file as expected");
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

    uint32_t test_keys = 250000;
    {
        shf->DebugVerbosityLess();
        double test_start_time = shf_get_time_in_seconds();
        for (uint32_t i = 0; i < test_keys; i++) {
            shf->MakeHash (SHF_CAST(const char *, &i), sizeof(i));
            shf->PutKeyVal(SHF_CAST(const char *, &i), sizeof(i));
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(1, "c++: put expected number of              keys // estimate %.0f keys per second", test_keys / test_elapsed_time);
        shf->DebugVerbosityMore();
    }

    {
        shf->DebugVerbosityLess();
        double test_start_time = shf_get_time_in_seconds();
        uint32_t keys_found = 0;
        for (uint32_t i = (test_keys * 2); i < (test_keys * 3); i++) {
                          shf->MakeHash     (SHF_CAST(const char *, &i), sizeof(i));
            keys_found += shf->GetKeyValCopy();
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(0 == keys_found, "c++: got expected number of non-existing keys // estimate %.0f keys per second", test_keys / test_elapsed_time);
        shf->DebugVerbosityMore();
    }

    {
        shf->DebugVerbosityLess();
        double test_start_time = shf_get_time_in_seconds();
        uint32_t keys_found = 0;
        for (uint32_t i = 0; i < test_keys; i++) {
                          shf->MakeHash     (SHF_CAST(const char *, &i), sizeof(i));
            keys_found += shf->GetKeyValCopy();
            SHF_ASSERT(sizeof(i) == shf_val_len, "INTERNAL: expected shf_val_len to be %lu but got %u\n", sizeof(i), shf_val_len);
            SHF_ASSERT(0 == memcmp(&i, shf_val, sizeof(i)), "INTERNAL: unexpected shf_val\n");
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(test_keys == keys_found, "c++: got expected number of     existing keys // estimate %.0f keys per second", test_keys / test_elapsed_time);
        shf->DebugVerbosityMore();
    }

    ok(0 == shf->DebugGetBytesMarkedAsDeleted(), "c++: graceful growth cleans up after itself as expected");

    {
        shf->DebugVerbosityLess();
        double test_start_time = shf_get_time_in_seconds();
        uint32_t keys_found = 0;
        for (uint32_t i = 0; i < test_keys; i++) {
                          shf->MakeHash (SHF_CAST(const char *, &i), sizeof(i));
            keys_found += shf->DelKeyVal();
        }
        double test_elapsed_time = shf_get_time_in_seconds() - test_start_time;
        ok(test_keys == keys_found, "c++: del expected number of     existing keys // estimate %.0f keys per second", test_keys / test_elapsed_time);
        shf->DebugVerbosityMore();
    }

    ok(0 != shf->DebugGetBytesMarkedAsDeleted(), "c++: del does not    clean  up after itself as expected");

    delete shf;

    return exit_status();
}
