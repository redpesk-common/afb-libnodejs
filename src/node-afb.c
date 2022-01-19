/*
 * Conoderight (C) 2015-2021 IoT.bzh Company
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

#include <uv.h>
#include <node_api.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <wrap-json.h>

#include "node-afb.h"
#include "node-utils.h"
#include "node-callbacks.h"

#include <glue-afb.h>
#include <glue-utils.h>

// global afbMain glue
AfbHandleT *afbMain=NULL;

#define GLUE_RETURN_NULL { \
    napi_value resultN; \
    napi_get_null(env, &resultN); \
    return resultN; }



static napi_value  GluePrintInfo(napi_env env, napi_callback_info info)
{
    napiPrintMsg(AFB_SYSLOG_LEVEL_INFO, env, info);
    GLUE_RETURN_NULL;
}

static napi_value  GluePrintError(napi_env env, napi_callback_info info)
{
    napiPrintMsg(AFB_SYSLOG_LEVEL_ERROR, env, info);
    GLUE_RETURN_NULL;
}

static napi_value  GluePrintWarning(napi_env env, napi_callback_info info)
{
    napiPrintMsg(AFB_SYSLOG_LEVEL_WARNING, env, info);
    GLUE_RETURN_NULL;
}

static napi_value  GluePrintNotice(napi_env env, napi_callback_info info)
{
    napiPrintMsg(AFB_SYSLOG_LEVEL_NOTICE, env, info);
    GLUE_RETURN_NULL;
}

static napi_value  GluePrintDebug(napi_env env, napi_callback_info info)
{
    napiPrintMsg(AFB_SYSLOG_LEVEL_DEBUG, env, info);
    GLUE_RETURN_NULL;
}


/*
static napi_value GlueBinderConf(napi_env env, napi_callback_info info)
{
    const char *errorMsg="syntax: binder(config)";
    if (afbMain) {
        errorMsg="(hoops) binder(config) already loaded";
        goto OnErrorExit;
    }

    // allocate afbMain glue and parse config to jsonC
    afbMain= calloc(1, sizeof(AfbHandleT));
    afbMain->magic= GLUE_BINDER_MAGIC;

    if (!nodeArg_ParseTuple(argsP, "O", &afbMain->binder.configR)) {
        errorMsg= "invalid config object";
        goto OnErrorExit;
    }
    json_object *configJ= nodeObjToJson(afbMain->binder.configR);
    if (!configJ) {
        errorMsg="json incompatible config";
        goto OnErrorExit;
    }

    errorMsg= AfbBinderConfig(configJ, &afbMain->binder.afb);
    if (errorMsg) goto OnErrorExit;

    // return afbMain glue as a nodejs capcule glue
    napi_value capcule= nodeCapsule_New(afbMain, GLUE_AFB_UID, NULL);
    return capcule;

OnErrorExit:
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
    nodeErr_Print();
    GLUE_RETURN_NULL;
}


static napi_value GlueBindingLoad(napi_env env, napi_callback_info info)
{
    const char *errorMsg =  "syntax: binding(config)";
    napi_value configR;

    if (!nodeArg_ParseTuple(argsP, "O", &configR)) goto OnErrorExit;

    json_object *configJ= nodeObjToJson(configR);
    if (!configJ) goto OnErrorExit;

    errorMsg = AfbBindingLoad(afbMain->binder.afb, configJ);
    if (errorMsg) goto OnErrorExit;

    GLUE_RETURN_NULL;

OnErrorExit:
        GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueAsyncCall(napi_env env, napi_callback_info info)
{
    const char *errorMsg= "syntax: callasync(handle, api, verb, callback, context, ...)";
    long index=0;

    // parse input arguments
    long count = nodeTuple_GET_SIZE(argsP);
    afb_data_t params[count];
    if (count < GLUE_FIVE_ARG) goto OnErrorExit;

    AfbHandleT* glue= nodeCapsule_GetPointer(nodeTuple_GetItem(argsP,0), GLUE_AFB_UID);
    if (!glue) goto OnErrorExit;

    const char* apiname= nodeUnicode_AsUTF8(nodeTuple_GetItem(argsP,1));
    if (!apiname) goto OnErrorExit;

    const char* verbname= nodeUnicode_AsUTF8(nodeTuple_GetItem(argsP,2));
    if (!verbname) goto OnErrorExit;

    napi_value callbackP=nodeTuple_GetItem(argsP,3);
    // check callback is a valid function
    if (!nodeCallable_Check(callbackP)) goto OnErrorExit;

    napi_value userdataP =nodeTuple_GetItem(argsP,4);
    if (userdataP != node_None) node_IncRef(userdataP);

    // retreive subcall optional argument(s)
    for (index= 0; index < count-GLUE_FIVE_ARG; index++)
    {
        json_object *argsJ = nodeObjToJson(nodeTuple_GetItem(argsP,index+GLUE_FIVE_ARG));
        if (!argsJ)
        {
            errorMsg = "invalid input argument type";
            goto OnErrorExit;
        }
        afb_create_data_raw(&params[index], AFB_PREDEFINED_TYPE_JSON_C, argsJ, 0, (void *)json_object_put, argsJ);
    }

    nodeAsyncCtxT *cbHandle= calloc(1,sizeof(nodeAsyncCtxT));
    cbHandle->glue= glue;
    cbHandle->callbackP= callbackP;
    cbHandle->userdataP= userdataP;
    node_IncRef(cbHandle->callbackP);

    switch (glue->magic) {
        case GLUE_RQT_MAGIC:
            afb_req_subcall (glue->rqt.afb, apiname, verbname, (int)index, params, afb_req_subcall_catch_events, GlueRqtSubcallCb, (void*)cbHandle);
            break;
        case GLUE_LOCK_MAGIC:
        case GLUE_API_MAGIC:
        case GLUE_BINDER_MAGIC:
            afb_api_call(GlueGetApi(glue), apiname, verbname, (int)index, params, GlueApiSubcallCb, (void*)cbHandle);
            break;

        default:
            errorMsg = "handle should be a req|api";
            goto OnErrorExit;
    }
    GLUE_RETURN_NULL;

OnErrorExit:
        GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueSyncCall(napi_env env, napi_callback_info info)
{
    const char *errorMsg= "syntax: callsync(handle, api, verb, ...)";
    int err, statusN;
    long index=0, count = nodeTuple_GET_SIZE(argsP);
    afb_data_t params[count];
    unsigned nreplies= SUBCALL_MAX_RPLY;
    afb_data_t replies[SUBCALL_MAX_RPLY];

    // parse input arguments
    if (count < GLUE_THREE_ARG) goto OnErrorExit;

    AfbHandleT* glue= nodeCapsule_GetPointer(nodeTuple_GetItem(argsP,0), GLUE_AFB_UID);
    if (!glue) goto OnErrorExit;

    const char* apiname= nodeUnicode_AsUTF8(nodeTuple_GetItem(argsP,1));
    if (!apiname) goto OnErrorExit;

    const char* verbname= nodeUnicode_AsUTF8(nodeTuple_GetItem(argsP,2));
    if (!verbname) goto OnErrorExit;

    // retreive subcall api argument(s)
    for (index = 0; index < count-GLUE_THREE_ARG; index++)
    {
        json_object *argsJ = nodeObjToJson(nodeTuple_GetItem(argsP,index+GLUE_THREE_ARG));
        if (!argsJ)
        {
            errorMsg = "(hoops) afb_subcall_sync fail";
            goto OnErrorExit;
        }
        afb_create_data_raw(&params[index], AFB_PREDEFINED_TYPE_JSON_C, argsJ, 0, (void *)json_object_put, argsJ);
    }

    switch (glue->magic) {
        case GLUE_RQT_MAGIC:
            err= afb_req_subcall_sync (glue->rqt.afb, apiname, verbname, (int)index, params, afb_req_subcall_catch_events, &statusN, &nreplies, replies);
            break;
        case GLUE_LOCK_MAGIC:
        case GLUE_API_MAGIC:
        case GLUE_BINDER_MAGIC:
            err= afb_api_call_sync (GlueGetApi(glue), apiname, verbname, (int)index, params, &statusN, &nreplies, replies);
            break;

        default:
            errorMsg = "handle should be a req|api";
            goto OnErrorExit;
    }

    if (err) {
        statusN   = err;
        errorMsg= "api subcall fail";
        goto OnErrorExit;
    }
    // subcall was refused
    if (AFB_IS_BINDER_ERRNO(statusN)) {
        errorMsg= afb_error_text(statusN);
        goto OnErrorExit;
    }

    // retreive response and build nodejs response
    napi_value resultN= nodeTuple_New(nreplies+1);
    nodeTuple_SetItem(resultN, 0, nodeLong_FromLong((long)statusN));

    errorMsg= nodePushAfbReply (resultN, 1, nreplies, replies);
    if (errorMsg) goto OnErrorExit;

    return resultN;

OnErrorExit:
        GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueEventPush(napi_env env, napi_callback_info info)
{
    const char *errorMsg = "syntax: eventpush(event, [arg1...argn])";
    long count = nodeTuple_GET_SIZE(argsP);
    afb_data_t params[count];
    long index=0;

    if (count < GLUE_ONE_ARG) goto OnErrorExit;
    AfbHandleT* glue= nodeCapsule_GetPointer(nodeTuple_GetItem(argsP,0), GLUE_AFB_UID);
    if (!glue || glue->magic != GLUE_EVT_MAGIC) goto OnErrorExit;

    // get response from node and push them as afb-v4 object
    for (index= 0; index < count-GLUE_ONE_ARG; index++)
    {
        json_object *argsJ = nodeObjToJson(nodeTuple_GetItem(argsP,index+GLUE_ONE_ARG));
        if (!argsJ)
        {
            errorMsg = "invalid argument type";
            goto OnErrorExit;
        }
        afb_create_data_raw(&params[index], AFB_PREDEFINED_TYPE_JSON_C, argsJ, 0, (void *)json_object_put, argsJ);
    }

    int statusN = afb_event_push(glue->evt.afb, (int) index, params);
    if (statusN < 0)
    {
        errorMsg = "afb_event_push fail sending event";
        goto OnErrorExit;
    }
    glue->evt.count++;
    GLUE_RETURN_NULL;

OnErrorExit:
        GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueEventSubscribe(napi_env env, napi_callback_info info)
{
    const char *errorMsg = "syntax: subscribe(rqt, event)";

    long count = nodeTuple_GET_SIZE(argsP);
    if (count != GLUE_TWO_ARG) goto OnErrorExit;

    AfbHandleT* glue= nodeCapsule_GetPointer(nodeTuple_GetItem(argsP,0), GLUE_AFB_UID);
    if (!glue || glue->magic != GLUE_RQT_MAGIC) goto OnErrorExit;

    AfbHandleT* handle= nodeCapsule_GetPointer(nodeTuple_GetItem(argsP,1), GLUE_AFB_UID);
    if (!handle || handle->magic != GLUE_EVT_MAGIC) goto OnErrorExit;

    int err = afb_req_subscribe(glue->rqt.afb, handle->evt.afb);
    if (err)
    {
        errorMsg = "fail subscribing afb event";
        goto OnErrorExit;
    }
    GLUE_RETURN_NULL;

OnErrorExit:
        GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueEventUnsubscribe(napi_env env, napi_callback_info info)
{
    const char *errorMsg = "syntax: unsubscribe(rqt, event)";

    long count = nodeTuple_GET_SIZE(argsP);
    if (count != GLUE_TWO_ARG) goto OnErrorExit;

    AfbHandleT* glue= nodeCapsule_GetPointer(nodeTuple_GetItem(argsP,0), GLUE_AFB_UID);
    if (!glue || glue->magic != GLUE_RQT_MAGIC) goto OnErrorExit;

    AfbHandleT* handle= nodeCapsule_GetPointer(nodeTuple_GetItem(argsP,1), GLUE_AFB_UID);
    if (!handle || handle->magic != GLUE_EVT_MAGIC) goto OnErrorExit;

    int err = afb_req_unsubscribe(glue->rqt.afb, handle->evt.afb);
    if (err)
    {
        errorMsg = "(hoops) afb_req_unsubscribe fail";
        goto OnErrorExit;
    }
    GLUE_RETURN_NULL;

OnErrorExit:
        GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueEventNew(napi_env env, napi_callback_info info)
{
    const char *errorMsg = "syntax: eventnew(api, config)";
    napi_value slotP;
    int err;

    long count = nodeTuple_GET_SIZE(argsP);
    if (count != GLUE_TWO_ARG) goto OnErrorExit;

    AfbHandleT* glue= nodeCapsule_GetPointer(nodeTuple_GetItem(argsP,0), GLUE_AFB_UID);
    if (!glue || glue->magic != GLUE_API_MAGIC) goto OnErrorExit;

    // create a new binder event
    errorMsg = "evtconf={'uid':'xxx', 'name':'yyyy'}";
    AfbHandleT *nodeEvt = calloc(1, sizeof(AfbHandleT));
    nodeEvt->magic = GLUE_EVT_MAGIC;
    nodeEvt->evt.apiv4= GlueGetApi(glue);

    nodeEvt->evt.configR = nodeTuple_GetItem(argsP,1);
    if (!nodeDict_Check(nodeEvt->evt.configR)) goto OnErrorExit;
    node_IncRef(nodeEvt->evt.configR);

    slotP= nodeDict_GetItemString(nodeEvt->evt.configR, "uid");
    if (!slotP || !nodeUnicode_Check(slotP)) goto OnErrorExit;
    nodeEvt->evt.uid= nodeUnicode_AsUTF8(slotP);

    slotP= nodeDict_GetItemString(nodeEvt->evt.configR, "name");
    if (!slotP) nodeEvt->evt.name = nodeEvt->evt.uid;
    else {
        if (!nodeUnicode_Check(slotP)) goto OnErrorExit;
        nodeEvt->evt.name= nodeUnicode_AsUTF8(slotP);
    }

    err= afb_api_new_event(glue->api.afb, nodeEvt->evt.name, &nodeEvt->evt.afb);
    if (err)
    {
        errorMsg = "(hoops) afb-afb_api_new_event fail";
        goto OnErrorExit;
    }

    // push event handler as a node opaque handle
    return nodeCapsule_New(nodeEvt, GLUE_AFB_UID, NULL);

OnErrorExit:
        GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueVerbAdd(napi_env env, napi_callback_info info)
{
    const char *errorMsg = "syntax: addverb(api, config, context)";

    long count = nodeTuple_GET_SIZE(argsP);
    if (count != GLUE_THREE_ARG) goto OnErrorExit;

    AfbHandleT* glue= nodeCapsule_GetPointer(nodeTuple_GetItem(argsP,0), GLUE_AFB_UID);
    if (!glue || glue->magic != GLUE_API_MAGIC) goto OnErrorExit;

    json_object *configJ= nodeObjToJson (nodeTuple_GetItem(argsP,1));
    if (!configJ) goto OnErrorExit;

    napi_value userdataP= nodeTuple_GetItem(argsP,2);
    if (userdataP) node_IncRef(userdataP);

    errorMsg= AfbAddOneVerb (afbMain->binder.afb, glue->api.afb, configJ, GlueVerbCb, userdataP);
    if (errorMsg) goto OnErrorExit;

    GLUE_RETURN_NULL;

OnErrorExit:
        GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueSetLoa(napi_env env, napi_callback_info info)
{
    const char *errorMsg = "syntax: setloa(rqt, newloa)";
    long count = nodeTuple_GET_SIZE(argsP);
    if (count != GLUE_TWO_ARG) goto OnErrorExit;

    AfbHandleT* glue= nodeCapsule_GetPointer(nodeTuple_GetItem(argsP,0), GLUE_AFB_UID);
    if (!glue && glue->magic != GLUE_RQT_MAGIC) goto OnErrorExit;

    int loa= (int)nodeLong_AsLong(nodeTuple_GetItem(argsP,1));
    if (loa < 0) goto OnErrorExit;

    int err= afb_req_session_set_LOA(glue->rqt.afb, loa);
    if (err < 0) {
        errorMsg="Invalid Rqt Session";
        goto OnErrorExit;
    }

    GLUE_RETURN_NULL;

OnErrorExit:
        GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueTimerAddref(napi_env env, napi_callback_info info) {
    const char *errorMsg="syntax: timeraddref(handle)";
    long count = nodeTuple_GET_SIZE(argsP);
    if (count != GLUE_ONE_ARG) goto OnErrorExit;

    AfbHandleT* glue= nodeCapsule_GetPointer(nodeTuple_GetItem(argsP,0), GLUE_AFB_UID);
    if (!glue || glue->magic != GLUE_TIMER_MAGIC) goto OnErrorExit;

    afb_timer_addref (glue->timer.afb);
    node_IncRef(glue->timer.configR);
    glue->timer.usage++;
    GLUE_RETURN_NULL;

OnErrorExit:
        GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueTimerUnref(napi_env env, napi_callback_info info) {
    const char *errorMsg="syntax: timerunref(handle)";
    long count = nodeTuple_GET_SIZE(argsP);
    if (count != GLUE_ONE_ARG) goto OnErrorExit;

    AfbHandleT* glue= nodeCapsule_GetPointer(nodeTuple_GetItem(argsP,0), GLUE_AFB_UID);
    if (!glue || glue->magic != GLUE_TIMER_MAGIC) goto OnErrorExit;

    GlueTimerClear(glue);
    GLUE_RETURN_NULL;

OnErrorExit:
        GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueEventHandler(napi_env env, napi_callback_info info)
{
    napi_value slotP;
    const char *errorMsg = "syntax: evthandler(handle, config, userdata)";
    long count = nodeTuple_GET_SIZE(argsP);
    if (count != GLUE_THREE_ARG) goto OnErrorExit;

    AfbHandleT* glue= nodeCapsule_GetPointer(nodeTuple_GetItem(argsP,0), GLUE_AFB_UID);
    if (!glue) goto OnErrorExit;

    // retreive API from lua handle
    afb_api_t afbApi= GlueGetApi(glue);
    if (!afbApi) goto OnErrorExit;

    AfbHandleT *handle= calloc(1, sizeof(AfbHandleT));
    handle->magic= GLUE_HANDLER_MAGIC;
    handle->handler.apiv4= afbApi;

    handle->handler.configR = nodeTuple_GetItem(argsP,1);
    if (!nodeDict_Check(handle->handler.configR)) goto OnErrorExit;
    node_IncRef(handle->handler.configR);

    errorMsg= "config={'uid':'xxx','pattern':'yyy','callback':'zzz'}";
    slotP= nodeDict_GetItemString(handle->handler.configR, "uid");
    if (!slotP || !nodeUnicode_Check(slotP)) goto OnErrorExit;
    handle->handler.uid= nodeUnicode_AsUTF8(slotP);

    slotP= nodeDict_GetItemString(handle->handler.configR, "pattern");
    if (!slotP || !nodeUnicode_Check(slotP)) goto OnErrorExit;
    const char *pattern= nodeUnicode_AsUTF8(slotP);

    handle->handler.callbackP= nodeDict_GetItemString(handle->handler.configR, "callback");
    if (!slotP || !nodeCallable_Check(handle->handler.callbackP)) goto OnErrorExit;
    node_IncRef(handle->handler.callbackP);

    handle->handler.userdataP = nodeTuple_GetItem(argsP,2);
    if (handle->handler.userdataP) node_IncRef(handle->handler.userdataP);

    errorMsg= AfbAddOneEvent (afbApi, handle->handler.uid, pattern, GlueEvtHandlerCb, handle);
    if (errorMsg) goto OnErrorExit;

    GLUE_RETURN_NULL;

OnErrorExit:
        GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueTimerNew(napi_env env, napi_callback_info info)
{
    const char *errorMsg = "syntax: timernew(api, config, context)";
    AfbHandleT* glue= nodeCapsule_GetPointer(nodeTuple_GetItem(argsP,0), GLUE_AFB_UID);
    if (!glue) goto OnErrorExit;

    AfbHandleT *handle= (AfbHandleT *)calloc(1, sizeof(AfbHandleT));
    handle->magic = GLUE_TIMER_MAGIC;
    handle->timer.configR = nodeTuple_GetItem(argsP,1);
    if (!nodeDict_Check(handle->timer.configR)) goto OnErrorExit;
    node_IncRef(handle->timer.configR);

    handle->timer.userdataP =nodeTuple_GetItem(argsP,2);
    if (handle->timer.userdataP != node_None) node_IncRef(handle->timer.userdataP);

    // parse config
    napi_value slotP;
    errorMsg= "timerconfig= {'uid':'xxx', 'callback': MyCallback, 'period': timer(ms), 'count': 0-xx}";
    slotP= nodeDict_GetItemString(handle->timer.configR, "uid");
    if (!slotP || !nodeUnicode_Check(slotP)) goto OnErrorExit;
    handle->timer.uid= nodeUnicode_AsUTF8(slotP);

    handle->timer.callbackP= nodeDict_GetItemString(handle->timer.configR, "callback");
    if (!handle->timer.callbackP || !nodeCallable_Check(handle->timer.callbackP)) goto OnErrorExit;

    slotP= nodeDict_GetItemString(handle->timer.configR, "period");
    if (!slotP || !nodeLong_Check(slotP)) goto OnErrorExit;
    long period= nodeLong_AsLong(slotP);
    if (period <= 0) goto OnErrorExit;


    slotP= nodeDict_GetItemString(handle->timer.configR, "count");
    if (!slotP || !nodeLong_Check(slotP)) goto OnErrorExit;
    long count= nodeLong_AsLong(slotP);
    if (period < 0) goto OnErrorExit;

    int err= afb_timer_create (&handle->timer.afb, 0, 0, 0, (int)count, (int)period, 0, GlueTimerCb, (void*)handle, 0);
    if (err) {
        errorMsg= "(hoops) afb_timer_create fail";
        goto OnErrorExit;
    }

    return nodeCapsule_New(handle, GLUE_AFB_UID, NULL);

OnErrorExit:
        GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueSchedWait(napi_env env, napi_callback_info info)
{
    AfbHandleT *lock=NULL;
    int err;

    long count = nodeTuple_GET_SIZE(argsP);
    const char *errorMsg = "schedwait(handle, timeout, callback, [context])";
    if (count < GLUE_THREE_ARG) goto OnErrorExit;

    AfbHandleT* glue= nodeCapsule_GetPointer(nodeTuple_GetItem(argsP,0), GLUE_AFB_UID);
    if (!glue) goto OnErrorExit;

    long timeout= nodeLong_AsLong(nodeTuple_GetItem(argsP,1));
    if (timeout < 0) goto OnErrorExit;

    napi_value callbackP= nodeTuple_GetItem(argsP,2);
    if (!nodeCallable_Check(callbackP)) goto OnErrorExit;
    node_IncRef(callbackP);

    napi_value userdataP= nodeTuple_GetItem(argsP,3);
    if (userdataP != node_None) node_IncRef(userdataP);

    lock= calloc (1, sizeof(AfbHandleT));
    lock->magic= GLUE_LOCK_MAGIC;
    lock->lock.apiv4= GlueGetApi(glue);
    lock->lock.callbackP= callbackP;
    lock->lock.userdataP= userdataP;

    err= afb_sched_enter(NULL, (int)timeout, GlueSchedWaitCb, lock);
    if (err) {
        errorMsg= "fail to register afb_sched_enter";
        goto OnErrorExit;
    }

    // free lock handle
    node_DecRef(lock->lock.callbackP);
    if (lock->lock.userdataP) node_DecRef(lock->lock.userdataP);
    long statusN= lock->lock.statusN;
    free (lock);

    return nodeLong_FromLong(statusN);

OnErrorExit:
    if (lock) {
        node_DecRef(lock->lock.callbackP);
        if (lock->lock.userdataP) node_DecRef(lock->lock.userdataP);
        free (lock);
    }
        GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueSchedCancel(napi_env env, napi_callback_info info)
{
    long count = nodeTuple_GET_SIZE(argsP);
    const char *errorMsg = "syntax: schedcancel(jobid)";
    if (count != GLUE_ONE_ARG) goto OnErrorExit;

    long jobid= nodeLong_AsLong(nodeTuple_GetItem(argsP,0));
    if (jobid <= 0) goto OnErrorExit;

    int err= afb_jobs_abort((int)jobid);
    if (err) goto OnErrorExit;
    GLUE_RETURN_NULL;

OnErrorExit:
        GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueSchedPost(napi_env env, napi_callback_info info)
{
    GlueHandleCbT *glue=calloc (1, sizeof(GlueHandleCbT));
    glue->magic = GLUE_SCHED_MAGIC;

    long count = nodeTuple_GET_SIZE(argsP);
    const char *errorMsg = "syntax: schedpost(glue, timeout, callback [,userdata])";
    if (count != node_FOUR_ARG) goto OnErrorExit;

    glue->handle= nodeCapsule_GetPointer(nodeTuple_GetItem(argsP,0), GLUE_AFB_UID);
    if (!glue->handle) goto OnErrorExit;

    long timeout= nodeLong_AsLong(nodeTuple_GetItem(argsP,1));
    if (timeout <= 0) goto OnErrorExit;

    glue->callbackP= nodeTuple_GetItem(argsP,2);
    if (!nodeCallable_Check(glue->callbackP)) {
        errorMsg="syntax: callback should be a valid callable function";
        goto OnErrorExit;
    }
    node_IncRef(glue->callbackP);

    glue->userdataP =nodeTuple_GetItem(argsP,3);
    if (glue->userdataP != node_None) node_IncRef(glue->userdataP);

    // ms delay for OnTimerCB (timeout is dynamic and depends on CURLOPT_LOW_SPEED_TIME)
    int jobid= afb_sched_post_job (NULL , timeout,  0 ,GlueSchedTimeoutCb, glue, Afb_Sched_Mode_Start);
	if (jobid <= 0) goto OnErrorExit;

    return nodeLong_FromLong(jobid);

OnErrorExit:
        GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueSchedUnlock(napi_env env, napi_callback_info info)
{
    int err;
    const char *errorMsg = "syntax: schedunlock(handle, lock, statusN)";
    long count = nodeTuple_GET_SIZE(argsP);
    if (count !=  GLUE_THREE_ARG) goto OnErrorExit;

    AfbHandleT* glue= nodeCapsule_GetPointer(nodeTuple_GetItem(argsP,0), GLUE_AFB_UID);
    if (!glue) goto OnErrorExit;

    AfbHandleT *lock= nodeCapsule_GetPointer(nodeTuple_GetItem(argsP,1), GLUE_AFB_UID);
    if (!lock || lock->magic != GLUE_LOCK_MAGIC) goto OnErrorExit;

    lock->lock.statusN= nodeLong_AsLong(nodeTuple_GetItem(argsP,2));

    err= afb_sched_leave(lock->lock.afb);
    if (err) {
        errorMsg= "fail to register afb_sched_enter";
        goto OnErrorExit;
    }

    GLUE_RETURN_NULL;

OnErrorExit:
    GLUE_AFB_ERROR(glue, errorMsg);
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueExit(napi_env env, napi_callback_info info)
{
    const char *errorMsg= "syntax: exit(handle, statusN)";
    long count = nodeTuple_GET_SIZE(argsP);
    if (count != GLUE_TWO_ARG) goto OnErrorExit;

    AfbHandleT* glue= nodeCapsule_GetPointer(nodeTuple_GetItem(argsP,0), GLUE_AFB_UID);
    if (!glue) goto OnErrorExit;

    long exitCode= nodeLong_AsLong(nodeTuple_GetItem(argsP,1));

    AfbBinderExit(afbMain->binder.afb, (int)exitCode);
    GLUE_RETURN_NULL;

OnErrorExit:
    GLUE_AFB_ERROR(glue, errorMsg);
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueClientInfo(napi_env env, napi_callback_info info)
{
    napi_value resultN;
    const char *errorMsg = "syntax: clientinfo(rqt, ['key'])";
    long count = nodeTuple_GET_SIZE(argsP);
    if (count != GLUE_ONE_ARG && count != GLUE_TWO_ARG) goto OnErrorExit;

    AfbHandleT* glue= nodeCapsule_GetPointer(nodeTuple_GetItem(argsP,0), GLUE_AFB_UID);
    if (!glue || glue->magic != GLUE_RQT_MAGIC) goto OnErrorExit;

    napi_value keyP= nodeTuple_GetItem(argsP,1);
    if (keyP && !nodeUnicode_Check(keyP)) goto OnErrorExit;

    json_object *clientJ= afb_req_get_client_info(glue->rqt.afb);
    if (!clientJ) {
        errorMsg= "(hoops) afb_req_get_client_info no session info";
        goto OnErrorExit;
    }

    if (!keyP) {
        resultN = jsonTonodeObj(clientJ);
    } else {
        json_object *keyJ= json_object_object_get(clientJ, nodeUnicode_AsUTF8(keyP));
        if (!keyJ) {
            errorMsg= "unknown client info key";
            goto OnErrorExit;
        }
        resultN = jsonTonodeObj(keyJ);
    }
    return resultN;

OnErrorExit:
    GLUE_AFB_ERROR(glue, errorMsg);
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
    GLUE_RETURN_NULL;
}

*/

