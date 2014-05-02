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

plan_tests(12);

console.log('nodejs: debug: about to require  SharedHashFile');
var SharedHashFile = require('./SharedHashFile.node');
console.log('nodejs: debug:          required SharedHashFile');

var shf = new SharedHashFile.sharedHashFile();

var testKeys = 100000;

{
    shf.debugVerbosityLess();
    var testStartTime = Date.now() / 1000;
    for (var i = 0; i < testKeys; i++) {
        shf.dummy1();
    }
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(1, "nodejs: did expected number of .dummy1()  calls  // estimate "+Math.round(testKeys / testElapsedTime).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")+" calls per second; object: n/a, input: n/a, output: n/a");
    shf.debugVerbosityMore();
}

{
    shf.debugVerbosityLess();
    var testStartTime = Date.now() / 1000;
    for (var i = 0; i < testKeys; i++) {
        shf.dummy2();
    }
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(1, "nodejs: did expected number of .dummy2()  calls  // estimate "+Math.round(testKeys / testElapsedTime).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")+" calls per second; object: unwrapped, input: n/a, output: n/a");
    shf.debugVerbosityMore();
}

{
    shf.debugVerbosityLess();
    var testStartTime = Date.now() / 1000;
    var arg1 = 1;
    for (var i = 0; i < testKeys; i++) {
        shf.dummy3a(arg1);
    }
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(1, "nodejs: did expected number of .dummy3a() calls  // estimate "+Math.round(testKeys / testElapsedTime).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")+" calls per second; object: unwrapped, input: 1 int, output: n/a");
    shf.debugVerbosityMore();
}

{
    shf.debugVerbosityLess();
    var testStartTime = Date.now() / 1000;
    var arg1 = 1; var arg2 = 2; var arg3 = 3;
    for (var i = 0; i < testKeys; i++) {
        shf.dummy3b(arg1, arg2, arg3);
    }
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(1, "nodejs: did expected number of .dummy3b() calls  // estimate "+Math.round(testKeys / testElapsedTime).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")+" calls per second; object: unwrapped, input: 3 ints, output: n/a");
    shf.debugVerbosityMore();
}

{
    shf.debugVerbosityLess();
    var testStartTime = Date.now() / 1000;
    var arg1 = 1; var arg2 = 2; var arg3 = 3;
    for (var i = 0; i < testKeys; i++) {
        var myint = shf.dummy4(arg1, arg2, arg3);
    }
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(1, "nodejs: did expected number of .dummy4()  calls  // estimate "+Math.round(testKeys / testElapsedTime).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")+" calls per second; object: unwrapped, input: 3 ints, output: int");
    shf.debugVerbosityMore();
}

{
    shf.debugVerbosityLess();
    var testStartTime = Date.now() / 1000;
    var arg1 = 1; var arg2 = 2; var arg3 = 3;
    for (var i = 0; i < testKeys; i++) {
        var mystr = shf.dummy5(arg1, arg2, arg3);
        assert(8 == mystr.length, "SharedHashFilejs: debug: ASSERT: expected mystr to be 8 bytes but got "+mystr.length+" bytes");
    }
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(1, "nodejs: did expected number of .dummy5()  calls  // estimate "+Math.round(testKeys / testElapsedTime).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")+" calls per second; object: unwrapped, input: 3 ints, output: 8 byte str");
    shf.debugVerbosityMore();
}

{
    shf.debugVerbosityLess();
    var testStartTime = Date.now() / 1000;
    var arg1 = 1; var arg2 = 2; var arg3 = 3;
    for (var i = 0; i < testKeys; i++) {
        var mystr = shf.dummy6(arg1, arg2, arg3);
        assert(4096 == mystr.length, "SharedHashFilejs: debug: ASSERT: expected mystr to be 4096 bytes but got "+mystr.length+" bytes");
    }
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(1, "nodejs: did expected number of .dummy6()  calls  // estimate "+Math.round(testKeys / testElapsedTime).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")+" calls per second; object: unwrapped, input: 3 ints, output: 4KB byte str");
    shf.debugVerbosityMore();
}

{
    shf.debugVerbosityLess();
    var testStartTime = Date.now() / 1000;
    var arg1 = 1; var arg2 = 2; var arg3 = 3;
    for (var i = 0; i < testKeys; i++) {
        var mystr = shf.dummy7a(arg1, arg2, arg3);
        assert(4096 == mystr.length, "SharedHashFilejs: debug: ASSERT: expected mystr to be 4096 bytes but got "+mystr.length+" bytes");
    }
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(1, "nodejs: did expected number of .dummy7a() calls  // estimate "+Math.round(testKeys / testElapsedTime).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")+" calls per second: object: unwrapped, input: 3 ints, output: 4KB byte str external");
    shf.debugVerbosityMore();
}

{
    shf.debugVerbosityLess();
    var testStartTime = Date.now() / 1000;
    var arg1 = 1; var arg2 = 2; var arg3 = 3;
    for (var i = 0; i < testKeys; i++) {
        var myarr = shf.dummy7b(arg1, arg2, arg3);
        assert(4096 == myarr[0].length, "SharedHashFilejs: debug: ASSERT: expected myarr[0] to be 4096 bytes but got "+myarr[0].length+" bytes");
    }
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(1, "nodejs: did expected number of .dummy7b() calls  // estimate "+Math.round(testKeys / testElapsedTime).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")+" calls per second: object: unwrapped, input: 3 ints, output: 4KB byte str external x1 in array");
    shf.debugVerbosityMore();
}

{
    shf.debugVerbosityLess();
    var testStartTime = Date.now() / 1000;
    var arg1 = 1; var arg2 = 2; var arg3 = 3;
    for (var i = 0; i < testKeys; i++) {
        var myarr = shf.dummy7c(arg1, arg2, arg3);
        assert(4096 == myarr[9].length, "SharedHashFilejs: debug: ASSERT: expected myarr[9] to be 4096 bytes but got "+myarr[9].length+" bytes");
    }
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(1, "nodejs: did expected number of .dummy7c() calls  // estimate "+Math.round(testKeys / testElapsedTime).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")+" calls per second: object: unwrapped, input: 3 ints, output: 4KB byte str external x10 in array");
    shf.debugVerbosityMore();
}

{
    shf.debugVerbosityLess();
    var testStartTime = Date.now() / 1000;
    var arg1 = 1; var arg2 = 2; var arg3 = 3;
    for (var i = 0; i < testKeys; i++) {
        var mystr = shf.dummy8(arg1, arg2, arg3);
        assert(4096 == mystr.length, "SharedHashFilejs: debug: ASSERT: expected mystr to be 4096 bytes but got "+mystr.length+" bytes");
    }
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(1, "nodejs: did expected number of .dummy8()  calls  // estimate "+Math.round(testKeys / testElapsedTime).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")+" calls per second: object: unwrapped, input: 3 ints, output: 4KB byte buffer");
    shf.debugVerbosityMore();
}

{
    shf.debugVerbosityLess();
    var testStartTime = Date.now() / 1000;
    var arg1 = 1; var arg2 = 2; var arg3 = 3;
    for (var i = 0; i < testKeys; i++) {
        var mystr = shf.dummy9(arg1, arg2, arg3);
        assert(4096 == mystr.length, "SharedHashFilejs: debug: ASSERT: expected mystr to be 4096 bytes but got "+mystr.length+" bytes");
    }
    var testElapsedTime = (Date.now() / 1000 - testStartTime);
    ok(1, "nodejs: did expected number of .dummy9()  calls  // estimate "+Math.round(testKeys / testElapsedTime).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")+" calls per second: object: unwrapped, input: 3 ints, output: 4KB byte buffer external");
    shf.debugVerbosityMore();
}

exit_status();
