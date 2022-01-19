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

#define GLUE_GLUE_MAX_LABEL 256

static void napiFreeJsonCtx (json_object *configJ, void *userdata) {
    napiJsoncUserDataT *napi= (napiJsoncUserDataT*) userdata;
    napi_delete_reference(napi->env, napi->ref);
    free (userdata);
}
void GlueRqtFree(void *userdata)
{
    AfbHandleT *glue= (AfbHandleT*)userdata;
    assert (glue && glue->magic == GLUE_RQT_MAGIC);

    free(glue);
    return;
}

// add a reference on Glue handle
void GlueRqtAddref(AfbHandleT *glue) {
    if (glue->magic == GLUE_RQT_MAGIC) {
        afb_req_unref (glue->rqt.afb);
    }
}

// add a reference on Glue handle
void GlueRqtUnref(AfbHandleT *glue) {
    if (glue->magic == GLUE_RQT_MAGIC) {
        afb_req_unref (glue->rqt.afb);
    }
}

// allocate and push a node request handle
AfbHandleT *GlueRqtNew(afb_req_t afbRqt)
{
    assert(afbRqt);

    // retreive interpreteur from API
    AfbHandleT *api = afb_api_get_userdata(afb_req_get_api(afbRqt));
    assert(api->magic == GLUE_API_MAGIC);

    AfbHandleT *glue = (AfbHandleT *)calloc(1, sizeof(AfbHandleT));
    glue->magic = GLUE_RQT_MAGIC;
    glue->rqt.afb = afbRqt;
    glue->env= api->env;

    // add node rqt handle to afb request livecycle
    afb_req_v4_set_userdata (afbRqt, (void*)glue, GlueRqtFree);

    return glue;
}

// retreive API from node handle
afb_api_t GlueGetApi(AfbHandleT*glue) {
   afb_api_t afbApi;
    switch (glue->magic) {
        case GLUE_API_MAGIC:
            afbApi= glue->api.afb;
            break;
        case GLUE_RQT_MAGIC:
            afbApi= afb_req_get_api(glue->rqt.afb);
            break;
        case GLUE_BINDER_MAGIC:
            afbApi= AfbBinderGetApi(glue->binder.afb);
            break;
        case GLUE_LOCK_MAGIC:
            afbApi= glue->lock.apiv4;
            break;
        case GLUE_EVT_MAGIC:
            afbApi= glue->evt.apiv4;
            break;
        default:
            afbApi=NULL;
    }
    return afbApi;
}

void GlueVerbose(AfbHandleT *handle, int level, const char *file, int line, const char *func, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    switch (handle->magic)
    {
    case GLUE_API_MAGIC:
    case GLUE_EVT_MAGIC:
    case GLUE_LOCK_MAGIC:
        afb_api_vverbose(GlueGetApi(handle), level, file, line, func, fmt, args);
        break;

    case GLUE_RQT_MAGIC:
        afb_req_vverbose(handle->rqt.afb, level, file, line, func, fmt, args);
        break;

    default:
        vverbose(level, file, line, func, fmt, args);
        break;
    }
    return;
}

json_object *napiJsonDbg(napi_env env, const char *code, const char *message)
{
    napi_status statusN;
    const char  *nodeError=NULL;
    json_object *errorJ;

    const napi_extended_error_info* errInfo;
    statusN= napi_get_last_error_info (env, &errInfo);
    if (statusN == napi_ok) {
        nodeError= errInfo->error_message;
    }

    wrap_json_pack(&errorJ, "{ss* ss* ss*}", "code", code, "message", message, "error", nodeError);
    return (errorJ);
}

