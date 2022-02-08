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
 * reference: https://stackoverflow.com/questions/58960713/how-to-use-napi-threadsafe-function-for-nodejs-native-addon
 */

#include <libafb/afb-v4.h>
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

typedef struct {
    void *magic;
    const char *uid;
    afb_req_t afbRqt;
    int usage;
    GlueHandleT *glue;
    unsigned nparams;
    unsigned nargs;
    afb_data_t const *params;
    napi_async_work workerN;
    AfbVcbDataT *vcbdata;
    int vcbfree;
    napi_value *argsN;
} GlueAsyncDataT;


const nsKeyEnumT napiStatusE[] = {
{"napi_ok",napi_ok},
{"napi_invalid_arg",napi_invalid_arg},
{"napi_object_expected",napi_object_expected},
{"napi_string_expected",napi_string_expected},
{"napi_name_expected",napi_name_expected},
{"napi_function_expected",napi_function_expected},
{"napi_number_expected",napi_number_expected},
{"napi_boolean_expected",napi_boolean_expected},
{"napi_array_expected",napi_array_expected},
{"napi_generic_failure",napi_generic_failure},
{"napi_pending_exception",napi_pending_exception},
{"napi_cancelled",napi_cancelled},
{"napi_escape_called_twice",napi_escape_called_twice},
{"napi_handle_scope_mismatch",napi_handle_scope_mismatch},
{"napi_callback_scope_mismatch",napi_callback_scope_mismatch},
{"napi_queue_full",napi_queue_full},
{"napi_closing",napi_closing},
{"napi_bigint_expected",napi_bigint_expected},
{"napi_date_expected",napi_date_expected},
{"napi_arraybuffer_expected",napi_arraybuffer_expected},
{"napi_detachable_arraybuffer_expected",napi_detachable_arraybuffer_expected},
{"napi_would_deadlock",napi_would_deadlock},
{NULL, -1} // sentinel
};

void GlueFreeHandleCb(napi_env env, GlueHandleT *handle) {
    if (!handle) goto OnErrorExit;

    //printf ("*** GlueFreeHandleCb type=%s handle=%p usage=%d\n", AfbMagicToString(handle->magic), handle, handle->usage);
    handle->usage--;
    switch (handle->magic) {
        case GLUE_EVT_MAGIC:
            if (handle->usage < 0) {
                free (handle->event.pattern);
                napi_delete_reference(env,handle->event.configR);
                if ( handle->event.configR) napi_delete_reference(env, handle->event.configR);
            }
            break;
        case GLUE_JOB_MAGIC:
            if (handle->usage < 0) {
                if (handle->uid)free (handle->uid);
            }
            break;
        case GLUE_POST_MAGIC:
            if (handle->usage < 0) {
                napi_delete_reference(env,handle->post.async.callbackR);
                if (handle->post.async.userdataR) napi_delete_reference(env, handle->post.async.userdataR);
                if (handle->post.async.uid)free (handle->post.async.uid);
            }
            break;
        case GLUE_TIMER_MAGIC:
            afb_timer_unref (handle->timer.afb);
            if (handle->usage < 0) {
                napi_delete_reference(env, handle->timer.async.callbackR);
                if (handle->timer.async.userdataR)napi_delete_reference(env, handle->timer.async.userdataR);
            }
            break;

        case GLUE_API_MAGIC:    // as today removing API is not supported bu libafb
        case GLUE_RQT_MAGIC:    // rqt live cycle is handle directly by libafb
        case GLUE_BINDER_MAGIC: // afbmain should never be released
        default:
            goto OnErrorExit;
    }
    if (handle->usage < 0) free (handle);
    return;

OnErrorExit:
    ERROR ("try to release a protected handle type=%s", AfbMagicToString(handle->magic));
}


