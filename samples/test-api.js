//!/usr/bin/nodejs3

/*
Copyright 2021 Fulup Ar Foll fulup@iot.bzh
Licence: $RP_BEGIN_LICENSE$ SPDX:MIT https://opensource.org/licenses/MIT $RP_END_LICENSE$

object:
    test-api.lua does not implement new api, but test existing APIs, it:
    - imports helloworld-event binding and api
    - call helloworld-event/startTimer to activate binding timer (1 event per second)
    - call helloworld-event/subscribe to subscribe to event
    - lock mainloop with EvtGet5Events and register the eventHandler (EventReceiveCB) with mainloop lock
    - finally (EventReceiveCB) count 5 events and release the mainloop lock received from EvtGet5Events

usage
    - from dev tree: LD_LIBRARY_PATH=../afb-libglue/build/src/ lua samples/test-api.lua
    - result of the test position mainloop exit status

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
global.evtCount=0

// called when an api/verb|event callback fail within node
function errorTestCB(rqt, uid, error, ...args) {
    try {
        var stack = error.stack.split("\n");
        libafb.error (rqt, "FATAL: uid=%s info=%s trace=%s", uid, error.message, stack[1].substring(7));
        if (libafb.config(rqt, 'verbose')) {
            console.log('\nuid:' + uid + ' Stacktrace:')
            console.log('==============================')
            console.log(error.stack);
        }
    } catch (exception) {
        libafb.error (rqt, "FATAL: uid=%s info=%s exception=%s", uid, error.message, exception);
    }

    if (libafb.replyable(rqt)) {
        libafb.reply (rqt, -1, {'uid':uid, 'info': error.message, 'trace': stack[1].substring(7)});
    }
}

function EventReceiveCB(evt, name, ctx, ...data) {
    libafb.notice (evt, "event=%s data=%s", name, data)
    global.evtCount += 1
    if (evtCount == ctx.count) {
        libafb.notice (evt, "*** EventReceiveCB releasing lock ***");
        libafb.jobkill (ctx.job, evtCount)
    }
}

function EvtStartTimer(job, userdata) {
    var response= libafb.callsync(job, "helloworld-event", "startTimer")
    if (response.status != 0) libafb.notice  (job, "helloworld event-timer fail status=%d", status)
    return response.status
}

function EvtSubscribe(job, userdata) {
    var response= libafb.callsync(job, "helloworld-event", "subscribe")
    if (response.status != 0) libafb.notice  (job, "helloworld subscribe-event fail status=%d", status)
    return response.status
}

function EvtUnSubscribe(job, userdata) {
    var response= libafb.callsync(job, "helloworld-event", "unsubscribe")
    if (response.status != 0) libafb.notice  (job, "helloworld unsubscribe-event fail status=%d", status)
    return response.status
}

// register an event handler that count 5 event before releasing current job from mainloop
function EvtGet5Events(job, timeout, userdata) {
    var status=0

    // add job handle to userdata
    userdata.job= job

    // register eventhandle passing job and count as userdata
    var evt= libafb.evthandler(job, {'uid':'timer-event', 'pattern':'helloworld-event/timerCount','callback':EventReceiveCB}, userdata)

    // onerror if evt==null
    if (evt === null) status=-1
    return (status)
}

// helloworld binding sample functioninition
var EventBinding = {
    'uid'    : 'helloworld',
    'export' : 'private',
    'path'   : 'afb-helloworld-subscribe-event.so',
}

// functionine and instantiate libafb-binder
var binderOpts = {
    'uid'     : 'node-binder',
    'verbose' : 9,
    'roothttp': './conf.d/project/htdocs',
    'rootdir' : '.',
    'ldpath' : [process.env.HOME + '/opt/helloworld-binding/lib','/usr/local/helloworld-binding/lib'],
    'alias'  : ['/devtools:/usr/share/afb-ui-devtools/binder'],
    'onerror' : errorTestCB,
}

// create and start binder
var binder= libafb.binder(binderOpts)
var hello = libafb.binding(EventBinding)

// loop synchronously on test list
function jobRunTest(binder, testlist, idx) {
    var test= testlist[idx];

    if (!test) {
        libafb.notice(binder, '---- Test Done -----');
        process.exit(0)
    }

    libafb.info(binder, "==> Starting [%s] info=[%s]", test.callback.name, test.info)
    libafb.jobstart(binder,  test.callback, test.timeout, test.userdata)
    .then  (status => {libafb.info (binder, "--> [%s] Success: status=%d" , test.callback.name, status)})
    .catch (status => {libafb.error(binder, "--> [%s] Fail: status=%d" , test.callback.name, status)})
    .finally(()    => {jobRunTest(binder, testlist, idx+1)})
}

// Minimalist test framework demo
var mytestlist = [
    {'callback':EvtStartTimer ,'timeout': 0,'userdata':null, 'expect':0, 'info': 'start helloworld binding timer'},
    {'callback':EvtSubscribe  ,'timeout': 0,'userdata':null, 'expect':0, 'info': 'subscribe to helloworld event'},
    {'callback':EvtGet5Events ,'timeout':10,'userdata':{'count':5}, 'expect':true, 'info': 'wait for 5 helloworld event'},
    {'callback':EvtUnSubscribe,'timeout': 0,'userdata':null, 'expect':0, 'info': 'subscribe to helloworld event'},
    null // sentinel
]
// launch test scenario
jobRunTest(binder, mytestlist, 0)
console.log ("### nodejs running ###")