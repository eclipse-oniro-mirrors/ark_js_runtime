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

#include "ecmascript/runtime_api.h"

#include "ecmascript/mem/parallel_work_helper.h"

namespace panda::ecmascript {
void RuntimeApi::MarkObject(uintptr_t slotAddr, Region *objectRegion, TaggedObject *value, Region *valueRegion)
{
    auto heap = valueRegion->GetHeap();
    bool isFullMark = heap->IsFullMark();
    if (!JSTaggedValue(value).IsWeakForHeapObject()) {
        if (!isFullMark && !valueRegion->InYoungGeneration()) {
            return;
        }
        auto valueBitmap = valueRegion->GetMarkBitmap();
        if (!valueBitmap->AtomicTestAndSet(value)) {
            valueRegion->GetWorkList()->Push(0, value, valueRegion);
        }
    }
    if (isFullMark && valueRegion->InCollectSet() && !objectRegion->InYoungOrCSetGeneration()) {
        auto set = objectRegion->GetOrCreateCrossRegionRememberedSet();
        set->AtomicInsert(slotAddr);
    }
}
}  // namespace panda::ecmascript::tooling
