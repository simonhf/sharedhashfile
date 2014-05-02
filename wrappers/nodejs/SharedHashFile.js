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

plan_tests(30);

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

var testQiid;
var shfQiidNone       = 4294967295;
var testPullItems     = 0;
var testQs            = 3;
var testQItems        = 10;
var testQItemSize     = 4096;
var shfQItems         = shf.qNew(testQs, testQItems , testQItemSize, 1);
ok( shfQItems.length ==                  testQItems * testQItemSize, "nodejs: .qNew() returned as expected");                   /* e.g. q items created  by process a */
var testQidFree       = shf.qNewName("qid-free");                                                                               /* e.g. q names set qids by process a */
var testQidA2b        = shf.qNewName("qid-a2b" );
var testQidB2a        = shf.qNewName("qid-b2a" );
ok( testQidFree      == shf.qGetName("qid-free")                   , "nodejs: .qGetName('qid-free') returned qid as expected"); /* e.g. q names get qids by process b */
ok( testQidA2b       == shf.qGetName("qid-a2b" )                   , "nodejs: .qGetName('qid-a2b' ) returned qid as expected");
ok( testQidB2a       == shf.qGetName("qid-b2a" )                   , "nodejs: .qGetName('qid-b2a' ) returned qid as expected");

testPullItems = 0;
while(shfQiidNone != (testQiid = shf.qPullTail(testQidFree          ))) {                                                       /* e.g. q items from unused to a2b q by process a */
                                 shf.qPushHead(testQidA2b , testQiid);
    if(testPullItems != parseInt(shfQItems.substr(testQiid * testQItemSize, 8), 16)) { console.log("INTERNAL: test expected "+testPullItems.toString()+" but got '"+shfQItems.substr(testQiid * testQItemSize, 8)+"'"); process.exit(1); };
    testPullItems ++;
}
ok(testQItems == testPullItems, "nodejs: pulled & pushed items from free to a2b  as expected");

testPullItems = 0;
while(shfQiidNone != (testQiid = shf.qPullTail(testQidA2b           ))) {                                                       /* e.g. q items from a2b to b2a queue by process b */
                                 shf.qPushHead(testQidB2a , testQiid);
    if(testPullItems != parseInt(shfQItems.substr(testQiid * testQItemSize, 8), 16)) { console.log("INTERNAL: test expected "+testPullItems.toString()+" but got '"+shfQItems.substr(testQiid * testQItemSize, 8)+"'"); process.exit(1); };
    testPullItems ++;
}
ok(testQItems == testPullItems, "nodejs: pulled & pushed items from a2b  to b2a  as expected");

testPullItems = 0;
while(shfQiidNone != (testQiid = shf.qPullTail(testQidB2a          ))) {                                                        /* e.g. q items from b2a to free queue by process a */
                                 shf.qPushHead(testQidFree, testQiid);
    if(testPullItems != parseInt(shfQItems.substr(testQiid * testQItemSize, 8), 16)) { console.log("INTERNAL: test expected "+testPullItems.toString()+" but got '"+shfQItems.substr(testQiid * testQItemSize, 8)+"'"); process.exit(1); };
    testPullItems ++;
}
ok(testQItems == testPullItems, "nodejs: pulled & pushed items from b2a  to free as expected");

var testKeys = 100000;
shf.setDataNeedFactor(250);

