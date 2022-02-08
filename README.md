# afb-libnodejs

Exposes afb-libafb to nodejs scripting language. This module allows to script in nodejs to either mock binding api, test client, quick prototyping, ... Afb-libnodejs runs as a standard nodejs C module, it provides a full access to afb-libafb functionalities, subcall, event, acls, mainloop control, ...

## Dependency

* afb-libafb (from jan/2022 version)
* afb-libglue
* nodejs
* afb-cmake-modules

## Building

```bash
    mkdir build
    cd build
    cmake ..
    make
```

## Testing

Make sure that your dependencies are reachable from nodejs scripting engine, before starting your test.

```bash
    export LD_LIBRARY_PATH=/path/to/afb-libglue.so
    export nodejsPATH=/path/to/afb-libafb.so
    nodejs [--v8-pool-size=num-threads] sample/simple-api.nodejs
```
## Debug from codium

Codium does not include GDP profile by default you should get them from Ms-Code repository

Go to code market place and download a version compatible with your editor version:

* https://github.com/microsoft/vscode-cpptools/releases
* https://github.com/microsoft/vscode-nodejs/releases

Install your extention

* codium --install-extension cpptools-linux.vsix
* codium --install-extension ms-nodejs-release.vsix

WARNING: the lastest version is probably not compatible with your codium version.

## Import afb-nodejsglue

Your nodejs script should import afb-nodejsglue. require return a table which contains the c-module api.

```nodejs
    #!/usr/bin/node

    # Note: afbnodeglue should point to __init__.node module loader
    from afbnodeglue import libafb
    import os
```

## Configure binder services/options

When running mock binding APIs a very simple configuration as following one should be enough. For full options of libafb.binder check libglue API documentation.


```nodejs
    # define and instanciate libafb-binder
    binderOpts = {
        'uid'     : 'node-binder',
        'port'    : 1234,
        'verbose' : 9,
        'roothttp': './conf.d/project/htdocs',
        'rootdir' : '.'
    }
    binder= libafb.binder(binderOpts)
```

For HTTPS cert+key should be added. Optionally a list of aliases and ldpath might also be added

```nodejs
    # define and instanciate libafb-binder
    binderOpts = {
        'uid'       : 'node-binder',
        'port'      : 1234,
        'verbose'   : 9,
        'roothttp'  : './conf.d/project/htdocs',
        'rootdir'   : '.'
        'https-cert': '/path/to/my/https.cert',
        'https-key' : '/path/to/my/https.key'
    }
    binder= libafb.binder(binderOpts)
```

## Exposing api/verbs

afb-libnodejs allows user to implement api/verb directly in scripting language. When api is export=public corresponding api/verbs are visible from HTTP. When export=private they remain visible only from internal calls. Restricted mode allows to exposer API as unix socket with uri='unix:@api' tag.

Expose a new api with ```libafb.apiadd(demoApi)``` as in following example.

```nodejs
## ping/pong test func
def pingCB(rqt, *args):
    global count
    count += 1
    libafb.notice  (rqt, "From pingCB count=%d", count)
    return (0, {"pong":count}) # implicit response

## api verb list
demoVerbs = [
    {'uid':'lua-ping', 'verb':'ping', 'callback':pingCB, 'info':'lua ping demo function'},
    {'uid':'lua-args', 'verb':'args', 'callback':argsCB, 'info':'lua check input query', 'sample':[{'arg1':'arg-one', 'arg2':'arg-two'}, {'argA':1, 'argB':2}]},
]

## define and instanciate API
demoApi = {
    'uid'     : 'lua-demo',
    'api'     : 'demo',
    'class'   : 'test',
    'info'    : 'lua api demo',
    'verbose' : 9,
    'export'  : 'public',
    'verbs'   : demoVerbs,
    'alias'   : ['/devtools:/usr/share/afb-ui-devtools/binder'],
}

myapi= libafb.apiadd(demoApi)
```

## API/RQT Subcalls

Both synchronous and asynchronous call are supported. The fact the subcall is done from a request or from a api context is abstracted to the user. When doing it from RQT context client security context is not propagated and remove event are claimed by the nodejs api.

Explicit response to a request is done with ``` libafb.reply(rqt,status,arg1,..,argn)```. When running a synchronous request an implicit response may also be done with ```return(status, arg1,...,arg-n)```. Note that with afb-v4 an application may return zero, one or many data.

```nodejs
def asyncRespCB(rqt, status, ctx, *args):
    libafb.notice  (rqt, "asyncRespCB status=%d ctx:'%s', response:'%s'", status, ctx, args)
    libafb.reply (rqt, status, 'async helloworld/testargs', args)

def syncCB(rqt, *args):
    libafb.notice  (rqt, "syncCB calling helloworld/testargs *args=%s", args)
    status= libafb.callsync(rqt, "helloworld","testargs", args[0])[0]
    if status != 0:
        libafb.reply (rqt, status, 'async helloworld/testargs fail')
    else:
        libafb.reply (rqt, status, 'async helloworld/testargs success')

def asyncCB(rqt, *args):
    userdata= "context-user-data"
    libafb.notice  (rqt, "asyncCB calling helloworld/testargs *args=%s", args)
    libafb.callasync (rqt,"helloworld", "testargs", asyncRespCB, userdata, args[0])
    # response within 'asyncRespCB' callback
```

## Events

Event should attached to an API. As binder as a building secret API, it is nevertheless possible to start a timer directly from a binder. Under normal circumstances, event should be created from API control callback, when API it's state=='ready'. Note that it is developer responsibility to make nodejsEvent handle visible from the function that create the event to the function that use the event.

