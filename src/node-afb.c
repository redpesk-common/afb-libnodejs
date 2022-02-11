#define _GNU_SOURCE
/*
 * Copyright (C) 2015-2021 IoT.bzh Company
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
 *
 *  References:
 *  - https://nodejs.org/api/n-api.html#napi_reference_ref
 *  - https://github.com/nodejs/node-addon-examples
*/

#include "json_object.h"
#include <node_api.h>
#include "node_api_types.h"
#include <uv.h>
#include <stdint.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <wrap-json.h>

#include "node-afb.h"
#include "node-utils.h"
#include "node-callbacks.h"

#include <glue-afb.h>
#include <glue-utils.h>
#include <stdio.h>

#include <time.h>


// global afbMain glue
GlueHandleT *afbMain=NULL;

static napi_value  GluePrintInfo(napi_env env, napi_callback_info info) {
    napiPrintMsg(AFB_SYSLOG_LEVEL_INFO, env, info);
    GLUE_RETURN_NULL;
}

static napi_value  GluePrintError(napi_env env, napi_callback_info info) {
    napiPrintMsg(AFB_SYSLOG_LEVEL_ERROR, env, info);
    GLUE_RETURN_NULL;
}

static napi_value  GluePrintWarning(napi_env env, napi_callback_info info) {
    napiPrintMsg(AFB_SYSLOG_LEVEL_WARNING, env, info);
    GLUE_RETURN_NULL;
}

static napi_value  GluePrintNotice(napi_env env, napi_callback_info info) {
    napiPrintMsg(AFB_SYSLOG_LEVEL_NOTICE, env, info);
    GLUE_RETURN_NULL;
}

static napi_value  GluePrintDebug(napi_env env, napi_callback_info info) {
    napiPrintMsg(AFB_SYSLOG_LEVEL_DEBUG, env, info);
    GLUE_RETURN_NULL;
}

static napi_value GlueEvtPush(napi_env env, napi_callback_info info)
{
    const char *errorCode="internal-error", *errorMsg = "syntax: eventpush(evtid, [arg1 ... argn])";
    napi_status statusN;
    json_object *valueJ;
    int  count=0;
    afb_data_t *params=NULL;
    afb_event_t evtid;

    // get argument count
    size_t argc;
    napi_get_cb_info(env, info, &argc, NULL, NULL, NULL);
    napi_value args[argc];

    // get arguments
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc < GLUE_ONE_ARG) {
        errorCode="too-few-arguments";
        goto OnErrorExit;
    }

    // retreive afb glue handle from 1st argument
    statusN= napi_get_value_external (env, args[0], (void**)&evtid);
    if (statusN != napi_ok || !afb_event_is_valid(evtid)) {
        errorCode="invalid-evt-handle";
        goto OnErrorExit;
    }

    if (argc > GLUE_ONE_ARG) {
        params= alloca(sizeof(afb_data_t*)*(argc-GLUE_ONE_ARG));
        for (int idx = GLUE_ONE_ARG ; idx < argc; idx++) {
            valueJ = napiValuetoJsonc(env, args[idx]);
            if (!valueJ)  {
                errorCode= "invalid-argument-data";
                goto OnErrorExit;
            }
            afb_create_data_raw(&params[count], AFB_PREDEFINED_TYPE_JSON_C, valueJ, 0, (void *)json_object_put, valueJ);
            count++;
        }
    }

    int err = afb_event_push(evtid, count, params);
    if (err < 0)
    {
        errorCode = "afb-event-push-fail";
        goto OnErrorExit;
    }
    GLUE_RETURN_NULL;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueEvtSubscribe(napi_env env, napi_callback_info info)
{
    const char *errorCode="internal-error", *errorMsg = "syntax: subscribe(rqt, evtid)";
    napi_status statusN;
    afb_event_t evtid;

    // get argument count
    size_t argc;
    napi_get_cb_info(env, info, &argc, NULL, NULL, NULL);
    napi_value args[argc];

    // get arguments
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc != GLUE_TWO_ARG) {
        errorCode="invalid-arguments-count";
        goto OnErrorExit;
    }

    // retreive afb glue handle from 1st argument
    GlueHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok || glue->magic != GLUE_RQT_MAGIC) {
        errorCode="invalid-rqt-handle";
        goto OnErrorExit;
    }

    statusN= napi_get_value_external (env, args[1], (void**)&evtid);
    if (statusN != napi_ok || !afb_event_is_valid(evtid)) {
        errorCode="invalid-event-handle";
        goto OnErrorExit;
    }

    int err = afb_req_subscribe(glue->rqt.afb, evtid);
    if (err)
    {
        errorCode = "afb-req-subscribe-fail";
        goto OnErrorExit;
    }
    GLUE_RETURN_NULL;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueEvtUnsubscribe(napi_env env, napi_callback_info info)
{
    const char *errorCode="internal-error", *errorMsg = "syntax: unsubscribe(rqt, evtid)";
    napi_status statusN;
    afb_event_t evtid;

    // get argument count
    size_t argc;
    napi_get_cb_info(env, info, &argc, NULL, NULL, NULL);
    napi_value args[argc];

    // get arguments
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc != GLUE_TWO_ARG) {
        errorCode="invalid-arguments-count";
        goto OnErrorExit;
    }

    // retreive afb glue handle from 1st argument
    GlueHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok || glue->magic != GLUE_RQT_MAGIC) {
        errorCode="invalid-rqt-handle";
        goto OnErrorExit;
    }

    statusN= napi_get_value_external (env, args[1], (void**)&evtid);
    if (statusN != napi_ok || !afb_event_is_valid(evtid)) {
        errorCode="invalid-event-handle";
        goto OnErrorExit;
    }

    int err = afb_req_unsubscribe(glue->rqt.afb, evtid);
    if (err)
    {
        errorCode = "afb-req-unsubscribe-fail";
        goto OnErrorExit;
    }
    GLUE_RETURN_NULL;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueEvtNew(napi_env env, napi_callback_info info)
{
    const char *errorCode="internal-error", *errorMsg = "syntax: evtid= eventnew(api,label)";
    napi_status statusN;
    afb_event_t evtid;

    // get argument count
    size_t argc;
    napi_get_cb_info(env, info, &argc, NULL, NULL, NULL);
    napi_value args[argc], resultN;

    // get arguments
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc != GLUE_TWO_ARG) {
        errorCode="invalid-arguments-count";
        goto OnErrorExit;
    }

    // retreive afb glue handle from 1st argument
    GlueHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok || !GlueGetApi(glue)) {
        errorCode="invalid-api-handle";
        goto OnErrorExit;
    }

    char *label= napiValueToString(env, args[1]);
    if (!label) {
        errorCode="invalid-evt-label";
        goto OnErrorExit;
    }

    int err= afb_api_new_event(GlueGetApi(glue), label, &evtid);
    if (err)
    {
        errorMsg = "afb-api-new-event-fail";
        goto OnErrorExit;
    }

    // push event handle as a node opaque handle
    statusN= napi_create_external(env, evtid, NULL, NULL, &resultN);
    if (statusN != napi_ok) goto OnErrorExit;
    return resultN;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}


