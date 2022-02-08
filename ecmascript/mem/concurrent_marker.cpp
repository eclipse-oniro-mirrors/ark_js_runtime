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

#include "ecmascript/mem/concurrent_marker.h"

#include "ecmascript/mem/allocator-inl.h"
#include "ecmascript/mem/clock_scope.h"
#include "ecmascript/mem/heap-inl.h"
#include "ecmascript/mem/object_xray-inl.h"
#include "ecmascript/mem/mark_word.h"
#include "ecmascript/mem/parallel_marker-inl.h"
#include "ecmascript/mem/space-inl.h"
#include "ecmascript/mem/verification.h"
#include "ecmascript/platform/platform.h"
#include "os/mutex.h"

namespace panda::ecmascript {
ConcurrentMarker::ConcurrentMarker(Heap *heap)
    : heap_(heap),
      vm_(heap->GetEcmaVM()),
      thread_(vm_->GetJSThread()),
      workList_(heap->GetWorkList())
{
}

void ConcurrentMarker::ConcurrentMarking()
{
    ECMA_GC_LOG() << "ConcurrentMarker: Concurrent Mark Begin";

    heap_->Prepare();
    ECMA_BYTRACE_NAME(BYTRACE_TAG_ARK, "ConcurrentMarker::ConcurrentMarking");
    thread_->SetMarkStatus(MarkStatus::MARKING);
    if (!heap_->IsSemiMarkNeeded()) {
        heapObjectSize_ = heap_->GetHeapObjectSize();
        if (heap_->GetSweeper()->CanSelectCset()) {
            const_cast<OldSpace *>(heap_->GetOldSpace())->SelectCSet();
        }
    } else {
        heapObjectSize_ = heap_->GetNewSpace()->GetHeapObjectSize();
    }

    InitializeMarking();
    Platform::GetCurrentPlatform()->PostTask(std::make_unique<MarkerTask>(heap_));
    if (heap_->IsSemiMarkNeeded() && heap_->IsParallelGCEnabled()) {
        heap_->PostParallelGCTask(ParallelGCTaskPhase::CONCURRENT_HANDLE_OLD_TO_NEW_TASK);
    }
}

void ConcurrentMarker::FinishPhase()
{
    size_t aliveSize = 0;
    workList_->Finish(aliveSize);
}

void ConcurrentMarker::ReMarking()
{
    ECMA_GC_LOG() << "ConcurrentMarker: Remarking Begin";
    Marker *nonMoveMarker =  heap_->GetNonMovableMarker();
    nonMoveMarker->MarkRoots(0);
    if (heap_->IsSemiMarkNeeded() && !heap_->IsParallelGCEnabled()) {
        heap_->GetNonMovableMarker()->ProcessOldToNew(0);
    } else {
        nonMoveMarker->ProcessMarkStack(0);
    }
    heap_->WaitRunningTaskFinished();
}

void ConcurrentMarker::HandleMarkFinished()  // js-thread wait for sweep
{
    os::memory::LockHolder lock(waitMarkingFinishedMutex_);
    if (notifyMarkingFinished_) {
        heap_->CollectGarbage(TriggerGCType::OLD_GC);
    }
}

void ConcurrentMarker::WaitConcurrentMarkingFinished()  // call in EcmaVm thread, wait for mark finished
{
    os::memory::LockHolder lock(waitMarkingFinishedMutex_);
    if (!notifyMarkingFinished_) {
        vmThreadWaitMarkingFinished_ = true;
        waitMarkingFinishedCV_.Wait(&waitMarkingFinishedMutex_);
    }
}

void ConcurrentMarker::Reset(bool isClearCSet)
{
    FinishPhase();
    thread_->SetMarkStatus(MarkStatus::READY_TO_MARK);
    notifyMarkingFinished_ = false;
    duration_ = 0.0;
    if (isClearCSet) {
        // Mix space gc clear cset when evacuation allocator finalize
        const_cast<OldSpace *>(heap_->GetOldSpace())->ClearRegionFromCSet();
    }
    heap_->EnumerateRegions([this](Region *current) {
        current->SetMarking(false);
        current->ClearFlag(RegionFlags::IS_IN_PROMOTE_SET);
    });
}

// -------------------- privete method ------------------------------------------
void ConcurrentMarker::InitializeMarking()
{
    heap_->EnumerateRegions([](Region *region) {
        // ensure mark bitmap
        auto markBitmap = region->GetMarkBitmap();
        if (markBitmap == nullptr) {
            region->GetOrCreateMarkBitmap();
        } else {
            markBitmap->ClearAllBits();
        }
        auto rememberset = region->GetCrossRegionRememberedSet();
        if (rememberset != nullptr) {
            rememberset->ClearAllBits();
        }
        region->SetMarking(true);
    });
    if (heap_->IsSemiMarkNeeded()) {
        heap_->EnumerateNewSpaceRegions([this](Region *current) {
            current->ResetAliveObject();
        });
    } else {
        heap_->EnumerateRegions([this](Region *current) {
            current->ResetAliveObject();
        });
    }
    workList_->Initialize(TriggerGCType::OLD_GC, ParallelGCTaskPhase::CONCURRENT_HANDLE_GLOBAL_POOL_TASK);
    heap_->GetNonMovableMarker()->MarkRoots(0);
}

bool ConcurrentMarker::MarkerTask::Run(uint32_t threadId)
{
    ClockScope clockScope;
    heap_->GetNonMovableMarker()->ProcessMarkStack(threadId);
    heap_->WaitRunningTaskFinished();
    heap_->GetConcurrentMarker()->MarkingFinished();
    heap_->GetConcurrentMarker()->SetDuration(clockScope.TotalSpentTime());
    return true;
}

void ConcurrentMarker::MarkingFinished()
{
    os::memory::LockHolder lock(waitMarkingFinishedMutex_);
    thread_->SetMarkStatus(MarkStatus::MARK_FINISHED);
    if (vmThreadWaitMarkingFinished_) {
        vmThreadWaitMarkingFinished_ = false;
        waitMarkingFinishedCV_.Signal();
    }
    notifyMarkingFinished_ = true;
}
}  // namespace panda::ecmascript
