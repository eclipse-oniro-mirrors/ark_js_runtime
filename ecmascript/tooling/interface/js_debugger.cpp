/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ecmascript/tooling/interface/js_debugger.h"

#include "ecmascript/base/builtins_base.h"
#include "ecmascript/ecma_macros.h"
#include "ecmascript/global_env.h"
#include "ecmascript/interpreter/fast_runtime_stub-inl.h"
#include "ecmascript/interpreter/frame_handler.h"
#include "ecmascript/interpreter/slow_runtime_stub.h"
#include "ecmascript/jspandafile/js_pandafile_manager.h"
#include "ecmascript/js_thread.h"
#include "ecmascript/tooling/interface/js_debugger_manager.h"
#include "ecmascript/napi/jsnapi_helper-inl.h"
#include "runtime/tooling/pt_method_private.h"

namespace panda::ecmascript::tooling {
using panda::ecmascript::Program;
using panda::ecmascript::base::BuiltinsBase;

bool JSDebugger::SetBreakpoint(const JSPtLocation &location, const Local<FunctionRef> &condFuncRef)
{
    JSMethod *method = FindMethod(location);
    if (method == nullptr) {
        LOG(ERROR, DEBUGGER) << "SetBreakpoint: Cannot find JSMethod";
        return false;
    }

    if (location.GetBytecodeOffset() >= method->GetCodeSize()) {
        LOG(ERROR, DEBUGGER) << "SetBreakpoint: Invalid breakpoint location";
        return false;
    }

    auto [_, success] = breakpoints_.emplace(method, location.GetBytecodeOffset(),
        Global<FunctionRef>(ecmaVm_, condFuncRef));
    if (!success) {
        // also return true
        LOG(WARNING, DEBUGGER) << "SetBreakpoint: Breakpoint already exists";
    }

    return true;
}

bool JSDebugger::RemoveBreakpoint(const JSPtLocation &location)
{
    JSMethod *method = FindMethod(location);
    if (method == nullptr) {
        LOG(ERROR, DEBUGGER) << "RemoveBreakpoint: Cannot find JSMethod";
        return false;
    }

    if (!RemoveBreakpoint(method, location.GetBytecodeOffset())) {
        LOG(ERROR, DEBUGGER) << "RemoveBreakpoint: Breakpoint not found";
        return false;
    }

    return true;
}

void JSDebugger::BytecodePcChanged(JSThread *thread, JSMethod *method, uint32_t bcOffset)
{
    ASSERT(bcOffset < method->GetCodeSize() && "code size of current JSMethod less then bcOffset");

    HandleExceptionThrowEvent(thread, method, bcOffset);

    // Step event is reported before breakpoint, according to the spec.
    if (!HandleStep(method, bcOffset)) {
        HandleBreakpoint(method, bcOffset);
    }
}

bool JSDebugger::HandleBreakpoint(const JSMethod *method, uint32_t bcOffset)
{
    auto breakpoint = FindBreakpoint(method, bcOffset);
    if (hooks_ == nullptr || !breakpoint.has_value()) {
        return false;
    }

    JSThread *thread = ecmaVm_->GetJSThread();
    auto condFuncRef = breakpoint.value().GetConditionFunction();
    if (condFuncRef->IsFunction()) {
        LOG(INFO, DEBUGGER) << "HandleBreakpoint: begin evaluate condition";
        auto handlerPtr = std::make_shared<InterpretedFrameHandler>(ecmaVm_->GetJSThread());
        auto res = DebuggerApi::EvaluateViaFuncCall(const_cast<EcmaVM *>(ecmaVm_),
            condFuncRef.ToLocal(ecmaVm_), handlerPtr);
        if (thread->HasPendingException()) {
            LOG(ERROR, DEBUGGER) << "HandleBreakpoint: has pending exception";
            thread->ClearException();
            return false;
        }
        bool isMeet = res->ToBoolean(ecmaVm_)->Value();
        if (!isMeet) {
            LOG(ERROR, DEBUGGER) << "HandleBreakpoint: condition not meet";
            return false;
        }
    }

    auto *pf = method->GetPandaFile();
    JSPtLocation location {pf->GetFilename().c_str(), method->GetFileId(), bcOffset};

    hooks_->Breakpoint(location);
    return true;
}

void JSDebugger::HandleExceptionThrowEvent(const JSThread *thread, const JSMethod *method, uint32_t bcOffset)
{
    if (hooks_ == nullptr || !thread->HasPendingException()) {
        return;
    }

    auto *pf = method->GetPandaFile();
    JSPtLocation throwLocation {pf->GetFilename().c_str(), method->GetFileId(), bcOffset};

    hooks_->Exception(throwLocation);
}

bool JSDebugger::HandleStep(const JSMethod *method, uint32_t bcOffset)
{
    if (hooks_ == nullptr) {
        return false;
    }

    auto *pf = method->GetPandaFile();
    JSPtLocation location {pf->GetFilename().c_str(), method->GetFileId(), bcOffset};

    return hooks_->SingleStep(location);
}

std::optional<JSBreakpoint> JSDebugger::FindBreakpoint(const JSMethod *method, uint32_t bcOffset) const
{
    for (const auto &bp : breakpoints_) {
        if (bp.GetBytecodeOffset() == bcOffset && bp.GetMethod()->GetPandaFile()->GetFilename() ==
            method->GetPandaFile()->GetFilename() && bp.GetMethod()->GetFileId() == method->GetFileId()) {
            return bp;
        }
    }

    return {};
}

bool JSDebugger::RemoveBreakpoint(const JSMethod *method, uint32_t bcOffset)
{
    for (auto it = breakpoints_.begin(); it != breakpoints_.end(); ++it) {
        const auto &bp = *it;
        if (bp.GetBytecodeOffset() == bcOffset && bp.GetMethod() == method) {
            it = breakpoints_.erase(it);
            return true;
        }
    }

    return false;
}

JSMethod *JSDebugger::FindMethod(const JSPtLocation &location) const
{
    JSMethod *method = nullptr;
    EcmaVM::GetJSPandaFileManager()->EnumerateJSPandaFiles([&method, location](
        const panda::ecmascript::JSPandaFile *jsPandaFile) {
        if (CString(location.GetPandaFile()) == jsPandaFile->GetJSPandaFileDesc()) {
            JSMethod *methodsData = jsPandaFile->GetMethods();
            uint32_t numberMethods = jsPandaFile->GetNumMethods();
            for (uint32_t i = 0; i < numberMethods; ++i) {
                if (methodsData[i].GetFileId() == location.GetMethodId()) {
                    method = methodsData + i;
                    return false;
                }
            }
        }
        return true;
    });
    return method;
}

JSTaggedValue JSDebugger::DebuggerSetValue(EcmaRuntimeCallInfo *argv)
{
    LOG(INFO, DEBUGGER) << "DebuggerSetValue: called";
    auto localEvalFunc = [](const EcmaVM *ecmaVm, InterpretedFrameHandler *frameHandler, int32_t regIndex,
        const CString &varName, Local<JSValueRef> value) {
        JSTaggedValue newVal = JSNApiHelper::ToJSTaggedValue(*value);
        frameHandler->SetVRegValue(regIndex, newVal);
        ecmaVm->GetJsDebuggerManager()->NotifyLocalScopeUpdated(varName, value);
        return newVal;
    };

    auto lexEvalFunc = [](const EcmaVM *ecmaVm, int32_t level, uint32_t slot,
        const CString &varName, Local<JSValueRef> value) {
        DebuggerApi::SetProperties(ecmaVm, level, slot, value);
        if (level == 0) {
            // local closure variable
            ecmaVm->GetJsDebuggerManager()->NotifyLocalScopeUpdated(varName, value);
        }
        return JSNApiHelper::ToJSTaggedValue(*value);
    };

    auto globalEvalFunc = [](const EcmaVM *ecmaVm, JSTaggedValue key,
        const CString &varName, JSTaggedValue value) {
        JSTaggedValue result = SetGlobalValue(ecmaVm, key, value);
        if (!result.IsException()) {
            return value;
        }

        JSThread *thread = ecmaVm->GetJSThread();
        CString msg = varName + " is not defined";
        THROW_REFERENCE_ERROR_AND_RETURN(thread, msg.data(), JSTaggedValue::Exception());
    };

    return Evaluate(argv, localEvalFunc, lexEvalFunc, globalEvalFunc);
}

JSTaggedValue JSDebugger::DebuggerGetValue(EcmaRuntimeCallInfo *argv)
{
    LOG(INFO, DEBUGGER) << "DebuggerGetValue: called";
    auto localEvalFunc = [](const EcmaVM *, InterpretedFrameHandler *frameHandler, int32_t regIndex,
        const CString &, Local<JSValueRef>) {
        return frameHandler->GetVRegValue(regIndex);
    };

    auto lexEvalFunc = [](const EcmaVM *ecmaVm, int32_t level, uint32_t slot,
        const CString &, Local<JSValueRef>) {
        Local<JSValueRef> valRef = DebuggerApi::GetProperties(ecmaVm, level, slot);
        return JSNApiHelper::ToJSTaggedValue(*valRef);
    };

    auto globalEvalFunc = [](const EcmaVM *ecmaVm, JSTaggedValue key,
        const CString &varName, JSTaggedValue isThrow) {
        JSTaggedValue globalVal = GetGlobalValue(ecmaVm, key);
        if (!globalVal.IsException()) {
            return globalVal;
        }

        JSThread *thread = ecmaVm->GetJSThread();
        if (isThrow.ToBoolean()) {
            CString msg = varName + " is not defined";
            THROW_REFERENCE_ERROR_AND_RETURN(thread, msg.data(), JSTaggedValue::Exception());
        }
        // in case of `typeof`, return undefined instead of exception
        thread->ClearException();
        return JSTaggedValue::Undefined();
    };

    return Evaluate(argv, localEvalFunc, lexEvalFunc, globalEvalFunc);
}

JSTaggedValue JSDebugger::Evaluate(EcmaRuntimeCallInfo *argv, LocalEvalFunc localEvalFunc,
    LexEvalFunc lexEvalFunc, GlobalEvalFunc globalEvalFunc)
{
    LOG(INFO, DEBUGGER) << "Evaluate: called";
    ASSERT(argv);
    JSThread *thread = argv->GetThread();
    const EcmaVM *ecmaVm = thread->GetEcmaVM();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);

