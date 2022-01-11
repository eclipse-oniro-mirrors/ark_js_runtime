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

//    in aot project, three Frame: Interpreter Frame、Runtime Frame、Optimized Frame.  Optimized Frame
//  call Runtime function, generate OptLeaveFrame Frame(gc related context (patchid、sp、fp) is saved to frame)
//  then return Optimized thread.sp restore orignal sp.

// Frame Layout
// Interpreter Frame(alias   **iframe** ) Layout as follow:
// ```
//   +----------------------------------+-------------------+
//   |    argv[n-1]                     |                   ^
//   |----------------------------------|                   |
//   |    ......                        |                   |
//   |----------------------------------|                   |
//   |    thisArg [maybe not exist]     |                   |
//   |----------------------------------|                   |
//   |    newTarget [maybe not exist]   |                   |
//   |----------------------------------|                   |
//   |    callTarget [deleted]          |                   |
//   |----------------------------------|                   |
//   |    ......                        |                   |
//   |----------------------------------|                   |
//   |    Vregs [not exist in native]   |                   |
//   +----------------------------------+--------+      interpreter frame
//   |    base.frameType                |        ^          |
//   |----------------------------------|        |          |
//   |    base.prev(pre stack pointer)  |        |          |
//   |----------------------------------|        |          |
//   |    numActualArgs [deleted]       |        |          |
//   |----------------------------------|        |          |
//   |    env                           |        |          |
//   |----------------------------------|        |          |
//   |    acc                           |        |          |
//   |----------------------------------|InterpretedFrame   |
//   |    profileTypeInfo               |        |          |
//   |----------------------------------|        |          |
//   |    constantpool                  |        |          |
//   |----------------------------------|        |          |
//   |    method [changed to function]  |        |          |
//   |----------------------------------|        |          |
//   |    sp(current stack point)       |        |          |
//   |----------------------------------|        |          |
//   |    pc(bytecode addr)             |        v          v
//   +----------------------------------+--------+----------+
// ```
// address space grow from high address to low address.we add new field  **FrameType** ,
// the field's value is INTERPRETER_FRAME(represent interpreter frame).
// **currentsp**  is pointer to  callTarget field address, sp field 's value is  **currentsp** ,
// pre field pointer pre stack frame point. fill JSthread's sp field with iframe sp field
// by  calling JSThread->SetCurrentSPFrame and save pre Frame address to iframe pre field.

// For Example:
// ```
//                     call                  call
//     foo    -----------------> bar   -----------------------> rtfunc
// (interpret frame)       (OptLeaveFrame)             (Runtime Frame)
// ```

// Frame Layout as follow:
// ```
//   +----------------------------------+-------------------+
//   |    argv[n-1]                     |                   ^
//   |----------------------------------|                   |
//   |    ......                        |                   |
//   |----------------------------------|                   |
//   |    thisArg [maybe not exist]     |                   |
//   |----------------------------------|                   |
//   |    newTarget [maybe not exist]   |                   |
//   |----------------------------------|                   |
//   |    ......                        |                   |
//   |----------------------------------|                   |
//   |    Vregs                         |                   |
//   +----------------------------------+--------+     foo's frame
//   |    base.frameType                |        ^          |
//   |----------------------------------|        |          |
//   |    base.prev(pre stack pointer)  |        |          |
//   |----------------------------------|        |          |
//   |    env                           |        |          |
//   |----------------------------------|        |          |
//   |    acc                           |        |          |
//   |----------------------------------|        |          |
//   |    profileTypeInfo               |InterpretedFrame   |
//   |----------------------------------|        |          |
//   |    constantpool                  |        |          |
//   |----------------------------------|        |          |
//   |    function                      |        |          |
//   |----------------------------------|        |          |
//   |    sp(current stack point)       |        |          |
//   |----------------------------------|        |          |
//   |    pc(bytecode addr)             |        v          v
//   +----------------------------------+--------+----------+
//   |                   .............                      |
//   +--------------------------+---------------------------+
//   |     patchID          |   ^                           ^
//   |- - - - - - - - -     |   |                           |
//   |       fp             | Fixed                         |
//   |- - - - - - - - -     | OptLeaveFrame                 |
//   |       sp             |   |                       bar's frame Header
//   |- - - - - - - - -     |   |                           |
//   |       prev           |   |
//   |- - - - - - - - -     |   |                           |
//   |       frameType      |   v                           |
//   +--------------------------+---------------------------+
//   |                   .............                      |
//   +--------------------------+---------------------------+
//   |                                                      |
//   |                 rtfunc's Frame                       |
//   |                                                      |
//   +------------------------------------------------------+
// ```
// Iterator:
// rtfunc get bar's Frame **currentfp** by calling GetCurrentSPFrame.
// then get bar's Frame pre field.
// bar's Frame pre field point to foo's Frame **currentsp**.
// finally we can iterator foo's Frame.

