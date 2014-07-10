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
    console.log("----:TestIpcSocketjs: debug: command line argument #" + index + ': ' + val);
});
assert(3 == process.argv.length, "----:TestIpcSocketjs: ERROR: assertion: please include path to unix domain socket on command line!");
var unix_domain_socket = process.argv[2];

var incoming_messages = 0;
var incoming_messages_max = 10000;
var net = require('net');
var client = net.connect({path: unix_domain_socket},
    function() { //'connect' listener
        console.log('----:TestIpcSocketjs: debug: client connected; sending connect, waiting for hello');
        client.write('connect!');
});
client.on('data', function(data) {
    incoming_messages ++;
    if (incoming_messages <= 3) {
        console.log('----:TestIpcSocketjs: debug: read: ' + data.length + " bytes: " + data.toString().substr(0,5) + "...");
    }
    if (incoming_messages < incoming_messages_max) {
        client.write('world!');
    }
    else {
        console.log('----:TestIpcSocketjs: debug: ending after ' + incoming_messages_max + ' messages');
        client.end();
    }
});
client.on('end', function() {
    console.log('----:TestIpcSocketjs: debug: client disconnected');
});