    // The arg2 is the new value for setter and throw flag for getter
    JSHandle<JSTaggedValue> arg1 = BuiltinsBase::GetCallArg(argv, 0);
    JSHandle<JSTaggedValue> arg2 = BuiltinsBase::GetCallArg(argv, 1);
    CString varName = ConvertToString(arg1.GetTaggedValue());
    Local<JSValueRef> valRef = JSNApiHelper::ToLocal<JSValueRef>(arg2);
    LOG(INFO, DEBUGGER) << "Evaluate: varName = " << varName;

    auto &frameHandler = ecmaVm->GetJsDebuggerManager()->GetEvalFrameHandler();
    ASSERT(frameHandler);
    JSMethod *method = frameHandler->GetMethod();
    int32_t regIndex = -1;
    bool found = EvaluateLocalValue(method, thread, varName, regIndex);
    if (found) {
        LOG(ERROR, DEBUGGER) << "Evaluate: found regIndex = " << regIndex;
        return localEvalFunc(ecmaVm, frameHandler.get(), regIndex, varName, valRef);
    }

    int32_t level = 0;
    uint32_t slot = 0;
    found = DebuggerApi::EvaluateLexicalValue(ecmaVm, varName, level, slot);
    if (found) {
        LOG(ERROR, DEBUGGER) << "Evaluate: found level = " << level;
        return lexEvalFunc(ecmaVm, level, slot, varName, valRef);
    }

