#!/usr/bin/node

/*
Copyright 2021 Fulup Ar Foll fulup@iot.bzh
Licence: $RP_BEGIN_LICENSE$ SPDX:MIT https://opensource.org/licenses/MIT $RP_END_LICENSE$

object:
    - loa/set current LOA level to 1
    - loa/reset current LOA level to 0
    - loa/check is protected by ACLS and requiere a LOA >=1 it display client session uuid

usage:
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

// ping/pong test func
function pingTestCB(rqt, ...args) {
    global.count += 1;
    libafb.notice  (rqt, "From pingTestCB count=%d", count);
    return [0, {"pong":count}]; // implicit response
}

function setLoaCB(rqt, ...args) {
    libafb.notice  (rqt, "setLoaCB LOA=1")
    libafb.setloa (rqt, 1)
    return 0
}

function resetLoaCB(rqt, ...args) {
    libafb.notice (rqt, "resetLoaCB LOA=0")
    libafb.setloa (rqt, 0)
    return 0
}

function checkLoaCB(rqt, ...args) {
    libafb.notice (rqt, "Protected API session uuid=%s", libafb.clientinfo(rqt, 'uuid'))
    return 0
}

// api verb list
var loaVerbs = [
    {'uid':'lua-ping' , 'verb':'ping'  , 'callback':pingTestCB,'auth':'anonymous', 'info':'lua ping loa function'},
    {'uid':'lua-set'  , 'verb':'set'   , 'callback':setLoaCB  ,'auth':'anonymous', 'info':'set LOA to 1'},
    {'uid':'lua-reset', 'verb':'reset' , 'callback':resetLoaCB,'auth':'anonymous', 'info':'reset LOA to 0'},
    {'uid':'lua-check', 'verb':'check' , 'callback':checkLoaCB,'auth':'authorized', 'info':'protected API requirer LOA>=1'},
]

// define permissions
var loaAlcs = [
    ['anonymous'      , 'loa', 0],
    ['authorized'     , 'loa', 1],
    ['perm-1'         , 'key', 'permission-1'],
    ['perm-2'         , 'key', 'permission-2'],
    ['login-and-roles', 'and', ['perm-2', 'perm-1']],
    ['login-or-roles' , 'or' , ['authorized', 'perm-1']],
]

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
var loaVerbs = [
    {'uid':'py-ping' , 'verb':'ping'  , 'callback':pingTestCB ,'auth':'anonymous', 'info':'py ping loa function'},
    {'uid':'py-set'  , 'verb':'set'   , 'callback':setLoaCB   ,'auth':'anonymous', 'info':'set LOA to 1'},
    {'uid':'py-reset', 'verb':'reset' , 'callback':resetLoaCB ,'auth':'anonymous', 'info':'reset LOA to 0'},
    {'uid':'py-check', 'verb':'check' , 'callback':checkLoaCB ,'auth':'authorized', 'info':'protected API requirer LOA>=1'},
]

// define permissions
var loaAlcs = [
    ['anonymous'      , 'loa', 0],
    ['authorized'      , 'loa', 1],
    ['perm-1'         , 'key', 'permission-1'],
    ['perm-2'         , 'key', 'permission-2'],
    ['login-and-roles', 'and', ['perm-2', 'perm-1']],
    ['login-or-roles' , 'or' , ['authorized', 'perm-1']],
]

// functionine and instantiate API
var loaApi = {
    'uid'     : 'node-loa',
    'api'     : 'loa',
    'class'   : 'test',
    'info'    : 'node loa acl/privilege check',
    'verbose' : 9,
    'export'  : 'public',
    'verbs'   : loaVerbs,
    'onerror' : errorTestCB,
    'alias'   : ['/devtools:/usr/share/afb-ui-devtools/binder'],
}

// functionine and instantiate libafb-binder
var loaOpts = {
    'uid'     : 'node-binder',
    'port'    : 1234,
    'verbose' : 9,
    'roothttp': './conf.d/project/htdocs',
    'ldpath' : [process.env.HOME + '/opt/helloworld-binding/lib','/usr/local/helloworld-binding/lib'],
    'alias'  : ['/devtools:/usr/share/afb-ui-devtools/binder'],
    'acls'    : loaAlcs,
}

var binder= libafb.binder(loaOpts);
libafb.apiadd(loaApi);

console.log ("### nodejs running ###")