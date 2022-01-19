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
 * reference: https://stackoverflow.com/questions/58960713/how-to-use-napi-threadsafe-function-for-nodejs-native-addon
 */

#include <node_api.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <wrap-json.h>

#include <glue-afb.h>
#include <glue-utils.h>

#include "node-afb.h"
#include "node-utils.h"
#include "node-callbacks.h"

/*

void GlueFreeHandleCb(napi_value capculeP) {
   AfbHandleT *handle=   nodeCapsule_GetPointer(capculeP, GLUE_AFB_UID);
   if (!handle) goto OnErrorExit;

    switch (handle->magic) {
        case GLUE_API_MAGIC:
            break;
        case GLUE_LOCK_MAGIC:
            if (handle->lock.userdataP) node_DecRef(handle->lock.userdataP);
            if (handle->lock.callbackP) node_DecRef(handle->lock.callbackP);
        default:
            goto OnErrorExit;

    }
    free (handle);

OnErrorExit:
    ERROR ("try to release a non glue handle");
}

void GlueSchedWaitCb (int signum, void *userdata, struct afb_sched_lock *afbLock) {

    const char *errorMsg = NULL;
    AfbHandleT *ctx= (AfbHandleT*)userdata;
    assert (ctx && ctx->magic == GLUE_LOCK_MAGIC);
    ctx->lock.afb= afbLock;

    // create a fake API for waitCB
    AfbHandleT *glue = calloc(1,sizeof(AfbHandleT));
    glue->magic= GLUE_API_MAGIC;
    glue->api.afb=ctx->lock.apiv4;

    // prepare calling argument list
    napi_value argsP= nodeTuple_New(GLUE_THREE_ARG);
    nodeTuple_SetItem (argsP, 0, nodeCapsule_New(glue, GLUE_AFB_UID, GlueFreeHandleCb));
    nodeTuple_SetItem (argsP, 1, nodeCapsule_New(ctx, GLUE_AFB_UID, NULL));
    nodeTuple_SetItem (argsP, 2, ctx->lock.userdataP);

    napi_value resultP= nodeObject_Call (ctx->lock.callbackP, argsP, NULL);
    if (!resultP) {
        errorMsg="async callback fail";
        goto OnErrorExit;
    }
    return;

OnErrorExit:
    GLUE_AFB_ERROR(ctx, errorMsg);
}

void GlueSchedTimeoutCb (int signum, void *userdata) {
    const char *errorMsg = NULL;
    GlueHandleCbT *ctx= (GlueHandleCbT*)userdata;
    assert (ctx && ctx->magic == GLUE_SCHED_MAGIC);

    // timer not cancel
    if (signum != SIGABRT) {
        // prepare calling argument list
        napi_value argsP= nodeTuple_New(GLUE_TWO_ARG);
        nodeTuple_SetItem (argsP, 0, nodeCapsule_New(ctx->handle, GLUE_AFB_UID, NULL));
        nodeTuple_SetItem (argsP, 1, ctx->userdataP);

        napi_value resultP= nodeObject_Call (ctx->callbackP, argsP, NULL);
        if (!resultP) {
            errorMsg="async callback fail";
            goto OnErrorExit;
        }
    }
    node_DECREF (ctx->callbackP);
    if (ctx->userdataP) node_DECREF (ctx->userdataP);
    free (ctx);
    return;

OnErrorExit:
    GLUE_AFB_ERROR(ctx->handle, errorMsg);
    node_DECREF (ctx->callbackP);
    if (ctx->userdataP) node_DECREF (ctx->userdataP);
    free (ctx);
}


static void GluePcallFunc (void *userdata, int statusN, unsigned nreplies, afb_data_t const replies[]) {
    nodeAsyncCtxT *ctx= (nodeAsyncCtxT*) userdata;
    const char *errorMsg = NULL;

    // subcall was refused
    if (AFB_IS_BINDER_ERRNO(statusN)) {
        errorMsg= afb_error_text(statusN);
        goto OnErrorExit;
    }

    // prepare calling argument list
    napi_value argsP= nodeTuple_New(nreplies+GLUE_THREE_ARG);
    nodeTuple_SetItem (argsP, 0, nodeCapsule_New(ctx->glue, GLUE_AFB_UID, NULL));
    nodeTuple_SetItem (argsP, 1, nodeLong_FromLong((long)statusN));
    nodeTuple_SetItem (argsP, 2, ctx->userdataP);

    errorMsg= nodePushAfbReply (argsP, 3, nreplies, replies);
    if (errorMsg) goto OnErrorExit;

    napi_value resultP= nodeObject_Call (ctx->callbackP, argsP, NULL);
    if (!resultP) {
        errorMsg="async callback fail";
        goto OnErrorExit;
    }

    // free afb request and glue
    if (ctx->userdataP != node_None) node_DECREF(ctx->userdataP);
    node_DECREF(ctx->callbackP);
    node_DECREF(argsP);
    free (ctx);
    return;

OnErrorExit: {
    afb_data_t reply;
    json_object *errorJ = nodeJsonDbg(errorMsg);
    GLUE_AFB_WARNING(ctx->glue, "nodejs=%s", json_object_get_string(errorJ));
    afb_create_data_raw(&reply, AFB_PREDEFINED_TYPE_JSON_C, errorJ, 0, (void *)json_object_put, errorJ);
    GlueReply(ctx->glue, -1, 1, &reply);
    }
}

void GlueApiSubcallCb (void *userdata, int statusN, unsigned nreplies, afb_data_t const replies[], afb_api_t api) {
    GluePcallFunc (userdata, statusN, nreplies, replies);
}

void GlueRqtSubcallCb (void *userdata, int statusN, unsigned nreplies, afb_data_t const replies[], afb_req_t req) {
    GluePcallFunc (userdata, statusN, nreplies, replies);
}

void GlueTimerClear(AfbHandleT *glue) {

    afb_timer_unref (glue->timer.afb);
    node_DecRef(glue->timer.configP);
    glue->timer.usage--;

    // free timer luaState and ctx
    if (glue->timer.usage <= 0) {
       glue->magic=0;
       node_DecRef(glue->timer.userdataP);
       node_DecRef(glue->timer.callbackP);
       free(glue);
    }
}

void GlueTimerCb (afb_timer_x4_t timer, void *userdata, int decount) {
    const char *errorMsg=NULL;
    AfbHandleT *ctx= (AfbHandleT*)userdata;
    assert (ctx && ctx->magic == GLUE_TIMER_MAGIC);
    long statusN=0;

    napi_value argsP= nodeTuple_New(GLUE_TWO_ARG);
    nodeTuple_SetItem (argsP, 0, nodeCapsule_New(ctx, GLUE_AFB_UID, NULL));
    nodeTuple_SetItem (argsP, 1, ctx->timer.userdataP);
    if (ctx->timer.userdataP) node_IncRef(ctx->timer.userdataP);

    napi_value resultP= nodeObject_Call (ctx->timer.callbackP, argsP, NULL);
    if (!resultP) {
        errorMsg="timer callback fail";
        goto OnErrorExit;
    }
    if (resultP != node_None) {
        if (!nodeLong_Check(resultP)) {
            errorMsg="TimerCB returned statusN should be integer";
            goto OnErrorExit;
        }
        statusN= nodeLong_AsLong(resultP);
    }

    // check for last timer interation
    if (decount == 1 || statusN != 0) goto OnUnrefExit;

    return;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, errorMsg);
    nodeErr_SetString(nodeExc_RuntimeError, errorMsg);
OnUnrefExit:
    GlueTimerClear(ctx);
}
*/

