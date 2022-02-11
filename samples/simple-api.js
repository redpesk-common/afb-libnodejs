#!/usr/bin/node

/*
Copyright 2021 Fulup Ar Foll fulup@iot.bzh
Licence: $RP_BEGIN_LICENSE$ SPDX:MIT https://opensource.org/licenses/MIT $RP_END_LICENSE$

object:
    simple-api.nodejs create a single api(demo) with two verbs 'ping' + 'args'
    this api can be requested from REST|websocket from a browser on http:localhost:1234

usage
    - from dev tree: LD_LIBRARY_PATH=../afb-libglue/build/src/ nodejs samples/simple-api.nodejs
    - point your browser at http://localhost:1234/devtools

config: following should match your installation paths
    - devtools alias should point to right path alias= {'/devtools:/usr/share/afb-ui-devtools/binder'},
    - nodejsPATH='/my-node-module-path' (to _afbnodeglue.so)
    - LD_LIBRARY_PATH='/my-glulib-path' (to libafb-glue.so
*/
'use strict';

// find and import libafb module
process.env.NODE_PATH='./build/src';
require("module").Module._initPaths();
const libafb=require('afbnodeglue');

// static variables
global.count=0

// ping/pong test func
function pingTestCB(rqt, ...args) {
    global.count += 1;
    libafb.notice  (rqt, "From pingTestCB count=%d", count);
    return [0, {"pong":count}]; // implicit response
}

function argsTestCB(rqt, ...args) {
    libafb.notice (rqt, "argsTestCB query=%s", args);
    libafb.reply (rqt, 0, {'query': args});
}

function failTestCB(rqt, ...args) {
    // try to print a non exiting variable
    libafb.notice (rqt, "failTestCB query=%s", snoopy);
    libafb.reply (rqt, 0, {'query': args});
}

// called when an api/verb|event callback fail within node
function errorTestCB(rqt, uid, error, ...args) {
    libafb.error (rqt, "FATAL: uid=%s info=%s args=%s", uid, error.message, args);

    if (libafb.config(rqt, 'verbose')) {
        console.log('\nuid:' + uid + ' Stacktrace:')
        console.log('==============================')
        console.log(error.stack);
    }

    if (libafb.replyable(rqt)) {
        var stack = error.stack.split("\n");
        libafb.reply (rqt, -1, {'uid':uid, 'info': error.message, 'trace': stack[1].substring(7)});
    }
}

// api verb list
var demoVerbs = [
    {'uid':'node-ping', 'verb':'ping', 'callback':pingTestCB, 'info':'ping demo function'},
    {'uid':'node-args', 'verb':'args', 'callback':argsTestCB, 'info':'check input query', 'sample':[{'arg1':'arg-one', 'arg2':'arg-two'}, {'argA':1, 'argB':2}]},
    {'uid':'node-fail', 'verb':'fail', 'callback':failTestCB, 'info':'call a failing callback', 'sample':[{'arg1':'arg-one', 'arg2':'arg-two'}, {'argA':1, 'argB':2}]},
]

// define and instantiate API
var demoApi = {
    'uid'     : 'node-demo',
    'api'     : 'demo',
    'class'   : 'test',
    'info'    : 'node api demo',
    'verbose' : 9,
    'export'  : 'public',
    'verbs'   : demoVerbs,
}

// define and instantiate libafb-binder
var demoOpts = {
    'uid'     : 'node-binder',
    'port'    : 1234,
    'verbose' : 9,
    'roothttp': './conf.d/project/htdocs',
    'rootdir' : '.',
    'onerror' : errorTestCB,
    'alias'   : ['/devtools:/usr/share/afb-ui-devtools/binder'],
}

var binder= libafb.binder(demoOpts);
var api = libafb.apiadd(demoApi);
console.log ("### nodejs running ###")