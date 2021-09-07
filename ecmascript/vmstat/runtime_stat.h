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

#ifndef ECMASCRIPT_VMSTAT_RUNTIME_STAT_H
#define ECMASCRIPT_VMSTAT_RUNTIME_STAT_H

#include "ecmascript/ecma_vm.h"
#include "ecmascript/js_thread.h"
#include "ecmascript/mem/c_containers.h"
#include "ecmascript/vmstat/caller_stat.h"

namespace panda::ecmascript {
class EcmaRuntimeStat {
public:
    // NOLINTNEXTLINE(modernize-avoid-c-arrays)
    explicit EcmaRuntimeStat(const char * const runtimeCallerNames[], int count);
    EcmaRuntimeStat() = default;
    virtual ~EcmaRuntimeStat() = default;

    DEFAULT_NOEXCEPT_MOVE_SEMANTIC(EcmaRuntimeStat);
    DEFAULT_COPY_SEMANTIC(EcmaRuntimeStat);

    void StartCount(PandaRuntimeTimer *timer, int callerId);
    void StopCount(const PandaRuntimeTimer *timer);
    CString GetAllStats() const;
    void ResetAllCount();
    void Print() const;

private:
    PandaRuntimeTimer *currentTimer_ = nullptr;
    CVector<PandaRuntimeCallerStat> callerStat_{};
};

class RuntimeTimerScope {
public:
    explicit RuntimeTimerScope(JSThread *thread, int callerId, EcmaRuntimeStat *stat)
    {
        bool statEnabled = thread->GetEcmaVM()->IsRuntimeStatEnabled();
        if (!statEnabled || stat == nullptr) {
            return;
        }
        stats_ = stat;
        stats_->StartCount(&timer_, callerId);
    }
    RuntimeTimerScope(const EcmaVM *vm, int callerId, EcmaRuntimeStat *stat)
    {
        bool statEnabled = vm->IsRuntimeStatEnabled();
        if (!statEnabled || stat == nullptr) {
            return;
        }
        stats_ = stat;
        stats_->StartCount(&timer_, callerId);
    }
    ~RuntimeTimerScope()
    {
        if (stats_ != nullptr) {
            stats_->StopCount(&timer_);
        }
    }
    NO_COPY_SEMANTIC(RuntimeTimerScope);
    NO_MOVE_SEMANTIC(RuntimeTimerScope);

private:
    PandaRuntimeTimer timer_{};
    EcmaRuntimeStat *stats_{nullptr};
};
}  // namespace panda::ecmascript
#endif