static napi_value GlueTimerAddref(napi_env env, napi_callback_info info) {
    const char *errorCode="internal-error", *errorMsg = "syntax:  timeraddref(timerid)";
    napi_status statusN;

    // get argument count
    size_t argc;
    napi_get_cb_info(env, info, &argc, NULL, NULL, NULL);
    napi_value args[argc];

    // get arguments
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc != GLUE_ONE_ARG) {
        errorCode="invalid-arguments-count";
        goto OnErrorExit;
    }

    // retreive afb glue handle from 1st argument
    GlueHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok || glue->magic != GLUE_TIMER_MAGIC) {
        errorCode="invalid-timer-handle";
        goto OnErrorExit;
    }

    glue->usage++;
    afb_timer_addref (glue->timer.afb);
    GLUE_RETURN_NULL;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueTimerUnref(napi_env env, napi_callback_info info) {
    const char *errorCode="internal-error", *errorMsg = "syntax:  timerunref(handle)";
    napi_status statusN;

    // get argument count
    size_t argc;
    napi_get_cb_info(env, info, &argc, NULL, NULL, NULL);
    napi_value args[argc];

    // get arguments
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc != GLUE_TWO_ARG) {
        errorCode="invalid-arguments-count";
        goto OnErrorExit;
    }

    // retreive afb glue handle from 1st argument
    GlueHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok || glue->magic != GLUE_TIMER_MAGIC) {
        errorCode="invalid-timer-handle";
        goto OnErrorExit;
    }

    GlueFreeHandleCb(env, glue);
    GLUE_RETURN_NULL;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueEvtHandler(napi_env env, napi_callback_info info)
{
    const char  *errorCode="internal-error", *errorMsg= "syntax: evthandler(api, {'uid':'xxx','pattern':'yyy','callback':'zzz'}, [userdata])";
    napi_status statusN;

    // get argument count
    size_t argc;
    napi_get_cb_info(env, info, &argc, NULL, NULL, NULL);
    napi_value args[argc];

    // get arguments
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc < GLUE_TWO_ARG || argc > GLUE_THREE_ARG) {
        errorCode="invalid-arguments-count";
        goto OnErrorExit;
    }

    // retreive afb glue handle from 1st argument
    GlueHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok) {
        errorCode="invalid-rqt-handle";
        goto OnErrorExit;
    }

    GlueHandleT *handle = calloc(1, sizeof(GlueHandleT));
    handle->magic = GLUE_EVT_MAGIC;
    handle->onError= glue->onError;
    handle->env=env;

    statusN= napi_create_reference(env, args[1], 1, &handle->event.configR);
    if (statusN != napi_ok) {
        errorCode= "config-not-referencable";
        goto OnErrorExit;
    }

    // retreive API from py handle
    handle->event.apiv4= GlueGetApi(glue);
    if (!handle->event.apiv4) {
        errorCode="handle-api-missing";
        goto OnErrorExit;
    }

    napi_value callbackN= napiGetElement(env, args[1], "callback");
    if (!callbackN || napiGetType(env, callbackN)!= napi_function) {
        errorCode = "callback-not-function";
        goto OnErrorExit;
    }

    statusN= napi_create_reference(env, callbackN, 1, &handle->event.async.callbackR);
    if (statusN != napi_ok) {
        errorCode= "config-referencable-callback";
        goto OnErrorExit;
    }

    if (argc == GLUE_THREE_ARG) {
      statusN= napi_create_reference(env, args[2], 1, (void*)&handle->event.async.userdataR);
        if (statusN != napi_ok) {
            errorCode= "userdata-not-referencable";
            goto OnErrorExit;
        }
    }

    handle->uid= napiGetString(glue->env, args[1], "uid");
    if (!handle->uid) {
        errorCode = "config-missing-uid";
        goto OnErrorExit;
    }

    handle->event.pattern= napiGetString(glue->env, args[1], "pattern");
    if (!handle->event.pattern) {
        errorCode = "config-missing-pattern";
        goto OnErrorExit;
    }

    errorMsg= AfbAddOneEvent (handle->event.apiv4, handle->uid, handle->event.pattern, GlueEventCb, handle);
    if (errorMsg) goto OnErrorExit;

    // push handle as a node opaque handle
    napi_value resultN;
    statusN= napi_create_external (env, handle, NULL, NULL, &resultN);
    if (statusN != napi_ok) goto OnErrorExit;
    return resultN;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueTimerNew(napi_env env, napi_callback_info info)
{
    const char  *errorCode="internal-error", *errorMsg= "syntax: timernew(api, {'uid':'xxx','callback':yyy,'period':ms,'count':nn}, userdata)";
    napi_status statusN;

    // get argument count
    size_t argc;
    napi_get_cb_info(env, info, &argc, NULL, NULL, NULL);
    napi_value args[argc];

    // get arguments
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc != GLUE_THREE_ARG) {
        errorCode="invalid-arguments-count";
        goto OnErrorExit;
    }

    // retreive afb glue handle from 1st argument
    GlueHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok) {
        errorCode="invalid-rqt-handle";
        goto OnErrorExit;
    }

    GlueHandleT *handle = calloc(1, sizeof(GlueHandleT));
    handle->magic = GLUE_TIMER_MAGIC;
    handle->env=env;
    handle->onError= glue->onError;

    statusN= napi_create_reference(env, args[1], 1, &handle->timer.configR);
    if (statusN != napi_ok) {
        errorCode= "config-not-referencable";
        goto OnErrorExit;
    }

    // retreive API from py handle
    handle->timer.apiv4= GlueGetApi(glue);
    if (!handle->timer.apiv4) {
        errorCode="handle-api-missing";
        goto OnErrorExit;
    }

    statusN= napi_create_reference(env, args[2], 1, (void*)&handle->timer.async.userdataR);
    if (statusN != napi_ok) {
        errorCode= "userdata-not-referencable";
        goto OnErrorExit;
    }

    napi_value callbackN= napiGetElement(env, args[1], "callback");
    if (!callbackN || napiGetType(env, callbackN)!= napi_function) {
        errorCode = "callback-not-function";
        goto OnErrorExit;
    }

    statusN= napi_create_reference(env, callbackN, 1, (void*)&handle->timer.async.callbackR);
    if (statusN != napi_ok) {
        errorCode= "config-referencable-callback";
        goto OnErrorExit;
    }

    handle->uid= napiGetString(glue->env, args[1], "uid");
    if (!handle->uid) {
        errorCode = "config-missing-uid";
        goto OnErrorExit;
    }

    napi_value periodN= napiGetElement(env, args[1], "period");
    if (!periodN) {
        errorCode= "config-period-missing";
        goto OnErrorExit;
    }
    int64_t period;
    statusN= napi_get_value_int64(env, periodN, &period);
    if (!periodN || period < 0) {
        errorCode= "period-not-valid";
        goto OnErrorExit;
    }

    napi_value countN= napiGetElement(env, args[1], "count");
    if (!countN) {
        errorCode= "config-count-missing";
        goto OnErrorExit;
    }

    int64_t count;
    statusN= napi_get_value_int64(env, countN, &count);
    if (!countN || count < 0) {
        errorCode= "count-not-valid";
        goto OnErrorExit;
    }

    handle->usage++;
    int err= afb_timer_create (&handle->timer.afb, 0, 0, 0, (int)count, (int)period, 0, GlueTimerCb, (void*)handle, 0);
    if (err) {
        errorMsg= "(hoops) afb_timer_create fail";
        goto OnErrorExit;
    }

    // push handler as a node opaque handle
    napi_value resultN;
    statusN= napi_create_external (env, handle, NULL, NULL, &resultN);
    if (statusN != napi_ok) goto OnErrorExit;
    return resultN;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueJobKill(napi_env env, napi_callback_info info)
{
    const char  *errorCode="internal-error", *errorMsg= "syntax: jobkill(job, status)";
    napi_status statusN;

    // get argument count
    size_t argc;
    napi_get_cb_info(env, info, &argc, NULL, NULL, NULL);
    napi_value args[argc];

    // get arguments
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc != GLUE_TWO_ARG) {
        errorCode="invalid-arguments-count";
        goto OnErrorExit;
    }

    // retreive afb loop handle from 1st argument
    GlueHandleT *handle;
    statusN= napi_get_value_external (env, args[0], (void**)&handle);
    if (statusN != napi_ok || handle->magic != GLUE_JOB_MAGIC) {
        errorCode="invalid-loop-handle";
        goto OnErrorExit;
    }

    statusN= napi_get_value_int64(env, args[1], &handle->job.thread->status);
    if (statusN != napi_ok || napiGetType(env, args[1]) != napi_number) {
        errorCode="invalid-status-number";
        goto OnErrorExit;
    }

    // release semaphore
    sem_post(&handle->job.thread->sem);
    GLUE_RETURN_NULL;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueApiCreate(napi_env env, napi_callback_info info)
{
    const char *errorCode=NULL, *errorMsg = "syntax: apiadd (config)";
    napi_value resultN;
    napi_status statusN;

    if (!afbMain) {
        errorMsg="Should use libafb.binder() first";
        goto OnErrorExit;
    }

    // get argument
    size_t argc= GLUE_ONE_ARG;
    napi_value args[GLUE_ONE_ARG];
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc != GLUE_ONE_ARG) goto OnErrorExit;
    napi_value configN= args[0];

    json_object *configJ= napiValuetoJsonc(env, args[0]);
    if (!configJ) {
        errorCode="invalid-config";
        goto OnErrorExit;
    }
    // allocate afbMain glue and parse config to jsonC
    GlueHandleT *glue= calloc(1, sizeof(GlueHandleT));
    glue->magic= GLUE_API_MAGIC;
    glue->uid= napiGetString(env, args[0], "uid");
    glue->env=env;

    // keep track of config reference within glue handle
    statusN= napi_create_reference(env, configN, 1, &glue->api.configR);
    if (statusN != napi_ok) {
        errorCode="unreference-config";
        goto OnErrorExit;
    }

    bool hasProperty;
    statusN= napi_has_named_property(glue->env, configN, "uri", &hasProperty);
    if (hasProperty)
    {
        // imported shadow api
        errorMsg = AfbApiImport(afbMain->binder.afb, configJ);
    }
    else
    {
        // extract control and error callback from configJ attached userdata
        json_object *onErrorJ=NULL, *controlJ=NULL;
        wrap_json_unpack (configJ, "{s?o,s?o}"
            ,"control", &controlJ
            ,"onerror", &onErrorJ
        );
        if (controlJ) glue->api.async= (GlueAsyncCtxT*)json_object_get_userdata (controlJ);
        if (onErrorJ) glue->onError=(GlueAsyncCtxT*)json_object_get_userdata (onErrorJ);
        else glue->onError= afbMain->onError; // default fall on binder onerror callback
        if (glue->api.async) {
            errorMsg = AfbApiCreate(afbMain->binder.afb, configJ, &glue->api.afb, GlueCtrlCb, GlueInfoCb, GlueRqtVerbCb, GlueEventCb, glue);
        } else {
            errorMsg = AfbApiCreate(afbMain->binder.afb, configJ, &glue->api.afb, NULL, GlueInfoCb, GlueRqtVerbCb, GlueEventCb, glue);
        }
    }
    if (errorMsg)  goto OnErrorExit;

    // return api glue as a nodejs capcule glue
    statusN= napi_create_external (env, glue, NULL, NULL, &resultN);
    if (statusN != napi_ok) goto OnErrorExit;
    return resultN;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode,errorMsg);
    GLUE_RETURN_NULL;
}

