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

var assert = require('assert');

process.argv.forEach(function (val, index, array) {
    console.log("TestIpcQueuejs: debug: command line argument #" + index + ': ' + val);
});
assert(3 == process.argv.length, "TestIpcQueuejs: debug: ASSERT: please include path to unix domain socket on command line!");
var testShfName = process.argv[2];

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

plan_tests(5);

console.log('nodejs: debug: about to require  SharedHashFile');
var SharedHashFile = require('./SharedHashFile.node');
console.log('nodejs: debug:          required SharedHashFile');

var testShfFolder = "/dev/shm";
var shfUidNone    = 4294967295;

var                   shf = new SharedHashFile.sharedHashFile();
                      shf.debugVerbosityLess();
ok(0               != shf.attachExisting(testShfFolder, testShfName), "nodejs: .attachExisting() works for existing file as expected");
var uidQueueUnused  = shf.queueGetName("queue-unused");
var uidQueueA2b     = shf.queueGetName("queue-a2b"   );
var uidQueueB2a     = shf.queueGetName("queue-b2a"   );
ok( uidQueueUnused != shfUidNone, "nodejs: .queueGetName('queue-unused') returned uid as expected");
ok( uidQueueA2b    != shfUidNone, "nodejs: .queueGetName('queue-a2b'   ) returned uid as expected");
ok( uidQueueB2a    != shfUidNone, "nodejs: .queueGetName('queue-b2a'   ) returned uid as expected");

var testKeysExpected = 100000;

{
    var testStartTime;
    testPullItemsTotal = 0;
    var testUidItem = shfUidNone;
    do {
        while('undefined' !== typeof (testValue = shf.queuePushPull(testUidItem, uidQueueB2a, uidQueueA2b))) {
            testUidItem = shf.uid();
            if (testPullItemsTotal  < testKeysExpected) { if (testPullItemsTotal != parseInt(testValue.substr(0,8), 10)) { console.log("INTERNAL: test expected "+testPullItemsTotal.toString()+" but got "+testValue.substr(0,8)); process.exit(1); } }
            if (testPullItemsTotal == testKeysExpected) { testStartTime = Date.now() / 1000; } /* start timing once testKeysExpected have done 1st loop */
            testPullItemsTotal ++;
        }
    } while (testPullItemsTotal < 500000);
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(1, "nodejs: moved   expected number of new queue items // estimate "+Math.round(testPullItemsTotal / testElapsedTime)+" keys per second");
}

exit_status();