typedef struct {
    void *magic;
    afb_req_t afbRqt;
    AfbHandleT *glue;
    unsigned nparams;
    afb_data_t const *params;
    napi_async_work workerN;
    AfbVcbDataT *vcbdata;
} GlueAsyncDataT;

void GlueEvtHandlerCb (void *userdata, const char *evtName, unsigned nparams, afb_data_x4_t const params[], afb_api_t api) {
    const char *errorMsg = NULL;
    afb_data_t argsD[nparams];
    json_object *argsJ[nparams];
    size_t argsCount= 0;
    napi_value argsValN[1+nparams];
    napi_value resultN;
    napi_status statusN;
    int err;

    AfbHandleT *glue= (AfbHandleT*) afb_api_get_userdata(api);
    assert (glue);

    // on first call we compile configJ to boost following py api/verb calls
    AfbVcbDataT *vcbData= userdata;
    if (vcbData->magic != (void*)AfbAddVerbs) {
        errorMsg = "(hoops) event invalid vcbData handle";
        goto OnErrorExit;
    }

    // on first call we need to retreive original callback object from configJ
    if (!vcbData->callback) {
        json_object *callbackJ=json_object_object_get(vcbData->configJ, "callback");
        if (!callbackJ) {
            errorMsg = "(hoops) event no callback defined";
            goto OnErrorExit;
        }

        // get callback reference and attach it to afb-vcbdata
        vcbData->callback = json_object_get_userdata (callbackJ);
        if (!vcbData->callback) {
            errorMsg = "(hoops) event no callback attached";
            goto OnErrorExit;
        }
    }

    // get callback object
    napi_value callbackN, glueN, globalN;
    napi_ref callbackR= (napi_ref) vcbData->callback;
    statusN= napi_get_reference_value(glue->env, callbackR, &callbackN);
    if (statusN != napi_ok) goto OnErrorExit;

    statusN= napi_create_external (glue->env, glue, NULL, NULL, &glueN);
    if (statusN != napi_ok) goto OnErrorExit;

    statusN=  napi_get_global(glue->env, &globalN);
    if (statusN != napi_ok) goto OnErrorExit;


    // retreive input arguments and convert them to json
    for (int idx = 0; idx < nparams; idx++)
    {
        
        err = afb_data_convert(params[idx], &afb_type_predefined_json_c, &argsD[idx]);
        if (err)
        {
            errorMsg = "fail converting input params to jsonc";
            goto OnErrorExit;
        }
        argsCount ++;
        argsJ[idx]  = afb_data_ro_pointer(argsD[idx]);
        argsValN[argsCount]= napiJsoncToValue(glue->env, argsJ[idx]);
    }

    statusN= napi_call_function(glue->env, globalN, callbackN, argsCount, argsValN, &resultN);
    switch (statusN) {
        napi_value errorN;
        case napi_ok:
            break;
        case napi_pending_exception:
            statusN= napi_get_and_clear_last_exception(glue->env, &errorN);
            napi_throw(glue->env, errorN);
        default:
            goto OnErrorExit;
    }

    // free json_object
    for (int idx=0; idx < nparams; idx++) json_object_put(argsJ[idx]);
    return;

OnErrorExit:
    GLUE_AFB_ERROR(glue, errorMsg);
    return;
}