    return globalEvalFunc(ecmaVm, arg1.GetTaggedValue(), varName, arg2.GetTaggedValue());
}

JSTaggedValue JSDebugger::GetGlobalValue(const EcmaVM *ecmaVm, JSTaggedValue key)
{
    JSTaggedValue globalObj = ecmaVm->GetGlobalEnv()->GetGlobalObject();
    JSThread *thread = ecmaVm->GetJSThread();

    JSTaggedValue globalRec = SlowRuntimeStub::LdGlobalRecord(thread, key);
    if (!globalRec.IsUndefined()) {
        ASSERT(globalRec.IsPropertyBox());
        return PropertyBox::Cast(globalRec.GetTaggedObject())->GetValue();
    }

    JSTaggedValue globalVar = FastRuntimeStub::GetGlobalOwnProperty(thread, globalObj, key);
    if (!globalVar.IsHole()) {
        return globalVar;
    } else {
        return SlowRuntimeStub::TryLdGlobalByName(thread, globalObj, key);
    }

    return JSTaggedValue::Exception();
}

JSTaggedValue JSDebugger::SetGlobalValue(const EcmaVM *ecmaVm, JSTaggedValue key, JSTaggedValue newVal)
{
    JSTaggedValue globalObj = ecmaVm->GetGlobalEnv()->GetGlobalObject();
    JSThread *thread = ecmaVm->GetJSThread();

    JSTaggedValue globalRec = SlowRuntimeStub::LdGlobalRecord(thread, key);
    if (!globalRec.IsUndefined()) {
        return SlowRuntimeStub::TryUpdateGlobalRecord(thread, key, newVal);
    }

    JSTaggedValue globalVar = FastRuntimeStub::GetGlobalOwnProperty(thread, globalObj, key);
    if (!globalVar.IsHole()) {
        return SlowRuntimeStub::StGlobalVar(thread, key, newVal);
    }

    return JSTaggedValue::Exception();
}