// Process all afb waiting jobs
static void GlueOnPoolUvCb(uv_poll_t* handle, int status, int events) {

    //write (2,".", 1);
    GluePollRunJobs();
}

static napi_value GlueBinderConf(napi_env env, napi_callback_info info)
{
    const char *errorCode="internal-error", *errorMsg="syntax: binder(config)";
    napi_value resultN;
    napi_status statusN;
    uint32_t unused=0;

    // allocate afbMain glue and parse config to jsonC
    if (!afbMain) {
        afbMain= calloc(1, sizeof(GlueHandleT));
        afbMain->magic= GLUE_BINDER_MAGIC;
        afbMain->env=env;
    }

    // get arguments
    size_t argc= GLUE_ONE_ARG;
    napi_value args[GLUE_ONE_ARG];
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc != GLUE_ONE_ARG) {
        errorCode= "invalid-argument-count";
        goto OnErrorExit;
    }

    json_object *configJ= napiValuetoJsonc(env, args[0]);
    if (!configJ) {
        errorCode="Invalid-Config";
        goto OnErrorExit;
    }

    errorMsg= AfbBinderConfig(configJ, &afbMain->binder.afb, afbMain);
    if (errorMsg) goto OnErrorExit;
    afbMain->uid= napiGetString(env, args[0], "uid");

    // keep track of napi config object
    statusN= napi_create_reference(env, args[0], 1, &afbMain->binder.configR);
    if (statusN != napi_ok) goto OnErrorExit;

    // extract control and error callback from configJ attached userdata
    json_object *onErrorJ= json_object_object_get(configJ, "onerror");
    if (onErrorJ) afbMain->onError= (GlueAsyncCtxT*)json_object_get_userdata (onErrorJ);

    int status;

    // main loop only return when binder startup func return statusN!=0
    GLUE_AFB_NOTICE(afbMain, "Entering binder mainloop");
    status = AfbBinderEnter(afbMain->binder.afb, NULL, NULL, NULL);
    if (status < 0) goto OnErrorExit;

    // retreive libafb jobs epool file handle
    int afbFd = afb_ev_mgr_get_fd();
	if (afbFd < 0) goto OnErrorExit;
    afb_ev_mgr_prepare();

    // insert afb within nodejs libuv event loop
    statusN = napi_get_uv_event_loop(env, &afbMain->binder.loopU);
	if (statusN != napi_ok) goto OnErrorExit;

    //add libafb epool fd into libuv pool
 	status = uv_poll_init(afbMain->binder.loopU, &afbMain->binder.poolU, afbFd);
	if (status < 0) goto OnErrorExit;

    // register callback handle for libafb jobs
	status = uv_poll_start(&afbMain->binder.poolU, UV_READABLE, GlueOnPoolUvCb);
	if (status < 0) goto OnErrorExit;

    // return afbMain glue as a nodejs capcule glue
    statusN= napi_create_external (env, afbMain, NULL, NULL, &resultN);
    if (statusN != napi_ok) goto OnErrorExit;
    return resultN;

