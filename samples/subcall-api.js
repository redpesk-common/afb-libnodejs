#!/usr/bin/node

/*
Copyright 2021 Fulup Ar Foll fulup@iot.bzh
Licence: $RP_BEGIN_LICENSE$ SPDX:MIT https://opensource.org/licenses/MIT $RP_END_LICENSE$

object:
    subcall-api.py
    - 1) load helloworld binding
    - 2) create a 'demo' api requiring 'helloworld' api
    - 3) check helloworld/ping is responsing before exposing http service (mainLoopCb)
    - 4) implement two verbs demo/sync|async those two verb subcall helloworld/testargs in synchronous/asynchronous mode
    - 5) subscribe event request timer event from helloworld-event api
    demo/sync|async|subscribe|unsubscribe can be requested from REST|websocket from a browser on http:localhost:1234

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

function helloEventCB (api, name, ...data) {
    libafb.notice  (api, "helloEventCB name=%s received", name)
}

function otherEventCB (api, name, ...data) {
    libafb.notice  (api, "otherEventCB name=%s data=%s", name, data)
}

function asyncRespCB(rqt, status, ctx, ...args) {
    libafb.notice  (rqt, "asyncRespCB status=%d ctx:'%s', response:'%s'", status, ctx, args)
    libafb.reply (rqt, status, 'async helloworld/testargs', args)
}

function asyncCB(rqt, ...args) {
    var userdata= {'userdata':"context-user-data"} // any object(rqt, argument, ...) to be used as userdata
    libafb.notice  (rqt, "asyncCB calling helloworld/testargs ...args=%s", args)
    libafb.callasync (rqt,"helloworld", "testargs", asyncRespCB, userdata, args[0])
    // response within 'asyncRespCB' callback
}

function promiseCB(rqt, ...args) {
    libafb.notice  (rqt, "asyncCB calling helloworld/testargs ...args=%s", args)

    libafb.subcall (rqt,"helloworld", "testargs", args[0])
        .then (response => {
            libafb.notice(rqt, "PromiseCB Success: status=%d response=%s" , response.status, response.args)
            libafb.reply (rqt, response.status, "PromiseCB Success")
            })
        .catch(response => {
            libafb.error (rqt, "PromiseCB Fail: status=%d response=%s" , response.status, response.args)
            libafb.reply (rqt, response.status, "PromiseCB Fail")
            })
        .finally(()=> {libafb.info  (rqt, "PromiseCB Done")})
}

function syncCB(rqt, ...args) {
    libafb.notice  (rqt, "syncCB calling helloworld/testargs ...args=%s", args)
    var response= libafb.callsync(rqt, "helloworld","testargs", args[0])

    if (response.status !== 0) libafb.reply (rqt, response.status, 'async helloworld/testargs fail')
    else libafb.reply (rqt, response.status, 'async helloworld/testargs success')
}

// api control function
function controlApiCB(api, state) {
    var apiname= libafb.config(api, "api")
    libafb.notice(api, "api=[%s] state=[%s]", apiname, state)

    if (state == 'config') {
        libafb.notice(api, "config=%s", libafb.getuid(api))
    }
    return 0 // ok
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

// verbs callback list
var demoVerbs = [
    {'uid':'node-ping'    , 'verb':'ping'   , 'callback':pingTestCB , 'info':'node ping demo function'},
    {'uid':'node-synccall', 'verb':'sync'   , 'callback':syncCB     , 'info':'synchronous subcall of private api' , 'sample':[{'cezam':'open'}, {'cezam':'close'}]},
    {'uid':'node-asyncall', 'verb':'async'  , 'callback':asyncCB    , 'info':'asynchronous subcall of private api', 'sample':[{'cezam':'open'}, {'cezam':'close'}]},
    {'uid':'node-promise' , 'verb':'promise', 'callback':promiseCB  , 'info':'asynchronous subcall of private api', 'sample':[{'cezam':'open'}, {'cezam':'close'}]},
]

// events callback list
var demoEvents = [
    {'uid':'py-event' , 'pattern':'helloworld-event/timerCount', 'callback':helloEventCB , 'info':'timer event handler'},
    {'uid':'py-other' , 'pattern':'*', 'callback':otherEventCB , 'info':'any other event handler'},
]

// functionine and instantiate API
var demoApi = {
    'uid'     : 'node-demo',
    'api'     : 'demo',
    'class'   : 'test',
    'info'    : 'node subcall demo',
    'verbose' : 9,
    'export'  : 'public',
    'require' : 'helloworld',
    'verbs'   : demoVerbs,
    'events'  : demoEvents,
    'control' : controlApiCB,
    'alias'   : ['/devtools:/usr/share/afb-ui-devtools/binder'],
}

// helloworld binding sample functioninition
var HelloBinding = {
    'uid'    : 'helloworld',
    'export' : 'private',
    'path'   : 'afb-helloworld-skeleton.so',
}

var EventBinding = {
    'uid'    : 'helloworld',
    'export' : 'private',
    'path'   : 'afb-helloworld-subscribe-event.so',
}

// functionine and instantiate libafb-binder
var demoOpts = {
    'uid'     : 'node-binder',
    'port'    : 1234,
    'verbose' : 9,
    'roothttp': './conf.d/project/htdocs',
    'rootdir' : '.',
    'ldpath' : [process.env.HOME + '/opt/helloworld-binding/lib','/usr/local/helloworld-binding/lib'],
    'alias'  : ['/devtools:/usr/share/afb-ui-devtools/binder'],
    'onerror' : errorTestCB,
}

libafb.binder(demoOpts);
libafb.binding(HelloBinding)
libafb.binding(EventBinding)
libafb.apiadd(demoApi);
console.log ("### nodejs running ###")