// this function is responsible to retreive argument from main thread to pass them over threadsafe callback
static void  GlueAsyncArgsCb (napi_env env, napi_value js_cb, void* context, void* userdata) {
    (void)context; // unused
    afb_req_t afbRqt;
    unsigned nparams;
    afb_data_t const *params;

    GlueAsyncDataT *data= (GlueAsyncDataT*)userdata;
    assert (data && data->magic == napi_create_threadsafe_function);

    // create a similar environement to synchronous call
    afbRqt = data->afbRqt;
    nparams= data->nparams;
    params = data->params;
    GlueVerbCb(afbRqt, nparams, params);
    free(data);
}

// this function run within a worker thread and has only access to JS through thread-safe functions
void NapiAsyncExecCb (napi_env env, void* context) {
    GlueAsyncDataT *data= (GlueAsyncDataT*)context;
    AfbVcbDataT *vcbdata = data->vcbdata;
    napi_threadsafe_function tsfunc= (napi_threadsafe_function)vcbdata->userdata;
    napi_status statusN;

    assert (data && data->magic == napi_create_threadsafe_function);

    statusN = napi_acquire_threadsafe_function(tsfunc);
    assert(statusN == napi_ok);

    statusN = napi_call_threadsafe_function(tsfunc, data, napi_tsfn_blocking);
    assert(statusN == napi_ok);

    statusN = napi_release_threadsafe_function(tsfunc, napi_tsfn_release);
    assert(statusN == napi_ok);
}

