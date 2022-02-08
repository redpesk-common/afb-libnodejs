/*
 * Conapiright (C) 2015-2021 IoT.bzh Company
 * Author: Fulup Ar Foll <fulup@iot.bzh>
 *
 * $RP_BEGIN_LICENSE$
 * Commercial License Usage
 *  Licensees holding valid commercial IoT.bzh licenses may use this file in
 *  accordance with the commercial license agreement provided with the
 *  Software or, alternatively, in accordance with the terms contained in
 *  a written agreement between you and The IoT.bzh Company. For licensing terms
 *  and conditions see https://www.iot.bzh/terms-conditions. For further
 *  information use the contact form at https://www.iot.bzh/contact.
 *
 * GNU General Public License Usage
 *  Alternatively, this file may be used under the terms of the GNU General
 *  Public license version 3. This license is as published by the Free Software
 *  Foundation and appearing in the file LICENSE.GPLv3 included in the packaging
 *  of this file. Please review the following information to ensure the GNU
 *  General Public License requirements will be met
 *  https://www.gnu.org/licenses/gpl-3.0.html.
 * $RP_END_LICENSE$
 */
#pragma once

#include <libafb/sys/verbose.h>

#include "glue-afb.h"
#include "js_native_api_types.h"
#include <node_api.h>
#include <uv.h>

#include <json-c/json.h>

#define SUBCALL_MAX_RPLY 8
#define API_MAX_LEN  256
#define VERB_MAX_LEN 256
#define LABEL_MAX_LEN 128

#define GLUE_RETURN_NULL { \
    napi_value resultN; \
    napi_get_null(env, &resultN); \
    return resultN; }

// compiled AFB verb context data
typedef struct {
    int magic;
    const char *verb;
    const char *info;
    napi_ref funcP;
} napiVerbDataT;

struct napiBinderHandleS {
    AfbBinderHandleT *afb;
    napi_ref configR;
    struct evmgr *evmgr;
    uv_loop_t *loopU;
    uv_poll_t poolU;
};

typedef struct {
    char *uid;
    napi_env env;
    napi_ref callbackR;
    napi_ref userdataR;
} GlueAsyncCtxT;

struct napiApiHandleS {
    afb_api_t  afb;
    napi_ref configR;
    GlueAsyncCtxT *async;
};

struct napiRqtHandleS {
    struct napiApiHandleS *api;
    int replied;
    afb_req_t afb;
};

struct napiEventHandleS {
    afb_event_t afb;
    afb_api_t apiv4;
    char *pattern;
    napi_ref configR;
    GlueAsyncCtxT async;
};

struct napiTimerHandleS {
    afb_timer_t afb;
    afb_api_t apiv4;
    napi_ref configR;
    GlueAsyncCtxT async;
};

struct napiPostHandleS {
    afb_api_t apiv4;
    napi_ref configR;
    GlueAsyncCtxT async;
};

struct napiJobHandleS {
    int64_t status;
    napi_deferred deferred;
    int64_t timeout;
    sem_t sem;
    afb_api_t apiv4;
};

typedef struct {
    GlueHandleMagicsE magic;
    GlueAsyncCtxT* onError;
    int usage;
    napi_env env;
    char *uid;
    union {
        struct napiBinderHandleS binder;
        struct napiApiHandleS api;
        struct napiRqtHandleS rqt;
        struct napiTimerHandleS timer;
        struct napiEventHandleS event;
        struct napiPostHandleS post;
        struct napiJobHandleS job;
    };
} GlueHandleT;


typedef struct  {
    GlueHandleMagicsE magic;
    GlueHandleT *glue;
    GlueAsyncCtxT async;
} GlueCallHandleT;


extern GlueHandleT *afbMain;