bool JSDebugger::EvaluateLocalValue(JSMethod *method, JSThread *thread, const CString &varName, int32_t &regIndex)
{
    if (method->IsNative()) {
        LOG(ERROR, DEBUGGER) << "EvaluateLocalValue: native frame not support";
        THROW_TYPE_ERROR_AND_RETURN(thread, "native frame not support", false);
    }
    PtJSExtractor *extractor = thread->GetEcmaVM()->GetDebugInfoExtractor(method->GetPandaFile());
    if (extractor == nullptr) {
        LOG(ERROR, DEBUGGER) << "EvaluateLocalValue: extractor is null";
        THROW_TYPE_ERROR_AND_RETURN(thread, "extractor is null", false);
    }

    auto varInfos = extractor->GetLocalVariableTable(method->GetFileId());
    for (const auto &varInfo : varInfos) {
        if (varInfo.name == std::string(varName)) {
            regIndex = varInfo.reg_number;
            return true;
        }
    }

    return false;
}

void JSDebugger::Init()
{
    JSThread *thread = ecmaVm_->GetJSThread();
    [[maybe_unused]] EcmaHandleScope scope(thread);

    ObjectFactory *factory = ecmaVm_->GetFactory();
    constexpr int32_t NUM_ARGS = 2;
    JSHandle<JSTaggedValue> getter(factory->NewFromUtf8("debuggerGetValue"));
    SetGlobalFunction(getter, JSDebugger::DebuggerGetValue, NUM_ARGS);
    JSHandle<JSTaggedValue> setter(factory->NewFromUtf8("debuggerSetValue"));
    SetGlobalFunction(setter, JSDebugger::DebuggerSetValue, NUM_ARGS);
}

void JSDebugger::SetGlobalFunction(const JSHandle<JSTaggedValue> &funcName,
    EcmaEntrypoint nativeFunc, int32_t numArgs) const
{
    ObjectFactory *factory = ecmaVm_->GetFactory();
    JSThread *thread = ecmaVm_->GetJSThread();
    [[maybe_unused]] EcmaHandleScope scope(thread);
    JSHandle<GlobalEnv> env = ecmaVm_->GetGlobalEnv();
    JSHandle<JSObject> globalObject(thread, env->GetGlobalObject());

    JSHandle<JSFunction> jsFunc = factory->NewJSFunction(env, reinterpret_cast<void *>(nativeFunc));
    JSFunction::SetFunctionLength(thread, jsFunc, JSTaggedValue(numArgs));
    JSHandle<JSFunctionBase> baseFunc(jsFunc);
    JSHandle<JSTaggedValue> undefined = thread->GlobalConstants()->GetHandledUndefined();
    JSFunction::SetFunctionName(thread, baseFunc, funcName, undefined);

    JSHandle<JSFunction> funcHandle(jsFunc);
    PropertyDescriptor desc(thread, JSHandle<JSTaggedValue>(funcHandle), true, false, true);
    JSObject::DefineOwnProperty(thread, globalObject, funcName, desc);
}
}  // panda::ecmascript::tooling