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
#pragma once

#include <node_api.h>
#include <json-c/json.h>

/*
void napiPrintMsg (enum afb_syslog_levels level, napi_value self, napi_value args);

#define GLUE_AFB_ERROR(Glue,...)   napiInfoDbg (Glue, AFB_SYSLOG_LEVEL_ERROR, __func__, __VA_ARGS__);

void napiRqtAddref(AfbHandleT *napiRqt);
void napiRqtUnref(AfbHandleT *napiRqt);

afb_api_t GlueGetApi(AfbHandleT*glue);
const char *napiPushAfbReply (napi_value responseP, int start, unsigned nreplies, const afb_data_t *replies);

*/

typedef struct {
    napi_env env;
    napi_ref ref;
} napiJsoncUserDataT;

#define GLUE_AFB_INFO(Glue,...)    GlueVerbose (Glue,AFB_SYSLOG_LEVEL_INFO,__file__,__LINE__,__func__,__VA_ARGS__)
#define GLUE_AFB_NOTICE(Glue,...)  GlueVerbose (Glue,AFB_SYSLOG_LEVEL_NOTICE,__file__,__LINE__,__func__,__VA_ARGS__)
#define GLUE_AFB_WARNING(Glue,...) GlueVerbose (Glue,AFB_SYSLOG_LEVEL_WARNING,__file__,__LINE__,__func__,__VA_ARGS__)
#define GLUE_AFB_ERROR(Glue,...)   GlueVerbose (Glue,AFB_SYSLOG_LEVEL_ERROR,__file__,__LINE__,__func__,__VA_ARGS__)

afb_api_t GlueGetApi(AfbHandleT*glue);
void GlueVerbose(AfbHandleT *handle, int level, const char *file, int line, const char *func, const char *fmt, ...);
void GlueRqtFree(void *userdata);
void GlueRqtAddref(AfbHandleT *glue);
void GlueRqtUnref(AfbHandleT *glue);
AfbHandleT *GlueRqtNew(afb_req_t afbRqt);
void GlueInfoCb(afb_req_t afbRqt, unsigned nparams, afb_data_t const params[]);
void GlueVerbose(AfbHandleT *handle, int level, const char *file, int line, const char *func, const char *fmt, ...);
#define GLUE_AFB_INFO(Glue,...)    GlueVerbose (Glue,AFB_SYSLOG_LEVEL_INFO,__file__,__LINE__,__func__,__VA_ARGS__)
#define GLUE_AFB_NOTICE(Glue,...)  GlueVerbose (Glue,AFB_SYSLOG_LEVEL_NOTICE,__file__,__LINE__,__func__,__VA_ARGS__)
#define GLUE_AFB_WARNING(Glue,...) GlueVerbose (Glue,AFB_SYSLOG_LEVEL_WARNING,__file__,__LINE__,__func__,__VA_ARGS__)
#define GLUE_AFB_ERROR(Glue,...)   GlueVerbose (Glue,AFB_SYSLOG_LEVEL_ERROR,__file__,__LINE__,__func__,__VA_ARGS__)
#define GLUE_DBG_ERROR(Glue,...) NapiInfoDbg (Glue, AFB_SYSLOG_LEVEL_ERROR, __func__, __VA_ARGS__);

json_object *napiJsonDbg(napi_env env, const char *code, const char *message);
napi_valuetype napiGetType (napi_env env, napi_value valueN);

void napiPrintMsg (enum afb_syslog_levels level, napi_env env, napi_callback_info cbInfo);
napi_ref napiJsoncToRef(napi_env env, json_object *valueJ);
json_object *napiRefToJsonc(napi_env env, napi_ref valueR);
json_object *napiValuetoJsonc(napi_env env, napi_value valueN);
napi_value napiJsoncToValue(napi_env env, json_object *valueJ);
int GlueReply(AfbHandleT *glue, long status, long nbreply, afb_data_t *reply);