OnErrorExit:
    napi_throw_type_error (env, errorCode, errorMsg);
    unused ++; // make compiler happy
    return NULL;
}

static napi_value GlueJobPost(napi_env env, napi_callback_info info)
{
    const char *errorCode="internal-error", *errorMsg="syntax: jobid=jobpost(glue,callback,delayms,[userdata])";
    napi_value resultN;
    napi_status statusN;
    int64_t timeout=0;

    // get argument
    size_t argc= GLUE_FOUR_ARG;
    napi_value args[GLUE_FOUR_ARG];
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc < GLUE_THREE_ARG || argc > GLUE_FOUR_ARG) {
        errorCode= "invalid-arg-count";
        goto OnErrorExit;
    }

    // retreive afb loop handle from 1st argument
    GlueHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok || !GlueGetApi(glue)) {
        errorCode="invalid-afb-handle";
        goto OnErrorExit;
    }

    // prepare handle for callback
    GlueCallHandleT *handle=calloc (1, sizeof(GlueCallHandleT));
    handle->magic= GLUE_POST_MAGIC;
    handle->glue=glue;
    handle->async.uid= napiGetString(env, args[1], "name");

    statusN= napi_create_reference(env, args[1], 1, &handle->async.callbackR);
    if (statusN != napi_ok) {
        errorCode= "config-referencable-callback";
        goto OnErrorExit;
    }

    statusN= napi_get_value_int64(env, args[2], &timeout);
    if (statusN != napi_ok || napiGetType(env,args[2]) != napi_number) {
        errorCode="Invalid-jobid-integer";
        goto OnErrorExit;
    }

    if (argc == GLUE_FOUR_ARG) {
        statusN= napi_create_reference(env, args[3], 1, (void*)&handle->async.userdataR);
        if (statusN != napi_ok) {
            errorCode= "userdata-not-referencable";
            goto OnErrorExit;
        }
    }

    // ms delay for OnTimerCB (timeout is dynamic and depends on CURLOPT_LOW_SPEED_TIME)
    int jobid= afb_sched_post_job (NULL /*group*/, timeout,  0 /*exec-timeout*/,GlueJobPostCb, handle, Afb_Sched_Mode_Start);
	if (jobid <= 0) goto OnErrorExit;

    statusN=  napi_create_int64(env, jobid, &resultN);
    if (statusN != napi_ok) goto OnErrorExit;

    return resultN;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode,errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueJobCancel(napi_env env, napi_callback_info info) {
    const char *errorCode="internal-error", *errorMsg="syntax: jobcancel(jobid)";
    napi_status statusN;
    int64_t jobid=0;
    int err;

    // get argument
    size_t argc= GLUE_ONE_ARG;
    napi_value args[GLUE_FOUR_ARG];
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc != GLUE_ONE_ARG) {
        errorCode= "invalid-arg-count";
        goto OnErrorExit;
    }

    // get jobid
    if (statusN == napi_ok) napi_get_value_int64(env, args[1], &jobid);
    if (statusN != napi_ok || napiGetType(env,args[2]) != napi_number) {
        errorCode="Invalid-jobid-integer";
        goto OnErrorExit;
    }

    err= afb_jobs_abort((int)jobid);
    if (err) goto OnErrorExit;
    GLUE_RETURN_NULL;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode,errorMsg);
    GLUE_RETURN_NULL;
}
// this routine execute within mainloop context when binder is ready to go
static napi_value GlueJobStart(napi_env env, napi_callback_info info)
{
    const char *errorCode="internal-error", *errorMsg="syntax: mainloop(callback,[timeout],[userdata])";
    napi_value resultN;
    napi_status statusN;
    napi_ref callbackR=NULL, userdataR=NULL;
    int64_t timeout=0;

    // get argument
    size_t argc= GLUE_FOUR_ARG;
    napi_value args[GLUE_FOUR_ARG];
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc < GLUE_TWO_ARG || argc > GLUE_FOUR_ARG) {
        errorCode= "invalid-arg-count";
        goto OnErrorExit;
    }

    // retreive afb loop handle from 1st argument
    GlueHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok || !GlueGetApi(glue)) {
        errorCode="invalid-afb-handle";
        goto OnErrorExit;
    }

    // create a reference to prevent callback from beeing deleted
    statusN= napi_create_reference(env, args[1], 1, &callbackR);
    if (statusN != napi_ok || napiGetType(env, args[1]) != napi_function) {
        errorCode="Invalid-callback-function";
        goto OnErrorExit;
    }

    if (argc >= GLUE_THREE_ARG) {
        if (statusN == napi_ok) napi_get_value_int64(env, args[2], &timeout);
        if (statusN != napi_ok || napiGetType(env,args[2]) != napi_number) {
            errorCode="Invalid-timeout-integer";
            goto OnErrorExit;
        }
    }

    if (argc == GLUE_FOUR_ARG && napiGetType(env, args[3]) != napi_null) {
        statusN= napi_create_reference(env, args[3], 1, &userdataR);
        if (statusN != napi_ok) {
            errorCode="Invalid-userdata-object";
            goto OnErrorExit;
        }
    }

    resultN= GlueJobStartCb (env, glue, callbackR, timeout, userdataR);
    if (!resultN) goto OnErrorExit;

    return resultN;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode,errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueGetUid(napi_env env, napi_callback_info info)
{
    const char *errorCode= NULL, *errorMsg = "syntax: getuid(handle)";
    napi_status statusN;

    // get arguments
    size_t argc= GLUE_ONE_ARG;
    napi_value resultN, args[GLUE_ONE_ARG];
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc != GLUE_ONE_ARG) goto OnErrorExit;

    // get afb glue handle from 1st argument
    GlueHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok || !glue->uid) {
        errorCode= "invalid-glue-handle";
        goto OnErrorExit;
    }

    statusN= napi_create_string_utf8(env, glue->uid, NAPI_AUTO_LENGTH, &resultN);
    if (statusN != napi_ok) {
        errorCode="missing-handle-uid";
        goto OnErrorExit;
    }

    return resultN;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueGetConfig(napi_env env, napi_callback_info info)
{
    const char *errorCode=NULL,*errorMsg = "syntax: config(handle[,key])";
    napi_status statusN;
    napi_value configN;
    napi_ref configR;

    // get arguments
    size_t argc= GLUE_TWO_ARG;
    napi_value resultN, args[GLUE_TWO_ARG];
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || (argc != GLUE_ONE_ARG && argc != GLUE_TWO_ARG)) {
        errorCode="invalid-arg-count";
        goto OnErrorExit;
    }

    // get afb glue handle from 1st argument
    GlueHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok) {
        errorCode="invalid-handle";
        goto OnErrorExit;
    }

    // optional key within second argument
    char *key=NULL;
    if (argc == GLUE_TWO_ARG) {
        size_t len;
        napi_value valueN=args[1];
        statusN = napi_get_value_string_utf8(env, valueN, NULL, 0, &len);
        if (statusN != napi_ok) {
            errorCode="invalid-key-string";
            goto OnErrorExit;
        }
        key= alloca(len+1);
        napi_get_value_string_utf8(env, valueN, key, len+1, &len);
    }

    switch (glue->magic)
    {
    case GLUE_API_MAGIC:
        configR = glue->api.configR;
        break;
    case GLUE_BINDER_MAGIC:
        configR = glue->binder.configR;
        break;
    case GLUE_TIMER_MAGIC:
        configR = glue->timer.configR;
        break;
    case GLUE_EVT_MAGIC:
        configR = glue->event.configR;
        break;
    case GLUE_JOB_MAGIC: {
        GlueHandleT *handle = afb_api_get_userdata(GlueGetApi(glue));
        if (handle->magic == GLUE_API_MAGIC) configR = handle->api.configR;
        else configR = afbMain->binder.configR;
        break;
    }
    case GLUE_RQT_MAGIC: {
        GlueHandleT *handle = afb_api_get_userdata(GlueGetApi(glue));
        configR = handle->api.configR;
        break;
    }
    default:
        errorCode = "invalid-glue-handle";
        goto OnErrorExit;
    }

    if (!configR) {
        errorMsg= "missing-config-reference";
        goto OnErrorExit;
    }

    statusN= napi_get_reference_value(glue->env, configR, &configN);
    if (statusN != napi_ok) {
        errorCode = "invalid-config-reference";
        goto OnErrorExit;
    }

    if (!key)
    {
        // TBD Fulup check how node object cycle works !!!
        uint32_t count;
        napi_reference_ref (env, configR, &count);
        resultN= configN;
    }
    else
    {
        resultN= napiGetElement(glue->env, configN, key);
        if (!resultN) napi_get_null(glue->env, &resultN);
    }
    return resultN;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "info=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueClientInfo(napi_env env, napi_callback_info info)
{
    napi_status statusN;
    napi_value resultN;
    const char *errorCode=NULL, *errorMsg = "syntax: clientinfo(rqt, ['key'])";

    // get argument count
    size_t argc;
    napi_get_cb_info(env, info, &argc, NULL, NULL, NULL);
    napi_value args[argc];

    // get arguments
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || (argc != GLUE_TWO_ARG && argc != GLUE_ONE_ARG)) {
        errorCode="too-few-arguments";
        goto OnErrorExit;
    }

    // retreive afb request handle from 1st argument
    GlueHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok || glue->magic != GLUE_RQT_MAGIC) {
        errorCode="invalid-rqt-handle";
        goto OnErrorExit;
    }

    // optional key within second argument
    char *key=NULL;
    if (argc == GLUE_TWO_ARG) {
        size_t len;
        napi_value valueN=args[1];
        statusN = napi_get_value_string_utf8(env, valueN, NULL, 0, &len);
        if (statusN != napi_ok) goto OnErrorExit;
        key= alloca(len+1);
        napi_get_value_string_utf8(env, valueN, key, len+1, &len);
    }

    json_object *clientJ= afb_req_get_client_info(glue->rqt.afb);
    if (!clientJ) {
        errorMsg= "(hoops) afb_req_get_client_info no session info";
        goto OnErrorExit;
    }

    if (!key) {
        resultN = napiJsoncToValue(env, clientJ);
    } else {
        json_object *keyJ= json_object_object_get(clientJ, key);
        if (!keyJ) {
            errorMsg= "unknown client info key";
            goto OnErrorExit;
        }
        resultN = napiJsoncToValue(env, keyJ);
    }
    return resultN;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}
