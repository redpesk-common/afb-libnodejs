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

// map number of node thread to number of CPU core
const os = require('os')
process.env.UV_THREADPOOL_SIZE = os.cpus().length

// include source tree within package search path
process.env.NODE_PATH='./build/src';
require("module").Module._initPaths();

//import libafb nodejs glue
var libafb=require('afbnodeglue');

// static variables
global.count=0

// ping/pong test func
function pingCB(rqt, ...args) {
    global.count += 1;
    console.log ("I'm in node");
    libafb.notice  (rqt, "From pingCB count=%d", count);
    return [0, {"pong":count}]; // implicit response
}

function argsCB(rqt, ...args) {
    libafb.notice (rqt, "argsCB query=%s", args);
    libafb.reply (rqt, 0, {'query': args});
}

// executed when binder is ready to serve
function loopBinderCb(binder) {
    libafb.notice(binder, "loopBinderCb binder id=%s", libafb.config(binder, "uid"));
    return 0; // keep running for ever
}

// api verb list
var demoVerbs = [
    {'uid':'node-ping', 'verb':'ping', 'callback':pingCB, 'info':'py ping demo function'},
    {'uid':'node-args', 'verb':'args', 'callback':argsCB, 'info':'py check input query', 'sample':[{'arg1':'arg-one', 'arg2':'arg-two'}, {'argA':1, 'argB':2}]},
]

// define and instantiate API
var demoApi = {
    'uid'     : 'node-demo',
    'api'     : 'demo',
    'class'   : 'test',
    'info'    : 'py api demo',
    'verbose' : 9,
    'export'  : 'public',
    'verbs'   : demoVerbs,
    'alias'   : ['/devtools:/usr/share/afb-ui-devtools/binder'],
}

// define and instantiate libafb-binder
var demoOpts = {
    'uid'     : 'node-binder',
    'port'    : 1234,
    'verbose' : 9,
    'roothttp': './conf.d/project/htdocs',
    'rootdir' : '.',
}

var binder= libafb.binder(demoOpts);
var api = libafb.apiadd(demoApi);

// enter mainloop
var status= libafb.mainloop(loopBinderCb)
if (status < 0) {
    libafb.error (binder, "OnError MainLoop Exit");
} else {
    libafb.notice(binder, "OnSuccess Mainloop Exit");
}