// pop afb waiting event and process them
static void GlueOnPoolUvCb(uv_poll_t* handle, int status, int events) {
    afb_ev_mgr_wait_and_dispatch(0);
    for (struct afb_job *job= afb_jobs_dequeue(0); job; job= afb_jobs_dequeue(0)) {
        afb_jobs_run(job);
    }
    afb_ev_mgr_prepare();
}

// this routine execute within mainloop context when binder is ready to go
static napi_value GlueMainLoop(napi_env env, napi_callback_info info)
{
    const char *errorCode, *errorMsg="syntax: mainloop([callback])";
    napi_value resultN;
    napi_status statusN;
    int statusU;
    uv_poll_t poolU;
    uv_loop_t *loopU;

    // get argument
    size_t argc= GLUE_ONE_ARG;
    napi_value args[GLUE_ONE_ARG];
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc != GLUE_ONE_ARG) goto OnErrorExit;

    napi_valuetype typeN;
	statusN = napi_typeof(env, args[0], &typeN);
    if (typeN != napi_function) {
        errorCode="Invalid Callback";
        goto OnErrorExit;
    }

    // create a reference to prevent callback from beeing deleted
    napi_ref callbackR;
    statusN= napi_create_reference(env, args[0], 1, &callbackR);
    if (statusN != napi_ok) goto OnErrorExit;

    // main loop only return when binder startup func return statusN!=0
    GLUE_AFB_NOTICE(afbMain, "Entering binder mainloop");
    statusN = AfbBinderEnter(afbMain->binder.afb, callbackR, GlueStartupCb, afbMain);
    afb_ev_mgr_prepare();

    // map afb loop back with node libuv
    int afbfd = afb_ev_mgr_get_fd();
	if (afbfd < 0) goto OnErrorExit;

    statusN = napi_get_uv_event_loop(env, &loopU);
	if (statusN != napi_ok) goto OnErrorExit;

 	statusU = uv_poll_init(loopU, &poolU, afbfd);
	if (statusU < 0) goto OnErrorExit;

	statusU = uv_poll_start(&poolU, UV_READABLE, GlueOnPoolUvCb);
	if (statusU < 0) goto OnErrorExit;

    // Fulup TBD check option
    statusU= uv_run(loopU, 1000000*360);
    if (statusU < 0) goto OnErrorExit;

    statusN= napi_create_int64 (env, (int64_t)statusN, &resultN);
    if (statusN != napi_ok) goto OnErrorExit;
    return resultN;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode,errorMsg);
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

    json_object *configJ= napiValuetoJsonc(env, configN);
    if (!configJ) {
        errorCode="Invalid-Config";
        goto OnErrorExit;
    }

    // allocate afbMain glue and parse config to jsonC
    AfbHandleT *glue= calloc(1, sizeof(AfbHandleT));
    glue->magic= GLUE_API_MAGIC;
    glue->env=env;

    // keep track of config reference within glue handle    
    statusN= napi_create_reference(env, configN, 1, &glue->api.configR);
    if (statusN != napi_ok) goto OnErrorExit;

    const char *afbApiUri = NULL;
    wrap_json_unpack(configJ, "{s?s}", "uri", &afbApiUri);
    if (afbApiUri)
    {
        // imported shadow api
        errorMsg = AfbApiImport(afbMain->binder.afb, configJ);
    } 
    else
    {
        bool hasProperty;
        // create a reference to prevent callback from beeing deleted
        statusN= napi_has_named_property (glue->env, configN, "control" , &hasProperty);
        if (hasProperty) {
            napi_value callbackN;
            napi_ref callbackR;  
            napi_get_named_property(glue->env, configN, "control", &callbackN);
            if (napiGetType(env, callbackN) != napi_function) {
                errorMsg="APi control func defined but but callable";
                goto OnErrorExit;
            }
            statusN= napi_create_reference(env, callbackN, 1, &callbackR);
            if (statusN != napi_ok) goto OnErrorExit;
            errorMsg = AfbApiCreate(afbMain->binder.afb, configJ, &glue->api.afb, GlueCtrlCb, GlueInfoCb, GlueAsyncVerbCb, GlueEvtHandlerCb, glue);
            } 
            else
            {
                errorMsg = AfbApiCreate(afbMain->binder.afb, configJ, &glue->api.afb, NULL, GlueInfoCb, GlueAsyncVerbCb, GlueEvtHandlerCb, glue);
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

static napi_value GlueBinderConf(napi_env env, napi_callback_info info)
{
    const char *errorCode=NULL, *errorMsg="syntax: binder(config)";
    napi_value resultN;
    napi_status statusN;
    uint32_t unused=0;

    if (afbMain) {
        errorMsg="(hoops) binder(config) already loaded";
        goto OnErrorExit;
    }

    // allocate afbMain glue and parse config to jsonC
    afbMain= calloc(1, sizeof(AfbHandleT));
    afbMain->magic= GLUE_BINDER_MAGIC;
    afbMain->env=env;

    // get arguments
    size_t argc= GLUE_ONE_ARG;
    napi_value args[GLUE_ONE_ARG];
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc != GLUE_ONE_ARG) goto OnErrorExit;

    json_object *configJ= napiValuetoJsonc(env, args[0]);
    if (!configJ) {
        errorCode="Invalid-Config";
        goto OnErrorExit;
    }

    errorMsg= AfbBinderConfig(configJ, &afbMain->binder.afb, afbMain);
    if (errorMsg) goto OnErrorExit;

    // keep track of napi config object
    statusN= napi_create_reference(env, args[0], 1, &afbMain->binder.configR);
    if (statusN != napi_ok) goto OnErrorExit;

    // return afbMain glue as a nodejs capcule glue
    statusN= napi_create_external (env, afbMain, NULL, NULL, &resultN);
    if (statusN != napi_ok) goto OnErrorExit;
    return resultN;

OnErrorExit:
    napi_throw_type_error (env, errorCode, errorMsg);
    unused ++; // make compiler happy
    return NULL;
}

static napi_value GlueGetConfig(napi_env env, napi_callback_info info)
{
    const char *errorMsg = "syntax: config(handle[,key])";
    napi_status statusN;
    napi_value configN;
    napi_ref configR;

    // get arguments
    size_t argc= GLUE_TWO_ARG;
    napi_value resultN, args[GLUE_TWO_ARG];
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || (argc != GLUE_ONE_ARG && argc != GLUE_TWO_ARG)) goto OnErrorExit;

    // get afb glue handle from 1st argument
    AfbHandleT *glue;
    statusN= napi_get_value_external (env, args[0], (void**)&glue);
    if (statusN != napi_ok) goto OnErrorExit;

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
        configR = glue->evt.configR;
        break;
    default:
        errorMsg = "GlueGetConfig: unsupported node/afb handle";
        goto OnErrorExit;
    }

    if (!configR) {
        errorMsg= "nodeHandle config missing";
        goto OnErrorExit;
    }

    statusN= napi_get_reference_value(glue->env, configR, &configN);
    if (statusN != napi_ok) {
        errorMsg = "ConfigRef no attached value";
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
        bool isPresent;
        napi_value keyN;
        statusN= napi_has_named_property (glue->env, configN, key, &isPresent);
        if (statusN != napi_ok || !isPresent) {
            errorMsg = "GlueGetConfig: unknown config key";
            goto OnErrorExit;
        }

        napi_get_named_property(glue->env, configN, key, &keyN);
        resultN= keyN;
    }
    return resultN;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "error=%s", errorMsg);
    napi_throw_error(env, NULL, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GlueRespond(napi_env env, napi_callback_info info)
{
    const char *errorCode="internal-error", *errorMsg = "syntax: response(rqt, statusN, [arg1 ... argn])";
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
    AfbHandleT *glue;
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
    GlueReply(glue, status, count, reply);
    GLUE_RETURN_NULL;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "code=%s error=%s", errorCode, errorMsg);
    napi_throw_error(env, errorCode, errorMsg);
    GLUE_RETURN_NULL;
}

