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

#ifndef ECMASCRIPT_MEM_LINEAR_SPACE_H
#define ECMASCRIPT_MEM_LINEAR_SPACE_H

#include "ecmascript/mem/space-inl.h"

namespace panda::ecmascript {
class LinearSpace : public Space {
public:
    explicit LinearSpace(Heap *heap, MemSpaceType type, size_t initialCapacity, size_t maximumCapacity);
    uintptr_t Allocate(size_t size, bool isPromoted = false);
    bool Expand(bool isPromoted);
    void Stop();
    void DecrementSurvivalObjectSize(size_t objSize)
    {
        survivalObjectSize_ -= objSize;
    }
protected:
    BumpPointerAllocator *allocator_;
    size_t overShootSize_ {0};
    size_t allocateAfterLastGC_ {0};
    size_t survivalObjectSize_ {0};
    uintptr_t waterLine_;
};

class SemiSpace : public LinearSpace {
public:
    explicit SemiSpace(Heap *heap, size_t initialCapacity, size_t maximumCapacity);
    ~SemiSpace() override = default;
    NO_COPY_SEMANTIC(SemiSpace);
    NO_MOVE_SEMANTIC(SemiSpace);

    void Initialize() override;
    void Restart();

    uintptr_t AllocateSync(size_t size);

    void SetOverShootSize(size_t size);
    bool AdjustCapacity(size_t allocatedSizeSinceGC);

    void SetWaterLine();

    uintptr_t GetWaterLine() const
    {
        return waterLine_;
    }
    uintptr_t GetTop() const
    {
        return allocator_->GetTop();
    }
    size_t GetHeapObjectSize() const;
    size_t GetSurvivalObjectSize() const;
    size_t GetAllocatedSizeSinceGC() const;

    bool SwapRegion(Region *region, SemiSpace *fromSpace);

    void IterateOverObjects(const std::function<void(TaggedObject *object)> &objectVisitor) const;

private:
    os::memory::Mutex lock_;
};

class SnapShotSpace : public LinearSpace {
public:
    explicit SnapShotSpace(Heap *heap, size_t initialCapacity, size_t maximumCapacity);
    ~SnapShotSpace() override = default;
    NO_COPY_SEMANTIC(SnapShotSpace);
    NO_MOVE_SEMANTIC(SnapShotSpace);
};
}  // namespace panda::ecmascript
#endif  // ECMASCRIPT_MEM_LINEAR_SPACE_H