static void GlueFreeAsyncData (GlueAsyncDataT *ctxdata) {
    ctxdata->usage --;
    if (ctxdata->usage < 0) {

        // static API vcbdata should not be deleted
        if (ctxdata->vcbfree && ctxdata->vcbdata) free(ctxdata->vcbdata);

        // clean everything else
        if (ctxdata->argsN) free(ctxdata->argsN);
        free (ctxdata);
    }
}

// free loop handle when done
void GlueFreeExternalCb (napi_env env, void* data, void* hint) {
    GlueHandleT *handle= (GlueHandleT*)data;
    GlueFreeHandleCb(env, handle);
}

// implicit respond to afb query when choose not to use libafb.reply()
static const char* GlueImplicitReply (GlueHandleT *glue, napi_value responseN) {
    const char *errorMsg= NULL;
    napi_status statusN;

    if (glue->magic != GLUE_RQT_MAGIC) {
        errorMsg= "invalid request handle";
        goto OnErrorExit;
    }

   switch (napiGetType(glue->env, responseN)) {
        int64_t status;
        case napi_undefined:
        case napi_null:
            break;
        case napi_number:
            napi_get_value_int64 (glue->env, responseN, &status);
            GlueAfbReply(glue, status, 0, NULL);
            break;
        case napi_object: {
            uint32_t count;
            bool isArray;
            json_object *slotJ;
            napi_value slotN;
            napi_is_array(glue->env, responseN, &isArray);
            napi_get_array_length(glue->env, responseN, &count);
            if (!isArray || count < 1) {
                errorMsg="response array should start with a status";
                goto OnErrorExit;
            }
            statusN = napi_get_element(glue->env, responseN, 0, &slotN);
            if (statusN != napi_ok || napiGetType(glue->env, slotN) != napi_number) {
                errorMsg= "response array 1st element should be the status number";
                goto OnErrorExit;
            }
            napi_get_value_int64 (glue->env, slotN, &status);
            if (count > 1) {
                afb_data_t reply[count];
                for (int idx = 0 ; idx < count-1 ; idx++) {
                    statusN = napi_get_element(glue->env, responseN, idx+1, &slotN);
                    if (statusN != napi_ok)  goto OnErrorExit;
                    slotJ = napiValuetoJsonc(glue->env, slotN);
                    if (!slotJ)  goto OnErrorExit;
                    afb_create_data_raw(&reply[idx], AFB_PREDEFINED_TYPE_JSON_C, slotJ, 0, (void *)json_object_put, slotJ);
                }
                GlueAfbReply(glue, status, count-1, reply);
            }
            break;
        }
        default:
            errorMsg = "unsupported callback response";
    }
    return NULL;

OnErrorExit:
    return errorMsg;
}

static napi_value GlueOnErrorCb (napi_env env, GlueAsyncDataT *ctxdata) {
    assert (ctxdata && ctxdata->magic == napi_call_function);
    napi_status statusN;
    napi_value onErrorN, globalN, resultN, errorN;
    napi_value argsN [ctxdata->nparams+GLUE_THREE_ARG]; // prefix api args with glue and node exception error
    GlueHandleT *glue;

    // retreive error handler from API handle (rqt | api for events)
    if (!ctxdata->afbRqt) glue= ctxdata->glue;
    else glue= afb_api_get_userdata(afb_req_get_api(ctxdata->glue->rqt.afb));

    // Should save/clear the exception before calling any napi function
    statusN= napi_get_and_clear_last_exception(env, &errorN);
    if (statusN != napi_ok) goto OnErrorExit;

    // if not error callback defined return without clearing the error
    if (!glue->onError) goto OnErrorExit;

    // 1st argument is the glue handle rqt|api
    statusN= napi_create_external (env, ctxdata->glue, NULL, NULL, &argsN[0]);
    if (statusN != napi_ok) goto OnErrorExit;

    // 2nd argument is the verb uid
    if ( ctxdata->uid) statusN = napi_create_string_utf8(env, ctxdata->uid, NAPI_AUTO_LENGTH, &argsN[1]);
    else statusN= napi_create_string_utf8(env, glue->uid, NAPI_AUTO_LENGTH, &argsN[1]);
    if (statusN != napi_ok) goto OnErrorExit;

    // 3rd argument the nodejs error object
    argsN[2]= errorN;

    statusN= napi_get_reference_value(env, glue->onError->callbackR, &onErrorN);
    if (statusN != napi_ok || !onErrorN) goto OnErrorExit;

    // copy API argument list
    for (int idx=0; idx < ctxdata->nparams; idx++) {
       argsN[idx+GLUE_THREE_ARG]=  ctxdata->argsN[idx+ctxdata->nargs];
    }

    statusN=  napi_get_global(env, &globalN);
    if (statusN != napi_ok) goto OnErrorExit;

    statusN= napi_call_function(env, globalN, onErrorN, ctxdata->nparams+GLUE_THREE_ARG, argsN, &resultN);
    if (statusN != napi_ok) {
        goto OnErrorExit; // no second chance for onerror callback
    }

    return resultN;

OnErrorExit: {
    char *message= napiGetString(env, errorN, "stack");
    GLUE_AFB_ERROR (glue, "Exec 'onerror' callback fail uid=%s error=%s", ctxdata->uid, message);
    if (message) free (message);
    return NULL;
    }
}