{
    shf.debugVerbosityLess();
    var testStartTime = Date.now() / 1000;
    for (var i = 0; i < testKeys; i++) {
        shf.putKeyVal("key"+i, "val"+i);
    }
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(1, "nodejs: put expected number of              keys // estimate "+Math.round(testKeys / testElapsedTime).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")+" keys per second");
    shf.debugVerbosityMore();
}

{
    shf.debugVerbosityLess();
    var testStartTime = Date.now() / 1000;
    for (var i = (testKeys * 2); i < (testKeys * 3); i++) {
        var value = shf.getKeyVal("key"+i);
        if ('undefined' === typeof value) { /**/ }
        else { console.log("INTERNAL: unexpected value: "+value); process.exit(1); }
    }
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(1, "nodejs: got expected number of non-existing keys // estimate "+Math.round(testKeys / testElapsedTime).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")+" keys per second");
    shf.debugVerbosityMore();
}

{
    shf.debugVerbosityLess();
    var testStartTime = Date.now() / 1000;
    for (var i = 0; i < testKeys; i++) {
        var value = shf.getKeyVal("key"+i);
        if (value === "val"+i) { /**/ }
        else { console.log("INTERNAL: unexpected value: "+value); process.exit(1); }
    }
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(1, "nodejs: got expected number of     existing keys // estimate "+Math.round(testKeys / testElapsedTime).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")+" keys per second");
    shf.debugVerbosityMore();
}

ok(0 == shf.debugGetGarbage(), "nodejs: graceful growth cleans up after itself as expected");

{
    shf.debugVerbosityLess();
    var testStartTime = Date.now() / 1000;
    for (var i = 0; i < testKeys; i++) {
        var value = shf.delKeyVal("key"+i);
        if (value != 1) { console.log("INTERNAL: unexpected value: "+value); process.exit(1); }
    }
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(1, "nodejs: del expected number of     existing keys // estimate "+Math.round(testKeys / testElapsedTime).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")+" keys per second");
    shf.debugVerbosityMore();
}

ok(0 != shf.debugGetGarbage(), "nodejs: del does not    clean  up after itself as expected");

testQItems = 100000;
shf.setDataNeedFactor(1);

{
    var testStartTime = Date.now() / 1000;
                            shf.debugVerbosityLess();
                            shf.qDel              ();
        shfQItems         = shf.qNew(testQs, testQItems , testQItemSize, 100);
    ok( shfQItems.length ==                  testQItems * testQItemSize, "nodejs: .qNew() returned as expected");
                            shf.debugVerbosityMore();
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(1, "nodejs: created expected number of new queue items // estimate "+Math.round(testKeys / testElapsedTime).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")+" q items per second");
}

{
    var testStartTime = Date.now() / 1000;
    shf.debugVerbosityLess();
    testPullItems = 0;
    while(shfQiidNone != (testQiid = shf.qPullTail(testQidFree         ))) {
                                     shf.qPushHead(testQidA2b , testQiid);
                                     testPullItems ++;
    }
    shf.debugVerbosityMore();
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(testQItems == testPullItems, "nodejs: moved   expected number of new queue items // estimate "+Math.round(testQItems / testElapsedTime).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")+" q items per second using 2 functions");
}

{
    var testStartTime = Date.now() / 1000;
    shf.debugVerbosityLess();
    testPullItems = 0;
    while(shfQiidNone != (testQiid = shf.qPullTail(testQidA2b         ))) {
                                     shf.qPushHead(testQidB2a, testQiid);
                                     testPullItems ++;
    }
    shf.debugVerbosityMore();
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(testQItems == testPullItems, "nodejs: moved   expected number of new queue items // estimate "+Math.round(testQItems / testElapsedTime).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")+" q items per second using 2 functions");
}

{
    var testStartTime = Date.now() / 1000;
    shf.debugVerbosityLess();
    testPullItems = 0;
    var testQiid = shfQiidNone;
    while(shfQiidNone != (testQiid = shf.qPushHeadPullTail(testQidFree, testQiid, testQidB2a))) {
        testPullItems ++;
    }
    shf.debugVerbosityMore();
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(testQItems == testPullItems, "nodejs: moved   expected number of new queue items // estimate "+Math.round(testQItems / testElapsedTime).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")+" q items per second using 1 function");
}

// todo: delete shf;

var exec    = require('child_process').exec;
var command = "echo 'test: shf size before deletion: '`du -h -d 0 "+testShfFolder+"/"+testShfName+".shf` ; rm -rf "+testShfFolder+"/"+testShfName+".shf/"; // todo: change this to auto delete mechanism
exec(command, function (error, output) {
    console.log(output);
    exit_status();
});
