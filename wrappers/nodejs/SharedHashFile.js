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

var ok_tests = 0;
var ok_tests_expected;
function ok(condition, what) {
    ok_tests ++;
    if (condition) { console.log(    "ok "+ok_tests+" - "+what); }
    else           { console.log("not ok "+ok_tests+" - "+what); process.exit(1); }
}
function plan_tests(tests_expected) {
    ok_tests_expected = tests_expected;
    console.log("1.."+ok_tests_expected);
}
function exit_status() {
    if (ok_tests == ok_tests_expected) { process.exit(0); }
    else                               { console.log("# Looks like you planned "+ok_tests_expected+" tests but ran "+ok_tests); process.exit(1); }
}

plan_tests(28);

console.log('nodejs: debug: about to require  SharedHashFile');
var SharedHashFile = require('./SharedHashFile.node');
console.log('nodejs: debug:          required SharedHashFile');

var testShfFolder = "/dev/shm";
var testShfName   = "test-js-"+process.pid;
var shfUidNone    = 4294967295;

var        shf =      new SharedHashFile.sharedHashFile();
ok(0           ==         shf.attachExisting(testShfFolder, testShfName), "nodejs: .attachExisting() fails for non-existing file as expected");
ok(0           !=         shf.attach        (testShfFolder, testShfName), "nodejs: .attach()         works for non-existing file as expected");
ok('undefined' === typeof shf.getKeyVal     ("key"                     ), "nodejs: .getKeyVal() could not find unput key as expected"        );
ok(0           ==         shf.delKeyVal     ("key"                     ), "nodejs: .delKeyVal() could not find unput key as expected"        );
var        uid =          shf.putKeyVal     ("key", "val"              )                                                                      ;
ok(        uid !=         shfUidNone                                    , "nodejs: .putKeyVal()                  put key as expected"        );
ok('val'       ===        shf.getKeyVal     ("key"                     ), "nodejs: .getKeyVal() could     find   put key as expected"        );
ok(1           ==         shf.delUidVal     ( uid                      ), "nodejs: .delUidVal() could     find   put key as expected"        );
ok('undefined' === typeof shf.getKeyVal     ("key"                     ), "nodejs: .getKeyVal() could not find   del key as expected"        );
ok(0           ==         shf.delUidVal     ( uid                      ), "nodejs: .delUidVal() could not find   del key as expected"        );
           uid =          shf.putKeyVal     ("key", "val2"             )                                                                      ;
ok(        uid !=         shfUidNone                                    , "nodejs: .putKeyVal()                reput key as expected"        );
ok('val2'      ===        shf.getUidVal     ( uid                      ), "nodejs: .getUidVal() could     find reput key as expected"        );
ok(1           ==         shf.delKeyVal     ("key"                     ), "nodejs: .delKeyVal() could     find reput key as expected"        );

var uidQueueUnused =  shf.queueNewName("queue-unused"); /* e.g. queue names created by process a */
var uidQueueA2b    =  shf.queueNewName("queue-a2b"   );
var uidQueueB2a    =  shf.queueNewName("queue-b2a"   );
ok( uidQueueUnused == shf.queueGetName("queue-unused"), "nodejs: .queueGetName('queue-unused') returned uid as expected");  /* e.g. queue names got by process b */
ok( uidQueueA2b    == shf.queueGetName("queue-a2b"   ), "nodejs: .queueGetName('queue-a2b'   ) returned uid as expected");
ok( uidQueueB2a    == shf.queueGetName("queue-b2a"   ), "nodejs: .queueGetName('queue-b2a'   ) returned uid as expected");

var testPullResult;
var testPullItemsTotal;
var testQueueItemsTotal = 10;
var testQueueItemDataSize = 4096;
for (var i = 0; i < testQueueItemsTotal; i++) { /* e.g. queue items created & queued in unused queue by process a */
    uid = shf.queueNewItem     (testQueueItemDataSize);
          shf.queuePushHeadData(uidQueueUnused, uid, i.toString());
}

testPullItemsTotal = 0;
var testValue;
var testUidItem = shfUidNone;
while('undefined' !== typeof (testValue = shf.queuePushPull(testUidItem, uidQueueA2b, uidQueueUnused))) { /* e.g. items transferred from unused to a2b queue by process a */
    //console.log('nodejs: debug: testValue.length '+testValue.length+', first byte is >'+testValue.substr(0,1)+'<');
    if(testPullItemsTotal != parseInt(testValue.substr(0,1), 10)) { console.log("INTERNAL: test expected "+testPullItemsTotal.toString()+" but got "+testValue.substr(0,1)); process.exit(1); };
    testUidItem = shf.uid();
    testPullItemsTotal ++;
}
ok(testQueueItemsTotal == testPullItemsTotal, "nodejs: pulled & pushed items from unused to a2b    as expected");

testPullItemsTotal = 0;
var testUidItem = shfUidNone;
while('undefined' !== typeof (testValue = shf.queuePushPull(testUidItem, uidQueueB2a, uidQueueA2b))) { /* e.g. items transferred from a2b to b2a queue by process b */
    if(testPullItemsTotal != parseInt(testValue.substr(0,1), 10)) { console.log("INTERNAL: test expected "+testPullItemsTotal.toString()+" but got "+testValue.substr(0,1)); process.exit(1); };
    testUidItem = shf.uid();
    testPullItemsTotal ++;
}
ok(testQueueItemsTotal == testPullItemsTotal, "nodejs: pulled & pushed items from a2b    to b2a    as expected");

