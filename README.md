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
    nodejs sample/simple-api.nodejs
```
## Debug from codium

Codium does not include GDB profile by default you should get them from Ms-Code repository
Go to code market place and download a version compatible with your editor version:

* https://github.com/microsoft/vscode-cpptools/releases
* https://github.com/microsoft/vscode-nodejs/releases

Install your extention

* codium --install-extension cpptools-linux.vsix
* codium --install-extension ms-nodejs-release.vsix

WARNING: the latest version of plugins are probably not compatible with your installed codium version.

## Import afb-nodejsglue

Your nodejs script should import afb-nodeglue.

```nodejs
    #!/usr/bin/node
    process.env.NODE_PATH='./build/src';
    require("module").Module._initPaths();
    const libafb=require('afbnodeglue');
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
    }
    libafb.binder(binderOpts)
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

// api verb list
var demoVerbs = [
    {'uid':'node-ping', 'verb':'ping', 'callback':pingTestCB, 'info':'ping demo function'},
    {'uid':'node-args', 'verb':'args', 'callback':argsTestCB, 'info':'check input query'
        , 'sample':[{'arg1':'arg-one', 'arg2':'arg-two'}, {'argA':1, 'argB':2}]},
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
myapi= libafb.apiadd(demoApi)
```

## Scripting Error handling

On clear issue with any scripting engine is the capability a trace error at run time. By default libafb-node returns a text error and the nodejs line where the error happen. Nevertheless to get a full trave of the error and chose how it should impact your service you may declare an error callback.

```
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
```

## API/RQT Subcalls

Both synchronous and asynchronous call are supported. The fact the subcall is done from a request or from a api context is abstracted to the user. When doing it from RQT context client security context is not propagated and remove event are claimed by the nodejs api.

Explicit response to a request is done with ``` libafb.reply(rqt,status,arg1,..,argn)```. When running a synchronous request an implicit response may also be done with ```return(status, arg1,...,arg-n)```. Note that with afb-v4 an application may return zero, one, or many data.

```nodejs
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
```

## Events

Event should be attached to an API. As binder as a building secret API, it is nevertheless possible to start a timer directly from the binder handle. Under normal circumstances, event should be created from API control callback, when API state=='ready'. Note that it is the developer responsibility to make nodejsEvent handle visible from the function that create the event to the function that use the event.

```nodejs
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
```

Client event subscription is handle with evtsubscribe|unsubcribe api. Subscription API should be call from a request context as in following example, extracted from sample/event-api.nodejs

```nodejs
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
```

## Timers

Timer are typically used to push event or to handle timeout. Timer is started with ```libafb.timernew``` Timer configuration includes a callback, a ticktime in ms and a number or run (count). When count==0 timer runs infinitely.
In following example, timer runs forever every 'tictime' and call TimerCallback' function. This callback being used to send an event.

```nodejs
    function evtTimerCB (api, name, data) {
        libafb.notice  (rqt, "evtTimerCB name=%s data=%s", name, data)
    }

    global.timer= libafb.timernew (api, {'uid':'node-timer','callback':onTimerCB, 'period':tictime, 'count':0}, global.event)
    if (!timer) {
        throw new error ('fail to create timer')
    }
```

afb-libafb timer API is exposed in nodejs

## Binder MainLoop

afb-libnode relies on nodejs/libuv mainloop. As a result no special action is requirer to enter the mainloop.

Nevertheless, developer should be warned that nodejs is not very friendly with events coming from any ```non-nodejs``` world.
Furthermore it has little to no multi-threading capabilities, which make it clearly a bad chose for any embedded devices with limited RAM/COU resources. As a result even if libafb-node leverages the same libuv loop as nodejs and limit itself to a single thread to conform with nodejs limitations. Every libafb events are considered as 'alien' by nodejs and are required to use a different handle-scope. In order to keep compatibility with other nodejs module, libafb pushes/pops a new handle/scope every time a verb/api is called. While this remains generally transparent to developer, in some specif cases when looping asynchronously with promised developer may find strange object live cycle behavior. For further information check nodejs documentation [here](https://nodejs.org/api/n-api.html#object-lifetime-management)

`When handle/scope is not properly initialized nodejs generate following error:`
```node
#
# Fatal error in v8::HandleScope::CreateHandle()
# Cannot create a handle without a HandleScope
#
```

## Miscellaneous APIs/utilities

* libafb.clientinfo(rqt) { returns client session info.
* libafb.config(handle, "key") { returns binder/rqt/timer/... config
* libafb.notice|warning|error|debug print corresponding hookable syslog trace

## ToDeDone

Despite nodejs limitations multithreading could be somehow improved. Due to strong nodejs limitations running a fully mutithreaded model equivalent to Python or Lua is out of scope. Current libafb-node version runs exclusively within nodejs unique thread, with a small exception for callsync that use a dedicated thread to wait for the end of async event. In the future we could somehow improve the situation by running imported C/C++ bindings within a different pool of libuv threads.