void GlueAsyncVerbCb(afb_req_t afbRqt, unsigned nparams, afb_data_t const params[]) {
    const char *errorMsg=NULL;
    napi_status statusN;
    napi_value uidN=NULL;
    napi_env env=NULL;
    napi_threadsafe_function callbackTS;

    // check verb vcbdata
    AfbVcbDataT *vcbData= afb_req_get_vcbdata(afbRqt);
    if (!vcbData || vcbData->magic != (void*)AfbAddVerbs) {
        errorMsg = "(hoops) verb invalid vcbData handle";
        goto OnErrorExit;
    }

    // on first call we need to retreive original callback object from configJ
    if (!vcbData->callback) {
        napi_value callbackN;
        
        json_object *callbackJ=json_object_object_get(vcbData->configJ, "callback");
        if (!callbackJ) {
            errorMsg = "(hoops) verb no callback defined";
            goto OnErrorExit;
        }

        // extract nodejs callback and env from callbackJ userdata
        napiJsoncUserDataT *userdata= (napiJsoncUserDataT*)json_object_get_userdata (callbackJ);
        if (!userdata) {
            errorMsg = "(hoops) verb no callback attached";
            goto OnErrorExit;
        }
        env= userdata->env;

        statusN= napi_create_string_utf8(env, vcbData->uid, NAPI_AUTO_LENGTH, &uidN);
        if (statusN != napi_ok) goto OnErrorExit;

        // get callback object
        statusN= napi_get_reference_value(env, userdata->ref, &callbackN);
        if (statusN != napi_ok || !callbackN) goto OnErrorExit;

        // Convert the callback retrieved from JavaScript into a thread-safe function
        statusN = napi_create_threadsafe_function(env, callbackN, NULL, uidN, 0, 1, NULL, NULL, vcbData, GlueAsyncArgsCb, &callbackTS);
        if (statusN != napi_ok) goto OnErrorExit;

        // user verb vcbdata->userdata to keep track of threadsafe callback
        vcbData->userdata= callbackTS;
        vcbData->state= (void*) userdata->env;
    } else {
        callbackTS= (napi_threadsafe_function)vcbData->userdata;
        env= (napi_env)vcbData->state;
    }

    GlueAsyncDataT *data= calloc (1, sizeof(GlueAsyncDataT));
    data->magic  = napi_create_threadsafe_function;
    data->nparams= nparams;
    data->params = params;
    data->afbRqt = afbRqt;
    data->vcbdata= vcbData;

    if (!uidN) {
        statusN= napi_create_string_utf8(env, vcbData->uid, NAPI_AUTO_LENGTH, &uidN);
        if (statusN != napi_ok) goto OnErrorExit;
    }

    // create a worker for this request
    statusN= napi_create_async_work(env, NULL, uidN, NapiAsyncExecCb, NULL, data, &data->workerN);
    if (statusN != napi_ok) goto OnErrorExit;

    // Queue the work item for execution.
    statusN = napi_queue_async_work(env, data->workerN);
    if (statusN != napi_ok) goto OnErrorExit;

    // afb-reply will be done later asynchronously
    afb_req_addref (afbRqt);

    return;

OnErrorExit: 
    {
    afb_data_t reply;
    AfbHandleT *glue= GlueRqtNew(afbRqt);
    json_object *errorJ = napiJsonDbg(glue->env, "create-async-fail", errorMsg);
    GLUE_AFB_WARNING(afbMain, "verb=[%s] nodejs=%s", afb_req_get_called_verb(afbRqt), json_object_get_string(errorJ));
    afb_create_data_raw(&reply, AFB_PREDEFINED_TYPE_JSON_C, errorJ, 0, (void *)json_object_put, errorJ);
    GlueReply(glue, -1, 1, &reply);
    } 
}

