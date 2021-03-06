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
 */

#include <node_api.h>
#include <wrap-json.h>

#include "js_native_api_types.h"
#include "node-afb.h"
#include "node-utils.h"

#include <glue-afb.h>
#include <glue-utils.h>

typedef struct {
    GlueHandleT *glue;
    afb_data_t const *args;
    unsigned argc;
    GlueCtxThreadT thread;
} GlueCtxDeferredT;

void GlueTimerCb (afb_timer_x4_t timer, void *userdata, int decount);
void GlueApiSubcallCb(void *userdata, int status, unsigned nreplies, afb_data_t const replies[], afb_api_t api);
void GlueRqtSubcallCb(void *userdata, int status, unsigned nreplies, afb_data_t const replies[], afb_req_t req);
void GlueApiPromiseCb(void *userdata, int status, unsigned nreplies, afb_data_t const replies[], afb_api_t api);
void GlueRqtPromiseCb(void *userdata, int status, unsigned nreplies, afb_data_t const replies[], afb_req_t req);
napi_value GluePromiseStartCb(napi_env env, GlueCtxThreadT *thread);
void GlueEventCb(void *userdata, const char *event_name,	unsigned nparams, afb_data_x4_t const params[],	afb_api_t api);
void GlueInfoCb(afb_req_t afbRqt, unsigned nparams, afb_data_t const params[]);
int  GlueStartupCb(void *config, void *userdata);
int  GlueCtrlCb(afb_api_t apiv4, afb_ctlid_t ctlid, afb_ctlarg_t ctlarg, void *userdata);
void GlueRqtVerbCb(afb_req_t afbRqt, unsigned nparams, afb_data_t const params[]);
napi_value GlueJobStartCb (napi_env env, GlueHandleT *glue, napi_ref callbackR, int64_t timeout, napi_ref userdataR);
void GlueJobPostCb (int signum, void *userdata);
void GlueFreeHandleCb(napi_env env, GlueHandleT *handle);
void GlueFreeExternalCb (napi_env env, void* data, void* hint);