static void GluePcallFunc (GlueHandleT *glue, GlueAsyncCtxT *async, const char *label, int status, unsigned nreplies, afb_data_t const replies[]) {
    const char *errorMsg = "internal-error";
    napi_value argsN [nreplies+GLUE_THREE_ARG];
    napi_value callbackN, resultN, globalN;
    napi_env env= glue->env;
    napi_status statusN;
    unsigned shift;

    napi_handle_scope scopeN;
    statusN= napi_open_handle_scope (afbMain->env, &scopeN);
    assert (statusN == napi_ok);

    // subcall was refused
    if (AFB_IS_BINDER_ERRNO(status)) {
        errorMsg= afb_error_text(status);
        goto OnErrorExit;
    }

    // 1st argument glue handle rqt
    statusN= napi_create_external (env, glue, NULL, NULL, &argsN[0]);
    if (statusN != napi_ok) {
        errorMsg= "invalid-glue-env";
        goto OnErrorExit;
    }

    // 2nd argument status or label, 3rd argument userdata
    if (label == (char*)-1) {
        shift= GLUE_ONE_ARG;
    } else {
         shift= GLUE_THREE_ARG;
        if (!label) statusN = napi_create_int64(env, status, &argsN[1]);
        else statusN = napi_create_string_utf8(env, label, NAPI_AUTO_LENGTH,&argsN[1]);
        if (statusN != napi_ok) goto OnErrorExit;

        if (async->userdataR) {
            statusN= napi_get_reference_value(env, async->userdataR, &argsN[2]);
            if (statusN != napi_ok) goto OnErrorExit;
        } else {
            napi_get_null(env, &argsN[2]);
        }
    }

    // add afb replied arguments
    errorMsg= napiPushAfbArgs (env, argsN, shift, nreplies, replies);
    if (errorMsg) goto OnErrorExit;

    statusN= napi_get_reference_value(env, async->callbackR, &callbackN);
    if (statusN != napi_ok || napiGetType(env, callbackN)!= napi_function) {
        errorMsg= "invalid-ref-callback";
        goto OnErrorExit;
    }

    statusN=  napi_get_global(env, &globalN);
    if (statusN != napi_ok) goto OnErrorExit;

    //statusN= napi_call_function(env, globalN, callbackN, nreplies+GLUE_THREE_ARG, argsN, &resultN);
    statusN= napi_call_function(env, globalN, callbackN, shift+nreplies, argsN, &resultN);
    switch (statusN) {
        //napi_value errorN;
        case napi_ok:
            break;
        case napi_pending_exception: {
            GlueAsyncDataT errCtx= {
                .magic= napi_call_function,
                .uid= async->uid,
                .glue= glue,
                .nparams= nreplies,
                .argsN= argsN,
                .nargs= shift,
            };
            resultN= GlueOnErrorCb (env, &errCtx);
            break;
        }
        default:
            errorMsg= utilValue2Label(napiStatusE, statusN);
            goto OnErrorExit;
    }

    if (glue->magic == GLUE_RQT_MAGIC) {
        // if resultN is set, then response the request
        errorMsg= GlueImplicitReply (glue, resultN);
        if (errorMsg) goto OnErrorExit;
    }
    statusN= napi_close_handle_scope (afbMain->env, scopeN);
    return;

OnErrorExit: {
    const char*uid= async->uid;
    if (!uid)  uid= glue->uid;
    if (glue->magic != GLUE_RQT_MAGIC)  GLUE_AFB_WARNING(glue, "uid=%s error=%s", uid, errorMsg);
    else {
        afb_data_t reply;
        json_object *errorJ = napiJsonDbg(env, uid, errorMsg);
        GLUE_AFB_WARNING(glue, "%s", json_object_get_string(errorJ));
        afb_create_data_raw(&reply, AFB_PREDEFINED_TYPE_JSON_C, errorJ, 0, (void *)json_object_put, errorJ);
        GlueAfbReply(glue, -1, 1, &reply);
    }
    statusN= napi_close_handle_scope (afbMain->env, scopeN);
  }
}

