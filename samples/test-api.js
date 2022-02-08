//!/usr/bin/nodejs3

/*
Copyright 2021 Fulup Ar Foll fulup@iot.bzh
Licence: $RP_BEGIN_LICENSE$ SPDX:MIT https://opensource.org/licenses/MIT $RP_END_LICENSE$

object:
    test-api.lua does not implement new api, but test existing APIs, it:
    - imports helloworld-event binding and api
    - call helloworld-event/startTimer to activate binding timer (1 event per second)
    - call helloworld-event/subscribe to subscribe to event
    - lock mainloop with EventGet5Test and register the eventHandler (EventReceiveCB) with mainloop lock
    - finally (EventReceiveCB) count 5 events and release the mainloop lock received from EventGet5Test

usage
    - from dev tree: LD_LIBRARY_PATH=../afb-libglue/build/src/ lua samples/test-api.lua
    - result of the test position mainloop exit status

config: following should match your installation paths
    - devtools alias should point to right path alias= {'/devtools:/usr/share/afb-ui-devtools/binder'},
    - nodejsPATH='/my-node-module-path' (to _afbnodeglue.so)
    - LD_LIBRARY_PATH='/my-glulib-path' (to libafb-glue.so
*/

// find and import libafb module
process.env.NODE_PATH='./build/src';
require("module").Module._initPaths();
var libafb=require('afbnodeglue');

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


function StartEventTimer(job, userdata) {
    var response= libafb.callsync(job, "helloworld-event", "startTimer")
    if (response.status != 0) libafb.notice  (job, "helloworld event-timer fail status=%d", status)
    return response.status
}

function EventSubscribe(job, userdata) {
    var response= libafb.callsync(job, "helloworld-event", "subscribe")
    if (response.status != 0) libafb.notice  (job, "helloworld subscribe-event fail status=%d", status)
    return response.status
}

function EventUnSubscribe(job, userdata) {
    var response= libafb.callsync(job, "helloworld-event", "unsubscribe")
    if (response.status != 0) libafb.notice  (job, "helloworld unsubscribe-event fail status=%d", status)
    return response.status
}

// register an event handler that count 5 event before releasing current job from mainloop
function EventGet5Test(job, userdata) {
    var ctx= {
        'job': job,
        'count': userdata
    }
    var evt= libafb.evthandler(job, {'uid':'timer-event', 'pattern':'helloworld-event/timerCount','callback':EventReceiveCB}, ctx)

    return (evt != null) // onerror if evt==null
}

// Minimalist test framework demo
var myTestCase = [
    {'callback':StartEventTimer, 'userdata':null, 'expect':0,    'info': 'start helloworld binding timer'},
    {'callback':EventSubscribe , 'userdata':null, 'expect':0,    'info': 'subscribe to helloworld event'},
    {'callback':EventGet5Test  , 'userdata':5   , 'expect':true, 'info': 'wait for 5 helloworld event'},
]

// executed when binder and all api/interfaces are ready to serv
function jobstartCB(job) {
    var status=0
    var timeout=4 // seconds
    libafb.notice(job, "startTestCB=[%s]", libafb.config(job, "uid"))

    for(var idx in myTestCase) {
        var test= myTestCase[idx];
        libafb.info(job, "starting test[%s] callback=[%s] info=[%s]", idx, test.callback.name, test.info)
        var result= test.callback (job, test.userdata)
        if (result != test.expect) {
            status= -1;
            break;
        }
    }

    libafb.notice (job, "jobstartCB return status=%d", status)
    return(status) // force jobkill  
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

// start async operation
libafb.jobstart(binder, jobstartCB, 100, null)
    .then  (job => {libafb.info (job, "jobstart-1 Done: status=%d"  , libafb.jobstatus(job))})
    .catch (job => {libafb.error(job, "jobstart-1 Abort: status=%d" , libafb.jobstatus(job))})
    .finally(() => {
        EventUnSubscribe (binder, null)
        libafb.notice(binder, '---- Test Done -----');
        process.exit(0)
    });

console.log ("### nodejs running ###")