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

plan_tests(7);

console.log('nodejs: debug: about to require  SharedHashFile');
var SharedHashFile = require('./SharedHashFile.node');
console.log('nodejs: debug:          required SharedHashFile');

var testShfFolder = "/dev/shm";
var shfUidNone    = 4294967295;
var shfQidNone    = 4294967295;
var shfQiidNone   = 4294967295;

var                   shf = new SharedHashFile.sharedHashFile();
                      shf.debugVerbosityLess();
ok(0               != shf.attachExisting(testShfFolder, testShfName), "nodejs: .attachExisting() works for existing file as expected");

var testQItemSize     = 4096; // todo: parse this via SharedHashFile from process A
var testQItems        = 100000; // todo: parse this via SharedHashFile from process A
var shfQItems         = shf.qGet();
ok( shfQItems.length != 0                       , "nodejs: .qGet() returned as expected");
var testQidFree       = shf.qGetName("qid-free");
var testQidA2b        = shf.qGetName("qid-a2b" );
var testQidB2a        = shf.qGetName("qid-b2a" );
ok( testQidFree      != shfQidNone              , "nodejs: .qGetName('qid-free') returned qid as expected");
ok( testQidA2b       != shfQidNone              , "nodejs: .qGetName('qid-a2b' ) returned qid as expected");
ok( testQidB2a       != shfQidNone              , "nodejs: .qGetName('qid-b2a' ) returned qid as expected");

{
    shf.raceStart("test-q-race-line", 2);
    ok(1, "nodejs: testing process b IPC queue a2b --> b2a speed");
    var testStartTime  = Date.now() / 1000;
    testPullItems = 0;
    var testQiid = shfQiidNone;
    do {
        while(shfQiidNone != (testQiid = shf.qPushHeadPullTail(testQidB2a, testQiid, testQidA2b))) {
            if (testPullItems % testQItems != parseInt(shfQItems.substr(testQiid * testQItemSize, 8), 16)) { console.log("INTERNAL: test expected "+testPullItems.toString()+" but got '"+shfQItems.substr(testQiid * testQItemSize, 8)+"'"); process.exit(1); };
                testPullItems ++;
            if (testPullItems == 1000000) { shf.qPushHead(testQidB2a, testQiid); break; }
        }
    } while (testPullItems < 1000000);
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(1, "nodejs: moved   expected number of new queue items // estimate "+Math.round(testPullItems / testElapsedTime)+" q items per second with contention");
}

exit_status();
