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

#ifndef ECMASCRIPT_MEM_REGION_INL_H
#define ECMASCRIPT_MEM_REGION_INL_H

#include "ecmascript/mem/region.h"

#include "ecmascript/mem/heap.h"
#include "ecmascript/mem/region_factory.h"
#include "ecmascript/mem/remembered_set.h"
#include "ecmascript/mem/space.h"

namespace panda::ecmascript {
inline void Region::SetSpace(Space *space)
{
    space_ = space;
    heap_ = space_->GetHeap();
}

inline RangeBitmap *Region::GetOrCreateMarkBitmap()
{
    if (UNLIKELY(markBitmap_ == nullptr)) {
        os::memory::LockHolder lock(lock_);
        if (markBitmap_ == nullptr) {
            markBitmap_ = CreateMarkBitmap();
        }
    }
    return markBitmap_;
}

RangeBitmap *Region::CreateMarkBitmap()
{
    size_t heapSize = IsFlagSet(RegionFlags::IS_HUGE_OBJECT) ? LARGE_BITMAP_MIN_SIZE : GetCapacity();
    // Only one huge object is stored in a region. The BitmapSize of a huge region will always be 8 Bytes.
    size_t bitmapSize = RangeBitmap::GetBitMapSizeInByte(heapSize);

    auto bitmapData = const_cast<RegionFactory *>(heap_->GetRegionFactory())->Allocate(bitmapSize);
    auto *ret = new RangeBitmap(this, heapSize, bitmapData);

    ret->ClearAllBits();
    return ret;
}

RememberedSet *Region::CreateRememberedSet()
{
    auto setSize = RememberedSet::GetSizeInByte(GetCapacity());
    auto setAddr = const_cast<RegionFactory *>(heap_->GetRegionFactory())->Allocate(setSize);
    uintptr_t setData = ToUintPtr(setAddr);
    auto ret = new RememberedSet(ToUintPtr(this), GetCapacity(), setData);
    ret->ClearAllBits();
    return ret;
}

RememberedSet *Region::GetOrCreateCrossRegionRememberedSet()
{
    if (UNLIKELY(crossRegionSet_ == nullptr)) {
        os::memory::LockHolder lock(lock_);
        if (crossRegionSet_ == nullptr) {
            crossRegionSet_ = CreateRememberedSet();
        }
    }
    return crossRegionSet_;
}

RememberedSet *Region::GetOrCreateOldToNewRememberedSet()
{
    if (UNLIKELY(oldToNewSet_ == nullptr)) {
        os::memory::LockHolder lock(lock_);
        if (oldToNewSet_ == nullptr) {
            oldToNewSet_ = CreateRememberedSet();
        }
    }
    return oldToNewSet_;
}

void Region::InsertCrossRegionRememberedSet(uintptr_t addr)
{
    auto set = GetOrCreateCrossRegionRememberedSet();
    set->Insert(addr);
}

void Region::AtomicInsertCrossRegionRememberedSet(uintptr_t addr)
{
    auto set = GetOrCreateCrossRegionRememberedSet();
    set->AtomicInsert(addr);
}

void Region::InsertOldToNewRememberedSet(uintptr_t addr)
{
    auto set = GetOrCreateOldToNewRememberedSet();
    set->Insert(addr);
}

WorkerHelper *Region::GetWorkList() const
{
    return heap_->GetWorkList();
}

void Region::AtomicInsertOldToNewRememberedSet(uintptr_t addr)
{
    auto set = GetOrCreateOldToNewRememberedSet();
    set->AtomicInsert(addr);
}

void Region::ClearMarkBitmap()
{
    if (markBitmap_ != nullptr) {
        auto size = RangeBitmap::GetBitMapSizeInByte(GetCapacity());
        const_cast<RegionFactory *>(heap_->GetRegionFactory())->Free(markBitmap_->GetBitMap().Data(), size);
        delete markBitmap_;
        markBitmap_ = nullptr;
    }
}

void Region::ClearCrossRegionRememberedSet()
{
    if (crossRegionSet_ != nullptr) {
        auto size = RememberedSet::GetSizeInByte(GetCapacity());
        const_cast<RegionFactory *>(heap_->GetRegionFactory())->Free(
            crossRegionSet_->GetBitMap().Data(), size);
        delete crossRegionSet_;
        crossRegionSet_ = nullptr;
    }
}

void Region::ClearOldToNewRememberedSet()
{
    if (oldToNewSet_ != nullptr) {
        auto size = RememberedSet::GetSizeInByte(GetCapacity());
        const_cast<RegionFactory *>(heap_->GetRegionFactory())->Free(
            oldToNewSet_->GetBitMap().Data(), size);
        delete oldToNewSet_;
        oldToNewSet_ = nullptr;
    }
}
}  // namespace panda::ecmascript
#endif  // ECMASCRIPT_MEM_REGION_INL_H