// reference: https://bbs.archlinux.org/viewtopic.php?id=31087
void napiPrintMsg (enum afb_syslog_levels level, napi_env env, napi_callback_info cbInfo) {
    const char *errorCode=NULL, *errorMsg= "syntax afbprint(handle, format, ...)";
    napi_status statusN;

    char const *filename=NULL;
    char const *funcname=NULL;
    int linenum=0;

    if (level > AFB_SYSLOG_LEVEL_NOTICE) {
        const napi_extended_error_info* errInfo;
        statusN= napi_get_last_error_info (env, &errInfo);

        if (statusN == napi_ok) {
            errorMsg= errInfo->error_message;
        }
    }

    // get argument count
    size_t argc;
    napi_get_cb_info(env, cbInfo, &argc, NULL, NULL, NULL);
    napi_value args[argc];

    // get arguments
    statusN = napi_get_cb_info(env, cbInfo, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc < GLUE_TWO_ARG) goto OnErrorExit;

    // retreive afb glue handle from 1st argument
    AfbHandleT *handle;
    statusN= napi_get_value_external (env, args[0], (void**)&handle);
    if (statusN != napi_ok) goto OnErrorExit;

    // retreive format from second argument
    size_t len;
    statusN = napi_get_value_string_utf8(env, args[1], NULL, 0, &len);
    if (statusN != napi_ok) goto OnErrorExit;
    char *format= alloca (len+1);
    statusN = napi_get_value_string_utf8(env, args[1], format, len+1, &len);
    if (statusN != napi_ok) goto OnErrorExit;

    if (argc > GLUE_TWO_ARG) {
        int count=0, index=0;
        void *param[10];
        json_object *paramJ[10];

        for (int idx=GLUE_TWO_ARG; idx < argc; idx ++) {
            napi_valuetype argsType;

          	statusN = napi_typeof(env, args[idx], &argsType);
            if (statusN != napi_ok)  goto OnErrorExit;

            switch (argsType) {

                case napi_null: {
    	            param[count++]=NULL;
                    break;
                }
                case napi_boolean: {
      	            bool isFlag;
                    statusN = napi_get_value_bool(env, args[idx], &isFlag);
                    if (statusN != napi_ok) goto OnErrorExit;
                    param[count++]=(void*)isFlag;
                    break;
                }
                case napi_number: {
                    int64_t nombre;
                    statusN = napi_get_value_int64(env, args[idx], &nombre);
                    if (statusN != napi_ok) goto OnErrorExit;
                    param[count++]=(void*)nombre;
                    break;
                }
                case napi_string: {
                    size_t len;
                    statusN = napi_get_value_string_utf8(env, args[idx], NULL, 0, &len);
                    if (statusN != napi_ok) goto OnErrorExit;
                    char *argString= alloca(len+1);
                    statusN = napi_get_value_string_utf8(env, args[idx], argString, len+1, &len);
                    if (statusN != napi_ok) goto OnErrorExit;
                    param[count++]=(void*)argString;
                    break;
                }
                default: 
                    paramJ[index]= napiValuetoJsonc(env, args[idx]);
                    if (!paramJ[index]) param[count++]=NULL;
                    else {
                        param[count++]= (void*)json_object_get_string(paramJ[index]);
                        index ++;
                    }
            }   

            // reach max number of arguments
            if (count == sizeof(param)/sizeof(void*)) break;
        }
        GlueVerbose(handle, level, filename, linenum, funcname, format, param[0],param[1],param[2],param[3],param[4],param[5],param[6],param[7],param[8],param[9]);

        // release json object of nay
        for (int idx=0; idx < index; idx++) json_object_put(paramJ[idx]);

    } else {
        GlueVerbose(handle, level, filename, linenum, funcname, format);
    }
    return;

OnErrorExit:
    napi_throw_type_error (env, errorCode, errorMsg);
}

napi_valuetype napiGetType (napi_env env, napi_value valueN) {
    napi_valuetype typeN;
    napi_status statusN;
  	statusN = napi_typeof(env, valueN, &typeN);
    if (statusN != napi_ok) goto OnErrorExit;
    return typeN;

OnErrorExit:    
    return -1;
}