void GlueEventCb (void *userdata, const char *label, unsigned nparams, afb_data_x4_t const params[], afb_api_t api) {
    GlueHandleT *glue= (GlueHandleT*) userdata;
    assert (glue->magic == GLUE_EVT_MAGIC);
    GluePcallFunc (glue, &glue->event.async, label, 0, nparams, params);
}

void GlueTimerCb (afb_timer_x4_t timer, void *userdata, int decount) {
   GlueHandleT *glue= (GlueHandleT*) userdata;
   assert (glue->magic == GLUE_TIMER_MAGIC);
   GluePcallFunc (glue, &glue->timer.async, NULL, decount, 0, NULL);
}

void GlueJobPostCb (int signum, void *userdata) {
    GlueCallHandleT *handle= (GlueCallHandleT*) userdata;
    assert (handle->magic == GLUE_POST_MAGIC);
    if (!signum) GluePcallFunc (handle->glue, &handle->async, NULL, signum, 0, NULL);
    free (handle->async.uid);
    free (handle);
}

// afb async api callback
void GlueApiSubcallCb (void *userdata, int status, unsigned nreplies, afb_data_t const replies[], afb_api_t api) {
    GlueCallHandleT *handle= (GlueCallHandleT*) userdata;
    assert (handle->magic == GLUE_CALL_MAGIC);
    GluePcallFunc (handle->glue, &handle->async, NULL, status, nreplies, replies);
    free (handle->async.uid);
    free (handle);
}

// afb async request callback
void GlueRqtSubcallCb (void *userdata, int status, unsigned nreplies, afb_data_t const replies[], afb_req_t req) {
    GlueCallHandleT *handle= (GlueCallHandleT*) userdata;
    assert (handle->magic == GLUE_CALL_MAGIC);
    GluePcallFunc (handle->glue, &handle->async, NULL, status, nreplies, replies);
    free (handle->async.uid);
    free (handle);
}