```nodejs
    def apiControlCb(api, state):
        global nodeEvent

        apiname= libafb.config(api, "api")
        #WARNING: from nodejs 3.10 use switch-case as elseif replacement
        if state == 'config':
            libafb.notice(api, "api=[%s] 'info':[%s]", apiname, libafb.config(api, 'info'))

        elif state == 'ready':
            tictime= libafb.config(api,'tictime')*1000 # move from second to ms
            libafb.notice(api, "api=[%s] start event tictime=%dms", apiname, tictime)

            nodeEvent= libafb.evtnew (api,{'uid':'node-event', 'info':'node testing event sample'})
            if (nodeEvent is None):
                raise Exception ('fail to create event')

            timer= libafb.timernew (api, {'uid':'node-timer','callback':timerCB, 'period':tictime, 'count':0}, nodeEvent)
            if (timer is None):
                raise Exception ('fail to create timer')

        elif state == 'orphan':
            libafb.warning(api, "api=[%s] receive an orphan event", apiname)

        return 0 # 0=ok -1=fatal

        # later event can be push with evtpush
        libafb.evtpush(event, {userdata})

```

Client event subscription is handle with evtsubscribe|unsubcribe api. Subscription API should be call from a request context as in following example, extracted from sample/event-api.nodejs

```nodejs
    def subscribeCB(rqt, *args):
        libafb.notice  (rqt, "subscribing api event")
        libafb.evtsubscribe (rqt, nodeEvent)
        return 0 # implicit respond

    def unsubscribeCB(rqt, *args):
        libafb.notice  (rqt, "subscribing api event")
        libafb.evtunsubscribe (rqt, nodeEvent)
        return 0 # implicit respond
```

## Timers

Timer are typically used to push event or to handle timeout. Timer is started with ```libafb.timernew``` Timer configuration includes a callback, a ticktime in ms and a number or run (count). When count==0 timer runs infinitely.
In following example, timer runs forever every 'tictime' and call TimerCallback' function. This callback being used to send an event.

```nodejs
    def timerCB (timer, data):
        libafb.notice  (rqt, "evttimerCB name=%s data=%s", name, data)

    timer= libafb.timernew (api,
        {'uid':'node-timer','callback':timerCB, 'period':tictime, 'count':0}
        , nodeEvent)
    if (timer is None):
        raise Exception ('fail to create timer')
```

afb-libafb timer API is exposed in nodejs

## Binder MainLoop

afb-libnode relies on nodejs/libuv mainloop. Nevertheless as node require a dedicated handle context for libuv callback
developer should explicitly request libuv mainloop at the end of initialization code.

```js
  // infinite libuv mainloop
  libafb.loopenter()
```

`Note: When not properly initialized nodejs generate following error:`
```node
#
# Fatal error in v8::HandleScope::CreateHandle()
# Cannot create a handle without a HandleScope
#
```

Under normal circumstance binder mainloop never returns. Nevertheless during test phase it is very common to wait and asynchronous event(s) before deciding if the test is successfully or not.
Mainloop starts with libafb.jobstart('xxx'), where 'xxx' is an optional startup function that control mainloop execution. They are two ways to control the mainloop:

* when startup function returns ```status!=0``` the binder immediately exit with corresponding status. This case is very typical when running pure synchronous api test.
* set a shedwait lock and control the main loop from asynchronous events. This later case is mandatory when we have to start the mainloop to listen event, but still need to exit it to run a new set of test.

Mainloop schedule wait is done with ```libafb.schedwait(binder,'xxx',timeout,{optional-user-data})```. Where 'xxx' is the name of the control callback that received the lock. Schedwait is released with ``` libafb.schedunlock(rqt/evt,lock,status)```

In following example:
* schedwait callback starts an event handler and passes the lock as evt context
* event handler: count the number of event and after 5 events release the lock.

Note:

* libafb.schedwait does not return before the lock is releases. As for events it is the developer responsibility to either carry the lock in a context or to store it within a share space, on order unlock function to access it.

* it is possible to serialize libafb.schedwait in order to build asynchronous cascade of asynchronous tests.

```nodejs

    def EventReceiveCB(evt, name, lock, *data):
        global evtCount
        libafb.notice (evt, "event=%s data=%s", name, data)
        evtCount += 1
        if evtCount == 5:
            libafb.notice (evt, "*** EventReceiveCB releasing lock ***");
            libafb.schedunlock (evt, lock, evtCount)


    def SchedWaitCB(api, lock, context):
        libafb.notice (api, "Schedlock timer-event handler register")
        libafb.evthandler(api, {'uid':'timer-event', 'pattern':'helloworld-event/timerCount','callback':EventReceiveCB}, lock)
        return 0

    # executed when binder and all api/interfaces are ready to serv
    def startTestCB(binder):
        status=0
        timeout=4 # seconds
        libafb.notice(binder, "startTestCB=[%s]", libafb.config(binder, "uid"))

        libafb.notice (binder, "waiting (%ds) for test to finish", timeout)
        status= libafb.schedwait(binder, timeout, SchedWaitCB, None)

        libafb.notice (binder, "test done status=%d", status)
        return(status) # negative status force mainloop exit

    # start mainloop
    status=libafb.jobstart(startTestCB)
```

## Miscellaneous APIs/utilities

* libafb.clientinfo(rqt): returns client session info.
* libafb.nodejsstrict(true): prevents nodejs from creating global variables.
* libafb.config(handle, "key"): returns binder/rqt/timer/... config
* libafb.notice|warning|error|debug print corresponding hookable syslog trace

## ToDeDone

The multithreading model should be improveds, as today everything run within one thread except for AFB data Marshalling /Unmarshalling. At minimum importing C binding should run within a different set of Thread.