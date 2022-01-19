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
 * Reference:
 * https://github.com/nodejs/node-addon-examples
 * https://nodejs.org/api/n-api.html
 */

#define DECLARE_GLUE_METHOD(name, func) { name, 0, func, 0, 0, 0, napi_default, 0 }

#include <assert.h>
#include <node_api.h>
#include <stdio.h>
#include <json-c/json.h>

#define GLUE_GLUE_MAX_LABEL 256

json_object *napiValuetoJsonc(napi_env env, napi_value valueN)
{
    const char* errorCode="napiValuetoJsonc", *errorMsg="Fail to convert nodeObj to JsonC";
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
                if (!slotJ)  goto OnErrorExit;
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

static napi_value napiPingTest(napi_env env, napi_callback_info info)
{
    napi_status statusN;
    napi_value  resultN, globalN;
    static int64_t count=0;
    fprintf (stderr, "From napiPingTest count=%ld\n", count);
    statusN= napi_create_int64 (env, count++, &resultN);
    assert(statusN == napi_ok);

    // get arguments
    size_t argc= 1;
    napi_value args[1];
    statusN = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (statusN != napi_ok || argc != 1) goto OnErrorExit;

    napi_get_global(env, &globalN);
    statusN = napi_call_function(env, globalN, args[0], 0, NULL, &resultN);

    if (statusN == napi_pending_exception) {
        napi_value exceptionN;

        statusN= napi_get_and_clear_last_exception(env, &exceptionN);
        json_object *errorJ= napiValuetoJsonc(env, exceptionN);

        fprintf (stderr, "exception=%s\n", json_object_get_string(errorJ));
    }



    return resultN;

OnErrorExit:
    fprintf (stderr, "**** hoops\n");
    return NULL;
}

//void Initialize(v8::Local<v8::Object> exports, v8::Local<v8::Value> module, void* priv) {
static napi_value Init(napi_env env, napi_value exports) {    
  napi_status statusN;
  napi_property_descriptor desc = DECLARE_GLUE_METHOD("ping", napiPingTest);
  statusN = napi_define_properties(env, exports, 1, &desc);
  assert(statusN == napi_ok);
  return exports;
}

GLUE_MODULE(TARGET_NAME, Init) 

// statusN = napi_get_named_property(env, global, "AddTwo", &add_two);