static napi_value GluePingtest(napi_env env, napi_callback_info info)
{
    napi_status statusN;
    napi_value  resultN;
    static int64_t count=0;
    long tid= pthread_self();
    fprintf (stderr, "From GluePingtest count=%ld tid=%ld\n", count, tid);
    statusN= napi_create_int64 (env, count++, &resultN);
    assert(statusN == napi_ok);

    return resultN;
}

/*
static nodeMethodDef MethodsDef[] = {
    {"config"        , GlueGetConfig          , METH_VARARGS, "Return glue handle full/partial config"},
    {"binding"       , GlueBindingLoad      , METH_VARARGS, "Load binding an expose corresponding api/verbs"},
    {"callasync"     , GlueAsyncCall        , METH_VARARGS, "AFB asynchronous subcall"},
    {"callsync"      , GlueSyncCall         , METH_VARARGS, "AFB synchronous subcall"},
    {"verbadd"       , GlueVerbAdd          , METH_VARARGS, "Add a verb to a non sealed API"},
    {"evtsubscribe"  , GlueEventSubscribe   , METH_VARARGS, "Subscribe to event"},
    {"evtunsubscribe", GlueEventUnsubscribe , METH_VARARGS, "Unsubscribe to event"},
    {"evthandler"    , GlueEventHandler     , METH_VARARGS, "Register event callback handler"},
    {"evtnew"        , GlueEventNew         , METH_VARARGS, "Create a new event"},
    {"evtpush"       , GlueEventPush        , METH_VARARGS, "Push a given event"},
    {"timerunref"    , GlueTimerUnref       , METH_VARARGS, "Unref existing timer"},
    {"timeraddref"   , GlueTimerAddref      , METH_VARARGS, "Addref to existing timer"},
    {"timernew"      , GlueTimerNew         , METH_VARARGS, "Create a new timer"},
    {"setloa"        , GlueSetLoa           , METH_VARARGS, "Set LOA (LevelOfAssurance)"},
    {"schedwait"     , GlueSchedWait        , METH_VARARGS, "Register a mainloop waiting lock"},
    {"schedunlock"   , GlueSchedUnlock      , METH_VARARGS, "Unlock schedwait"},
    {"schedpost"     , GlueSchedPost        , METH_VARARGS, "Post a job after timeout(ms)"},
    {"schedcancel"   , GlueSchedCancel      , METH_VARARGS, "Cancel a schedpost timer"},
    {"clientinfo"    , GlueClientInfo       , METH_VARARGS, "Return seesion info about client"},
    {"exit"          , GlueExit             , METH_VARARGS, "Exit binder with statusN"},

    {NULL}  // sentinel
};
*/