// afb api/verb request callback
void GlueRqtVerbCb(afb_req_t afbRqt, unsigned nparams, afb_data_t const params[]) {
    const char*errorMsg;

    GlueHandleT *glue= GlueRqtNew(afbRqt);

    AfbVcbDataT *vcbdata= afb_req_get_vcbdata(afbRqt);
    if (!vcbdata || vcbdata->magic != AfbAddVerbs)  {
         errorMsg = "(hoops) verb invalid vcbdata handle";
        goto OnErrorExit;
    }
    if (!vcbdata->callback) {
        json_object *callbackJ=json_object_object_get(vcbdata->configJ, "callback");
        if (!callbackJ) {
            errorMsg = "(hoops) verb no callback defined";
            goto OnErrorExit;
        }

        // extract nodejs callback and env from callbackJ cbdata
        vcbdata->callback= json_object_get_userdata (callbackJ);
        if (!vcbdata->callback) {
            errorMsg = "(hoops) verb no callback attached";
            goto OnErrorExit;
        }
    }

    GlueAsyncCtxT *async= (GlueAsyncCtxT*)vcbdata->callback;
    GluePcallFunc (glue, async, (char*)-1, 0, nparams, params);

    return;

OnErrorExit:
    {
    afb_data_t reply;
    GlueHandleT *glue= GlueRqtNew(afbRqt);
    json_object *errorJ = napiJsonDbg(glue->env, "create-async-fail", errorMsg);
    GLUE_AFB_WARNING(afbMain, "verb=[%s] nodejs=%s", afb_req_get_called_verb(afbRqt), json_object_get_string(errorJ));
    afb_create_data_raw(&reply, AFB_PREDEFINED_TYPE_JSON_C, errorJ, 0, (void *)json_object_put, errorJ);
    GlueAfbReply(glue, -1, 1, &reply);
    }
}    

// this function call nodejs callback within main node thread.
static void  GlueVerbExecCb (napi_env env, napi_value js_cb, void* context, void* data) {
    const char *errorMsg=NULL;
    napi_value callbackN, resultN, globalN;
    napi_status statusN;

    AfbVcbDataT *vcbdata= (AfbVcbDataT*)context;
    assert (vcbdata && vcbdata->magic == AfbAddVerbs);

    GlueAsyncDataT *ctxdata= (GlueAsyncDataT*)data;
    assert (ctxdata && ctxdata->magic == napi_call_function);
    ctxdata->usage ++;

    // get callback object
    statusN= napi_get_reference_value(env, (napi_ref)vcbdata->callback, &callbackN);
    if (statusN != napi_ok || !callbackN) goto OnErrorExit;

    statusN= napi_create_external (env, ctxdata->glue, NULL, NULL, &ctxdata->argsN[0]);
    if (statusN != napi_ok) goto OnErrorExit;

    // we have a private context (ctxdata->nargs == 2)
    if (vcbdata->userdata) {
        statusN= napi_get_reference_value(env, (napi_ref)vcbdata->userdata, &ctxdata->argsN[1]);
        if (statusN != napi_ok || !ctxdata->argsN[1]) goto OnErrorExit;
    }

    statusN=  napi_get_global(env, &globalN);
    if (statusN != napi_ok) goto OnErrorExit;

    statusN= napi_call_function(env, globalN, callbackN, ctxdata->nparams+ctxdata->nargs, ctxdata->argsN, &resultN);
    switch (statusN) {
        //napi_value errorN;
        case napi_ok:
            break;
        case napi_pending_exception: {
            resultN= GlueOnErrorCb (env, ctxdata);
            break;
        }
        default:
            goto OnErrorExit;
    }

    if (ctxdata->afbRqt) {
        errorMsg= GlueImplicitReply (ctxdata->glue, resultN);
        if (errorMsg) goto OnErrorExit;
    }

    // if loop retreive status and release promise
    if (ctxdata->glue->magic == GLUE_JOB_MAGIC) {
        if (napiGetType(env, resultN) != napi_undefined) {
            int64_t status;
            statusN= napi_get_value_int64(env, resultN, &status);
            if (statusN == napi_ok && napiGetType(env, resultN) == napi_number) ctxdata->glue->job.status= status;
            else ctxdata->glue->job.status= -1;
            if (status) sem_post(&ctxdata->glue->job.sem); // when status set release the job
        }
    }

    GlueFreeAsyncData(ctxdata);
    return;

OnErrorExit:
    if (ctxdata->afbRqt) {
        afb_data_t reply;
        json_object *errorJ = napiJsonDbg(env, ctxdata->uid, errorMsg);
        GLUE_AFB_WARNING(ctxdata->glue, "verb=[%s] nodejs=%s", afb_req_get_called_verb(ctxdata->afbRqt), json_object_get_string(errorJ));
        afb_create_data_raw(&reply, AFB_PREDEFINED_TYPE_JSON_C, errorJ, 0, (void *)json_object_put, errorJ);
        GlueAfbReply(ctxdata->glue, -1, 1, &reply);
    } else {
        GLUE_AFB_ERROR(ctxdata->glue, "verb=[%s] nodejs=%s", ctxdata->uid, errorMsg);
        if (ctxdata->glue->magic == GLUE_JOB_MAGIC) sem_post(&ctxdata->glue->job.sem);
    }
    GlueFreeAsyncData(ctxdata);
}