void GlueVerbCb(afb_req_t afbRqt, unsigned nparams, afb_data_t const params[]) {
    const char *errorMsg = NULL;
    afb_data_t argsD[nparams];
    json_object *argsJ[nparams];
    size_t argsCount= 0;
    napi_value argsValN[1+nparams];
    napi_value resultN, globalN, callbackN;
    napi_status statusN;
    int err;

    // new afb request
    AfbHandleT *glue= GlueRqtNew(afbRqt);

    // on first call we compile configJ to boost following py api/verb calls
    AfbVcbDataT *vcbData= afb_req_get_vcbdata(afbRqt);
    if (vcbData->magic != (void*)AfbAddVerbs) {
        errorMsg = "(hoops) verb invalid vcbData handle";
        goto OnErrorExit;
    }

    // on first call we need to retreive original callback object from configJ
    if (!vcbData->callback) {
        json_object *callbackJ=json_object_object_get(vcbData->configJ, "callback");
        if (!callbackJ) {
            errorMsg = "(hoops) verb no callback defined";
            goto OnErrorExit;
        }

        // extract nodejs callback and env from callbackJ userdata
        napiJsoncUserDataT *userdata= (napiJsoncUserDataT*)json_object_get_userdata (callbackJ);
        if (!userdata) {
            errorMsg = "(hoops) verb no callback attached";
            goto OnErrorExit;
        }
        vcbData->callback= (void*) userdata->ref;
        vcbData->state =(void*) userdata->env;
    }

    // get callback object
    statusN= napi_get_reference_value(glue->env, (napi_ref)vcbData->callback, &callbackN);
    if (statusN != napi_ok || !callbackN) goto OnErrorExit;

    statusN= napi_create_external (glue->env, glue, NULL, NULL, &argsValN[0]);
    if (statusN != napi_ok) goto OnErrorExit;

    statusN=  napi_get_global(glue->env, &globalN);
    if (statusN != napi_ok) goto OnErrorExit;

    // retreive input arguments and convert them to json
    for (int idx = 0; idx < nparams; idx++)
    {
        err = afb_data_convert(params[idx], &afb_type_predefined_json_c, &argsD[idx]);
        if (err)
        {
            errorMsg = "fail converting input params to jsonc";
            goto OnErrorExit;
        }
        argsCount ++;
        argsJ[idx]  = afb_data_ro_pointer(argsD[idx]);
        argsValN[argsCount]= napiJsoncToValue(glue->env, argsJ[idx]);
    }

    statusN= napi_call_function(glue->env, globalN, callbackN, argsCount+1, argsValN, &resultN);
    switch (statusN) {
        napi_value errorN;
        case napi_ok:
            break;
        case napi_pending_exception:
            statusN= napi_get_and_clear_last_exception(glue->env, &errorN);
            napi_throw(glue->env, errorN);
        default:
            goto OnErrorExit;
    }

    switch (napiGetType(glue->env, resultN)) {
        int32_t status;
        case napi_undefined:
        case napi_null:
            break;
        case napi_number:
            napi_get_value_int32 (glue->env, resultN, &status);
            GlueReply(glue, statusN, 0, NULL);
            break;
        case napi_object: {
            uint32_t count;
            bool isArray;
            json_object *slotJ;
            napi_value slotN;
            napi_is_array(glue->env, resultN, &isArray);
            napi_get_array_length(glue->env, resultN, &count);
            if (!isArray || count < 1) {
                errorMsg="response array should start with a status";
                goto OnErrorExit;
            }
            statusN = napi_get_element(glue->env, resultN, 0, &slotN);
            if (statusN != napi_ok || napiGetType(glue->env, slotN) != napi_number) {
                errorMsg= "response array 1st element should be the status number";
                goto OnErrorExit;
            }  
            napi_get_value_int32 (glue->env, slotN, &status);
            if (count > 1) {
                afb_data_t reply[count];
                for (int idx = 0 ; idx < count-1 ; idx++) {
                    statusN = napi_get_element(glue->env, resultN, idx+1, &slotN);
                    if (statusN != napi_ok)  goto OnErrorExit;
                    slotJ = napiValuetoJsonc(glue->env, slotN);
                    if (!slotJ)  goto OnErrorExit;
                    afb_create_data_raw(&reply[idx], AFB_PREDEFINED_TYPE_JSON_C, slotJ, 0, (void *)json_object_put, slotJ);
                }
                GlueReply(glue, statusN, count-1, reply);
            } 
            break;
        }
        default:
            errorMsg = "unsupported callback response";    
    }
       
    // respond done, free ressources.
    for (int idx=0; idx <nparams; idx++) json_object_put(argsJ[idx]);
    return;

OnErrorExit:
    {
        afb_data_t reply;
        json_object *errorJ = napiJsonDbg(glue->env, NULL, errorMsg);
        GLUE_AFB_WARNING(glue, "verb=[%s] nodejs=%s", afb_req_get_called_verb(afbRqt), json_object_get_string(errorJ));
        afb_create_data_raw(&reply, AFB_PREDEFINED_TYPE_JSON_C, errorJ, 0, (void *)json_object_put, errorJ);
        GlueReply(glue, -1, 1, &reply);
    }
}

