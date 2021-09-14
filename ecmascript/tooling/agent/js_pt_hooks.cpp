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

#include "ecmascript/tooling/agent/js_pt_hooks.h"
#include "ecmascript/tooling/agent/js_backend.h"

namespace panda::tooling::ecmascript {
void JSPtHooks::Breakpoint([[maybe_unused]] PtThread thread, const PtLocation &location)
{
    if (thread.GetId() != ManagedThread::NON_INITIALIZED_THREAD_ID) {
        // Skip none-js thread
        return;
    }
    LOG(DEBUG, DEBUGGER) << "JSPtHooks: Breakpoint => " << location.GetMethodId() << ": "
                         << location.GetBytecodeOffset();

    [[maybe_unused]] LocalScope scope(backend_->ecmaVm_);
    backend_->NotifyPaused(location, INSTRUMENTATION);
}

void JSPtHooks::Paused(PauseReason reason)
{
    LOG(DEBUG, DEBUGGER) << "JSPtHooks: Paused";

    [[maybe_unused]] LocalScope scope(backend_->ecmaVm_);
    backend_->NotifyPaused({}, reason);
}

void JSPtHooks::Exception(PtThread thread, [[maybe_unused]] const PtLocation &location,
                          [[maybe_unused]] PtObject exceptionObject, [[maybe_unused]] const PtLocation &catchLocation)
{
    if (thread.GetId() != ManagedThread::NON_INITIALIZED_THREAD_ID) {
        // Skip none-js thread
        return;
    }
    LOG(DEBUG, DEBUGGER) << "JSPtHooks: Exception";

    [[maybe_unused]] LocalScope scope(backend_->ecmaVm_);
    Local<JSValueRef> exception = DebuggerApi::GetException(backend_->ecmaVm_);
    DebuggerApi::ClearException(backend_->ecmaVm_);

    backend_->NotifyPaused({}, EXCEPTION);

    if (!exception->IsHole()) {
        DebuggerApi::SetException(backend_->ecmaVm_, exception);
    }
}

void JSPtHooks::SingleStep(PtThread thread, const PtLocation &location)
{
    if (thread.GetId() != ManagedThread::NON_INITIALIZED_THREAD_ID) {
        // Skip none-js thread
        return;
    }
    LOG(DEBUG, DEBUGGER) << "JSPtHooks: SingleStep => " << location.GetBytecodeOffset();

    [[maybe_unused]] LocalScope scope(backend_->ecmaVm_);
    if (firstTime_) {
        firstTime_ = false;

        backend_->NotifyPaused({}, BREAK_ON_START);
        return;
    }

    // pause or step complete
    if (backend_->StepComplete(location)) {
        backend_->NotifyPaused({}, OTHER);
    } else {
        backend_->ProcessCommand();
    }
}

void JSPtHooks::LoadModule(std::string_view pandaFileName)
{
    LOG(DEBUG, DEBUGGER) << "JSPtHooks: LoadModule: " << pandaFileName;

    [[maybe_unused]] LocalScope scope(backend_->ecmaVm_);
    static bool firstTime = true;
    if (firstTime) {
        firstTime = false;
        backend_->WaitForDebugger();
    }
    static int32_t scriptId = 0;

    if (backend_->NotifyScriptParsed(scriptId++, DebuggerApi::ConvertToString(pandaFileName.data()))) {
        firstTime_ = true;
    }
}
}  // namespace panda::tooling::ecmascript