json_object *napiValuetoJsonc(napi_env env, napi_value valueN)
{
    const char* errorCode=NULL, *errorMsg="Fail to convert nodeObj to JsonC";
	json_object *resultJ=NULL;
	napi_valuetype typeN;
	napi_status statusN;
	int err;

	statusN = napi_typeof(env, valueN, &typeN);
    if (statusN != napi_ok)  goto OnErrorExit;

    switch (typeN) {

    case napi_null: {
    	resultJ = NULL;
        break;
    }
    case napi_boolean: {
      	bool isFlag;

        statusN = napi_get_value_bool(env, valueN, &isFlag);
        if (statusN != napi_ok) goto OnErrorExit;
        resultJ = json_object_new_boolean(isFlag);
        break;
    }

    case napi_number: {
        // node does not differentiate double and integer !!!
      	double number;
        int64_t nombre;

        statusN = napi_get_value_double(env, valueN, &number);
        if (statusN != napi_ok) goto OnErrorExit;
        nombre = (int64_t)number; // evil trick to determine wether n fits in an integer. (stolen from ltcl.c)
        if (number == nombre) resultJ = json_object_new_int64(nombre);
        else  resultJ = json_object_new_double(number);   
        break;
    }
    case napi_string: {
        size_t len;
        statusN = napi_get_value_string_utf8(env, valueN, NULL, 0, &len);
        if (statusN != napi_ok) goto OnErrorExit;
        char valString[len+1];
        statusN = napi_get_value_string_utf8(env, valueN, valString, len+1, &len);
        if (statusN != napi_ok) goto OnErrorExit;
        resultJ = json_object_new_string_len(valString, (int)len);
        if (!resultJ)  goto OnErrorExit;
        break;
    }
  
    case napi_object: {
        bool isArray;
        statusN = napi_is_array(env, valueN, &isArray);
        if (statusN)  goto OnErrorExit;
        if (isArray) {
            uint32_t count;
            resultJ = json_object_new_array();
            if (!resultJ)  goto OnErrorExit;

            statusN = napi_get_array_length(env, valueN, &count);
            if (statusN != napi_ok)  goto OnErrorExit;

            for (int idx = 0 ; idx < count ; idx++) {
                napi_value slotN;
                json_object *slotJ;
                statusN = napi_get_element(env, valueN, idx, &slotN);
                if (statusN != napi_ok)  goto OnErrorExit;
                slotJ = napiValuetoJsonc(env, slotN);
                if (!slotJ)  {
                    goto OnErrorExit;
                }
                err = json_object_array_add(resultJ, slotJ);
                if (err < 0) {
                    json_object_put(slotJ);
                    goto OnErrorExit;
                }
            }
        } else { // json object
         	napi_value keysN;
            size_t len;
            uint32_t count;

            resultJ = json_object_new_object();
            if (!resultJ) goto OnErrorExit;
            statusN = napi_get_property_names(env, valueN, &keysN);
            if (statusN != napi_ok)  goto OnErrorExit;
            statusN = napi_get_array_length(env, keysN, &count);
            if (statusN != napi_ok)  goto OnErrorExit;

            for (int idx = 0 ; idx < count ; idx++) {
                char label[GLUE_GLUE_MAX_LABEL];
                napi_value labelN, slotN;

                statusN = napi_get_element(env, keysN, idx, &labelN);
                if (statusN != napi_ok) goto OnErrorExit;
                statusN = napi_get_property(env, valueN, labelN, &slotN);
                if (statusN != napi_ok) goto OnErrorExit;

                statusN = napi_get_value_string_utf8(env, labelN, label, sizeof(label), &len);
                if (statusN != napi_ok || len == sizeof(label)) {
                    errorCode= "Label-TooLong";
                    goto OnErrorExit;
                }

                json_object *slotJ = napiValuetoJsonc(env, slotN);
                if (!slotJ)goto OnErrorExit;
                json_object_object_add(resultJ, label, slotJ);
            }
        }
        break;
    }

    case napi_function: {
        // keep track of function as json userdata
        napi_ref callbackR;
        resultJ = json_object_new_string("nodeCB");
        statusN= napi_create_reference(env, valueN, 1, &callbackR);
        if (statusN != napi_ok) goto OnErrorExit;

        // attach napi object reference to jsonc object
        napiJsoncUserDataT *userdata= calloc(1,sizeof(napiJsoncUserDataT));
        userdata->ref= callbackR;
        userdata->env= env;
        json_object_set_userdata(resultJ, userdata, napiFreeJsonCtx);
        break;
    }
    case napi_symbol:
    case napi_undefined:
    case napi_external:
    default:
        goto OnErrorExit;
    }

	return resultJ;

OnErrorExit:
    napi_throw_type_error(env, errorCode, errorMsg);
    if (resultJ) json_object_put(resultJ);
    return NULL;
}

json_object *napiRefToJsonc(napi_env env, napi_ref valueR) {
    json_object *valueJ;
    napi_value valueN;
    napi_status statusN;

    statusN= napi_get_reference_value(env, valueR, &valueN);
    if (statusN != napi_ok) {
        napi_throw_type_error(env, "empty-ref", "No value attached to reference");
        goto OnErrorExit;
    }
    valueJ= napiValuetoJsonc (env, valueN);

    return valueJ;
OnErrorExit:
    return NULL;    
}