int GlueCtrlCb(afb_api_t apiv4, afb_ctlid_t ctlid, afb_ctlarg_t ctlarg, void *userdata) {
    const char *errorMsg= "(hoops) api control fail";
    GlueHandleT *glue= (GlueHandleT*) userdata;
    static int orphan=0;
    const char *state;
    int64_t status=0;

    // assert userdata validity
    assert (glue && glue->magic == GLUE_API_MAGIC);

    switch (ctlid) {
    case afb_ctlid_Root_Entry:
        state="root";
        break;

    case afb_ctlid_Pre_Init: {
        state="config";
        glue->api.afb= apiv4;
        break;
    }
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

    if (!glue->api.async) {
        GLUE_AFB_INFO(glue,"GlueCtrlCb: No init callback state=[%s]", state);

    } else {
        napi_value globalN, callbackN, resultN;
        napi_status statusN;
        napi_value args[GLUE_TWO_ARG];

        // Fulup ??? not sure if libuv scope should be unique or reset by callback ???
        napi_handle_scope scopeN;
        statusN= napi_open_handle_scope (afbMain->env, &scopeN);
        assert (statusN == napi_ok);

        // effectively exec node script code
        GLUE_AFB_NOTICE(glue,"GlueCtrlCb: state=[%s]", state);

        statusN= napi_get_reference_value(glue->env, glue->api.async->callbackR, &callbackN);
        if (statusN != napi_ok) goto OnErrorExit;

        statusN= napi_create_external (glue->env, glue, NULL, NULL, &args[0]);
        if (statusN != napi_ok) goto OnErrorExit;

		statusN = napi_create_string_utf8(glue->env, state, NAPI_AUTO_LENGTH, &args[1]);
		if (statusN != napi_ok) goto OnErrorExit;

        statusN=  napi_get_global(glue->env, &globalN);
        if (statusN != napi_ok) goto OnErrorExit;

        statusN= napi_call_function(glue->env, globalN, callbackN, GLUE_TWO_ARG, args, &resultN);
        switch (statusN) {
            case napi_ok:
                break;
            case napi_pending_exception: {
                napi_value errorN;
                json_object *errorJ;
                GlueAsyncDataT errCtx= {
                    .magic= (void*)napi_call_function,
                    .uid=  glue->uid,
                    .glue= glue,
                };
                errorN= GlueOnErrorCb (glue->env, &errCtx);
                errorJ= napiValuetoJsonc(glue->env, errorN);
                GLUE_AFB_INFO(glue, "status=%s", json_object_get_string(errorJ));
                json_object_put(errorJ);
                errorMsg= "api-control-cb: on-error";
                goto OnErrorExit;
                break;
            }
            default:
                errorMsg= "api-control-cb: on-abort";
                goto OnErrorExit;
        }

        if (napiGetType(glue->env, resultN) != napi_number) {
            errorMsg= "api control callback return status should be a number";
            goto OnErrorExit;
        }
        napi_get_value_int64 (glue->env, resultN, &status);

        statusN= napi_close_handle_scope (afbMain->env, scopeN);
        assert (statusN == napi_ok);
    }
    return (int)status;

OnErrorExit:
    GLUE_AFB_ERROR(afbMain, "api=%s info=%s", afb_api_name(apiv4),errorMsg);
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
    GlueHandleT *glue = afb_api_get_userdata(apiv4);
    assert(glue->magic == GLUE_API_MAGIC);

    // Fulup ??? not sure if libuv scope should be unique or reset by callback ???
    napi_handle_scope scopeN;
    statusN= napi_open_handle_scope (afbMain->env, &scopeN);
    assert (statusN == napi_ok);

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
            AfbVcbDataT *vcbdata= afbVerb->vcbdata;
            if (vcbdata->magic != AfbAddVerbs) continue;
            json_object_array_add(verbsJ, vcbdata->configJ);
            json_object_get(verbsJ);
        }
    }
    // info devtool require a group array
    json_object *groupsJ, *infoJ;
    wrap_json_pack(&groupsJ, "[{so}]", "verbs", verbsJ);

    wrap_json_pack(&infoJ, "{so so}", "metadata", metaJ, "groups", groupsJ);
    afb_create_data_raw(&reply, AFB_PREDEFINED_TYPE_JSON_C, infoJ, 0, (void *)json_object_put, infoJ);
    afb_req_reply(afbRqt, 0, 1, &reply);

    // Fulup ??? not sure if libuv scope should be unique or reset by callback ???
    statusN= napi_close_handle_scope (afbMain->env, scopeN);
    assert (statusN == napi_ok);

    return;