static napi_value GlueIsReplyable(napi_env env, napi_callback_info info)
{
    const char *errorCode="internal-error", *errorMsg = "syntax: replyable(rqt)";
    napi_status statusN;
    napi_value  replyableN;
    bool replyable;

    // get argument count
    size_t argc;
    napi_get_cb_info(env, info, &argc, NULL, NULL, NULL);
    napi_value args[argc];

    // get arguments
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc != GLUE_ONE_ARG) {
        errorCode="too-many-arguments";
        goto OnErrorExit;
    }

    // retreive afb glue handle from 1st argument
    GlueHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok || glue->magic != GLUE_RQT_MAGIC) {
        replyable=0;
    } else {
        replyable= !glue->rqt.replied;
    }

    statusN = napi_get_boolean(env, replyable, &replyableN);
    return replyableN;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;}

static napi_value GlueRespond(napi_env env, napi_callback_info info)
{
    const char *errorCode="internal-error", *errorMsg = "syntax: reply(rqt, statusN, [arg1 ... argn])";
    napi_status statusN;
    json_object *valueJ;
    int64_t status, count=0;
    afb_data_t *reply=NULL;

    // get argument count
    size_t argc;
    napi_get_cb_info(env, info, &argc, NULL, NULL, NULL);
    napi_value args[argc];

    // get arguments
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc < GLUE_TWO_ARG) {
        errorCode="too-few-arguments";
        goto OnErrorExit;
    }

    // retreive afb glue handle from 1st argument
    GlueHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok || glue->magic != GLUE_RQT_MAGIC) {
        errorCode="invalid-rqt-handle";
        goto OnErrorExit;
    }

    // second argument should be the status
    if (napiGetType(glue->env, args[1]) != napi_number) {
        errorCode="status-not-number";
        goto OnErrorExit;
    }
    statusN= napi_get_value_int64 (glue->env, args[1], &status);
    if (statusN != napi_ok) goto OnErrorExit;

    if (argc > GLUE_TWO_ARG) {
        reply= alloca(sizeof(afb_data_t*)*(argc-GLUE_TWO_ARG));
        for (int idx = GLUE_TWO_ARG ; idx < argc; idx++) {
            valueJ = napiValuetoJsonc(glue->env, args[idx]);
            if (!valueJ)  goto OnErrorExit;
            afb_create_data_raw(&reply[count], AFB_PREDEFINED_TYPE_JSON_C, valueJ, 0, (void *)json_object_put, valueJ);
            count++;
        }
    }
    // respond request and free ressources.
    GlueAfbReply(glue, status, count, reply);
    GLUE_RETURN_NULL;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueSleepThread(napi_env env, napi_callback_info info)
{
    napi_status statusN;
    napi_value  resultN;
    static int64_t count=0;
    long tid= pthread_self();
    int64_t seconds;

    // get argument count
    size_t argc=0;
    napi_get_cb_info(env, info, &argc, NULL, NULL, NULL);
    napi_value args[argc];
    napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    fprintf (stderr, "-- Enter GlueSleepThread count=%ld tid=%ld\n", count, tid);

    if (argc > 0) {
        napi_get_value_int64 (env, args[0], &seconds);
        if (seconds > 0) {
            struct timespec timeout = {
                .tv_sec = (time_t) seconds,
                .tv_nsec=0,
            };
            nanosleep(&timeout, NULL);
        }
        fprintf (stderr, "-- Exit GlueSleepThread count=%ld tid=%ld\n\n", count, tid);
    }
    statusN= napi_create_int64 (env, count++, &resultN);
    assert(statusN == napi_ok);
    return resultN;
}


