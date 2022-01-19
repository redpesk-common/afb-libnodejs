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
#include <node_api.h>
#include <json-c/json.h>

#define SUBCALL_MAX_RPLY 8

typedef enum {
    GLUE_NO_ARG=0,
    GLUE_ONE_ARG=1,
    GLUE_TWO_ARG=2,
    GLUE_THREE_ARG=3,
    GLUE_FOUR_ARG=4,
    GLUE_FIVE_ARG=5,
    GLUE_SIX_ARG=6,
} NAPINumberFixArgs;

typedef enum {
    GLUE_BINDER_MAGIC=936714582,
    GLUE_API_MAGIC=852951357,
    GLUE_RQT_MAGIC=684756123,
    GLUE_EVT_MAGIC=894576231,
    GLUE_TIMER_MAGIC=4628170,
    GLUE_LOCK_MAGIC=379645852,
    GLUE_SCHED_MAGIC=73498127,
} napiGlueMagicsE;

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
};

struct napiSchedWaitS {
    napi_ref callbackR;
    napi_ref userDataR;
    struct afb_sched_lock *afb;
    afb_api_t  apiv4;
    long status;
};

struct napiApiHandleS {
    afb_api_t  afb;
    napi_ref ctrlCbR;
    napi_ref configR;
};

struct napiRqtHandleS {
    struct napiApiHandleS *api;
    int replied;
    afb_req_t afb;
};

struct napiTimerHandleS {
    const char *uid;
    afb_timer_t afb;
    napi_ref callbackR;
    napi_ref configR;
    napi_ref userDataR;
    int usage;
};

struct napiEvtHandleS {
    const char *uid;
    const char *name;
    afb_event_t afb;
    napi_ref configR;
    afb_api_t apiv4;
    int count;
};

struct napiHandlerHandleS {
    const char *uid;
    napi_ref callbackR;
    napi_ref configR;
    napi_ref userDataR;
    afb_api_t apiv4;
    int count;
};

typedef struct {
    napiGlueMagicsE magic;
    napi_env env;
    union {
        struct napiBinderHandleS binder;
        struct napiEvtHandleS evt;
        struct napiApiHandleS api;
        struct napiRqtHandleS rqt;
        struct napiTimerHandleS timer;
        struct napiSchedWaitS lock;
        struct napiHandlerHandleS handler;
    };
} AfbHandleT;

typedef struct {
    int magic;
    AfbHandleT *handle;
    napi_ref callbackR;
    napi_ref userDataR;
} GlueHandleCbT;

extern AfbHandleT *afbMain;