OnErrorExit: {
    afb_data_t reply;
    json_object *errorJ = napiJsonDbg(glue->env, "internal-error", "invalid glue handle config");
    GLUE_AFB_WARNING(glue, "verb=[info] node=%s", json_object_get_string(errorJ));
    afb_create_data_raw(&reply, AFB_PREDEFINED_TYPE_JSON_C, errorJ, 0, (void *)json_object_put, errorJ);
    GlueAfbReply(glue, -1, 1, &reply);
    }
}

static void GluJobThread(napi_env env, void* data) {
    GlueAsyncDataT* ctxdata = (GlueAsyncDataT*)data;
    assert (ctxdata && ctxdata->magic == (void*)napi_call_function);
    GlueHandleT *handle= ctxdata->glue;
    AfbVcbDataT *vcbdata= ctxdata->vcbdata;
    napi_status statusN;
    int status;
    struct timespec ts;
   
    // create callback arguments
    ctxdata->argsN  = malloc (ctxdata->nargs* sizeof(napi_value));

    handle->usage++;
    statusN= napi_create_external (env, handle, GlueFreeExternalCb, NULL, &ctxdata->argsN[0]);
    assert(statusN == napi_ok);

    if (!vcbdata->userdata) {
        statusN= napi_get_null(env, &ctxdata->argsN[1]);
        assert(statusN == napi_ok);
    }
    else {
        statusN= napi_get_reference_value(env, (napi_ref)vcbdata->userdata, &ctxdata->argsN[1]);
        assert(statusN == napi_ok);
    }

    if (ctxdata->vcbdata->thread) {
        statusN = napi_call_threadsafe_function(ctxdata->vcbdata->thread, ctxdata, napi_tsfn_blocking);
        assert(statusN == napi_ok);
    }

    if (handle->job.timeout == 0) {
        status= sem_wait(&handle->job.sem);
    } else {
        status= clock_gettime(CLOCK_REALTIME, &ts);
        if (status < 0) goto OnErrorExit;
        ts.tv_sec += handle->job.timeout;
        status= sem_timedwait(&handle->job.sem, &ts);
    }
    if (status < 0) goto OnErrorExit;
    return;

OnErrorExit:
    handle->job.status=-1;
    return;
}

