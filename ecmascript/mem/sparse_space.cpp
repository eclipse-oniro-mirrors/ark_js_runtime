/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "ecmascript/mem/sparse_space.h"

#include "ecmascript/js_hclass-inl.h"
#include "ecmascript/mem/concurrent_sweeper.h"
#include "ecmascript/mem/free_object_set.h"
#include "ecmascript/mem/heap.h"
#include "ecmascript/mem/mem_controller.h"
#include "ecmascript/mem/remembered_set.h"
#include "ecmascript/runtime_call_id.h"

namespace panda::ecmascript {
SparseSpace::SparseSpace(Heap *heap, MemSpaceType type, size_t initialCapacity, size_t maximumCapacity)
    : Space(heap, type, initialCapacity, maximumCapacity), sweepState_(SweepState::NO_SWEEP), liveObjectSize_(0)
{
    allocator_ = new FreeListAllocator(heap);
}

void SparseSpace::Initialize()
{
    Region *region = heapRegionAllocator_->AllocateAlignedRegion(this, DEFAULT_REGION_SIZE);
    region->InitializeSet();
    region->SetFlag(GetRegionFlag());
    if (spaceType_ == MemSpaceType::MACHINE_CODE_SPACE) {
        int res = region->SetCodeExecutableAndReadable();
        LOG_ECMA_MEM(DEBUG) << "MachineCodeSpace::Expand() SetCodeExecutableAndReadable" << res;
    }
    AddRegion(region);

    allocator_->Initialize(region);
}

void SparseSpace::Reset()
{
    allocator_->RebuildFreeList();
    ReclaimRegions();
}

uintptr_t SparseSpace::Allocate(size_t size, bool isAllowGC)
{
    auto object = allocator_->Allocate(size);
    CHECK_OBJECT_AND_INC_OBJ_SIZE(size);

    if (sweepState_ == SweepState::SWEEPING) {
        object = AllocateAfterSweepingCompleted(size);
        CHECK_OBJECT_AND_INC_OBJ_SIZE(size);
    }

    if (isAllowGC) {
        // Check whether it is necessary to trigger Old GC before expanding or OOM risk.
        heap_->CheckAndTriggerOldGC();
    }

    if (Expand()) {
        object = allocator_->Allocate(size);
        CHECK_OBJECT_AND_INC_OBJ_SIZE(size);
        return object;
    }

    if (isAllowGC) {
        heap_->CollectGarbage(TriggerGCType::OLD_GC);
        object = Allocate(size, false);
        // Size is already increment
    }
    return object;
}

bool SparseSpace::Expand()
{
    if (committedSize_ >= initialCapacity_) {
        LOG_ECMA_MEM(INFO) << "Expand::Committed size " << committedSize_ << " of old space is too big. ";
        return false;
    }

    Region *region = heapRegionAllocator_->AllocateAlignedRegion(this, DEFAULT_REGION_SIZE);
    region->SetFlag(GetRegionFlag());
    if (spaceType_ == MemSpaceType::MACHINE_CODE_SPACE) {
        int res = region->SetCodeExecutableAndReadable();
        LOG_ECMA_MEM(DEBUG) << "MachineCodeSpace::Expand() SetCodeExecutableAndReadable" << res;
    }
    region->InitializeSet();
    AddRegion(region);
    allocator_->AddFree(region);
    return true;
}

uintptr_t SparseSpace::AllocateAfterSweepingCompleted(size_t size)
{
    ASSERT(sweepState_ == SweepState::SWEEPING);
    MEM_ALLOCATE_AND_GC_TRACE(heap_->GetEcmaVM(), ConcurrentSweepingWait);
    if (FillSweptRegion()) {
        auto object = allocator_->Allocate(size);
        if (object != 0) {
            liveObjectSize_ += size;
            return object;
        }
    }
    // Parallel
    heap_->GetSweeper()->EnsureTaskFinished(spaceType_);
    return allocator_->Allocate(size);
}

void SparseSpace::PrepareSweeping()
{
    liveObjectSize_ = 0;
    EnumerateRegions([this](Region *current) {
        if (!current->InCollectSet()) {
            IncrementLiveObjectSize(current->AliveObject());
            current->ResetWasted();
            AddSweepingRegion(current);
        }
    });
    SortSweepingRegion();
    sweepState_ = SweepState::SWEEPING;
    allocator_->RebuildFreeList();
}

void SparseSpace::AsyncSweeping(bool isMain)
{
    Region *current = GetSweepingRegionSafe();
    while (current != nullptr) {
        FreeRegion(current, isMain);
        // Main thread sweeping region is added;
        if (!isMain) {
            AddSweptRegionSafe(current);
        }
        current = GetSweepingRegionSafe();
    }
}

void SparseSpace::Sweeping()
{
    liveObjectSize_ = 0;
    sweepState_ = SweepState::SWEEPING;
    allocator_->RebuildFreeList();
    EnumerateRegions([this](Region *current) {
        if (!current->InCollectSet()) {
            IncrementLiveObjectSize(current->AliveObject());
            current->ResetWasted();
            FreeRegion(current);
        }
    });
}

bool SparseSpace::FillSweptRegion()
{
    if (sweptList_.empty()) {
        return false;
    }
    Region *region = nullptr;
    while ((region = GetSweptRegionSafe()) != nullptr) {
        allocator_->CollectFreeObjectSet(region);
    }
    sweepState_ = SweepState::SWEPT;
    return true;
}

void SparseSpace::AddSweepingRegion(Region *region)
{
    sweepingList_.emplace_back(region);
}

void SparseSpace::SortSweepingRegion()
{
    // Sweep low alive object size at first
    std::sort(sweepingList_.begin(), sweepingList_.end(), [](Region *first, Region *second) {
        return first->AliveObject() < second->AliveObject();
    });
}

Region *SparseSpace::GetSweepingRegionSafe()
{
    os::memory::LockHolder holder(lock_);
    Region *region = nullptr;
    if (!sweepingList_.empty()) {
        region = sweepingList_.back();
        sweepingList_.pop_back();
    }
    return region;
}

void SparseSpace::AddSweptRegionSafe(Region *region)
{
    os::memory::LockHolder holder(lock_);
    sweptList_.emplace_back(region);
}

Region *SparseSpace::GetSweptRegionSafe()
{
    os::memory::LockHolder holder(lock_);
    Region *region = nullptr;
    if (!sweptList_.empty()) {
        region = sweptList_.back();
        sweptList_.pop_back();
    }
    return region;
}

void SparseSpace::FreeRegion(Region *current, bool isMain)
{
    auto markBitmap = current->GetMarkBitmap();
    ASSERT(markBitmap != nullptr);
    uintptr_t freeStart = current->GetBegin();
    markBitmap->IterateOverMarkedChunks([this, &current, &freeStart, isMain](void *mem) {
        ASSERT(current->InRange(ToUintPtr(mem)));
        auto header = reinterpret_cast<TaggedObject *>(mem);
        auto klass = header->GetClass();
        auto size = klass->SizeFromJSHClass(header);

        uintptr_t freeEnd = ToUintPtr(mem);
        if (freeStart != freeEnd) {
            FreeLiveRange(current, freeStart, freeEnd, isMain);
        }
        freeStart = freeEnd + size;
    });
    uintptr_t freeEnd = current->GetEnd();
    if (freeStart != freeEnd) {
        FreeLiveRange(current, freeStart, freeEnd, isMain);
    }
}

void SparseSpace::FreeLiveRange(Region *current, uintptr_t freeStart, uintptr_t freeEnd, bool isMain)
{
    heap_->ClearSlotsRange(current, freeStart, freeEnd);
    allocator_->Free(freeStart, freeEnd - freeStart, isMain);
}

void SparseSpace::IterateOverObjects(const std::function<void(TaggedObject *object)> &visitor) const
{
    allocator_->FillBumpPoint();
    EnumerateRegions([&](Region *region) {
        if (region->InCollectSet()) {
            return;
        }
        uintptr_t curPtr = region->GetBegin();
        uintptr_t endPtr = region->GetEnd();
        while (curPtr < endPtr) {
            auto freeObject = FreeObject::Cast(curPtr);
            size_t objSize;
            if (!freeObject->IsFreeObject()) {
                auto obj = reinterpret_cast<TaggedObject *>(curPtr);
                visitor(obj);
                objSize = obj->GetClass()->SizeFromJSHClass(obj);
            } else {
                objSize = freeObject->Available();
            }
            curPtr += objSize;
            CHECK_OBJECT_SIZE(objSize);
        }
        CHECK_REGION_END(curPtr, endPtr);
    });
}

size_t SparseSpace::GetHeapObjectSize() const
{
    return liveObjectSize_;
}

size_t SparseSpace::GetTotalAllocatedSize() const
{
    return allocator_->GetAllocatedSize();
}

void SparseSpace::DetachFreeObjectSet(Region *region)
{
    allocator_->DetachFreeObjectSet(region);
}

OldSpace::OldSpace(Heap *heap, size_t initialCapacity, size_t maximumCapacity)
    : SparseSpace(heap, OLD_SPACE, initialCapacity, maximumCapacity) {}

Region *OldSpace::TryToGetExclusiveRegion(size_t size)
{
    os::memory::LockHolder lock(lock_);
    uintptr_t result = allocator_->LookupSuitableFreeObject(size);
    if (result != 0) {
        // Remove region from global old space
        Region *region = Region::ObjectAddressToRange(result);
        RemoveRegion(region);
        allocator_->DetachFreeObjectSet(region);
        DecrementLiveObjectSize(region->AliveObject());
        return region;
    }
    return nullptr;
}

void OldSpace::Merge(LocalSpace *localSpace)
{
    localSpace->FreeBumpPoint();
    os::memory::LockHolder lock(lock_);
    localSpace->EnumerateRegions([&](Region *region) {
        localSpace->DetachFreeObjectSet(region);
        localSpace->RemoveRegion(region);
        localSpace->DecrementLiveObjectSize(region->AliveObject());
        region->SetSpace(this);
        AddRegion(region);
        IncrementLiveObjectSize(region->AliveObject());
        allocator_->CollectFreeObjectSet(region);
    });
    if (committedSize_ >= maximumCapacity_) {
        LOG_ECMA_MEM(FATAL) << "Merge::Committed size " << committedSize_ << " of old space is too big. ";
    }

    localSpace->GetRegionList().Clear();
    allocator_->IncrementAllocatedSize(localSpace->GetTotalAllocatedSize());
}

void OldSpace::SelectCSet()
{
    if (sweepState_ != SweepState::SWEPT) {
        return;
    }
    CheckRegionSize();
    // 1、Select region which alive object largger than 80%
    EnumerateRegions([this](Region *region) {
        if (!region->MostObjectAlive()) {
            collectRegionSet_.emplace_back(region);
        }
    });
    if (collectRegionSet_.size() < PARTIAL_GC_MIN_COLLECT_REGION_SIZE) {
        LOG_ECMA_MEM(DEBUG) << "Select CSet failure: number is too few";
        isCSetEmpty_ = true;
        collectRegionSet_.clear();
        return;
    }
    // sort
    std::sort(collectRegionSet_.begin(), collectRegionSet_.end(), [](Region *first, Region *second) {
        return first->AliveObject() < second->AliveObject();
    });
    unsigned long selectedRegionNumber = GetSelectedRegionNumber();
    if (collectRegionSet_.size() > selectedRegionNumber) {
        collectRegionSet_.resize(selectedRegionNumber);
    }

    EnumerateCollectRegionSet([&](Region *current) {
        RemoveRegion(current);
        DecrementLiveObjectSize(current->AliveObject());
        allocator_->DetachFreeObjectSet(current);
        current->SetFlag(RegionFlags::IS_IN_COLLECT_SET);
    });
    isCSetEmpty_ = false;
    sweepState_ = SweepState::NO_SWEEP;
    LOG_ECMA_MEM(DEBUG) << "Select CSet success: number is " << collectRegionSet_.size();
}

void OldSpace::CheckRegionSize()
{
#ifndef NDEBUG
    if (sweepState_ == SweepState::SWEEPING) {
        heap_->GetSweeper()->EnsureTaskFinished(spaceType_);
    }
    size_t available = allocator_->GetAvailableSize();
    size_t wasted = allocator_->GetWastedSize();
    if (GetHeapObjectSize() + wasted + available != objectSize_) {
        LOG(DEBUG, RUNTIME) << "Actual live object size:" << GetHeapObjectSize()
                            << ", free object size:" << available
                            << ", wasted size:" << wasted
                            << ", but exception totoal size:" << objectSize_;
    }
#endif
}

void OldSpace::RevertCSet()
{
    EnumerateCollectRegionSet([&](Region *region) {
        region->ClearFlag(RegionFlags::IS_IN_COLLECT_SET);
        region->SetSpace(this);
        AddRegion(region);
        allocator_->CollectFreeObjectSet(region);
        IncrementLiveObjectSize(region->AliveObject());
    });
    collectRegionSet_.clear();
    isCSetEmpty_ = true;
}

void OldSpace::ReclaimCSet()
{
    EnumerateCollectRegionSet([this](Region *region) {
        region->SetSpace(nullptr);
        region->DeleteMarkBitmap();
        region->DeleteCrossRegionRememberedSet();
        region->DeleteOldToNewRememberedSet();
        region->DestroySet();
        heapRegionAllocator_->FreeRegion(region);
    });
    collectRegionSet_.clear();
    isCSetEmpty_ = true;
}

LocalSpace::LocalSpace(Heap *heap, size_t initialCapacity, size_t maximumCapacity)
    : SparseSpace(heap, LOCAL_SPACE, initialCapacity, maximumCapacity) {}

bool LocalSpace::AddRegionToList(Region *region)
{
    if (committedSize_ >= maximumCapacity_) {
        LOG_ECMA_MEM(FATAL) << "AddRegionTotList::Committed size " << committedSize_ << " of local space is too big.";
        return false;
    }
    region->SetSpace(this);
    AddRegion(region);
    allocator_->CollectFreeObjectSet(region);
    IncrementLiveObjectSize(region->AliveObject());
    return true;
}

void LocalSpace::FreeBumpPoint()
{
    allocator_->FreeBumpPoint();
}

NonMovableSpace::NonMovableSpace(Heap *heap, size_t initialCapacity, size_t maximumCapacity)
    : SparseSpace(heap, MemSpaceType::NON_MOVABLE, initialCapacity, maximumCapacity)
{
}

uintptr_t LocalSpace::Allocate(size_t size, bool isExpand)
{
    auto object = allocator_->Allocate(size);
    if (object == 0) {
        if (isExpand && Expand()) {
            object = allocator_->Allocate(size);
        }
    }
    return object;
}

MachineCodeSpace::MachineCodeSpace(Heap *heap, size_t initialCapacity, size_t maximumCapacity)
    : SparseSpace(heap, MemSpaceType::MACHINE_CODE_SPACE, initialCapacity, maximumCapacity)
{
}
}  // namespace panda::ecmascript