// NodeJS NAPI module initialisation
static napi_value NapiGlueInit(napi_env env, napi_value exports) {
  napi_status statusN;
  napi_property_descriptor descs[] = {
    {"info"    , NULL, GluePrintInfo    , NULL, NULL, NULL, napi_default, NULL },
    {"error"   , NULL, GluePrintError   , NULL, NULL, NULL, napi_default, NULL },
    {"debug"   , NULL, GluePrintDebug   , NULL, NULL, NULL, napi_default, NULL },
    {"warning" , NULL, GluePrintWarning , NULL, NULL, NULL, napi_default, NULL },
    {"notice"  , NULL, GluePrintNotice  , NULL, NULL, NULL, napi_default, NULL },
    {"ping"    , NULL, GluePingtest     , NULL, NULL, NULL, napi_default, NULL },
    {"binder"  , NULL, GlueBinderConf   , NULL, NULL, NULL, napi_default, NULL },
    {"apiadd"  , NULL, GlueApiCreate    , NULL, NULL, NULL, napi_default, NULL },
    {"mainloop", NULL, GlueMainLoop     , NULL, NULL, NULL, napi_default, NULL },
    {"config"  , NULL, GlueGetConfig    , NULL, NULL, NULL, napi_default, NULL },
    {"reply"   , NULL, GlueRespond      , NULL, NULL, NULL, napi_default, NULL },
  };

  statusN = napi_define_properties(env, exports, sizeof(descs)/sizeof(napi_property_descriptor), descs);
  assert(statusN == napi_ok);
  return exports;
}
NAPI_MODULE(TARGET_NAME, NapiGlueInit)