// this function is executed within mainthread when GluJobThread finishes
void GluJobExit (napi_env env, napi_status statusN, void*data) {
    GlueAsyncDataT* ctxdata = (GlueAsyncDataT*)data;
    assert (ctxdata && ctxdata->magic == (void*)napi_call_function);
    GlueHandleT *handle= ctxdata->glue;

    // we do not need ctxdata anymore
    GlueFreeAsyncData(ctxdata);

    // prepare promise response value
    napi_value responseN;
    statusN= napi_create_external (env, handle, GlueFreeExternalCb, NULL, &responseN);
    assert(statusN == napi_ok);

    if (ctxdata->vcbdata->thread) {
        statusN = napi_release_threadsafe_function(ctxdata->vcbdata->thread, napi_tsfn_release);
        if (statusN != napi_ok) goto OnErrorExit;
    }

    if (handle->job.status < 0) goto OnErrorExit;
    napi_resolve_deferred(env, handle->job.deferred, responseN);
    napi_delete_async_work(env, ctxdata->workerN);
    return;

OnErrorExit:
    statusN= napi_reject_deferred(env, handle->job.deferred, responseN);
    napi_delete_async_work(env, ctxdata->workerN);
    return;
}

napi_value GlueJobStartCb (napi_env env, GlueHandleT *glue, napi_ref callbackR, int64_t timeout, napi_ref userdataR) {
    int status;
    napi_status statusN;
    napi_value uidN;

    // create a loop glue handle, inherited from binder handle
    GlueHandleT *handle= calloc (1, sizeof(GlueHandleT));
    handle->magic= GLUE_JOB_MAGIC;
    status= sem_init(&handle->job.sem, 0, 0);
    if (status < 0) goto OnErrorExit;

    handle->env= env;
    handle->job.timeout= timeout;
    handle->job.apiv4 = GlueGetApi(glue);
    handle->onError= afbMain->onError;

    // try to get callback name from properties
    napi_value callbackN;
    napi_get_reference_value(env, callbackR, &callbackN);
    handle->uid= napiGetString(env, callbackN, "name");

    // build uid string for async job
    if (handle->uid) napi_create_string_utf8(env,handle->uid, NAPI_AUTO_LENGTH, &uidN);
    else napi_create_string_utf8(env,afbMain->uid, NAPI_AUTO_LENGTH, &uidN);

    // create handle for GlueVerbExecCb (static callback data)
    AfbVcbDataT *vcbdata= calloc (1, sizeof(AfbVcbDataT));
    vcbdata->magic   = (void*)AfbAddVerbs;
    vcbdata->uid     = handle->uid;
    vcbdata->state   = (void*)env;
    vcbdata->userdata= (void*)userdataR;
    vcbdata->callback= (void*)callbackR;

    // create handle for GluJobThread (dynamic callback data)
    GlueAsyncDataT *ctxdata= calloc (1, sizeof(GlueAsyncDataT));
    ctxdata->magic  = (void*)napi_call_function;
    ctxdata->uid    = vcbdata->uid;
    ctxdata->glue   = handle;
    ctxdata->vcbdata= vcbdata;
    ctxdata->vcbfree= 1;
    ctxdata->nargs  = GLUE_TWO_ARG; // loop, userdata

    napi_value promiseN;
    statusN= napi_create_promise(env, &handle->job.deferred, &promiseN);
    if (statusN != napi_ok) goto OnErrorExit;

    statusN = napi_create_threadsafe_function(env, callbackN, NULL, uidN, 0, 1, NULL, NULL, vcbdata, GlueVerbExecCb, (void*)&vcbdata->thread);
    if (statusN != napi_ok) goto OnErrorExit;

    statusN= napi_create_async_work(env, NULL, uidN, GluJobThread, GluJobExit, ctxdata, &ctxdata->workerN);
    if (statusN != napi_ok) goto OnErrorExit;

    // Queue the work item for execution.
    statusN = napi_queue_async_work(env, ctxdata->workerN);
    if (statusN != napi_ok) goto OnErrorExit;

    return promiseN;

OnErrorExit:
    GLUE_RETURN_NULL;
}