int GlueCtrlCb(afb_api_t apiv4, afb_ctlid_t ctlid, afb_ctlarg_t ctlarg, void *userdata) {
    const char *errorMsg= "(hoops) api control fail";
    AfbHandleT *glue= (AfbHandleT*) userdata;
    static int orphan=0;
    const char *state;
    int64_t status=0;

    // assert userdata validity
    assert (glue && glue->magic == GLUE_API_MAGIC);

    switch (ctlid) {
    case afb_ctlid_Root_Entry:
        state="root";
        break;

    case afb_ctlid_Pre_Init:
        state="config";
        glue->api.afb= apiv4;
        break;

    case afb_ctlid_Init:
        state="ready";
        break;

    case afb_ctlid_Class_Ready:
        state="class";
        break;

    case afb_ctlid_Orphan_Event:
        GLUE_AFB_WARNING (glue, "Orphan event=%s count=%d", ctlarg->orphan_event.name, orphan++);
        state="orphan";
        break;

    case afb_ctlid_Exiting:
        state="exit";
        break;

    default:
        break;
    }

    if (!glue->api.ctrlCbR) {
        GLUE_AFB_INFO(glue,"GlueCtrlCb: No init callback state=[%s]", state);

    } else {
        napi_value globalN, callbackN, glueN, resultN;
        napi_status statusN;

        // effectively exec node script code
        GLUE_AFB_NOTICE(glue,"GlueCtrlCb: state=[%s]", state);

        statusN= napi_get_reference_value(glue->env, glue->api.ctrlCbR, &callbackN);
        if (statusN != napi_ok) goto OnErrorExit;

        statusN= napi_create_external (glue->env, glue, NULL, NULL, &glueN);
        if (statusN != napi_ok) goto OnErrorExit;

        statusN=  napi_get_global(glue->env, &globalN);
        if (statusN != napi_ok) goto OnErrorExit;

        statusN= napi_call_function(glue->env, globalN, callbackN, GLUE_ONE_ARG, &glueN, &resultN);
        switch (statusN) {
            napi_value errorN;
            case napi_ok:
                break;
            case napi_pending_exception:
                statusN= napi_get_and_clear_last_exception(glue->env, &errorN);
                napi_throw(glue->env, errorN);
                break;
            default:
                errorMsg= "invalid napi function call status";
                goto OnErrorExit;
        }

        if (napiGetType(glue->env, resultN) != napi_number) {
            errorMsg= "api control callback return status should be a number";
            goto OnErrorExit;
        }
        napi_get_value_int64 (glue->env, resultN, &status);

    }
    return (int)status;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, errorMsg);
    return -1;
}

