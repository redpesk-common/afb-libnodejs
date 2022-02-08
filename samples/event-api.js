//!/usr/bin/node

/*
Copyright 2021 Fulup Ar Foll fulup@iot.bzh
Licence: $RP_BEGIN_LICENSE$ SPDX:MIT https://opensource.org/licenses/MIT $RP_END_LICENSE$

object:
    event-api.node
    - 1st create 'demo' api
    - 2nd when API ready, create an event named 'node-event'
    - 3rd creates a timer(node-timer) that tic every 3s and call onTimerCB that fire previsouly created event
    - 4rd implements two verbs demo/subscribe and demo/unscribe
    - 5rd attaches two events handler to the API evtTimerCB/evtOtherCB those handlers requiere a subcall to subscribe some event
    demo/subscribe|unsubscribe can be requested from REST|websocket from a browser on http:localhost:1234

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
var libafb=require('afbnodeglue');

// static variables
global.count=0
global.tic=0
global.event=null

// ping/pong test func
function pingTestCB(rqt, ...args) {
    global.count += 1;
    libafb.notice  (rqt, "From pingTestCB count=%d", count);
    return [0, {"pong":count}]; // implicit response
}

// timer handle callback
function onTimerCB (timer, count, evt) {
    global.tic += 1
    libafb.notice (evt, "timer':%s' count=%d tic=%d,", libafb.getuid(timer), count, tic)
    libafb.evtpush(evt, {'tic':tic})
    //return -1 // should exit timer
}

function subscribeCB(rqt, ...args) {
    libafb.notice  (rqt, "subscribing api event")
    libafb.evtsubscribe (rqt, global.event)
    return 0 // implicit respond
}

function unsubscribeCB(rqt, ...args) {
    libafb.notice  (rqt, "subscribing api event")
    libafb.evtunsubscribe (rqt, global.event)
    return 0 // implicit respond
}

function evtTimerCB (api, name, data) {
    libafb.notice  (rqt, "evtTimerCB name=%s data=%s", name, data)
}

function evtOtherCB (api, name, data) {
    libafb.notice  (rqt, "evtOtherCB name=%s data=%s", name, data)
}

// When Api ready (state==init) start event & timer
function apiControlCb(api, state) {
    var apiname= libafb.config(api, "api")

    switch (state) {
        case 'config':
            libafb.notice(api, "api=[%s] 'info':[%s]", apiname, libafb.config(api, 'info'))
            break;

        case 'ready':
            var tictime= libafb.config(api,'tictime')*1000 // move from second to ms
            libafb.notice(api, "api=[%s] start event tictime=%dms", apiname, tictime)
            global.event= libafb.evtnew (api, 'node-event')
            if (!global.event) {
                throw new error('fail to create event')
            }
            global.timer= libafb.timernew (api, {'uid':'node-timer','callback':onTimerCB, 'period':tictime, 'count':0}, global.event)
            if (!timer) {
                throw new error ('fail to create timer')
            }
            break;

        case 'orphan':
            libafb.warning(api, "api=[%s] receive an orphan event", apiname)
            break;
        }
    return 0 // 0=ok -1=fatal
}

// called when an api/verb|event callback fail within node
function errorTestCB(rqt, uid, error, ...args) {
    var stack = error.stack.split("\n");
    libafb.error (rqt, "FATAL: uid=%s info=%s trace=%s", uid, error.message, stack[1].substring(7));

    if (libafb.config(rqt, 'verbose')) {
        console.log('\nuid:' + uid + ' Stacktrace:')
        console.log('==============================')
        console.log(error.stack);
    }

    if (libafb.replyable(rqt)) {
        libafb.reply (rqt, -1, {'uid':uid, 'info': error.message, 'trace': stack[1].substring(7)});
    }
}

// api verb list
var apiVerbs = [
    {'uid':'node-ping'       , 'verb':'ping'       , 'callback':pingTestCB   , 'info':'ping event function'},
    {'uid':'node-subscribe'  , 'verb':'subscribe'  , 'callback':subscribeCB  , 'info':'subscribe to event'},
    {'uid':'node-unsubscribe', 'verb':'unsubscribe', 'callback':unsubscribeCB, 'info':'unsubscribe to event'},
]

var apiEvents = [
    {'uid':'node-event' , 'pattern':'node-event', 'callback':evtTimerCB , 'info':'timer event handler'},
    {'uid':'node-other' , 'pattern':'*', 'callback':evtOtherCB , 'info':'any other event handler'},
]

// functionine and instanciate API
var apiOpts = {
    'uid'     : 'node-event',
    'info'    : 'node api event demonstration',
    'api'     : 'event',
    'provide' : 'node-test',
    'verbose' : 9,
    'export'  : 'public',
    'control' : apiControlCb,
    'tictime' : 3,
    'verbs'   : apiVerbs,
    'events'  : apiEvents,
    'alias'  : ['/devtools:/usr/share/afb-ui-devtools/binder'],
}

// functionine and instanciate libafb-binder
var binderOpts = {
    'uid'     : 'node-binder',
    'port'    : 1234,
    'verbose' : 9,
    'roothttp': './conf.d/project/htdocs',
    'onerror' : errorTestCB,
    'rootdir' : '.'
}

// create and start binder
libafb.binder(binderOpts)
libafb.apiadd(apiOpts)

console.log ("### nodejs running ###")