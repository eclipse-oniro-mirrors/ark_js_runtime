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

#ifndef ECMASCRIPT_MEM_HEAP_MANAGER_INL_H
#define ECMASCRIPT_MEM_HEAP_MANAGER_INL_H

#include "ecmascript/mem/mem_manager.h"

#include <ctime>

#include "ecmascript/mem/allocator-inl.h"
#include "ecmascript/mem/heap-inl.h"
#include "ecmascript/mem/object_xray.h"
#include "ecmascript/js_hclass.h"
#include "ecmascript/js_hclass.h"

namespace panda::ecmascript {
TaggedObject *MemManager::AllocateYoungGenerationOrHugeObject(JSHClass *hclass)
{
    size_t size = hclass->GetObjectSize();
    return AllocateYoungGenerationOrHugeObject(hclass, size);
}

TaggedObject *MemManager::AllocateYoungGenerationOrHugeObject(JSHClass *hclass, size_t size)
{
    if (size > MAX_REGULAR_HEAP_OBJECT_SIZE) {
        return AllocateHugeObject(hclass, size);
    }
    // regular objects
    auto object = reinterpret_cast<TaggedObject *>(newSpaceAllocator_.Allocate(size));
    if (LIKELY(object != nullptr)) {
        SetClass(object, hclass);
        heap_->OnAllocateEvent(reinterpret_cast<uintptr_t>(object));
        return object;
    }

    // hclass must nonmovable
    if (!heap_->FillNewSpaceAndTryGC(&newSpaceAllocator_)) {
        LOG_ECMA_MEM(FATAL) << "OOM : extend failed";
        UNREACHABLE();
    }
    object = reinterpret_cast<TaggedObject *>(newSpaceAllocator_.Allocate(size));
    if (UNLIKELY(object == nullptr)) {
        if (!heap_->FillNewSpaceAndTryGC(&newSpaceAllocator_)) {
            LOG_ECMA_MEM(FATAL) << "OOM : extend failed" << __LINE__;
            UNREACHABLE();
        }
        object = reinterpret_cast<TaggedObject *>(newSpaceAllocator_.Allocate(size));
        if (UNLIKELY(object == nullptr)) {
            heap_->CollectGarbage(TriggerGCType::OLD_GC);
            object = reinterpret_cast<TaggedObject *>(newSpaceAllocator_.Allocate(size));
            if (UNLIKELY(object == nullptr)) {
                heap_->ThrowOutOfMemoryError(size, "AllocateYoungGenerationOrHugeObject");
                UNREACHABLE();
            }
        }
    }

    SetClass(object, hclass);
    heap_->OnAllocateEvent(reinterpret_cast<uintptr_t>(object));
    return object;
}

TaggedObject *MemManager::TryAllocateYoungGeneration(size_t size)
{
    if (size > MAX_REGULAR_HEAP_OBJECT_SIZE) {
        return nullptr;
    }
    return reinterpret_cast<TaggedObject *>(newSpaceAllocator_.Allocate(size));
}

TaggedObject *MemManager::AllocateDynClassClass(JSHClass *hclass, size_t size)
{
    auto object = reinterpret_cast<TaggedObject *>(GetNonMovableSpaceAllocator().Allocate(size));
    if (UNLIKELY(object == nullptr)) {
        LOG_ECMA_MEM(FATAL) << "MemManager::AllocateDynClassClass can not allocate any space";
    }
    *reinterpret_cast<MarkWordType *>(ToUintPtr(object)) = reinterpret_cast<MarkWordType>(hclass);
    heap_->OnAllocateEvent(reinterpret_cast<uintptr_t>(object));
    return object;
}

TaggedObject *MemManager::AllocateNonMovableOrHugeObject(JSHClass *hclass, size_t size)
{
    if (size > MAX_REGULAR_HEAP_OBJECT_SIZE) {
        return AllocateHugeObject(hclass, size);
    }
    auto object = reinterpret_cast<TaggedObject *>(GetNonMovableSpaceAllocator().Allocate(size));
    if (UNLIKELY(object == nullptr)) {
        if (heap_->CheckAndTriggerNonMovableGC()) {
            object = reinterpret_cast<TaggedObject *>(GetNonMovableSpaceAllocator().Allocate(size));
        }
        if (UNLIKELY(object == nullptr)) {
            // hclass must be nonmovable
            if (!heap_->FillNonMovableSpaceAndTryGC(&GetNonMovableSpaceAllocator())) {
                LOG_ECMA_MEM(FATAL) << "OOM : extend failed";
                UNREACHABLE();
            }
            object = reinterpret_cast<TaggedObject *>(GetNonMovableSpaceAllocator().Allocate(size));
            if (UNLIKELY(object == nullptr)) {
                heap_->ThrowOutOfMemoryError(size, "AllocateNonMovableOrHugeObject");
                UNREACHABLE();
            }
        }
    }
    SetClass(object, hclass);
    heap_->OnAllocateEvent(reinterpret_cast<uintptr_t>(object));
    return object;
}

uintptr_t MemManager::AllocateSnapShotSpace(size_t size)
{
    uintptr_t object = snapshotSpaceAllocator_.Allocate(size);
    if (UNLIKELY(object == 0)) {
        if (!heap_->FillSnapShotSpace(&snapshotSpaceAllocator_)) {
            LOG_ECMA_MEM(FATAL) << "alloc failed";
            UNREACHABLE();
        }
        object = snapshotSpaceAllocator_.Allocate(size);
        if (UNLIKELY(object == 0)) {
            LOG_ECMA_MEM(FATAL) << "alloc failed";
            UNREACHABLE();
        }
    }
    return object;
}

void MemManager::SetClass(TaggedObject *header, JSHClass *hclass)
{
    header->SetClass(hclass);
}

TaggedObject *MemManager::AllocateNonMovableOrHugeObject(JSHClass *hclass)
{
    size_t size = hclass->GetObjectSize();
    return AllocateNonMovableOrHugeObject(hclass, size);
}

TaggedObject *MemManager::AllocateOldGenerationOrHugeObject(JSHClass *hclass, size_t size)
{
    ASSERT(size > 0);
    if (size > MAX_REGULAR_HEAP_OBJECT_SIZE) {
        return AllocateHugeObject(hclass, size);
    }
    auto object = reinterpret_cast<TaggedObject *>(GetOldSpaceAllocator().Allocate(size));
    if (UNLIKELY(object == nullptr)) {
        if (heap_->CheckAndTriggerOldGC()) {
            object = reinterpret_cast<TaggedObject *>(GetOldSpaceAllocator().Allocate(size));
        }
        if (UNLIKELY(object == nullptr)) {
            // hclass must nonmovable
            if (!heap_->FillOldSpaceAndTryGC(&GetOldSpaceAllocator())) {
                LOG_ECMA_MEM(FATAL) << "OOM : extend failed";
                UNREACHABLE();
            }
            object = reinterpret_cast<TaggedObject *>(GetOldSpaceAllocator().Allocate(size));
            if (UNLIKELY(object == nullptr)) {
                heap_->ThrowOutOfMemoryError(size, "AllocateOldGenerationOrHugeObject");
                UNREACHABLE();
            }
        }
    }
    SetClass(object, hclass);
    heap_->OnAllocateEvent(reinterpret_cast<uintptr_t>(object));
    Region *objectRegion = Region::ObjectAddressToRange(object);
    objectRegion->IncrementAliveObject(size);
    return object;
}

TaggedObject *MemManager::AllocateHugeObject(JSHClass *hclass, size_t size)
{
    ASSERT(size > MAX_REGULAR_HEAP_OBJECT_SIZE);
    // large objects
    heap_->CheckAndTriggerCompressGC();
    auto *object =
        reinterpret_cast<TaggedObject *>(const_cast<HugeObjectSpace *>(heap_->GetHugeObjectSpace())->Allocate(size));
    if (UNLIKELY(object == nullptr)) {
        heap_->CollectGarbage(TriggerGCType::HUGE_GC);
        object = reinterpret_cast<TaggedObject *>(
                    const_cast<HugeObjectSpace *>(heap_->GetHugeObjectSpace())->Allocate(size));
        if (UNLIKELY(object == nullptr)) {
            heap_->ThrowOutOfMemoryError(size, "AllocateHugeObject");
            UNREACHABLE();
        }
    }
    SetClass(object, hclass);
    heap_->OnAllocateEvent(reinterpret_cast<uintptr_t>(object));
    return object;
}

TaggedObject *MemManager::AllocateMachineCodeSpaceObject(JSHClass *hclass, size_t size)
{
    auto object = reinterpret_cast<TaggedObject *>(GetMachineCodeSpaceAllocator().Allocate(size));
    if (UNLIKELY(object == nullptr)) {
        if (heap_->CheckAndTriggerMachineCodeGC()) {
            object = reinterpret_cast<TaggedObject *>(GetMachineCodeSpaceAllocator().Allocate(size));
        }
        if (UNLIKELY(object == nullptr)) {
            if (!heap_->FillMachineCodeSpaceAndTryGC(&GetMachineCodeSpaceAllocator())) {
                return nullptr;
            }
            object = reinterpret_cast<TaggedObject *>(GetMachineCodeSpaceAllocator().Allocate(size));
            if (UNLIKELY(object == nullptr)) {
                heap_->ThrowOutOfMemoryError(size, "AllocateMachineCodeSpaceObject");
                return nullptr;
            }
        }
    }
    SetClass(object, hclass);
    heap_->OnAllocateEvent(reinterpret_cast<uintptr_t>(object));
    return object;
}
}  // namespace panda::ecmascript

#endif  // ECMASCRIPT_MEM_HEAP_MANAGER_INL_H