// this routine execute within mainloop userdata when binder is ready to go
void GlueInfoCb(afb_req_t afbRqt, unsigned nparams, afb_data_t const params[])
{
    afb_api_t apiv4 = afb_req_get_api(afbRqt);
    afb_data_t reply;
    napi_status statusN;
    napi_value configN;
    char uid [128];
    char info[512];
    size_t len;

    // retreive interpreteur from API
    AfbHandleT *glue = afb_api_get_userdata(apiv4);
    assert(glue->magic == GLUE_API_MAGIC);

    // extract uid + info from API config
    statusN= napi_get_reference_value(glue->env, glue->api.configR, &configN);
    if (statusN != napi_ok) goto OnErrorExit;

    char *infoPtr=NULL;
    bool isPresent;
    napi_value uidN, infoN;
    statusN= napi_has_named_property (glue->env, configN, "uid", &isPresent);
    if (statusN != napi_ok || !isPresent) goto OnErrorExit;
    napi_get_named_property(glue->env, configN, "uid", &uidN);
    statusN = napi_get_value_string_utf8(glue->env, uidN, uid, sizeof(uid), &len);
    if (statusN != napi_ok) goto OnErrorExit;

    statusN= napi_has_named_property (glue->env, configN, "info", &isPresent);
    if (statusN != napi_ok || !isPresent) {
        napi_get_named_property(glue->env, configN, "uid", &infoN);
        statusN = napi_get_value_string_utf8(glue->env, infoN, info, sizeof(info), &len);
        if (statusN != napi_ok) goto OnErrorExit;
        infoPtr= info;
    }

    json_object *metaJ;
    wrap_json_pack (&metaJ, "{ss ss*}"
        ,"uid", uid
        ,"info", infoPtr
    );

    // extract info from each verb
    json_object *verbsJ = json_object_new_array();
    for (int idx = 0; idx < afb_api_v4_verb_count(apiv4); idx++)
    {
        const afb_verb_t *afbVerb = afb_api_v4_verb_at(apiv4, idx);

        if (!afbVerb) break;
       if (afbVerb->vcbdata != glue) {
            AfbVcbDataT *vcbData= afbVerb->vcbdata;
            if (vcbData->magic != AfbAddVerbs) continue;
            json_object_array_add(verbsJ, vcbData->configJ);
            json_object_get(verbsJ);
        }
    }
    // info devtool require a group array
    json_object *groupsJ, *infoJ;
    wrap_json_pack(&groupsJ, "[{so}]", "verbs", verbsJ);

    wrap_json_pack(&infoJ, "{so so}", "metadata", metaJ, "groups", groupsJ);
    afb_create_data_raw(&reply, AFB_PREDEFINED_TYPE_JSON_C, infoJ, 0, (void *)json_object_put, infoJ);
    afb_req_reply(afbRqt, 0, 1, &reply);
    return;

OnErrorExit: {
    afb_data_t reply;
    json_object *errorJ = napiJsonDbg(glue->env, "internal-error", "invalid glue handle config");
    GLUE_AFB_WARNING(glue, "verb=[info] node=%s", json_object_get_string(errorJ));
    afb_create_data_raw(&reply, AFB_PREDEFINED_TYPE_JSON_C, errorJ, 0, (void *)json_object_put, errorJ);
    GlueReply(glue, -1, 1, &reply);
    }
}

int GlueStartupCb(void *callback, void *userdata)
{
    napi_ref callbackR= (napi_ref) callback;
    napi_value glueN, globalN, callbackN, resultN;
    napi_status statusN;
    int32_t result=0;

    AfbHandleT *glue = (AfbHandleT *)userdata;
    assert(glue && glue->magic == GLUE_BINDER_MAGIC);

    if (callbackR) {

        statusN= napi_get_reference_value(glue->env, callbackR, &callbackN);
        if (statusN != napi_ok) goto OnErrorExit;

        statusN= napi_create_external (glue->env, glue, NULL, NULL, &glueN);
        if (statusN != napi_ok) goto OnErrorExit;

        statusN=  napi_get_global(glue->env, &globalN);
        if (statusN != napi_ok) goto OnErrorExit;

/*
    napi_create_threadsafe_function(env,
      js_cb, NULL, work_name, 0, 1, NULL, NULL, NULL,
      ThreadSafeCFunction4CallingJS, // the C/C++ callback function
      // out: the asynchronous thread-safe JavaScript function
      &(async_stream_data_ex->tsfn_StreamSearch)); 
*/

        // napi_acquire_threadsafe_function
        statusN= napi_call_function(glue->env, globalN, callbackN, 1, &glueN, &resultN);
        switch (statusN) {
            napi_value errorN;
            case napi_ok:
                break;
            case napi_pending_exception:
                statusN= napi_get_and_clear_last_exception(glue->env, &errorN);
                napi_throw(glue->env, errorN);
                break;
            default:
                goto OnErrorExit;
        }

        switch (napiGetType(glue->env, resultN)) {
            case napi_undefined:
            case napi_null:
                result=0;
                break;
            case napi_number:
                statusN = napi_get_value_int32(glue->env, resultN, &result);
                if (statusN != napi_ok) goto OnErrorExit;
                break;
            default: goto OnErrorExit;
        }
    }

    return (int)result;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "fail mainloop startup");
    return -1;
}