static napi_value GlueExit(napi_env env, napi_callback_info info)
{
    const char *errorCode=NULL, *errorMsg= "syntax: exit(handle, statusN)";
    napi_status statusN;
    int64_t exitCode=-1;

    // get argument count
    size_t argc;
    napi_get_cb_info(env, info, &argc, NULL, NULL, NULL);
    napi_value args[argc];

    // get arguments
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc != GLUE_TWO_ARG) {
        errorCode="too-few-arguments";
        goto OnErrorExit;
    }

    // retreive afb glue handle from 1st argument
    GlueHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok) {
        errorCode="invalid-glue-handle";
        goto OnErrorExit;
    }

    statusN= napi_get_value_int64 (env, args[1], &exitCode);
    if (statusN != napi_ok) {
        errorCode="invalid-status-integer";
        goto OnErrorExit;
    }

    AfbBinderExit(afbMain->binder.afb, (int)exitCode);
    GLUE_RETURN_NULL;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueSetLoa(napi_env env, napi_callback_info info)
{
    const char *errorCode=NULL, *errorMsg = "syntax: setloa(rqt, newloa)";
    napi_status statusN;
    int64_t loa=0;

    // get argument count
    size_t argc;
    napi_get_cb_info(env, info, &argc, NULL, NULL, NULL);
    napi_value args[argc];

    // get arguments
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc != GLUE_TWO_ARG) {
        errorCode="too-few-arguments";
        goto OnErrorExit;
    }

    // retreive afb glue handle from 1st argument
    GlueHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok || glue->magic != GLUE_RQT_MAGIC) {
        errorCode="invalid-rqt-handle";
        goto OnErrorExit;
    }

    statusN= napi_get_value_int64 (env, args[1], &loa);
    if (statusN != napi_ok) {
        errorCode="invalid-status-integer";
        goto OnErrorExit;
    }

    int err= afb_req_session_set_LOA(glue->rqt.afb, (int)loa);
    if (err < 0) {
        errorMsg="Invalid Rqt Session";
        goto OnErrorExit;
    }

    GLUE_RETURN_NULL;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueBindingLoad(napi_env env, napi_callback_info info)
{
    const char *errorCode=NULL, *errorMsg =  "syntax: binding(config)";
    napi_status statusN;

    if (!afbMain) {
        errorCode="binder-not-initialized";
        goto OnErrorExit;
    }

    // get argument
    size_t argc= GLUE_ONE_ARG;
    napi_value args[GLUE_ONE_ARG];
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc != GLUE_ONE_ARG) goto OnErrorExit;

    json_object *configJ= napiValuetoJsonc(env, args[0]);
    if (!configJ) {
        errorCode="Invalid-Config";
        goto OnErrorExit;
    }

    errorCode = AfbBindingLoad(afbMain->binder.afb, configJ);
    if (errorCode) goto OnErrorExit;

    GLUE_RETURN_NULL;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueVerbAdd(napi_env env, napi_callback_info info)
{
    const char *errorCode=NULL, *errorMsg = "syntax: addverb(api, config, context)";
    napi_status statusN;

    // get arguments
    size_t argc= GLUE_THREE_ARG;
    napi_value args[GLUE_ONE_ARG];
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc != GLUE_THREE_ARG) goto OnErrorExit;

    // retreive afb glue handle from 1st argument
    GlueHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok || glue->magic != GLUE_API_MAGIC) {
        errorCode="invalid-api-handle";
        goto OnErrorExit;
    }

    json_object *configJ= napiValuetoJsonc(env, args[1]);
    if (!configJ) {
        errorCode="Invalid-Config";
        goto OnErrorExit;
    }

    // create a reference to prevent userdata from beeing deleted
    napi_ref userdataR;
    statusN= napi_create_reference(env, args[2], 1, &userdataR);
    if (statusN != napi_ok) goto OnErrorExit;

    AfbVcbDataT *vcbData= calloc (1, sizeof(AfbVcbDataT));
    vcbData->magic= (void*)AfbAddVerbs;
    vcbData->configJ= configJ;
    vcbData->userdata= (void*)
    json_object_get(vcbData->configJ);

    errorCode= AfbAddOneVerb (afbMain->binder.afb, glue->api.afb, configJ, GlueRqtVerbCb, vcbData);
    if (errorCode) goto OnErrorExit;

    GLUE_RETURN_NULL;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueCallAsync(napi_env env, napi_callback_info info)
{
    const char  *errorCode="internal-error", *errorMsg= "syntax: callasync(handle, api, verb, callback, context, ...args)";
    napi_status statusN;
    json_object *valueJ;
    int count=0;

    // get argument count
    size_t argc;
    napi_get_cb_info(env, info, &argc, NULL, NULL, NULL);
    napi_value args[argc];

    // get arguments
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc < GLUE_FIVE_ARG) {
        errorCode="too-few-arguments";
        goto OnErrorExit;
    }

    // retreive afb glue handle from 1st argument
    GlueHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok) {
        errorCode="invalid-rqt-handle";
        goto OnErrorExit;
    }

    // retreive API and VERB string
    size_t len;
    char apiname[API_MAX_LEN];
    char verbname[VERB_MAX_LEN];
    statusN = napi_get_value_string_utf8(env, args[1], apiname, API_MAX_LEN, &len);
    if (statusN != napi_ok || len == API_MAX_LEN) {
        errorCode="apiname-too-long";
        goto OnErrorExit;
    }
    statusN = napi_get_value_string_utf8(env, args[2], verbname, VERB_MAX_LEN, &len);
    if (statusN != napi_ok || len == VERB_MAX_LEN) {
        errorCode="verbname-too-long";
        goto OnErrorExit;
    }

    // prepare callback userdata handle
    GlueCallHandleT *handle= calloc(1,sizeof(GlueCallHandleT));
    handle->magic= GLUE_CALL_MAGIC;
    handle->glue=glue;
    asprintf (&handle->async.uid, "%s/%s", apiname, verbname);

    statusN= napi_create_reference(env, args[3], 1, &handle->async.callbackR);
    if (statusN != napi_ok  || napiGetType(env, args[3]) != napi_function) goto OnErrorExit;

    // create a reference to prevent userdata from beeing deleted
    statusN= napi_create_reference(env, args[4], 1, &handle->async.userdataR);
    if (statusN != napi_ok) {
        errorCode="context-not-object";
        goto OnErrorExit;
    }

    int nparams= (int)argc-GLUE_FIVE_ARG;
    afb_data_t *params=NULL;
    if (nparams > 0) {
        params= alloca(sizeof(afb_data_t*)*nparams);
        for (int idx= 0 ; idx < nparams; idx++) {
            valueJ = napiValuetoJsonc(glue->env, args[idx+GLUE_FIVE_ARG]);
            if (!valueJ)  goto OnErrorExit;
            afb_create_data_raw(&params[count], AFB_PREDEFINED_TYPE_JSON_C, valueJ, 0, (void *)json_object_put, valueJ);
            count++;
        }
    }

    switch (glue->magic) {
        case GLUE_RQT_MAGIC:
            afb_req_subcall (glue->rqt.afb, apiname, verbname, nparams, params, afb_req_subcall_catch_events, GlueRqtSubcallCb, (void*)handle);
            break;
        default:
            afb_api_call(GlueGetApi(glue), apiname, verbname, nparams, params, GlueApiSubcallCb, (void*)handle);
    }
    GLUE_RETURN_NULL;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueCallPromise(napi_env env, napi_callback_info info)
{
    const char  *errorCode="internal-error", *errorMsg= "syntax: call(handle, api, verb, ...args)";
    napi_status statusN;
    json_object *valueJ;
    int count=0;

    // get argument count
    size_t argc;
    napi_get_cb_info(env, info, &argc, NULL, NULL, NULL);
    napi_value args[argc];

    // get arguments
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc <  GLUE_THREE_ARG) {
        errorCode="invalid-argument-count";
        goto OnErrorExit;
    }

    // retreive afb glue handle from 1st argument
    GlueHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok) {
        errorCode="invalid-rqt-handle";
        goto OnErrorExit;
    }

    // retreive API and VERB string
    size_t len;
    char apiname[API_MAX_LEN];
    char verbname[VERB_MAX_LEN];
    statusN = napi_get_value_string_utf8(env, args[1], apiname, API_MAX_LEN, &len);
    if (statusN != napi_ok || len == API_MAX_LEN) {
        errorCode="apiname-too-long";
        goto OnErrorExit;
    }
    statusN = napi_get_value_string_utf8(env, args[2], verbname, VERB_MAX_LEN, &len);
    if (statusN != napi_ok || len == VERB_MAX_LEN) {
        errorCode="verbname-too-long";
        goto OnErrorExit;
    }

    int nparams= (int)argc-GLUE_THREE_ARG;
    afb_data_t *params=NULL;
    if (nparams > 0) {
        params= alloca(sizeof(afb_data_t*)*nparams);
        for (int idx= 0 ; idx < nparams; idx++) {
            valueJ = napiValuetoJsonc(env, args[idx+GLUE_THREE_ARG]);
            if (!valueJ)  goto OnErrorExit;
            afb_create_data_raw(&params[count], AFB_PREDEFINED_TYPE_JSON_C, valueJ, 0, (void *)json_object_put, valueJ);
            count++;
        }
    }

    GlueCtxThreadT *thread= calloc(1, sizeof(GlueCtxThreadT));
    asprintf (&thread->uid, "%s/%s", apiname, verbname);
    napi_value promiseN= GluePromiseStartCb(env, thread);
    if (!promiseN) goto OnErrorExit;

    switch (glue->magic) {
        case GLUE_RQT_MAGIC:
            afb_req_subcall (glue->rqt.afb, apiname, verbname, nparams, params, afb_req_subcall_catch_events, GlueRqtPromiseCb, (void*)thread);
            break;
        default:
            afb_api_call(GlueGetApi(glue), apiname, verbname, nparams, params, GlueApiPromiseCb, (void*)thread);
    }
    return promiseN;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueCallSync(napi_env env, napi_callback_info info)
{
    const char *errorCode="internal-error", *errorMsg= "syntax: callsync(handle, api, verb, ...args)";
    napi_status statusN;
    json_object *valueJ;
    int status, err, count=0;

    // get argument count
    size_t argc;
    napi_get_cb_info(env, info, &argc, NULL, NULL, NULL);
    napi_value args[argc];

    // get arguments
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc < GLUE_THREE_ARG) {
        errorCode="too-few-arguments";
        goto OnErrorExit;
    }

    // retreive afb glue handle from 1st argument
    GlueHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok) {
        errorCode="invalid-rqt-handle";
        goto OnErrorExit;
    }

    // retreive API and VERB string
    size_t len;
    char apiname[API_MAX_LEN];
    char verbname[VERB_MAX_LEN];
    statusN = napi_get_value_string_utf8(env, args[1], apiname, API_MAX_LEN, &len);
    if (statusN != napi_ok || len == API_MAX_LEN) {
        errorCode="apiname-too-long";
        goto OnErrorExit;
    }
    statusN = napi_get_value_string_utf8(env, args[2], verbname, VERB_MAX_LEN, &len);
    if (statusN != napi_ok || len == VERB_MAX_LEN) {
        errorCode="verbname-too-long";
        goto OnErrorExit;
    }

    int nparams= (int)argc-GLUE_THREE_ARG;
    afb_data_t *params=NULL;
    if (nparams > 0) {
        params= alloca(sizeof(afb_data_t*)*nparams);
        for (int idx= 0 ; idx < nparams; idx++) {
            valueJ = napiValuetoJsonc(glue->env, args[idx+GLUE_THREE_ARG]);
            if (!valueJ)  goto OnErrorExit;
            afb_create_data_raw(&params[count], AFB_PREDEFINED_TYPE_JSON_C, valueJ, 0, (void *)json_object_put, valueJ);
            count++;
        }
    }
    unsigned nreplies= SUBCALL_MAX_RPLY;
    afb_data_t replies[SUBCALL_MAX_RPLY];

    switch (glue->magic) {
        case GLUE_RQT_MAGIC:
            err= afb_req_subcall_sync (glue->rqt.afb, apiname, verbname, nparams, params, afb_req_subcall_catch_events, &status, &nreplies, replies);
            break;
        default:
            err= afb_api_call_sync (GlueGetApi(glue), apiname, verbname, nparams, params, &status, &nreplies, replies);
    }

    if (err) {
        status = err;
        errorCode= "api subcall fail";
        goto OnErrorExit;
    }
    // subcall was refused
    if (AFB_IS_BINDER_ERRNO(status)) {
        errorCode= afb_error_text(status);
        goto OnErrorExit;
    }

    // create response
    napi_value resultN, slotN, argsN;
    statusN = napi_create_object(env, &resultN);

    // status is first element of response array
    statusN= napi_create_int64(env, (int64_t)status, &slotN);
	if (statusN != napi_ok) goto OnErrorExit;
	statusN = napi_set_named_property(env, resultN, "status", slotN);
	if (statusN != napi_ok) goto OnErrorExit;

    // push afb replies into a node array as response
    napi_value *repliesN= alloca (sizeof (napi_value) *nreplies);
    errorMsg= napiPushAfbArgs (env, repliesN, 0, nreplies, replies);
    if (errorMsg) goto OnErrorExit;

    statusN= napi_create_array_with_length(env, nreplies, &argsN);
	if (statusN != napi_ok) goto OnErrorExit;

    for (int idx=0; idx < nreplies; idx++) {
        if (repliesN[idx]) statusN = napi_set_element(env, argsN, idx, repliesN[idx]);
        if (statusN != napi_ok) goto OnErrorExit;
    }
	statusN = napi_set_named_property(env, resultN, "args", argsN);
	if (statusN != napi_ok) goto OnErrorExit;

    return resultN;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}