#ifndef ECMASCRIPT_FRAMES_H
#define ECMASCRIPT_FRAMES_H

#include "ecmascript/js_tagged_value.h"

namespace panda::ecmascript {
class JSThread;
enum class FrameType: uint64_t {
    OPTIMIZED_FRAME = 0,
    OPTIMIZED_ENTRY_FRAME = 1,
    INTERPRETER_FRAME = 2,
    OPTIMIZED_LEAVE_FRAME = 3,
};

class OptimizedFrame {
public:
    OptimizedFrame() = default;
    ~OptimizedFrame() = default;
    uint64_t frameType;
    JSTaggedType *prev; // for llvm :c-fp ; for interrupt: thread-fp for gc
    static OptimizedFrame* GetFrameFromSp(JSTaggedType *sp)
    {
        return reinterpret_cast<OptimizedFrame *>(reinterpret_cast<uintptr_t>(sp)
            - MEMBER_OFFSET(OptimizedFrame, prev));
    }
};

class InterpretedFrameBase {
public:
    InterpretedFrameBase() = default;
    ~InterpretedFrameBase() = default;
    JSTaggedType  *prev; // for llvm :c-fp ; for interrupt: thread-fp for gc
    uint64_t frameType;
};

class OptimizedEntryFrame {
public:
    OptimizedEntryFrame() = default;
    ~OptimizedEntryFrame() = default;
    JSTaggedType  *threadFp; // for gc
    OptimizedFrame base;
    static OptimizedEntryFrame* GetFrameFromSp(JSTaggedType *sp)
    {
        return reinterpret_cast<OptimizedEntryFrame *>(reinterpret_cast<uintptr_t>(sp) -
            MEMBER_OFFSET(OptimizedEntryFrame, base.prev));
    }
};

#define INTERPRETED_FRAME_OFFSET_LIST(V)                                                            \
    V(SP, PC, sizeof(uint32_t), sizeof(uint64_t))                                                   \
    V(CONSTPOOL, SP, sizeof(uint32_t), sizeof(uint64_t))                                            \
    V(FUNCTION, CONSTPOOL, JSTaggedValue::TaggedTypeSize(), JSTaggedValue::TaggedTypeSize())        \
    V(PROFILETYPEINFO, FUNCTION, JSTaggedValue::TaggedTypeSize(), JSTaggedValue::TaggedTypeSize())  \
    V(ACC, PROFILETYPEINFO, JSTaggedValue::TaggedTypeSize(), JSTaggedValue::TaggedTypeSize())       \
    V(ENV, ACC, JSTaggedValue::TaggedTypeSize(), JSTaggedValue::TaggedTypeSize())                   \
    V(BASE, ENV, JSTaggedValue::TaggedTypeSize(), JSTaggedValue::TaggedTypeSize())                  \

static constexpr uint32_t INTERPRETED_FRAME_PC_OFFSET_32 = 0U;
static constexpr uint32_t INTERPRETED_FRAME_PC_OFFSET_64 = 0U;
#define INTERPRETED_FRAME_OFFSET_MACRO(name, lastName, lastSize32, lastSize64)                        \
    static constexpr uint32_t INTERPRETED_FRAME_##name##_OFFSET_32 = INTERPRETED_FRAME_##lastName##_OFFSET_32 + (lastSize32); \
    static constexpr uint32_t INTERPRETED_FRAME_##name##_OFFSET_64 = INTERPRETED_FRAME_##lastName##_OFFSET_64 + (lastSize64);
INTERPRETED_FRAME_OFFSET_LIST(INTERPRETED_FRAME_OFFSET_MACRO)
#undef INTERPRETED_FRAME_OFFSET_MACRO
static constexpr uint32_t SIZEOF_INTERPRETED_FRAME_32 = INTERPRETED_FRAME_ENV_OFFSET_32 +
    JSTaggedValue::TaggedTypeSize() + sizeof(uint32_t) + sizeof(uint64_t);
static constexpr uint32_t SIZEOF_INTERPRETED_FRAME_64 = INTERPRETED_FRAME_ENV_OFFSET_64 +
    JSTaggedValue::TaggedTypeSize() + sizeof(uint64_t) + sizeof(uint64_t);
// align with 8
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct InterpretedFrame {
    const uint8_t *pc;
    JSTaggedType *sp;
    // aligned with 8 bits
    alignas(sizeof(uint64_t)) JSTaggedValue constpool;
    JSTaggedValue function;
    JSTaggedValue profileTypeInfo;
    JSTaggedValue acc;
    JSTaggedValue env;
    InterpretedFrameBase base;

    static InterpretedFrame* GetFrameFromSp(JSTaggedType *sp)
    {
        return reinterpret_cast<InterpretedFrame *>(sp) - 1;
    }

    static constexpr uint32_t GetSpOffset(bool isArm32)
    {
        if (isArm32) {
            return INTERPRETED_FRAME_SP_OFFSET_32;
        }
        return INTERPRETED_FRAME_SP_OFFSET_64;
    }

    static constexpr uint32_t GetConstpoolOffset(bool isArm32)
    {
        if (isArm32) {
            return INTERPRETED_FRAME_CONSTPOOL_OFFSET_32;
        }
        return INTERPRETED_FRAME_CONSTPOOL_OFFSET_64;
    }

    static constexpr uint32_t GetFunctionOffset(bool isArm32)
    {
        if (isArm32) {
            return INTERPRETED_FRAME_FUNCTION_OFFSET_32;
        }
        return INTERPRETED_FRAME_FUNCTION_OFFSET_64;
    }

    static constexpr uint32_t GetProfileTypeInfoOffset(bool isArm32)
    {
        if (isArm32) {
            return INTERPRETED_FRAME_PROFILETYPEINFO_OFFSET_32;
        }
        return INTERPRETED_FRAME_PROFILETYPEINFO_OFFSET_64;
    }

    static constexpr uint32_t GetAccOffset(bool isArm32)
    {
        if (isArm32) {
            return INTERPRETED_FRAME_ACC_OFFSET_32;
        }
        return INTERPRETED_FRAME_ACC_OFFSET_64;
    }

    static constexpr uint32_t GetEnvOffset(bool isArm32)
    {
        if (isArm32) {
            return INTERPRETED_FRAME_ENV_OFFSET_32;
        }
        return INTERPRETED_FRAME_ENV_OFFSET_64;
    }
    
    static constexpr uint32_t GetBaseOffset(bool isArm32)
    {
        if (isArm32) {
            return INTERPRETED_FRAME_BASE_OFFSET_32;
        }
        return INTERPRETED_FRAME_BASE_OFFSET_64;
    }

    static constexpr uint32_t GetSize(bool isArm32)
    {
        if (isArm32) {
            return SIZEOF_INTERPRETED_FRAME_32;
        }
        return SIZEOF_INTERPRETED_FRAME_64;
    }
};
 
struct OptLeaveFrame {
    uint64_t frameType;
    JSTaggedType  *prev; // set cursp here
    uintptr_t sp;
    uintptr_t fp;
    uint64_t patchId;
    static OptLeaveFrame* GetFrameFromSp(JSTaggedType *sp)
    {
        return reinterpret_cast<OptLeaveFrame *>(reinterpret_cast<uintptr_t>(sp) -
            MEMBER_OFFSET(OptLeaveFrame, prev));
    }
};

class FrameCommonConstants {
public:
    static constexpr size_t FRAME_TYPE_OFFSET = -sizeof(uint64_t);
#ifdef PANDA_TARGET_AMD64
    static constexpr int SP_DWARF_REG_NUM = 7;
    static constexpr int FP_DWARF_REG_NUM = 6;
    static constexpr int SP_OFFSET = 2;
#else
#ifdef PANDA_TARGET_ARM64
    static constexpr int SP_DWARF_REG_NUM = 31;  /* x31 */
    static constexpr int FP_DWARF_REG_NUM = 29;  /* x29 */
    static constexpr int SP_OFFSET = -3;
#else
#ifdef PANDA_TARGET_ARM32
    static constexpr int SP_DWARF_REG_NUM = 13;
    static constexpr int FP_DWARF_REG_NUM = 11;
    static constexpr int SP_OFFSET = 0;
#else
    static constexpr int SP_DWARF_REG_NUM = 0;
    static constexpr int FP_DWARF_REG_NUM = 0;
    static constexpr int SP_OFFSET = 0;
#endif
#endif
#endif
    static constexpr int AARCH64_SLOT_SIZE = 8;
    static constexpr int AMD64_SLOT_SIZE = 8;
    static constexpr int ARM32_SLOT_SIZE = 4;
};
}  // namespace panda::ecmascript
#endif // ECMASCRIPT_FRAMES_H
