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

#include "ecmascript/mem/mem_controller.h"

#include "ecmascript/mem/concurrent_marker.h"
#include "ecmascript/mem/mem_manager.h"
#include "ecmascript/mem/heap-inl.h"
#include "ecmascript/mem/parallel_evacuation.h"

namespace panda::ecmascript {
MemController::MemController(Heap *heap, bool isDelayGCMode) : heap_(heap), isDelayGCMode_(isDelayGCMode) {}

double MemController::CalculateAllocLimit(size_t currentSize, size_t minSize, size_t maxSize, size_t newSpaceCapacity,
                                          double factor) const
{
    const uint64_t limit = std::max(static_cast<uint64_t>(currentSize * factor),
                                    static_cast<uint64_t>(currentSize) + MIN_AllOC_LIMIT_GROWING_STEP) +
                           newSpaceCapacity;

    const uint64_t limitAboveMinSize = std::max<uint64_t>(limit, minSize);
    const uint64_t halfToMaxSize = (static_cast<uint64_t>(currentSize) + maxSize) / 2;
    const auto result = static_cast<size_t>(std::min(limitAboveMinSize, halfToMaxSize));
    return result;
}

double MemController::CalculateGrowingFactor(double gcSpeed, double mutatorSpeed)
{
    static constexpr double minGrowingFactor = 1.3;
    static constexpr double maxGrowingFactor = 2.0;
    static constexpr double targetMutatorUtilization = 0.97;
    if (gcSpeed == 0 || mutatorSpeed == 0) {
        return maxGrowingFactor;
    }

    const double speedRatio = gcSpeed / mutatorSpeed;

    const double a = speedRatio * (1 - targetMutatorUtilization);
    const double b = speedRatio * (1 - targetMutatorUtilization) -  targetMutatorUtilization;

    double factor = (a < b * maxGrowingFactor) ? a / b : maxGrowingFactor;
    factor = std::min(maxGrowingFactor, factor);
    factor = std::max(factor, minGrowingFactor);
    return factor;
}

void MemController::StartCalculationBeforeGC()
{
    startCounter_++;
    if (startCounter_ != 1) {
        return;
    }

    auto heapManager = heap_->GetHeapManager();
    // It's unnecessary to calculate newSpaceAllocAccumulatorSize. newSpaceAllocBytesSinceGC can be calculated directly.
    size_t newSpaceAllocBytesSinceGC = heap_->GetNewSpace()->GetAllocatedSizeSinceGC();
    size_t hugeObjectAllocSizeSinceGC = heap_->GetHugeObjectSpace()->GetHeapObjectSize() - hugeObjectAllocSizeSinceGC_;
    size_t oldSpaceAllocAccumulatorSize = heapManager->GetOldSpaceAllocator().GetAllocatedSize()
                                          + heap_->GetEvacuation()->GetPromotedAccumulatorSize();
    size_t nonMovableSpaceAllocAccumulatorSize = heapManager->GetNonMovableSpaceAllocator().GetAllocatedSize();
    size_t codeSpaceAllocAccumulatorSize = heapManager->GetMachineCodeSpaceAllocator().GetAllocatedSize();
    if (allocTimeMs_ == 0) {
        allocTimeMs_ = GetSystemTimeInMs();
        oldSpaceAllocAccumulatorSize_ = oldSpaceAllocAccumulatorSize;
        nonMovableSpaceAllocAccumulatorSize_ = nonMovableSpaceAllocAccumulatorSize;
        codeSpaceAllocAccumulatorSize_ = codeSpaceAllocAccumulatorSize;
        return;
    }
    double currentTimeInMs = GetSystemTimeInMs();
    gcStartTime_ = currentTimeInMs;
    size_t oldSpaceAllocSize = oldSpaceAllocAccumulatorSize - oldSpaceAllocAccumulatorSize_;
    size_t nonMovableSpaceAllocSize = nonMovableSpaceAllocAccumulatorSize - nonMovableSpaceAllocAccumulatorSize_;
    size_t codeSpaceAllocSize = codeSpaceAllocAccumulatorSize - codeSpaceAllocAccumulatorSize_;

    double duration = currentTimeInMs - allocTimeMs_;
    allocTimeMs_ = currentTimeInMs;
    oldSpaceAllocAccumulatorSize_ = oldSpaceAllocAccumulatorSize;
    nonMovableSpaceAllocAccumulatorSize_ = nonMovableSpaceAllocAccumulatorSize;
    codeSpaceAllocAccumulatorSize_ = codeSpaceAllocAccumulatorSize;
    allocDurationSinceGc_ += duration;
    newSpaceAllocSizeSinceGC_ += newSpaceAllocBytesSinceGC;
    oldSpaceAllocSizeSinceGC_ += oldSpaceAllocSize;
    oldSpaceAllocSizeSinceGC_ += hugeObjectAllocSizeSinceGC;
    nonMovableSpaceAllocSizeSinceGC_ += nonMovableSpaceAllocSize;
    codeSpaceAllocSizeSinceGC_ += codeSpaceAllocSize;
}

void MemController::StopCalculationAfterGC(TriggerGCType gcType)
{
    startCounter_--;
    if (startCounter_ != 0) {
        return;
    }

    gcEndTime_ = GetSystemTimeInMs();
    allocTimeMs_ = gcEndTime_;
    if (allocDurationSinceGc_ > 0) {
        recordedNewSpaceAllocations_.Push(MakeBytesAndDuration(newSpaceAllocSizeSinceGC_, allocDurationSinceGc_));
        recordedOldSpaceAllocations_.Push(MakeBytesAndDuration(oldSpaceAllocSizeSinceGC_, allocDurationSinceGc_));
        recordedNonmovableSpaceAllocations_.Push(
            MakeBytesAndDuration(nonMovableSpaceAllocSizeSinceGC_, allocDurationSinceGc_));
        recordedCodeSpaceAllocations_.Push(MakeBytesAndDuration(codeSpaceAllocSizeSinceGC_, allocDurationSinceGc_));
    }
    allocDurationSinceGc_ = 0.0;
    newSpaceAllocSizeSinceGC_ = 0;
    oldSpaceAllocSizeSinceGC_ = 0;
    nonMovableSpaceAllocSizeSinceGC_ = 0;
    codeSpaceAllocSizeSinceGC_ = 0;

    hugeObjectAllocSizeSinceGC_ = heap_->GetHugeObjectSpace()->GetHeapObjectSize();

    double duration = gcEndTime_ - gcStartTime_;

    switch (gcType) {
        case TriggerGCType::SEMI_GC:
        case TriggerGCType::NON_MOVE_GC:
        case TriggerGCType::HUGE_GC:
        case TriggerGCType::MACHINE_CODE_GC:
             break;
        case TriggerGCType::OLD_GC:
        case TriggerGCType::COMPRESS_FULL_GC: {
            size_t heapObjectSize = heap_->GetHeapObjectSize();
            recordedMarkCompacts_.Push(MakeBytesAndDuration(heapObjectSize, duration));
            break;
        }
        default:
            UNREACHABLE();
            break;
    }
}

void MemController::RecordAfterConcurrentMark(const bool isSemi, const ConcurrentMarker *marker)
{
    double duration = marker->GetDuration();
    if (isSemi) {
        recordedSemiConcurrentMarks_.Push(MakeBytesAndDuration(marker->GetHeapObjectSize(), duration));
    } else {
        recordedConcurrentMarks_.Push(MakeBytesAndDuration(marker->GetHeapObjectSize(), duration));
    }
}

double MemController::CalculateMarkCompactSpeedPerMS()
{
    if (markCompactSpeedCache_ > 0) {
        return markCompactSpeedCache_;
    }
    markCompactSpeedCache_ = CalculateAverageSpeed(recordedMarkCompacts_);
    if (markCompactSpeedCache_ > 0) {
        return markCompactSpeedCache_;
    }
    return 0;
}

double MemController::CalculateAverageSpeed(const base::GCRingBuffer<BytesAndDuration, LENGTH> &buffer,
                                            const BytesAndDuration &initial, const double timeMs)
{
    BytesAndDuration sum = buffer.Sum(
        [timeMs](BytesAndDuration a, BytesAndDuration b) {
            if (timeMs != 0 && a.second >= timeMs) {
                return a;
            }
            return std::make_pair(a.first + b.first, a.second + b.second);
        },
        initial);
    uint64_t bytes = sum.first;
    double durations = sum.second;
    if (durations == 0.0) {
        return 0;
    }
    double speed = bytes / durations;
    const int maxSpeed = 1024 * 1024 * 1024;
    const int minSpeed = 1;
    if (speed >= maxSpeed) {
        return maxSpeed;
    }
    if (speed <= minSpeed) {
        return minSpeed;
    }
    return speed;
}

double MemController::CalculateAverageSpeed(const base::GCRingBuffer<BytesAndDuration, LENGTH> &buffer)
{
    return CalculateAverageSpeed(buffer, MakeBytesAndDuration(0, 0), 0);
}

double MemController::GetCurrentOldSpaceAllocationThroughtputPerMS(double timeMs) const
{
    size_t allocatedSize = oldSpaceAllocSizeSinceGC_;
    double duration = allocDurationSinceGc_;
    return CalculateAverageSpeed(recordedOldSpaceAllocations_,
                                 MakeBytesAndDuration(allocatedSize, duration), timeMs);
}

double MemController::GetNewSpaceAllocationThroughtPerMS() const
{
    return CalculateAverageSpeed(recordedNewSpaceAllocations_);
}

double MemController::GetNewSpaceConcurrentMarkSpeedPerMS() const
{
    return CalculateAverageSpeed(recordedSemiConcurrentMarks_);
}

double MemController::GetOldSpaceAllocationThroughtPerMS() const
{
    return CalculateAverageSpeed(recordedOldSpaceAllocations_);
}

double MemController::GetFullSpaceConcurrentMarkSpeedPerMS() const
{
    return CalculateAverageSpeed(recordedConcurrentMarks_);
}

MemController *CreateMemController(Heap *heap, std::string_view gcTriggerType)
{
    MemController *ret{nullptr};
    if (gcTriggerType == "no-gc-for-start-up") {
        ret = new MemController(heap, true);
    } else if (gcTriggerType == "heap-trigger") {
        ret = new MemController(heap, false);
    }
    return ret;
}
}  // namespace panda::ecmascript
