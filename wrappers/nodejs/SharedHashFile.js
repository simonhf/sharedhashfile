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

plan_tests(17);

console.log('nodejs: debug: about to require  SharedHashFile');
var SharedHashFile = require('./SharedHashFile.node');
console.log('nodejs: debug:          required SharedHashFile');

var testShfFolder = "/dev/shm";
var testShfName   = "test-js-"+process.pid;

var myShf = new SharedHashFile.sharedHashFile();
ok(0           ==         myShf.attachExisting(testShfFolder, testShfName), "nodejs: .attachExisting() fails for non-existing file as expected");
ok(0           !=         myShf.attach        (testShfFolder, testShfName), "nodejs: .attach()         works for non-existing file as expected");
ok('undefined' === typeof myShf.getKeyVal     ("key"                     ), "nodejs: .getKeyVal() could not find unput key as expected");
ok(0           ==         myShf.delKey        ("key"                     ), "nodejs: .delKey()    could not find unput key as expected");
ok(0           !=         myShf.putKeyVal     ("key", "val"              ), "nodejs: .putKeyVal()                  put key as expected");
ok('val'       ===        myShf.getKeyVal     ("key"                     ), "nodejs: .getKeyVal() could     find   put key as expected");
ok(1           ==         myShf.delKey        ("key"                     ), "nodejs: .delKey()    could     find   put key as expected");
ok('undefined' === typeof myShf.getKeyVal     ("key"                     ), "nodejs: .getKeyVal() could not find   del key as expected");
ok(0           ==         myShf.delKey        ("key"                     ), "nodejs: .delKey()    could not find   del key as expected");
ok(0           !=         myShf.putKeyVal     ("key", "val2"             ), "nodejs: .putKeyVal()                reput key as expected");
ok('val2'      ===        myShf.getKeyVal     ("key"                     ), "nodejs: .getKeyVal() could     find reput key as expected");

var test_keys = 250000;

{
    myShf.debugVerbosityLess();
    var test_start_time = Date.now() / 1000;
    for (var i = 0; i < test_keys; i++) {
        myShf.putKeyVal("key"+i, "val"+i);
    }
    var test_elapsed_time = (Date.now() / 1000 - test_start_time);
    ok(1, "nodejs: put expected number of              keys // estimate "+Math.round(test_keys / test_elapsed_time)+" keys per second");
    myShf.debugVerbosityMore();
}

{
    myShf.debugVerbosityLess();
    var test_start_time = Date.now() / 1000;
    for (var i = (test_keys * 2); i < (test_keys * 3); i++) {
        var value = myShf.getKeyVal("key"+i);
        if ('undefined' === typeof value) { /**/ }
        else { console.log("INTERNAL: unexpected value: "+value); process.exit(1); }
    }
    var test_elapsed_time = (Date.now() / 1000 - test_start_time);
    ok(1, "nodejs: got expected number of non-existing keys // estimate "+Math.round(test_keys / test_elapsed_time)+" keys per second");
    myShf.debugVerbosityMore();
}

{
    myShf.debugVerbosityLess();
    var test_start_time = Date.now() / 1000;
    for (var i = 0; i < test_keys; i++) {
        var value = myShf.getKeyVal("key"+i);
        if (value === "val"+i) { /**/ }
        else { console.log("INTERNAL: unexpected value: "+value); process.exit(1); }
    }
    var test_elapsed_time = (Date.now() / 1000 - test_start_time);
    ok(1, "nodejs: got expected number of     existing keys // estimate "+Math.round(test_keys / test_elapsed_time)+" keys per second");
    myShf.debugVerbosityMore();
}

ok(0 == myShf.debugGetBytesMarkedAsDeleted(), "nodejs: graceful growth cleans up after itself as expected");

{
    myShf.debugVerbosityLess();
    var test_start_time = Date.now() / 1000;
    for (var i = 0; i < test_keys; i++) {
        var value = myShf.delKey("key"+i);
        if (value != 1) { console.log("INTERNAL: unexpected value: "+value); process.exit(1); }
    }
    var test_elapsed_time = (Date.now() / 1000 - test_start_time);
    ok(1, "nodejs: del expected number of     existing keys // estimate "+Math.round(test_keys / test_elapsed_time)+" keys per second");
    myShf.debugVerbosityMore();
}

ok(0 != myShf.debugGetBytesMarkedAsDeleted(), "nodejs: del does not    clean  up after itself as expected");

// todo: delete myShf;

exit_status();