napi_value napiJsoncToValue(napi_env env, json_object *valueJ)
{
	napi_status statusN;
    napi_value  resultN;

	switch (json_object_get_type(valueJ)) {
	case json_type_boolean: {
		json_bool jval = json_object_get_boolean(valueJ);
        // Note: get_boolean create a boolean value !!!
		statusN = napi_get_boolean(env, jval, &resultN);
		if (statusN != napi_ok) goto OnErrorExit;
		break;
	}
	case json_type_double: {
		double jval = json_object_get_double(valueJ);
		statusN = napi_create_double(env, jval, &resultN);
		if (statusN != napi_ok) goto OnErrorExit;
		break;
	}
	case json_type_int: {
		int jval = json_object_get_int(valueJ);
		statusN = napi_create_int32(env, jval, &resultN);
		if (statusN != napi_ok) goto OnErrorExit;
		break;
	}
	case json_type_object: {
		statusN = napi_create_object(env, &resultN);
		if (statusN != napi_ok) goto OnErrorExit;
		json_object_object_foreach(valueJ, key, slotJ) {
			napi_value keyN;
			statusN = napi_create_string_utf8(env, key, strlen(key), &keyN);
			if (statusN != napi_ok) goto OnErrorExit;

            napi_value slotN= napiJsoncToValue (env, slotJ);
			if (!slotN) goto OnErrorExit;

			statusN = napi_set_property(env, resultN, keyN, slotN);
			if (statusN != napi_ok) goto OnErrorExit;
		}
		break;
	}
	case json_type_array: {
		size_t count = json_object_array_length(valueJ);
		statusN = napi_create_array_with_length(env, count, &resultN);
		if (statusN != napi_ok) goto OnErrorExit;

		for (int idx = 0; idx < count; idx++) {
			json_object* slotJ = json_object_array_get_idx(valueJ, idx);
            napi_value slotN= napiJsoncToValue (env, slotJ);
			if (statusN != napi_ok) goto OnErrorExit;
			statusN = napi_set_element(env, resultN, idx, slotN);
			if (statusN != napi_ok) goto OnErrorExit;
		}
		break;
	}
	case json_type_string: {
		const char * jval = json_object_get_string(valueJ);
		statusN = napi_create_string_utf8(env, jval, json_object_get_string_len(valueJ), &resultN);
		if (statusN != napi_ok) goto OnErrorExit;
		break;
	}
	case json_type_null: {
		statusN =  napi_get_null(env, &resultN);
		if (statusN != napi_ok) goto OnErrorExit;
		break;
	}
	}
	return resultN;

OnErrorExit:
    napi_throw_type_error(env, "invalid-jsonc", "convertion not supported");
	return NULL;
}

napi_ref napiJsoncToRef(napi_env env, json_object *valueJ) {
    napi_ref valueR;
    napi_value valueN;
    napi_status statusN;

    valueN= napiJsoncToValue(env, valueJ);
    if (!valueN) goto OnErrorExit;

    statusN= napi_create_reference(env, valueN, 1, &valueR);
    if (statusN != napi_ok) goto OnErrorExit;
    return valueR;

OnErrorExit:
    return NULL;    
}


// reply afb request only once and unref py handle
int GlueReply(AfbHandleT *glue, int64_t status, int64_t nbreply, afb_data_t *reply)
{
    if (glue->rqt.replied) goto OnErrorExit;
    afb_req_reply(glue->rqt.afb, (int)status, (int)nbreply, reply);
    glue->rqt.replied = 1;
    return 0;

OnErrorExit:
    GLUE_AFB_ERROR(glue, "unique response require");
    return -1;
}


