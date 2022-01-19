
#include <climits>  // INT_MAX
#include <cmath>
#include <algorithm> 
#include "env-inl.h"
#include "js_native_api_v8.h"
#include "js_native_api.h"
#include "util-inl.h"


napi_status napi_throw2(napi_env env, napi_value error) {
   GLUE_PREAMBLE(env);
   CHECK_ARG(env, error);
 
   v8::Isolate* isolate = env->isolate;
 
   isolate->ThrowException(v8impl::V8LocalValueFromJsValue(error));
   // any VM calls after this point and before returning
   // to the javascript invoker will fail
   return napi_clear_last_error(env);
 }


napi_status napi_exception(napi_env env, napi_value error) {
  GLUE_PREAMBLE(env);
  CHECK_ARG(env, error);
 
  v8::Isolate* isolate = env->isolate;
  v8::TryCatch* trycatch try_catch(isolate);
  v8::StackTrace stack = v8::Exception::GetStackTrace(trycatch);
  v8::Local<v8::StackTrace> stack =  v8::Exception::GetStackTrace(try_catch.Exception());

  int framcount = stack->GetFrameCount();
  int linenum   = frame->GetLineNumber();
  int colnum    = frame->GetColumn();

  napi_value funcname= frame->GetFunctionName();
  return funcname;
 }


/*
  TEST(CompileFunctionInContextScriptOrigin) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  LocalContext env;
  v8::ScriptOrigin origin(isolate, v8_str("test"), 22, 41);
  v8::ScriptCompiler::Source script_source(v8_str("throw new Error()"), origin);
  Local<ScriptOrModule> script;
  v8::Local<v8::Function> fun =
      v8::ScriptCompiler::CompileFunctionInContext(
          env.local(), &script_source, 0, nullptr, 0, nullptr,
          v8::ScriptCompiler::CompileOptions::kNoCompileOptions,
          v8::ScriptCompiler::NoCacheReason::kNoCacheNoReason, &script)
          .ToLocalChecked();
  CHECK(!fun.IsEmpty());
  CHECK(!script.IsEmpty());
  CHECK(script->GetResourceName()->StrictEquals(v8_str("test")));
  v8::TryCatch try_catch(CcTest::isolate());
  CcTest::isolate()->SetCaptureStackTraceForUncaughtExceptions(true);
  CHECK(fun->Call(env.local(), env->Global(), 0, nullptr).IsEmpty());
  CHECK(try_catch.HasCaught());
  CHECK(!try_catch.Exception().IsEmpty());
  v8::Local<v8::StackTrace> stack =
      v8::Exception::GetStackTrace(try_catch.Exception());
  CHECK(!stack.IsEmpty());
  CHECK_GT(stack->GetFrameCount(), 0);
  v8::Local<v8::StackFrame> frame = stack->GetFrame(CcTest::isolate(), 0);
  CHECK_EQ(23, frame->GetLineNumber());
  CHECK_EQ(42 + strlen("throw "), static_cast<unsigned>(frame->GetColumn()));
} */