// NodeJS NAPI module initialisation
static napi_value NapiGlueInit(napi_env env, napi_value exports) {
  napi_status statusN;
  napi_property_descriptor descs[] = {
    {"info"          , NULL, GluePrintInfo     , NULL, NULL, NULL, napi_default, NULL },
    {"error"         , NULL, GluePrintError    , NULL, NULL, NULL, napi_default, NULL },
    {"debug"         , NULL, GluePrintDebug    , NULL, NULL, NULL, napi_default, NULL },
    {"warning"       , NULL, GluePrintWarning  , NULL, NULL, NULL, napi_default, NULL },
    {"notice"        , NULL, GluePrintNotice   , NULL, NULL, NULL, napi_default, NULL },
    {"sleep"         , NULL, GlueSleepThread   , NULL, NULL, NULL, napi_default, NULL },
    {"binder"        , NULL, GlueBinderConf    , NULL, NULL, NULL, napi_default, NULL },
    {"binding"       , NULL, GlueBindingLoad   , NULL, NULL, NULL, napi_default, NULL },
    {"apiadd"        , NULL, GlueApiCreate     , NULL, NULL, NULL, napi_default, NULL },
    {"verbadd"       , NULL, GlueVerbAdd       , NULL, NULL, NULL, napi_default, NULL },
    {"config"        , NULL, GlueGetConfig     , NULL, NULL, NULL, napi_default, NULL },
    {"clientinfo"    , NULL, GlueClientInfo    , NULL, NULL, NULL, napi_default, NULL },
    {"reply"         , NULL, GlueRespond       , NULL, NULL, NULL, napi_default, NULL },
    {"exit"          , NULL, GlueExit          , NULL, NULL, NULL, napi_default, NULL },
    {"setloa"        , NULL, GlueSetLoa        , NULL, NULL, NULL, napi_default, NULL },
    {"subcall"       , NULL, GlueCallPromise   , NULL, NULL, NULL, napi_default, NULL },
    {"callasync"     , NULL, GlueCallAsync     , NULL, NULL, NULL, napi_default, NULL },
    {"callsync"      , NULL, GlueCallSync      , NULL, NULL, NULL, napi_default, NULL },
    {"replyable"     , NULL, GlueIsReplyable   , NULL, NULL, NULL, napi_default, NULL },
    {"evtsubscribe"  , NULL, GlueEvtSubscribe  , NULL, NULL, NULL, napi_default, NULL },
    {"evtunsubscribe", NULL, GlueEvtUnsubscribe, NULL, NULL, NULL, napi_default, NULL },
    {"evthandler"    , NULL, GlueEvtHandler    , NULL, NULL, NULL, napi_default, NULL },
    {"evtnew"        , NULL, GlueEvtNew        , NULL, NULL, NULL, napi_default, NULL },
    {"evtpush"       , NULL, GlueEvtPush       , NULL, NULL, NULL, napi_default, NULL },
    {"timerunref"    , NULL, GlueTimerUnref    , NULL, NULL, NULL, napi_default, NULL },
    {"timeraddref"   , NULL, GlueTimerAddref   , NULL, NULL, NULL, napi_default, NULL },
    {"timernew"      , NULL, GlueTimerNew      , NULL, NULL, NULL, napi_default, NULL },
    {"jobpost"       , NULL, GlueJobPost       , NULL, NULL, NULL, napi_default, NULL },
    {"jobcancel"     , NULL, GlueJobCancel     , NULL, NULL, NULL, napi_default, NULL },
    {"jobkill"       , NULL, GlueJobKill       , NULL, NULL, NULL, napi_default, NULL },
    {"jobstart"      , NULL, GlueJobStart      , NULL, NULL, NULL, napi_default, NULL },
    {"getuid"        , NULL, GlueGetUid        , NULL, NULL, NULL, napi_default, NULL },
    // promisses pour les call async

 /*

   call async avec promise

    - si pas de callback async dans la config
      * creer une promise
      * la retourner à l'appellant
      * sur resolution la resoudre
      * creer un object de retour de promise type {status:xx error:xxx args:[]}
*/

  };

  statusN = napi_define_properties(env, exports, sizeof(descs)/sizeof(napi_property_descriptor), descs);
  assert(statusN == napi_ok);
  return exports;
}
NAPI_MODULE(TARGET_NAME, NapiGlueInit)