/*

// retreive subcall response and build node response
const char *nodePushAfbReply (napi_value resultP, int start, unsigned nreplies, const afb_data_t *replies) {
    const char *errorMsg=NULL;

    for (int idx = 0; idx < nreplies; idx++)
    {
        if (replies[idx]) {
            switch (afb_typeid(afb_data_type(replies[idx])))  {

                case Afb_Typeid_Predefined_Stringz: {
                    const char *valueN= (char*)afb_data_ro_pointer(replies[idx]);
                    if (valueN && valueN[0]) {
                        nodeTuple_SetItem(resultP, idx+start, nodeUnicode_FromString(valueN));
                    } else {
                        nodeTuple_SetItem(resultP, idx+start, node_None);
                    }
                    break;
                }
                case Afb_Typeid_Predefined_Bool: {
                    const long *valueN= (long*)afb_data_ro_pointer(replies[idx]);
                    nodeTuple_SetItem(resultP, idx+start, nodeBool_FromLong(*valueN));
                    break;
                }
                case Afb_Typeid_Predefined_I8:
                case Afb_Typeid_Predefined_U8:
                case Afb_Typeid_Predefined_I16:
                case Afb_Typeid_Predefined_U16:
                case Afb_Typeid_Predefined_I64:
                case Afb_Typeid_Predefined_U64:
                case Afb_Typeid_Predefined_I32:
                case Afb_Typeid_Predefined_U32: {
                    const long *valueN= (long*)afb_data_ro_pointer(replies[idx]);
                    nodeTuple_SetItem(resultP, idx+start, nodeBool_FromLong(*valueN));
                    break;
                }
                case Afb_Typeid_Predefined_Double:
                case Afb_Typeid_Predefined_Float: {
                    const double *valueN= (double*)afb_data_ro_pointer(replies[idx]);
                    nodeTuple_SetItem(resultP, idx+start, nodeFloat_FromDouble(*valueN));
                    break;
                }

                case  Afb_Typeid_Predefined_Json: {
                    afb_data_t data;
                    json_object *valueJ;
                    int err;

                    err = afb_data_convert(replies[idx], &afb_type_predefined_json_c, &data);
                    if (err) {
                        errorMsg= "unsupported json string";
                        goto OnErrorExit;
                    }
                    valueJ= (json_object*)afb_data_ro_pointer(data);
                    nodeTuple_SetItem(resultP, idx+start, jsonTonodeObj(valueJ));
                    afb_data_unref(data);
                    break;
                }
                case  Afb_Typeid_Predefined_Json_C: {
                    json_object *valueJ= (json_object*)afb_data_ro_pointer(replies[idx]);
                    if (valueJ) {
                        nodeTuple_SetItem(resultP, idx+start, jsonTonodeObj(valueJ));
                    } else {
                        nodeTuple_SetItem(resultP, idx+start, node_None);
                    }
                    break;
                }
                default:
                    errorMsg= "unsupported return data typeN";
                    goto OnErrorExit;
            }
        }
    }
    return NULL;

OnErrorExit:
    return errorMsg;
}


void napiInfoDbg (AfbHandleT *handle, enum afb_syslog_levels level, const char*funcname, const char * format, ...) {
    char const *cbInfo, *filename;
    int linenum;
    va_list args;

    //nodeErr_Print();
    napi_value typeP, *valueP, *tracebackP;
    nodeErr_Fetch(&typeP, &valueP, &tracebackP);
    if (valueP) cbInfo= nodeUnicode_AsUTF8(valueP);
    if (tracebackP) {
        nodeTracebackObject* traceback = (nodeTracebackObject*)tracebackP;
        linenum= traceback->tb_lineno;
        filename= nodeUnicode_AsUTF8(traceback->tb_frame->f_code->co_filename);
        if (filename) funcname=filename;
    }

    GlueVerbose(handle, level, cbInfo, linenum, funcname, format, args);
}


void nodeFreeJsonCtx (json_object *configJ, void *userdata) {
    napi_value nodeObj= (nodeObject*) userdata;
    node_DecRef (nodeObj);
}


// add a reference on Glue handle
void nodeRqtAddref(AfbHandleT *glue) {
    if (glue->magic == GLUE_RQT_MAGIC) {
        afb_req_unref (glue->rqt.afb);
    }
}

// add a reference on Glue handle
void nodeRqtUnref(AfbHandleT *glue) {
    if (glue->magic == GLUE_RQT_MAGIC) {
        afb_req_unref (glue->rqt.afb);
    }

}

typedef struct {
    pthread_once_t *once;
    void *ctx;
} nodeThreadUserData;


// reply afb request only once and unref node handle
int GlueReply(AfbHandleT *glue, long statusN, long nbreply, afb_data_t *reply)
{
    if (glue->rqt.replied) goto OnErrorExit;
    afb_req_reply(glue->rqt.afb, (int)statusN, (int)nbreply, reply);
    glue->rqt.replied = 1;
    return 0;

OnErrorExit:
    GLUE_AFB_ERROR(glue, "unique response require");
    return -1;
}
*/