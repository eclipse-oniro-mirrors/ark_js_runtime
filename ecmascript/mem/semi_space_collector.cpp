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

#include "ecmascript/mem/semi_space_collector-inl.h"

#include "ecmascript/ecma_vm.h"
#include "ecmascript/mem/clock_scope.h"
#include "ecmascript/mem/concurrent_marker.h"
#include "ecmascript/mem/mem_manager.h"
#include "ecmascript/mem/heap-inl.h"
#include "ecmascript/mem/object_xray-inl.h"
#include "ecmascript/mem/mark_stack.h"
#include "ecmascript/mem/mem.h"
#include "ecmascript/mem/parallel_marker-inl.h"
#include "ecmascript/mem/space-inl.h"
#include "ecmascript/mem/tlab_allocator-inl.h"
#include "ecmascript/runtime_call_id.h"
#include "ecmascript/vmstat/runtime_stat.h"

namespace panda::ecmascript {
SemiSpaceCollector::SemiSpaceCollector(Heap *heap, bool paralledGc)
    : heap_(heap), paralledGc_(paralledGc), workList_(heap->GetWorkList())
{
}

void SemiSpaceCollector::RunPhases()
{
    [[maybe_unused]] ecmascript::JSThread *thread = heap_->GetEcmaVM()->GetJSThread();
    INTERPRETER_TRACE(thread, SemiSpaceCollector_RunPhases);
    [[maybe_unused]] ClockScope clockScope;

    ECMA_BYTRACE_NAME(BYTRACE_TAG_ARK, "SemiSpaceCollector::RunPhases");
    bool concurrentMark = heap_->CheckConcurrentMark(thread);
    if (concurrentMark) {
        ECMA_GC_LOG() << "SemiSpaceCollector after ConcurrentMarking";
        heap_->GetConcurrentMarker()->Reset();  // HPPGC use mark result to move TaggedObject.
    }
    InitializePhase();
    ParallelMarkingPhase();
    SweepPhases();
    FinishPhase();
    heap_->GetEcmaVM()->GetEcmaGCStats()->StatisticSemiCollector(clockScope.GetPauseTime(), semiCopiedSize_,
                                                                 promotedSize_, commitSize_);
    ECMA_GC_LOG() << "SemiSpaceCollector::RunPhases " << clockScope.TotalSpentTime();
}

void SemiSpaceCollector::InitializePhase()
{
    ECMA_BYTRACE_NAME(BYTRACE_TAG_ARK, "SemiSpaceCollector::InitializePhase");
    heap_->Prepare();
    workList_->Initialize(TriggerGCType::SEMI_GC, ParallelGCTaskPhase::SEMI_HANDLE_GLOBAL_POOL_TASK);
    heap_->GetSemiGcMarker()->Initialized();
    heap_->GetEvacuationAllocator()->Initialize(TriggerGCType::SEMI_GC);
    promotedSize_ = 0;
    semiCopiedSize_ = 0;
    commitSize_ = heap_->GetFromSpace()->GetCommittedSize();
}

void SemiSpaceCollector::ParallelMarkingPhase()
{
    ECMA_BYTRACE_NAME(BYTRACE_TAG_ARK, "SemiSpaceCollector::ParallelMarkingPhase");
    auto region = heap_->GetOldSpace()->GetCurrentRegion();

    if (paralledGc_) {
        heap_->PostParallelGCTask(ParallelGCTaskPhase::SEMI_HANDLE_THREAD_ROOTS_TASK);
        heap_->PostParallelGCTask(ParallelGCTaskPhase::SEMI_HANDLE_SNAPSHOT_TASK);
        heap_->GetSemiGcMarker()->ProcessOldToNew(0, region);
    } else {
        heap_->GetSemiGcMarker()->ProcessOldToNew(0, region);
        heap_->GetSemiGcMarker()->ProcessSnapshotRSet(0);
        heap_->GetSemiGcMarker()->MarkRoots(0);
        heap_->GetSemiGcMarker()->ProcessMarkStack(0);
    }
    heap_->WaitRunningTaskFinished();

    auto totalThreadCount = Platform::GetCurrentPlatform()->GetTotalThreadNum() + 1;  // gc thread and main thread
    for (uint32_t i = 0; i < totalThreadCount; i++) {
        SlotNeedUpdate needUpdate(nullptr, ObjectSlot(0));
        while (workList_->GetSlotNeedUpdate(i, &needUpdate)) {
            UpdatePromotedSlot(needUpdate.first, needUpdate.second);
        }
    }
}

void SemiSpaceCollector::SweepPhases()
{
    ECMA_BYTRACE_NAME(BYTRACE_TAG_ARK, "SemiSpaceCollector::SweepPhases");
    auto totalThreadCount = Platform::GetCurrentPlatform()->GetTotalThreadNum() + 1;  // gc thread and main thread
    for (uint32_t i = 0; i < totalThreadCount; i++) {
        ProcessQueue *queue = workList_->GetWeakReferenceQueue(i);
        while (true) {
            auto obj = queue->PopBack();
            if (UNLIKELY(obj == nullptr)) {
                break;
            }
            ObjectSlot slot(ToUintPtr(obj));
            JSTaggedValue value(slot.GetTaggedType());
            auto header = value.GetTaggedWeakRef();
            MarkWord markWord(header);
            if (markWord.IsForwardingAddress()) {
                TaggedObject *dst = markWord.ToForwardingAddress();
                auto weakRef = JSTaggedValue(JSTaggedValue(dst).CreateAndGetWeakRef()).GetRawTaggedObject();
                slot.Update(weakRef);
            } else {
                slot.Update(static_cast<JSTaggedType>(JSTaggedValue::Undefined().GetRawData()));
            }
        }
    }

    auto stringTable = heap_->GetEcmaVM()->GetEcmaStringTable();
    WeakRootVisitor gcUpdateWeak = [](TaggedObject *header) {
        Region *objectRegion = Region::ObjectAddressToRange(reinterpret_cast<TaggedObject *>(header));
        if (!objectRegion->InYoungGeneration()) {
            return header;
        }

        MarkWord markWord(header);
        if (markWord.IsForwardingAddress()) {
            TaggedObject *dst = markWord.ToForwardingAddress();
            return dst;
        }
        return reinterpret_cast<TaggedObject *>(ToUintPtr(nullptr));
    };
    stringTable->SweepWeakReference(gcUpdateWeak);
    heap_->GetEcmaVM()->GetJSThread()->IterateWeakEcmaGlobalStorage(gcUpdateWeak);
    heap_->GetEcmaVM()->ProcessReferences(gcUpdateWeak);
    heap_->UpdateDerivedObjectInStack();
}

void SemiSpaceCollector::FinishPhase()
{
    ECMA_BYTRACE_NAME(BYTRACE_TAG_ARK, "SemiSpaceCollector::FinishPhase");
    workList_->Finish(semiCopiedSize_, promotedSize_);
    heap_->GetEvacuationAllocator()->Finalize(TriggerGCType::SEMI_GC);
}
}  // namespace panda::ecmascript