testPullItemsTotal = 0;
var testUidItem = shfUidNone;
while('undefined' !== typeof (testValue = shf.queuePushPull(testUidItem, uidQueueUnused, uidQueueB2a))) { /* e.g. items transferred from b2a to unused queue by process a */
    if(testPullItemsTotal != parseInt(testValue.substr(0,1), 10)) { console.log("INTERNAL: test expected "+testPullItemsTotal.toString()+" but got "+testValue.substr(0,1)); process.exit(1); };
    testUidItem = shf.uid();
    testPullItemsTotal ++;
}
ok(testQueueItemsTotal == testPullItemsTotal, "nodejs: pulled & pushed items from b2a    to unused as expected");

var test_keys = 250000;
shf.setDataNeedFactor(250);

{
    shf.debugVerbosityLess();
    var test_start_time = Date.now() / 1000;
    for (var i = 0; i < test_keys; i++) {
        shf.putKeyVal("key"+i, "val"+i);
    }
    var test_elapsed_time = (Date.now() / 1000 - test_start_time);
    ok(1, "nodejs: put expected number of              keys // estimate "+Math.round(test_keys / test_elapsed_time)+" keys per second");
    shf.debugVerbosityMore();
}

{
    shf.debugVerbosityLess();
    var test_start_time = Date.now() / 1000;
    for (var i = (test_keys * 2); i < (test_keys * 3); i++) {
        var value = shf.getKeyVal("key"+i);
        if ('undefined' === typeof value) { /**/ }
        else { console.log("INTERNAL: unexpected value: "+value); process.exit(1); }
    }
    var test_elapsed_time = (Date.now() / 1000 - test_start_time);
    ok(1, "nodejs: got expected number of non-existing keys // estimate "+Math.round(test_keys / test_elapsed_time)+" keys per second");
    shf.debugVerbosityMore();
}

{
    shf.debugVerbosityLess();
    var test_start_time = Date.now() / 1000;
    for (var i = 0; i < test_keys; i++) {
        var value = shf.getKeyVal("key"+i);
        if (value === "val"+i) { /**/ }
        else { console.log("INTERNAL: unexpected value: "+value); process.exit(1); }
    }
    var test_elapsed_time = (Date.now() / 1000 - test_start_time);
    ok(1, "nodejs: got expected number of     existing keys // estimate "+Math.round(test_keys / test_elapsed_time)+" keys per second");
    shf.debugVerbosityMore();
}

ok(0 == shf.debugGetGarbage(), "nodejs: graceful growth cleans up after itself as expected");

{
    shf.debugVerbosityLess();
    var test_start_time = Date.now() / 1000;
    for (var i = 0; i < test_keys; i++) {
        var value = shf.delKeyVal("key"+i);
        if (value != 1) { console.log("INTERNAL: unexpected value: "+value); process.exit(1); }
    }
    var test_elapsed_time = (Date.now() / 1000 - test_start_time);
    ok(1, "nodejs: del expected number of     existing keys // estimate "+Math.round(test_keys / test_elapsed_time)+" keys per second");
    shf.debugVerbosityMore();
}

ok(0 != shf.debugGetGarbage(), "nodejs: del does not    clean  up after itself as expected");

{
    shf.debugVerbosityLess();
    var test_start_time = Date.now() / 1000;
    for (var i = 0; i < test_keys; i++) {
        uid = shf.queueNewItem (testQueueItemDataSize);
              shf.queuePushHeadData(uidQueueUnused, uid, i.toString());
    }
    var test_elapsed_time = (Date.now() / 1000 - test_start_time);
    ok(1, "nodejs: created expected number of new queue items // estimate "+Math.round(test_keys / test_elapsed_time)+" keys per second");
    shf.debugVerbosityMore();
}

{
    shf.debugVerbosityLess();
    var test_start_time = Date.now() / 1000;
    testPullItemsTotal = 0;
    var testUidItem = shfUidNone;
    while('undefined' !== typeof (testValue = shf.queuePushPull(testUidItem, uidQueueA2b, uidQueueUnused))) {
        testUidItem = shf.uid();
        testPullItemsTotal ++;
    }
    var test_elapsed_time = (Date.now() / 1000 - test_start_time);
    ok(testQueueItemsTotal + test_keys == testPullItemsTotal, "nodejs: moved   expected number of new queue items // estimate "+Math.round(test_keys / test_elapsed_time)+" keys per second");
    shf.debugVerbosityMore();
}

{
    shf.debugVerbosityLess();
    var test_start_time = Date.now() / 1000;
    testPullItemsTotal = 0;
    var testUidItem = shfUidNone;
    while('undefined' !== typeof (testValue = shf.queuePushPull(testUidItem, uidQueueB2a, uidQueueA2b))) {
        testUidItem = shf.uid();
        testPullItemsTotal ++;
    }
    var test_elapsed_time = (Date.now() / 1000 - test_start_time);
    ok(testQueueItemsTotal + test_keys == testPullItemsTotal, "nodejs: moved   expected number of new queue items // estimate "+Math.round(test_keys / test_elapsed_time)+" keys per second");
    shf.debugVerbosityMore();
}

{
    shf.debugVerbosityLess();
    var test_start_time = Date.now() / 1000;
    testPullItemsTotal = 0;
    var testUidItem = shfUidNone;
    while('undefined' !== typeof (testValue = shf.queuePushPull(testUidItem, uidQueueUnused, uidQueueB2a))) {
        testUidItem = shf.uid();
        testPullItemsTotal ++;
    }
    var test_elapsed_time = (Date.now() / 1000 - test_start_time);
    ok(testQueueItemsTotal + test_keys == testPullItemsTotal, "nodejs: moved   expected number of new queue items // estimate "+Math.round(test_keys / test_elapsed_time)+" keys per second");
    shf.debugVerbosityMore();
}

// todo: delete shf;

exit_status();
