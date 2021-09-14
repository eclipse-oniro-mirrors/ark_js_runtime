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

#ifndef ECMASCRIPT_INTERPRETER_INTERPRETER_INL_H
#define ECMASCRIPT_INTERPRETER_INTERPRETER_INL_H

#include "ecmascript/class_linker/program_object-inl.h"
#include "ecmascript/ecma_string.h"
#include "ecmascript/ecma_vm.h"
#include "ecmascript/global_env.h"
#include "ecmascript/ic/ic_runtime_stub-inl.h"
#include "ecmascript/interpreter/fast_runtime_stub-inl.h"
#include "ecmascript/interpreter/interpreter.h"
#include "ecmascript/interpreter/slow_runtime_stub.h"
#include "ecmascript/js_generator_object.h"
#include "ecmascript/js_tagged_value.h"
#include "ecmascript/literal_data_extractor.h"
#include "ecmascript/runtime_call_id.h"
#include "ecmascript/template_string.h"
#include "ecmascript/vmstat/runtime_stat.h"
#include "include/runtime_notification.h"
#include "libpandafile/code_data_accessor.h"
#include "libpandafile/file.h"
#include "libpandafile/method_data_accessor.h"

namespace panda::ecmascript {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvoid-ptr-dereference"
#pragma clang diagnostic ignored "-Wgnu-label-as-value"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_INST() LOG(DEBUG, INTERPRETER) << ": "

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define HANDLE_OPCODE(handle_opcode) \
    handle_opcode:  // NOLINT(clang-diagnostic-gnu-label-as-value, cppcoreguidelines-macro-usage)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ADVANCE_PC(offset) \
    pc += (offset);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines-macro-usage)

#define GOTO_NEXT()  // NOLINT(clang-diagnostic-gnu-label-as-value, cppcoreguidelines-macro-usage)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DISPATCH(format)                                       \
    do {                                                       \
        ADVANCE_PC(BytecodeInstruction::Size(format))          \
        opcode = READ_INST_OP(); goto * dispatchTable[opcode]; \
    } while (false)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DISPATCH_OFFSET(offset)                                \
    do {                                                       \
        ADVANCE_PC(offset)                                     \
        opcode = READ_INST_OP(); goto * dispatchTable[opcode]; \
    } while (false)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define GET_FRAME(CurrentSp) \
    (reinterpret_cast<FrameState *>(CurrentSp) - 1)  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SAVE_PC() (GET_FRAME(sp)->pc = pc)  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SAVE_ACC() (GET_FRAME(sp)->acc = acc)  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define RESTORE_ACC() (acc = GET_FRAME(sp)->acc)  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define INTERPRETER_GOTO_EXCEPTION_HANDLER()          \
    do {                                              \
        SAVE_PC();                                    \
        goto *dispatchTable[EcmaOpcode::LAST_OPCODE]; \
    } while (false)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CHECK_SWITCH_TO_DEBUGGER_TABLE()        \
    if (Runtime::GetCurrent()->IsDebugMode()) { \
        dispatchTable = debugDispatchTable;     \
    }

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define REAL_GOTO_DISPATCH_OPCODE(opcode) \
    do {                                  \
        goto *instDispatchTable[opcode];  \
    } while (false)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define REAL_GOTO_EXCEPTION_HANDLER()                     \
    do {                                                  \
        SAVE_PC();                                        \
        goto *instDispatchTable[EcmaOpcode::LAST_OPCODE]; \
    } while (false)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define INTERPRETER_RETURN_IF_ABRUPT(result)      \
    do {                                          \
        if (result.IsException()) {               \
            INTERPRETER_GOTO_EXCEPTION_HANDLER(); \
        }                                         \
    } while (false)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define NOTIFY_DEBUGGER_EVENT()          \
    do {                                 \
        SAVE_ACC();                      \
        SAVE_PC();                       \
        NotifyBytecodePcChanged(thread); \
        RESTORE_ACC();                   \
    } while (false)

#if ECMASCRIPT_ENABLE_IC
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define UPDATE_HOTNESS_COUNTER_NON_ACC(offset)   (UpdateHotnessCounter(thread, sp, acc, offset))

#define UPDATE_HOTNESS_COUNTER(offset)                       \
    do {                                                     \
        if (UpdateHotnessCounter(thread, sp, acc, offset)) { \
            RESTORE_ACC();                                   \
        }                                                    \
    } while (false)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define UPDATE_HOTNESS_COUNTER(offset) static_cast<void>(0)
#define UPDATE_HOTNESS_COUNTER_NON_ACC(offset) static_cast<void>(0)
#endif

#define READ_INST_OP() READ_INST_8(0)               // NOLINT(hicpp-signed-bitwise, cppcoreguidelines-macro-usage)
#define READ_INST_4_0() (READ_INST_8(1) & 0xf)      // NOLINT(hicpp-signed-bitwise, cppcoreguidelines-macro-usage)
#define READ_INST_4_1() (READ_INST_8(1) >> 4 & 0xf) // NOLINT(hicpp-signed-bitwise, cppcoreguidelines-macro-usage)
#define READ_INST_4_2() (READ_INST_8(2) & 0xf)      // NOLINT(hicpp-signed-bitwise, cppcoreguidelines-macro-usage)
#define READ_INST_4_3() (READ_INST_8(2) >> 4 & 0xf) // NOLINT(hicpp-signed-bitwise, cppcoreguidelines-macro-usage)
#define READ_INST_8_0() READ_INST_8(1)              // NOLINT(hicpp-signed-bitwise, cppcoreguidelines-macro-usage)
#define READ_INST_8_1() READ_INST_8(2)              // NOLINT(hicpp-signed-bitwise, cppcoreguidelines-macro-usage)
#define READ_INST_8_2() READ_INST_8(3)              // NOLINT(hicpp-signed-bitwise, cppcoreguidelines-macro-usage)
#define READ_INST_8_3() READ_INST_8(4)              // NOLINT(hicpp-signed-bitwise, cppcoreguidelines-macro-usage)
#define READ_INST_8_4() READ_INST_8(5)              // NOLINT(hicpp-signed-bitwise, cppcoreguidelines-macro-usage)
#define READ_INST_8_5() READ_INST_8(6)              // NOLINT(hicpp-signed-bitwise, cppcoreguidelines-macro-usage)
#define READ_INST_8_6() READ_INST_8(7)              // NOLINT(hicpp-signed-bitwise, cppcoreguidelines-macro-usage)
#define READ_INST_8_7() READ_INST_8(8)              // NOLINT(hicpp-signed-bitwise, cppcoreguidelines-macro-usage)
#define READ_INST_8_8() READ_INST_8(9)              // NOLINT(hicpp-signed-bitwise, cppcoreguidelines-macro-usage)
#define READ_INST_8(offset) (*(pc + offset))
#define MOVE_AND_READ_INST_8(currentInst, offset) \
    currentInst <<= 8;                            \
    currentInst += READ_INST_8(offset);           \

#define READ_INST_16_0() READ_INST_16(2)
#define READ_INST_16_1() READ_INST_16(3)
#define READ_INST_16_2() READ_INST_16(4)
#define READ_INST_16_3() READ_INST_16(5)
#define READ_INST_16_5() READ_INST_16(7)
#define READ_INST_16(offset)                          \
    ({                                                \
        uint16_t currentInst = READ_INST_8(offset);   \
        MOVE_AND_READ_INST_8(currentInst, offset - 1) \
    })

#define READ_INST_32_0() READ_INST_32(4)
#define READ_INST_32_1() READ_INST_32(5)
#define READ_INST_32_2() READ_INST_32(6)
#define READ_INST_32(offset)                          \
    ({                                                \
        uint32_t currentInst = READ_INST_8(offset);   \
        MOVE_AND_READ_INST_8(currentInst, offset - 1) \
        MOVE_AND_READ_INST_8(currentInst, offset - 2) \
        MOVE_AND_READ_INST_8(currentInst, offset - 3) \
    })

#define READ_INST_64_0()                       \
    ({                                         \
        uint64_t currentInst = READ_INST_8(8); \
        MOVE_AND_READ_INST_8(currentInst, 7)   \
        MOVE_AND_READ_INST_8(currentInst, 6)   \
        MOVE_AND_READ_INST_8(currentInst, 5)   \
        MOVE_AND_READ_INST_8(currentInst, 4)   \
        MOVE_AND_READ_INST_8(currentInst, 3)   \
        MOVE_AND_READ_INST_8(currentInst, 2)   \
        MOVE_AND_READ_INST_8(currentInst, 1)   \
    })

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define GET_VREG(idx) (sp[idx])  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define GET_VREG_VALUE(idx) (JSTaggedValue(sp[idx]))  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SET_VREG(idx, val) (sp[idx] = (val));  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
#define GET_ACC() (acc)                        // NOLINT(cppcoreguidelines-macro-usage)
#define SET_ACC(val) (acc = val);              // NOLINT(cppcoreguidelines-macro-usage)

JSTaggedValue EcmaInterpreter::ExecuteNative(JSThread *thread, const CallParams& params)
{
    JSTaggedType *sp = const_cast<JSTaggedType *>(thread->GetCurrentSPFrame());
    JSMethod *methodToCall = params.callTarget->GetCallTarget();
    ASSERT(methodToCall->GetNumVregs() == 0);
    uint32_t numActualArgs = params.argc + RESERVED_CALL_ARGCOUNT;
    // Tags and values of thread, new_tgt and argv_length are put into frames too for native frames
    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    size_t frameSize = FRAME_STATE_SIZE + numActualArgs;
    JSTaggedType *newSp = sp - frameSize;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    if (thread->DoStackOverflowCheck(newSp) || thread->HasPendingException()) {
        return JSTaggedValue::Undefined();
    }

    EcmaRuntimeCallInfo ecmaRuntimeCallInfo(thread, numActualArgs, reinterpret_cast<JSTaggedValue *>(newSp));
    newSp[RESERVED_INDEX_CALL_TARGET] = reinterpret_cast<TaggedType>(params.callTarget);
    newSp[RESERVED_INDEX_NEW_TARGET] = params.newTarget;
    newSp[RESERVED_INDEX_THIS] = params.thisArg;
    for (size_t i = 0; i < params.argc; i++) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        newSp[i + RESERVED_CALL_ARGCOUNT] = params.argv[i];
    }

    FrameState *state = GET_FRAME(newSp);
    state->prev = sp;
    state->pc = nullptr;
    state->sp = newSp;
    state->method = methodToCall;
    thread->SetCurrentSPFrame(newSp);
    LOG(DEBUG, INTERPRETER) << "Entry: Runtime Call.";
    JSTaggedValue tagged =
        reinterpret_cast<EcmaEntrypoint>(const_cast<void *>(methodToCall->GetNativePointer()))(&ecmaRuntimeCallInfo);
    LOG(DEBUG, INTERPRETER) << "Exit: Runtime Call.";
    thread->SetCurrentSPFrame(sp);
    return tagged;
}

JSTaggedValue EcmaInterpreter::Execute(JSThread *thread, const CallParams& params)
{
    JSMethod *method = params.callTarget->GetCallTarget();
    ASSERT(thread->IsEcmaInterpreter());
    if (method->IsNative()) {
        return EcmaInterpreter::ExecuteNative(thread, params);
    }

    JSTaggedType *originalPrevSp = const_cast<JSTaggedType *>(thread->GetCurrentSPFrame());

    // push break state
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    JSTaggedType *newSp = originalPrevSp - FRAME_STATE_SIZE;
    if (thread->DoStackOverflowCheck(newSp) || thread->HasPendingException()) {
        return JSTaggedValue::Undefined();
    }
    FrameState *breakState = GET_FRAME(newSp);
    breakState->pc = nullptr;
    breakState->sp = nullptr;
    breakState->prev = originalPrevSp;
    breakState->numActualArgs = 0;
    JSTaggedType *prevSp = newSp;

    uint32_t numActualArgs = params.argc + RESERVED_CALL_ARGCOUNT;
    // push method frame
    uint32_t numVregs = method->GetNumVregs();
    uint32_t numDeclaredArgs = method->GetNumArgs();
    size_t frameSize = FRAME_STATE_SIZE + numVregs + std::max(numDeclaredArgs, numActualArgs);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    newSp = prevSp - frameSize;
    if (thread->DoStackOverflowCheck(newSp) || thread->HasPendingException()) {
        return JSTaggedValue::Undefined();
    }
    // push args
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    newSp[numVregs + RESERVED_INDEX_CALL_TARGET] = reinterpret_cast<TaggedType>(params.callTarget);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    newSp[numVregs + RESERVED_INDEX_NEW_TARGET] = params.newTarget;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    newSp[numVregs + RESERVED_INDEX_THIS] = params.thisArg;
    for (size_t i = 0; i < params.argc; i++) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        newSp[numVregs + i + RESERVED_CALL_ARGCOUNT] = params.argv[i];
    }
    InterpreterFrameCopyArgs(newSp, numVregs, numActualArgs, numDeclaredArgs);

    const uint8_t *pc = JSMethod::Cast(method)->GetBytecodeArray();
    FrameState *state = GET_FRAME(newSp);
    state->pc = pc;
    state->sp = newSp;
    state->method = method;
    state->acc = JSTaggedValue::Hole();
    JSHandle<JSTaggedValue> callTargetHandle(thread, params.callTarget);
    ASSERT(callTargetHandle->IsJSFunction());
    JSHandle<JSFunction> thisFunc = JSHandle<JSFunction>::Cast(callTargetHandle);
    ConstantPool *constpool = ConstantPool::Cast(thisFunc->GetConstantPool().GetTaggedObject());
    state->constpool = constpool;
    state->profileTypeInfo = thisFunc->GetProfileTypeInfo();
    state->prev = prevSp;
    state->numActualArgs = numActualArgs;

    JSTaggedValue env = thisFunc->GetLexicalEnv();
    state->env = env;

    thread->SetCurrentSPFrame(newSp);

    LOG(DEBUG, INTERPRETER) << "break Entry: Runtime Call " << std::hex << reinterpret_cast<uintptr_t>(newSp) << " "
                            << std::hex << reinterpret_cast<uintptr_t>(pc);

    EcmaInterpreter::RunInternal(thread, constpool, pc, newSp);

    // NOLINTNEXTLINE(readability-identifier-naming)
    const JSTaggedValue resAcc = state->acc;
    // pop frame
    thread->SetCurrentSPFrame(originalPrevSp);

    return resAcc;
}

JSTaggedValue EcmaInterpreter::GeneratorReEnterInterpreter(JSThread *thread, JSHandle<GeneratorContext> context)
{
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    JSHandle<JSFunction> func = JSHandle<JSFunction>::Cast(JSHandle<JSTaggedValue>(thread, context->GetMethod()));
    JSMethod *method = func->GetCallTarget();

    JSTaggedType *currentSp = const_cast<JSTaggedType *>(thread->GetCurrentSPFrame());

    // push break frame
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    JSTaggedType *breakSp = currentSp - FRAME_STATE_SIZE;
    if (thread->DoStackOverflowCheck(breakSp) || thread->HasPendingException()) {
        return JSTaggedValue::Exception();
    }
    FrameState *breakState = GET_FRAME(breakSp);
    breakState->pc = nullptr;
    breakState->sp = nullptr;
    breakState->prev = currentSp;
    breakState->numActualArgs = 0;

    // create new frame and resume sp and pc
    uint32_t nregs = context->GetNRegs().GetInt();
    size_t newFrameSize = FRAME_STATE_SIZE + nregs;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic
    JSTaggedType *newSp = breakSp - newFrameSize;
    if (thread->DoStackOverflowCheck(newSp) || thread->HasPendingException()) {
        return JSTaggedValue::Exception();
    }
    JSHandle<TaggedArray> regsArray(thread, context->GetRegsArray());
    for (size_t i = 0; i < nregs; i++) {
        newSp[i] = regsArray->Get(i).GetRawData();  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }
    ConstantPool *constpool = ConstantPool::Cast(func->GetConstantPool().GetTaggedObject());
    JSTaggedValue pcOffset = context->GetBCOffset();
    // pc = first_inst + offset + size(Opcode::SUSPENDGENERATOR_IMM8_V8_V8)
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const uint8_t *resumePc = JSMethod::Cast(method)->GetBytecodeArray() + static_cast<uint32_t>(pcOffset.GetInt()) +
                              BytecodeInstruction::Size(BytecodeInstruction::Format::PREF_V8_V8);

    FrameState *state = GET_FRAME(newSp);
    state->pc = resumePc;
    state->sp = newSp;
    state->method = method;
    state->constpool = constpool;
    state->profileTypeInfo = func->GetProfileTypeInfo();
    state->acc = context->GetAcc();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    state->prev = breakSp;
    JSTaggedValue env = context->GetLexicalEnv();
    state->env = env;
    // execute interpreter
    thread->SetCurrentSPFrame(newSp);
    EcmaInterpreter::RunInternal(thread, constpool, resumePc, newSp);

    JSTaggedValue res = state->acc;
    // pop frame
    thread->SetCurrentSPFrame(currentSp);

    return res;
}

void EcmaInterpreter::ChangeGenContext(JSThread *thread, JSHandle<GeneratorContext> context)
{
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    JSHandle<JSFunction> func = JSHandle<JSFunction>::Cast(JSHandle<JSTaggedValue>(thread, context->GetMethod()));
    JSMethod *method = func->GetCallTarget();

    JSTaggedType *currentSp = const_cast<JSTaggedType *>(thread->GetCurrentSPFrame());

    // push break frame
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    JSTaggedType *breakSp = currentSp - FRAME_STATE_SIZE;
    if (thread->DoStackOverflowCheck(breakSp) || thread->HasPendingException()) {
        return;
    }
    FrameState *breakState = GET_FRAME(breakSp);
    breakState->pc = nullptr;
    breakState->sp = nullptr;
    breakState->prev = currentSp;

    // create new frame and resume sp and pc
    uint32_t nregs = context->GetNRegs().GetInt();
    size_t newFrameSize = FRAME_STATE_SIZE + nregs;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic
    JSTaggedType *newSp = breakSp - newFrameSize;
    if (thread->DoStackOverflowCheck(newSp) || thread->HasPendingException()) {
        return;
    }
    JSHandle<TaggedArray> regsArray(thread, context->GetRegsArray());
    for (size_t i = 0; i < nregs; i++) {
        newSp[i] = regsArray->Get(i).GetRawData();  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }
    ConstantPool *constpool = ConstantPool::Cast(func->GetConstantPool().GetTaggedObject());
    JSTaggedValue pcOffset = context->GetBCOffset();
    // pc = first_inst + offset
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const uint8_t *pc = JSMethod::Cast(method)->GetBytecodeArray() + static_cast<uint32_t>(pcOffset.GetInt());

    FrameState *state = GET_FRAME(newSp);
    state->pc = pc;
    state->sp = newSp;
    state->method = method;
    state->constpool = constpool;
    state->profileTypeInfo = func->GetProfileTypeInfo();
    state->acc = context->GetAcc();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    state->prev = breakSp;
    state->env = context->GetLexicalEnv();

    thread->SetCurrentSPFrame(newSp);
}

void EcmaInterpreter::ResumeContext(JSThread *thread)
{
    JSTaggedType *sp = const_cast<JSTaggedType *>(thread->GetCurrentSPFrame());
    FrameState *state = GET_FRAME(sp);
    thread->SetCurrentSPFrame(state->prev);
}

void EcmaInterpreter::NotifyBytecodePcChanged(JSThread *thread)
{
    EcmaFrameHandler frameHandler(thread);
    for (; frameHandler.HasFrame(); frameHandler.PrevFrame()) {
        JSMethod *method = frameHandler.GetMethod();
        // Skip builtins method
        if (method->IsNative()) {
            continue;
        }
        auto bcOffset = frameHandler.GetBytecodeOffset();
        Runtime::GetCurrent()->GetNotificationManager()->BytecodePcChangedEvent(thread, method, bcOffset);
        return;
    }
}

// NOLINTNEXTLINE(readability-function-size)
NO_UB_SANITIZE void EcmaInterpreter::RunInternal(JSThread *thread, ConstantPool *constpool, const uint8_t *pc,
                                                 JSTaggedType *sp)
{
    INTERPRETER_TRACE(thread, RunInternal);
    uint8_t opcode = READ_INST_OP();
    JSTaggedValue acc = JSTaggedValue::Hole();
    EcmaVM *ecmaVm = thread->GetEcmaVM();
    JSHandle<GlobalEnv> globalEnv = ecmaVm->GetGlobalEnv();
    JSTaggedValue globalObj = globalEnv->GetGlobalObject();
    ObjectFactory *factory = ecmaVm->GetFactory();

    constexpr size_t numOps = 0x100;

    static std::array<const void *, numOps> instDispatchTable{
#include "templates/instruction_dispatch.inl"
    };

    static std::array<const void *, numOps> debugDispatchTable{
#include "templates/debugger_instruction_dispatch.inl"
    };

    std::array<const void *, numOps> dispatchTable = instDispatchTable;
    CHECK_SWITCH_TO_DEBUGGER_TABLE();
    goto *dispatchTable[opcode];

    HANDLE_OPCODE(HANDLE_MOV_V4_V4) {
        uint16_t vdst = READ_INST_4_0();
        uint16_t vsrc = READ_INST_4_1();
        LOG_INST() << "mov v" << vdst << ", v" << vsrc;
        uint64_t value = GET_VREG(vsrc);
        SET_VREG(vdst, value)
        DISPATCH(BytecodeInstruction::Format::V4_V4);
    }

    HANDLE_OPCODE(HANDLE_MOV_DYN_V8_V8) {
        uint16_t vdst = READ_INST_8_0();
        uint16_t vsrc = READ_INST_8_1();
        LOG_INST() << "mov.dyn v" << vdst << ", v" << vsrc;
        uint64_t value = GET_VREG(vsrc);
        SET_VREG(vdst, value)
        DISPATCH(BytecodeInstruction::Format::V8_V8);
    }
    HANDLE_OPCODE(HANDLE_MOV_DYN_V16_V16) {
        uint16_t vdst = READ_INST_16_0();
        uint16_t vsrc = READ_INST_16_2();
        LOG_INST() << "mov.dyn v" << vdst << ", v" << vsrc;
        uint64_t value = GET_VREG(vsrc);
        SET_VREG(vdst, value)
        DISPATCH(BytecodeInstruction::Format::V16_V16);
    }
    HANDLE_OPCODE(HANDLE_LDA_STR_ID32) {
        uint32_t stringId = READ_INST_32_0();
        LOG_INST() << "lda.str " << std::hex << stringId;
        SET_ACC(constpool->GetObjectFromCache(stringId));
        DISPATCH(BytecodeInstruction::Format::ID32);
    }
    HANDLE_OPCODE(HANDLE_JMP_IMM8) {
        int8_t offset = READ_INST_8_0();
        UPDATE_HOTNESS_COUNTER(offset);
        LOG_INST() << "jmp " << std::hex << static_cast<int32_t>(offset);
        DISPATCH_OFFSET(offset);
    }
    HANDLE_OPCODE(HANDLE_JMP_IMM16) {
        int16_t offset = READ_INST_16_0();
        UPDATE_HOTNESS_COUNTER(offset);
        LOG_INST() << "jmp " << std::hex << static_cast<int32_t>(offset);
        DISPATCH_OFFSET(offset);
    }
    HANDLE_OPCODE(HANDLE_JMP_IMM32) {
        int32_t offset = READ_INST_32_0();
        UPDATE_HOTNESS_COUNTER(offset);
        LOG_INST() << "jmp " << std::hex << offset;
        DISPATCH_OFFSET(offset);
    }
    HANDLE_OPCODE(HANDLE_JEQZ_IMM8) {
        int8_t offset = READ_INST_8_0();
        LOG_INST() << "jeqz ->\t"
                   << "cond jmpz " << std::hex << static_cast<int32_t>(offset);
        if (GET_ACC() == JSTaggedValue::False() || (GET_ACC().IsInt() && GET_ACC().GetInt() == 0) ||
            (GET_ACC().IsDouble() && GET_ACC().GetDouble() == 0)) {
            UPDATE_HOTNESS_COUNTER(offset);
            DISPATCH_OFFSET(offset);
        } else {
            DISPATCH(BytecodeInstruction::Format::PREF_NONE);
        }
    }
    HANDLE_OPCODE(HANDLE_JEQZ_IMM16) {
        int16_t offset = READ_INST_16_0();
        LOG_INST() << "jeqz ->\t"
                   << "cond jmpz " << std::hex << static_cast<int32_t>(offset);
        if (GET_ACC() == JSTaggedValue::False() || (GET_ACC().IsInt() && GET_ACC().GetInt() == 0) ||
            (GET_ACC().IsDouble() && GET_ACC().GetDouble() == 0)) {
            UPDATE_HOTNESS_COUNTER(offset);
            DISPATCH_OFFSET(offset);
        } else {
            DISPATCH(BytecodeInstruction::Format::IMM16);
        }
    }
    HANDLE_OPCODE(HANDLE_JNEZ_IMM8) {
        int8_t offset = READ_INST_8_0();
        LOG_INST() << "jnez ->\t"
                   << "cond jmpz " << std::hex << static_cast<int32_t>(offset);
        if (GET_ACC() == JSTaggedValue::True() || (GET_ACC().IsInt() && GET_ACC().GetInt() != 0) ||
            (GET_ACC().IsDouble() && GET_ACC().GetDouble() != 0)) {
            UPDATE_HOTNESS_COUNTER(offset);
            DISPATCH_OFFSET(offset);
        } else {
            DISPATCH(BytecodeInstruction::Format::PREF_NONE);
        }
    }
    HANDLE_OPCODE(HANDLE_JNEZ_IMM16) {
        int16_t offset = READ_INST_16_0();
        LOG_INST() << "jnez ->\t"
                   << "cond jmpz " << std::hex << static_cast<int32_t>(offset);
        if (GET_ACC() == JSTaggedValue::True() || (GET_ACC().IsInt() && GET_ACC().GetInt() != 0) ||
            (GET_ACC().IsDouble() && GET_ACC().GetDouble() != 0)) {
            UPDATE_HOTNESS_COUNTER(offset);
            DISPATCH_OFFSET(offset);
        } else {
            DISPATCH(BytecodeInstruction::Format::IMM16);
        }
    }
    HANDLE_OPCODE(HANDLE_LDA_DYN_V8) {
        uint16_t vsrc = READ_INST_8_0();
        LOG_INST() << "lda.dyn v" << vsrc;
        uint64_t value = GET_VREG(vsrc);
        SET_ACC(JSTaggedValue(value))
        DISPATCH(BytecodeInstruction::Format::V8);
    }
    HANDLE_OPCODE(HANDLE_STA_DYN_V8) {
        uint16_t vdst = READ_INST_8_0();
        LOG_INST() << "sta.dyn v" << vdst;
        SET_VREG(vdst, GET_ACC().GetRawData())
        DISPATCH(BytecodeInstruction::Format::V8);
    }
    HANDLE_OPCODE(HANDLE_LDAI_DYN_IMM32) {
        int32_t imm = READ_INST_32_0();
        LOG_INST() << "ldai.dyn " << std::hex << imm;
        SET_ACC(JSTaggedValue(imm))
        DISPATCH(BytecodeInstruction::Format::IMM32);
    }

    HANDLE_OPCODE(HANDLE_FLDAI_DYN_IMM64) {
        auto imm = bit_cast<double>(READ_INST_64_0());
        LOG_INST() << "fldai.dyn " << imm;
        SET_ACC(JSTaggedValue(imm))
        DISPATCH(BytecodeInstruction::Format::IMM64);
    }
    {
        uint32_t actualNumArgs;
        uint32_t funcReg;
        bool callThis;
        bool callRange;

        HANDLE_OPCODE(HANDLE_CALLARG0DYN_PREF_V8) {
            funcReg = READ_INST_8_1();
            actualNumArgs = ActualNumArgsOfCall::CALLARG0;

            LOG_INST() << "callarg0.dyn "
                       << "v" << funcReg;
            callRange = false;
            callThis = false;
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto, hicpp-avoid-goto)
            goto handlerCall;
        }
        HANDLE_OPCODE(HANDLE_CALLARG1DYN_PREF_V8_V8) {
            funcReg = READ_INST_8_1();
            actualNumArgs = ActualNumArgsOfCall::CALLARG1;

            LOG_INST() << "callarg1.dyn "
                       << "v" << funcReg << ", v" << READ_INST_8_2();
            callRange = false;
            callThis = false;
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto, hicpp-avoid-goto)
            goto handlerCall;
        }
        HANDLE_OPCODE(HANDLE_CALLARGS2DYN_PREF_V8_V8_V8) {
            funcReg = READ_INST_8_1();
            actualNumArgs = ActualNumArgsOfCall::CALLARGS2;

            LOG_INST() << "callargs2.dyn "
                       << "v" << funcReg << ", v" << READ_INST_8_2() << ", v" << READ_INST_8_3();
            callRange = false;
            callThis = false;
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto, hicpp-avoid-goto)
            goto handlerCall;
        }
        HANDLE_OPCODE(HANDLE_CALLARGS3DYN_PREF_V8_V8_V8_V8) {
            funcReg = READ_INST_8_1();
            actualNumArgs = ActualNumArgsOfCall::CALLARGS3;

            LOG_INST() << "callargs3.dyn "
                       << "v" << funcReg << ", v" << READ_INST_8_2() << ", v" << READ_INST_8_3()
                       << ", v" << READ_INST_8_4();
            callRange = false;
            callThis = false;
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto, hicpp-avoid-goto)
            goto handlerCall;
        }
        HANDLE_OPCODE(HANDLE_CALLITHISRANGEDYN_PREF_IMM16_V8) {
            actualNumArgs = READ_INST_16_1() + 2;  // 2: func and newTarget
            funcReg = READ_INST_8_3();

            LOG_INST() << "calli.dyn.this.range " << actualNumArgs << ", v" << funcReg;
            callRange = true;
            callThis = true;
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto, hicpp-avoid-goto)
            goto handlerCall;
        }
        HANDLE_OPCODE(HANDLE_CALLSPREADDYN_PREF_V8_V8_V8) {
            uint16_t v0 = READ_INST_8_1();
            uint16_t v1 = READ_INST_8_2();
            uint16_t v2 = READ_INST_8_3();
            LOG_INST() << "intrinsics::callspreaddyn"
                       << " v" << v0 << " v" << v1 << " v" << v2;
            JSTaggedValue func = GET_VREG_VALUE(v0);
            JSTaggedValue obj = GET_VREG_VALUE(v1);
            JSTaggedValue array = GET_VREG_VALUE(v2);

            JSTaggedValue res = SlowRuntimeStub::CallSpreadDyn(thread, func, obj, array);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            SET_ACC(res);

            DISPATCH(BytecodeInstruction::Format::PREF_V8_V8_V8);
        }
        HANDLE_OPCODE(HANDLE_CALLIRANGEDYN_PREF_IMM16_V8) {
            actualNumArgs = READ_INST_16_1() + NUM_MANDATORY_JSFUNC_ARGS;
            funcReg = READ_INST_8_3();
            callRange = true;
            callThis = false;
            LOG_INST() << "calli.rangedyn " << actualNumArgs << ", v" << funcReg;

        handlerCall:
            JSTaggedValue func = GET_VREG_VALUE(funcReg);
            if (!func.IsCallable()) {
                {
                    [[maybe_unused]] EcmaHandleScope handleScope(thread);
                    JSHandle<JSObject> error = factory->GetJSError(ErrorType::TYPE_ERROR, "is not callable");
                    thread->SetException(error.GetTaggedValue());
                }
                INTERPRETER_GOTO_EXCEPTION_HANDLER();
            }
            ECMAObject *thisFunc = ECMAObject::Cast(func.GetTaggedObject());
            JSMethod *methodToCall = thisFunc->GetCallTarget();
            if (methodToCall->IsNative()) {
                ASSERT(methodToCall->GetNumVregs() == 0);
                // Tags and values of thread, new_tgt and argv_length are put into frames too for native frames
                // NOLINTNEXTLINE(hicpp-signed-bitwise)
                size_t frameSize = FRAME_STATE_SIZE + actualNumArgs;
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                JSTaggedType *newSp = sp - frameSize;
                if (thread->DoStackOverflowCheck(newSp) || thread->HasPendingException()) {
                    INTERPRETER_GOTO_EXCEPTION_HANDLER();
                }
                EcmaRuntimeCallInfo ecmaRuntimeCallInfo(thread, actualNumArgs,
                                                        reinterpret_cast<JSTaggedValue *>(newSp));
                uint32_t startIdx = 0;
                // func
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                newSp[startIdx++] = static_cast<JSTaggedType>(ToUintPtr(thisFunc));
                // new target
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                newSp[startIdx++] = JSTaggedValue::VALUE_UNDEFINED;

                if (callRange) {
                    size_t copyArgs = actualNumArgs - 2;  // 2: skip func and new target
                    if (!callThis) {
                        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                        newSp[startIdx++] = JSTaggedValue::VALUE_UNDEFINED;
                        // skip this
                        copyArgs--;
                    }
                    for (size_t i = 1; i <= copyArgs; i++) {
                        JSTaggedValue arg = GET_VREG_VALUE(funcReg + i);
                        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                        newSp[startIdx++] = arg.GetRawData();
                    }
                } else {
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                    newSp[startIdx] = JSTaggedValue::VALUE_UNDEFINED;
                    switch (actualNumArgs) {
                        case ActualNumArgsOfCall::CALLARGS3: {
                            uint32_t reg = READ_INST_8_4();
                            JSTaggedValue arg = GET_VREG_VALUE(reg);
                            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                            newSp[ActualNumArgsOfCall::CALLARGS3 - 1] = arg.GetRawData();
                            [[fallthrough]];
                        }
                        case ActualNumArgsOfCall::CALLARGS2: {
                            uint32_t reg = READ_INST_8_3();
                            JSTaggedValue arg = GET_VREG_VALUE(reg);
                            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                            newSp[ActualNumArgsOfCall::CALLARGS2 - 1] = arg.GetRawData();
                            [[fallthrough]];
                        }
                        case ActualNumArgsOfCall::CALLARG1: {
                            uint32_t reg = READ_INST_8_2();
                            JSTaggedValue arg = GET_VREG_VALUE(reg);
                            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                            newSp[ActualNumArgsOfCall::CALLARG1 - 1] = arg.GetRawData();
                            [[fallthrough]];
                        }
                        case ActualNumArgsOfCall::CALLARG0: {
                            break;
                        }
                        default:
                            UNREACHABLE();
                    }
                }

                FrameState *state = GET_FRAME(newSp);
                state->prev = sp;
                state->pc = nullptr;
                state->sp = newSp;
                state->method = methodToCall;
                thread->SetCurrentSPFrame(newSp);
                LOG(DEBUG, INTERPRETER) << "Entry: Runtime Call.";
                JSTaggedValue retValue = reinterpret_cast<EcmaEntrypoint>(
                    const_cast<void *>(methodToCall->GetNativePointer()))(&ecmaRuntimeCallInfo);
                if (UNLIKELY(thread->HasPendingException())) {
                    INTERPRETER_GOTO_EXCEPTION_HANDLER();
                }
                LOG(DEBUG, INTERPRETER) << "Exit: Runtime Call.";
                thread->SetCurrentSPFrame(sp);
                SET_ACC(retValue);
                size_t jumpSize = GetJumpSizeAfterCall(pc);
                DISPATCH_OFFSET(jumpSize);
            } else {
                if (JSFunction::Cast(thisFunc)->IsClassConstructor()) {
                    {
                        [[maybe_unused]] EcmaHandleScope handleScope(thread);
                        JSHandle<JSObject> error =
                            factory->GetJSError(ErrorType::TYPE_ERROR, "class constructor cannot called without 'new'");
                        thread->SetException(error.GetTaggedValue());
                    }
                    INTERPRETER_GOTO_EXCEPTION_HANDLER();
                }
                SAVE_PC();
                uint32_t numVregs = methodToCall->GetNumVregs();
                uint32_t numDeclaredArgs = methodToCall->GetNumArgs();
                size_t frameSize = FRAME_STATE_SIZE + numVregs + std::max(numDeclaredArgs, actualNumArgs);
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                JSTaggedType *newSp = sp - frameSize;
                if (thread->DoStackOverflowCheck(newSp) || thread->HasPendingException()) {
                    INTERPRETER_GOTO_EXCEPTION_HANDLER();
                }

                // copy args
                uint32_t startIdx = numVregs;
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                newSp[startIdx++] = static_cast<JSTaggedType>(ToUintPtr(thisFunc));
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                newSp[startIdx++] = JSTaggedValue::VALUE_UNDEFINED;
                if (callRange) {
                    size_t copyArgs = actualNumArgs - 2;  // 2: skip func and new target
                    if (!callThis) {
                        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                        newSp[startIdx++] = JSTaggedValue::VALUE_UNDEFINED;
                        // skip this
                        copyArgs--;
                    }
                    for (size_t i = 1; i <= copyArgs; i++) {
                        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                        newSp[startIdx++] = sp[funcReg + i];
                    }
                } else {
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                    newSp[startIdx] = JSTaggedValue::VALUE_UNDEFINED;
                    switch (actualNumArgs) {
                        case ActualNumArgsOfCall::CALLARGS3:
                            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                            newSp[numVregs + ActualNumArgsOfCall::CALLARGS3 - 1] = sp[READ_INST_8_4()];
                            [[fallthrough]];
                        case ActualNumArgsOfCall::CALLARGS2:
                            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                            newSp[numVregs + ActualNumArgsOfCall::CALLARGS2 - 1] = sp[READ_INST_8_3()];
                            [[fallthrough]];
                        case ActualNumArgsOfCall::CALLARG1:
                            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                            newSp[numVregs + ActualNumArgsOfCall::CALLARG1 - 1] = sp[READ_INST_8_2()];
                            [[fallthrough]];
                        case ActualNumArgsOfCall::CALLARG0:
                            break;
                        default:
                            UNREACHABLE();
                    }
                }
                InterpreterFrameCopyArgs(newSp, numVregs, actualNumArgs, numDeclaredArgs);

                FrameState *state = GET_FRAME(newSp);
                state->prev = sp;
                state->pc = pc = JSMethod::Cast(methodToCall)->GetBytecodeArray();
                state->sp = sp = newSp;
                state->method = methodToCall;
                state->acc = JSTaggedValue::Hole();
                state->constpool = constpool =
                    ConstantPool::Cast(JSFunction::Cast(thisFunc)->GetConstantPool().GetTaggedObject());
                state->profileTypeInfo = JSFunction::Cast(thisFunc)->GetProfileTypeInfo();
                state->numActualArgs = actualNumArgs;
                JSTaggedValue env = JSFunction::Cast(thisFunc)->GetLexicalEnv();
                state->env = env;

                thread->SetCurrentSPFrame(newSp);
                LOG(DEBUG, INTERPRETER) << "Entry: Runtime Call " << std::hex << reinterpret_cast<uintptr_t>(sp) << " "
                                        << std::hex << reinterpret_cast<uintptr_t>(pc);
                DISPATCH_OFFSET(0);
            }
        }
    }
    HANDLE_OPCODE(HANDLE_RETURN_DYN) {
        LOG_INST() << "return.dyn";
        FrameState *state = GET_FRAME(sp);
        LOG(DEBUG, INTERPRETER) << "Exit: Runtime Call " << std::hex << reinterpret_cast<uintptr_t>(state->sp) << " "
                                << std::hex << reinterpret_cast<uintptr_t>(state->pc);
        [[maybe_unused]] auto fistPC = state->method->GetInstructions();
        UPDATE_HOTNESS_COUNTER(-(pc - fistPC));
        sp = state->prev;
        ASSERT(sp != nullptr);
        FrameState *prevState = GET_FRAME(sp);
        pc = prevState->pc;

        // break frame
        if (pc == nullptr) {
            state->acc = acc;
            return;
        }
        thread->SetCurrentSPFrame(sp);

        constpool = prevState->constpool;

        size_t jumpSize = GetJumpSizeAfterCall(pc);
        DISPATCH_OFFSET(jumpSize);
    }
    HANDLE_OPCODE(HANDLE_RETURNUNDEFINED_PREF) {
        LOG_INST() << "return.undefined";
        FrameState *state = GET_FRAME(sp);
        LOG(DEBUG, INTERPRETER) << "Exit: Runtime Call " << std::hex << reinterpret_cast<uintptr_t>(sp) << " "
                                << std::hex << reinterpret_cast<uintptr_t>(state->pc);
        [[maybe_unused]] auto fistPC = state->method->GetInstructions();
        UPDATE_HOTNESS_COUNTER_NON_ACC(-(pc - fistPC));
        sp = state->prev;
        ASSERT(sp != nullptr);
        FrameState *prevState = GET_FRAME(sp);
        pc = prevState->pc;

        // break frame
        if (pc == nullptr) {
            state->acc = JSTaggedValue::Undefined();
            return;
        }
        thread->SetCurrentSPFrame(sp);

        constpool = prevState->constpool;

        acc = JSTaggedValue::Undefined();
        size_t jumpSize = GetJumpSizeAfterCall(pc);
        DISPATCH_OFFSET(jumpSize);
    }
    HANDLE_OPCODE(HANDLE_LDNAN_PREF) {
        LOG_INST() << "intrinsics::ldnan";
        SET_ACC(JSTaggedValue(base::NAN_VALUE));
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_LDINFINITY_PREF) {
        LOG_INST() << "intrinsics::ldinfinity";
        SET_ACC(JSTaggedValue(base::POSITIVE_INFINITY));
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_LDGLOBALTHIS_PREF) {
        LOG_INST() << "intrinsics::ldglobalthis";
        SET_ACC(globalObj)
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_LDUNDEFINED_PREF) {
        LOG_INST() << "intrinsics::ldundefined";
        SET_ACC(JSTaggedValue::Undefined())
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_LDNULL_PREF) {
        LOG_INST() << "intrinsics::ldnull";
        SET_ACC(JSTaggedValue::Null())
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_LDSYMBOL_PREF) {
        LOG_INST() << "intrinsics::ldsymbol";
        SET_ACC(globalEnv->GetSymbolFunction().GetTaggedValue());
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_LDGLOBAL_PREF) {
        LOG_INST() << "intrinsics::ldglobal";
        SET_ACC(globalObj)
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_LDTRUE_PREF) {
        LOG_INST() << "intrinsics::ldtrue";
        SET_ACC(JSTaggedValue::True())
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_LDFALSE_PREF) {
        LOG_INST() << "intrinsics::ldfalse";
        SET_ACC(JSTaggedValue::False())
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_LDLEXENVDYN_PREF) {
        LOG_INST() << "intrinsics::ldlexenvDyn ";
        FrameState *state = GET_FRAME(sp);
        JSTaggedValue currentLexenv = state->env;
        SET_ACC(currentLexenv);
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_GETUNMAPPEDARGS_PREF) {
        LOG_INST() << "intrinsics::getunmappedargs";

        FrameState *state = GET_FRAME(sp);
        uint32_t numVregs = state->method->GetNumVregs();
        // Exclude func, newTarget and "this"
        uint32_t actualNumArgs = state->numActualArgs - NUM_MANDATORY_JSFUNC_ARGS;
        uint32_t startIdx = numVregs + NUM_MANDATORY_JSFUNC_ARGS;

        JSTaggedValue res = SlowRuntimeStub::GetUnmapedArgs(thread, sp, actualNumArgs, startIdx);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_ASYNCFUNCTIONENTER_PREF) {
        LOG_INST() << "intrinsics::asyncfunctionenter";
        JSTaggedValue res = SlowRuntimeStub::AsyncFunctionEnter(thread);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_TONUMBER_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();

        LOG_INST() << "intrinsics::tonumber"
                   << " v" << v0;
        JSTaggedValue value = GET_VREG_VALUE(v0);
        if (value.IsNumber()) {
            // fast path
            SET_ACC(value);
        } else {
            // slow path
            JSTaggedValue res = SlowRuntimeStub::ToNumber(thread, value);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            SET_ACC(res);
        }
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_NEGDYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();
        LOG_INST() << "intrinsics::negdyn"
                   << " v" << v0;
        JSTaggedValue value = GET_VREG_VALUE(v0);
        // fast path
        if (value.IsInt()) {
            if (value.GetInt() == 0) {
                SET_ACC(JSTaggedValue(-0.0));
            } else {
                SET_ACC(JSTaggedValue(-value.GetInt()));
            }
        } else if (value.IsDouble()) {
            SET_ACC(JSTaggedValue(-value.GetDouble()));
        } else {  // slow path
            JSTaggedValue res = SlowRuntimeStub::NegDyn(thread, value);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            SET_ACC(res);
        }
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_NOTDYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();

        LOG_INST() << "intrinsics::notdyn"
                   << " v" << v0;
        JSTaggedValue value = GET_VREG_VALUE(v0);
        int32_t number;
        // number, fast path
        if (value.IsInt()) {
            number = static_cast<int32_t>(value.GetInt());
            SET_ACC(JSTaggedValue(~number));  // NOLINT(hicpp-signed-bitwise)
        } else if (value.IsDouble()) {
            number = base::NumberHelper::DoubleToInt(value.GetDouble(), base::INT32_BITS);
            SET_ACC(JSTaggedValue(~number));  // NOLINT(hicpp-signed-bitwise)
        } else {
            // slow path
            JSTaggedValue res = SlowRuntimeStub::NotDyn(thread, value);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            SET_ACC(res);
        }
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_INCDYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();

        LOG_INST() << "intrinsics::incdyn"
                   << " v" << v0;

        JSTaggedValue value = GET_VREG_VALUE(v0);
        // number fast path
        if (value.IsInt()) {
            int32_t a0 = value.GetInt();
            if (UNLIKELY(a0 == INT32_MAX)) {
                auto ret = static_cast<double>(a0) + 1.0;
                SET_ACC(JSTaggedValue(ret))
            } else {
                SET_ACC(JSTaggedValue(a0 + 1))
            }
        } else if (value.IsDouble()) {
            SET_ACC(JSTaggedValue(value.GetDouble() + 1.0))
        } else {
            // slow path
            JSTaggedValue res = SlowRuntimeStub::IncDyn(thread, value);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            SET_ACC(res);
        }
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_DECDYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();
        LOG_INST() << "intrinsics::decdyn"
                   << " v" << v0;

        JSTaggedValue value = GET_VREG_VALUE(v0);
        // number, fast path
        if (value.IsInt()) {
            int32_t a0 = value.GetInt();
            if (UNLIKELY(a0 == INT32_MIN)) {
                auto ret = static_cast<double>(a0) - 1.0;
                SET_ACC(JSTaggedValue(ret))
            } else {
                SET_ACC(JSTaggedValue(a0 - 1))
            }
        } else if (value.IsDouble()) {
            SET_ACC(JSTaggedValue(value.GetDouble() - 1.0))
        } else {
            // slow path
            JSTaggedValue res = SlowRuntimeStub::DecDyn(thread, value);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            SET_ACC(res);
        }
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_THROWDYN_PREF) {
        LOG_INST() << "intrinsics::throwdyn";
        SlowRuntimeStub::ThrowDyn(thread, GET_ACC());
        INTERPRETER_GOTO_EXCEPTION_HANDLER();
    }
    HANDLE_OPCODE(HANDLE_TYPEOFDYN_PREF) {
        LOG_INST() << "intrinsics::typeofdyn";
        JSTaggedValue res = FastRuntimeStub::FastTypeOf(thread, GET_ACC());
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_GETPROPITERATOR_PREF) {
        LOG_INST() << "intrinsics::getpropiterator";
        JSTaggedValue res = SlowRuntimeStub::GetPropIterator(thread, GET_ACC());
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_RESUMEGENERATOR_PREF_V8) {
        LOG_INST() << "intrinsics::resumegenerator";
        uint16_t vs = READ_INST_8_1();
        JSGeneratorObject *obj = JSGeneratorObject::Cast(GET_VREG_VALUE(vs).GetTaggedObject());
        SET_ACC(obj->GetResumeResult());
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_GETRESUMEMODE_PREF_V8) {
        LOG_INST() << "intrinsics::getresumemode";
        uint16_t vs = READ_INST_8_1();
        JSGeneratorObject *obj = JSGeneratorObject::Cast(GET_VREG_VALUE(vs).GetTaggedObject());
        SET_ACC(obj->GetResumeMode());
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_GETITERATOR_PREF) {
        LOG_INST() << "intrinsics::getiterator";
        JSTaggedValue obj = GET_ACC();

        // fast path: Generator obj is already store in acc
        if (!obj.IsGeneratorObject()) {
            // slow path
            JSTaggedValue res = SlowRuntimeStub::GetIterator(thread, obj);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            SET_ACC(res);
        }
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_THROWCONSTASSIGNMENT_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();
        LOG_INST() << "throwconstassignment"
                   << " v" << v0;
        SlowRuntimeStub::ThrowConstAssignment(thread, GET_VREG_VALUE(v0));
        INTERPRETER_GOTO_EXCEPTION_HANDLER();
    }
    HANDLE_OPCODE(HANDLE_THROWTHROWNOTEXISTS_PREF) {
        LOG_INST() << "throwthrownotexists";

        SlowRuntimeStub::ThrowThrowNotExists(thread);
        INTERPRETER_GOTO_EXCEPTION_HANDLER();
    }
    HANDLE_OPCODE(HANDLE_THROWPATTERNNONCOERCIBLE_PREF) {
        LOG_INST() << "throwpatternnoncoercible";

        SlowRuntimeStub::ThrowPatternNonCoercible(thread);
        INTERPRETER_GOTO_EXCEPTION_HANDLER();
    }
    HANDLE_OPCODE(HANDLE_THROWIFNOTOBJECT_PREF_V8) {
        LOG_INST() << "throwifnotobject";
        uint16_t v0 = READ_INST_8_1();

        JSTaggedValue value = GET_VREG_VALUE(v0);
        // fast path
        if (value.IsECMAObject()) {
            DISPATCH(BytecodeInstruction::Format::PREF_V8);
        }

        // slow path
        SlowRuntimeStub::ThrowIfNotObject(thread);
        INTERPRETER_GOTO_EXCEPTION_HANDLER();
    }
    HANDLE_OPCODE(HANDLE_ITERNEXT_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();
        LOG_INST() << "intrinsics::iternext"
                   << " v" << v0;
        JSTaggedValue iter = GET_VREG_VALUE(v0);
        JSTaggedValue res = SlowRuntimeStub::IterNext(thread, iter);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_CLOSEITERATOR_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();
        LOG_INST() << "intrinsics::closeiterator"
                   << " v" << v0;
        JSTaggedValue iter = GET_VREG_VALUE(v0);
        JSTaggedValue res = SlowRuntimeStub::CloseIterator(thread, iter);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_ADD2DYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();
        LOG_INST() << "intrinsics::add2dyn"
                   << " v" << v0;
        int32_t a0;
        int32_t a1;
        JSTaggedValue left = GET_VREG_VALUE(v0);
        JSTaggedValue right = GET_ACC();
        // number, fast path
        if (left.IsInt() && right.IsInt()) {
            a0 = left.GetInt();
            a1 = right.GetInt();
            if ((a0 > 0 && a1 > INT32_MAX - a0) || (a0 < 0 && a1 < INT32_MIN - a0)) {
                auto ret = static_cast<double>(a0) + static_cast<double>(a1);
                SET_ACC(JSTaggedValue(ret))
            } else {
                SET_ACC(JSTaggedValue(a0 + a1))
            }
        } else if (left.IsNumber() && right.IsNumber()) {
            double a0Double = left.IsInt() ? left.GetInt() : left.GetDouble();
            double a1Double = right.IsInt() ? right.GetInt() : right.GetDouble();
            double ret = a0Double + a1Double;
            SET_ACC(JSTaggedValue(ret))
        } else {
            // one or both are not number, slow path
            JSTaggedValue res = SlowRuntimeStub::Add2Dyn(thread, ecmaVm, left, right);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            SET_ACC(res);
        }
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_SUB2DYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();
        LOG_INST() << "intrinsics::sub2dyn"
                   << " v" << v0;
        int32_t a0;
        int32_t a1;
        JSTaggedValue left = GET_VREG_VALUE(v0);
        JSTaggedValue right = GET_ACC();
        if (left.IsInt() && right.IsInt()) {
            a0 = left.GetInt();
            a1 = -right.GetInt();
            if ((a0 > 0 && a1 > INT32_MAX - a0) || (a0 < 0 && a1 < INT32_MIN - a0)) {
                auto ret = static_cast<double>(a0) + static_cast<double>(a1);
                SET_ACC(JSTaggedValue(ret))
            } else {
                SET_ACC(JSTaggedValue(a0 + a1))
            }
        } else if (left.IsNumber() && right.IsNumber()) {
            double a0Double = left.IsInt() ? left.GetInt() : left.GetDouble();
            double a1Double = right.IsInt() ? right.GetInt() : right.GetDouble();
            double ret = a0Double - a1Double;
            SET_ACC(JSTaggedValue(ret))
        } else {
            // one or both are not number, slow path
            JSTaggedValue res = SlowRuntimeStub::Sub2Dyn(thread, left, right);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            SET_ACC(res);
        }
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_MUL2DYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();
        LOG_INST() << "intrinsics::mul2dyn"
                   << " v" << v0;
        JSTaggedValue left = GET_VREG_VALUE(v0);
        JSTaggedValue right = acc;
        JSTaggedValue value = FastRuntimeStub::FastMul(left, right);
        if (!value.IsHole()) {
            SET_ACC(value);
        } else {
            // slow path
            JSTaggedValue res = SlowRuntimeStub::Mul2Dyn(thread, left, right);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            SET_ACC(res);
        }
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_DIV2DYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();
        LOG_INST() << "intrinsics::div2dyn"
                   << " v" << v0;
        JSTaggedValue left = GET_VREG_VALUE(v0);
        JSTaggedValue right = acc;
        // fast path
        JSTaggedValue res = FastRuntimeStub::FastDiv(left, right);
        if (!res.IsHole()) {
            SET_ACC(res);
        } else {
            // slow path
            JSTaggedValue slowRes = SlowRuntimeStub::Div2Dyn(thread, left, right);
            INTERPRETER_RETURN_IF_ABRUPT(slowRes);
            SET_ACC(slowRes);
        }
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_MOD2DYN_PREF_V8) {
        uint16_t vs = READ_INST_8_1();
        LOG_INST() << "intrinsics::mod2dyn"
                   << " v" << vs;
        JSTaggedValue left = GET_VREG_VALUE(vs);
        JSTaggedValue right = GET_ACC();
        // fast path
        JSTaggedValue res = FastRuntimeStub::FastMod(left, right);
        if (!res.IsHole()) {
            SET_ACC(res);
        } else {
            // slow path
            JSTaggedValue slowRes = SlowRuntimeStub::Mod2Dyn(thread, left, right);
            INTERPRETER_RETURN_IF_ABRUPT(slowRes);
            SET_ACC(slowRes);
        }
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_EQDYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();

        LOG_INST() << "intrinsics::eqdyn"
                   << " v" << v0;
        JSTaggedValue left = GET_VREG_VALUE(v0);
        JSTaggedValue right = acc;
        JSTaggedValue res = FastRuntimeStub::FastEqual(left, right);
        if (!res.IsHole()) {
            SET_ACC(res);
        } else {
            // slow path
            res = SlowRuntimeStub::EqDyn(thread, left, right);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            SET_ACC(res);
        }

        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_NOTEQDYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();

        LOG_INST() << "intrinsics::noteqdyn"
                   << " v" << v0;
        JSTaggedValue left = GET_VREG_VALUE(v0);
        JSTaggedValue right = acc;

        JSTaggedValue res = FastRuntimeStub::FastEqual(left, right);
        if (!res.IsHole()) {
            res = res.IsTrue() ? JSTaggedValue::False() : JSTaggedValue::True();
            SET_ACC(res);
        } else {
            // slow path
            res = SlowRuntimeStub::NotEqDyn(thread, left, right);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            SET_ACC(res);
        }
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_LESSDYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();

        LOG_INST() << "intrinsics::lessdyn"
                   << " v" << v0;
        JSTaggedValue left = GET_VREG_VALUE(v0);
        JSTaggedValue right = GET_ACC();
        if (left.IsNumber() && right.IsNumber()) {
            // fast path
            double valueA = left.IsInt() ? static_cast<double>(left.GetInt()) : left.GetDouble();
            double valueB = right.IsInt() ? static_cast<double>(right.GetInt()) : right.GetDouble();
            bool ret = JSTaggedValue::StrictNumberCompare(valueA, valueB) == ComparisonResult::LESS;
            SET_ACC(ret ? JSTaggedValue::True() : JSTaggedValue::False())
        } else {
            // slow path
            JSTaggedValue res = SlowRuntimeStub::LessDyn(thread, left, right);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            SET_ACC(res);
        }
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_LESSEQDYN_PREF_V8) {
        uint16_t vs = READ_INST_8_1();
        LOG_INST() << "intrinsics::lesseqdyn "
                   << " v" << vs;
        JSTaggedValue left = GET_VREG_VALUE(vs);
        JSTaggedValue right = GET_ACC();
        if (left.IsNumber() && right.IsNumber()) {
            // fast path
            double valueA = left.IsInt() ? static_cast<double>(left.GetInt()) : left.GetDouble();
            double valueB = right.IsInt() ? static_cast<double>(right.GetInt()) : right.GetDouble();
            bool ret = JSTaggedValue::StrictNumberCompare(valueA, valueB) <= ComparisonResult::EQUAL;
            SET_ACC(ret ? JSTaggedValue::True() : JSTaggedValue::False())
        } else {
            // slow path
            JSTaggedValue res = SlowRuntimeStub::LessEqDyn(thread, left, right);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            SET_ACC(res);
        }
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_GREATERDYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();

        LOG_INST() << "intrinsics::greaterdyn"
                   << " v" << v0;
        JSTaggedValue left = GET_VREG_VALUE(v0);
        JSTaggedValue right = acc;
        if (left.IsNumber() && right.IsNumber()) {
            // fast path
            double valueA = left.IsInt() ? static_cast<double>(left.GetInt()) : left.GetDouble();
            double valueB = right.IsInt() ? static_cast<double>(right.GetInt()) : right.GetDouble();
            bool ret = JSTaggedValue::StrictNumberCompare(valueA, valueB) == ComparisonResult::GREAT;
            SET_ACC(ret ? JSTaggedValue::True() : JSTaggedValue::False())
        } else {
            // slow path
            JSTaggedValue res = SlowRuntimeStub::GreaterDyn(thread, left, right);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            SET_ACC(res);
        }
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_GREATEREQDYN_PREF_V8) {
        uint16_t vs = READ_INST_8_1();
        LOG_INST() << "intrinsics::greateqdyn "
                   << " v" << vs;
        JSTaggedValue left = GET_VREG_VALUE(vs);
        JSTaggedValue right = GET_ACC();
        if (left.IsNumber() && right.IsNumber()) {
            // fast path
            double valueA = left.IsInt() ? static_cast<double>(left.GetInt()) : left.GetDouble();
            double valueB = right.IsInt() ? static_cast<double>(right.GetInt()) : right.GetDouble();
            ComparisonResult comparison = JSTaggedValue::StrictNumberCompare(valueA, valueB);
            bool ret = (comparison == ComparisonResult::GREAT) || (comparison == ComparisonResult::EQUAL);
            SET_ACC(ret ? JSTaggedValue::True() : JSTaggedValue::False())
        } else {
            // slow path
            JSTaggedValue res = SlowRuntimeStub::GreaterEqDyn(thread, left, right);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            SET_ACC(res);
        }
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_SHL2DYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();

        LOG_INST() << "intrinsics::shl2dyn"
                   << " v" << v0;
        int32_t opNumber0;
        int32_t opNumber1;
        JSTaggedValue left = GET_VREG_VALUE(v0);
        JSTaggedValue right = GET_ACC();
        // both number, fast path
        if (left.IsInt() && right.IsInt()) {
            opNumber0 = left.GetInt();
            opNumber1 = right.GetInt();
        } else if (left.IsNumber() && right.IsNumber()) {
            opNumber0 =
                left.IsInt() ? left.GetInt() : base::NumberHelper::DoubleToInt(left.GetDouble(), base::INT32_BITS);
            opNumber1 =
                right.IsInt() ? right.GetInt() : base::NumberHelper::DoubleToInt(right.GetDouble(), base::INT32_BITS);
        } else {
            // slow path
            SAVE_ACC();
            JSTaggedValue taggedNumber0 = SlowRuntimeStub::ToJSTaggedValueWithInt32(thread, left);
            INTERPRETER_RETURN_IF_ABRUPT(taggedNumber0);
            RESTORE_ACC();
            right = GET_ACC();  // Maybe moved by GC
            JSTaggedValue taggedNumber1 = SlowRuntimeStub::ToJSTaggedValueWithUint32(thread, right);
            INTERPRETER_RETURN_IF_ABRUPT(taggedNumber1);
            opNumber0 = taggedNumber0.GetInt();
            opNumber1 = taggedNumber1.GetInt();
        }

        uint32_t shift =
            static_cast<uint32_t>(opNumber1) & 0x1f;  // NOLINT(hicpp-signed-bitwise, readability-magic-numbers)
        using unsigned_type = std::make_unsigned_t<int32_t>;
        auto ret =
            static_cast<int32_t>(static_cast<unsigned_type>(opNumber0) << shift);  // NOLINT(hicpp-signed-bitwise)
        SET_ACC(JSTaggedValue(ret))
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_SHR2DYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();

        LOG_INST() << "intrinsics::shr2dyn"
                   << " v" << v0;
        int32_t opNumber0;
        int32_t opNumber1;
        JSTaggedValue left = GET_VREG_VALUE(v0);
        JSTaggedValue right = GET_ACC();
        // both number, fast path
        if (left.IsInt() && right.IsInt()) {
            opNumber0 = left.GetInt();
            opNumber1 = right.GetInt();
        } else if (left.IsNumber() && right.IsNumber()) {
            opNumber0 =
                left.IsInt() ? left.GetInt() : base::NumberHelper::DoubleToInt(left.GetDouble(), base::INT32_BITS);
            opNumber1 =
                right.IsInt() ? right.GetInt() : base::NumberHelper::DoubleToInt(right.GetDouble(), base::INT32_BITS);
        } else {
            // slow path
            SAVE_ACC();
            JSTaggedValue taggedNumber0 = SlowRuntimeStub::ToJSTaggedValueWithInt32(thread, left);
            INTERPRETER_RETURN_IF_ABRUPT(taggedNumber0);
            RESTORE_ACC();
            right = GET_ACC();  // Maybe moved by GC
            JSTaggedValue taggedNumber1 = SlowRuntimeStub::ToJSTaggedValueWithUint32(thread, right);
            INTERPRETER_RETURN_IF_ABRUPT(taggedNumber1);
            opNumber0 = taggedNumber0.GetInt();
            opNumber1 = taggedNumber1.GetInt();
        }

        uint32_t shift =
            static_cast<uint32_t>(opNumber1) & 0x1f;          // NOLINT(hicpp-signed-bitwise, readability-magic-numbers)
        auto ret = static_cast<int32_t>(opNumber0 >> shift);  // NOLINT(hicpp-signed-bitwise)
        SET_ACC(JSTaggedValue(ret))
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_ASHR2DYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();

        LOG_INST() << "intrinsics::ashr2dyn"
                   << " v" << v0;
        int32_t opNumber0;
        int32_t opNumber1;
        JSTaggedValue left = GET_VREG_VALUE(v0);
        JSTaggedValue right = GET_ACC();
        if (left.IsInt() && right.IsInt()) {
            opNumber0 = left.GetInt();
            opNumber1 = right.GetInt();
        } else if (left.IsNumber() && right.IsNumber()) {
            opNumber0 =
                left.IsInt() ? left.GetInt() : base::NumberHelper::DoubleToInt(left.GetDouble(), base::INT32_BITS);
            opNumber1 =
                right.IsInt() ? right.GetInt() : base::NumberHelper::DoubleToInt(right.GetDouble(), base::INT32_BITS);
        } else {
            // slow path
            SAVE_ACC();
            JSTaggedValue taggedNumber0 = SlowRuntimeStub::ToJSTaggedValueWithUint32(thread, left);
            INTERPRETER_RETURN_IF_ABRUPT(taggedNumber0);
            RESTORE_ACC();
            right = GET_ACC();  // Maybe moved by GC
            JSTaggedValue taggedNumber1 = SlowRuntimeStub::ToJSTaggedValueWithUint32(thread, right);
            INTERPRETER_RETURN_IF_ABRUPT(taggedNumber1);
            opNumber0 = taggedNumber0.GetInt();
            opNumber1 = taggedNumber1.GetInt();
        }

        uint32_t shift =
            static_cast<uint32_t>(opNumber1) & 0x1f;  // NOLINT(hicpp-signed-bitwise, readability-magic-numbers)
        using unsigned_type = std::make_unsigned_t<uint32_t>;
        auto ret =
            static_cast<uint32_t>(static_cast<unsigned_type>(opNumber0) >> shift);  // NOLINT(hicpp-signed-bitwise)
        SET_ACC(JSTaggedValue(ret))

        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_AND2DYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();

        LOG_INST() << "intrinsics::and2dyn"
                   << " v" << v0;
        int32_t opNumber0;
        int32_t opNumber1;
        JSTaggedValue left = GET_VREG_VALUE(v0);
        JSTaggedValue right = GET_ACC();
        // both number, fast path
        if (left.IsInt() && right.IsInt()) {
            opNumber0 = left.GetInt();
            opNumber1 = right.GetInt();
        } else if (left.IsNumber() && right.IsNumber()) {
            opNumber0 =
                left.IsInt() ? left.GetInt() : base::NumberHelper::DoubleToInt(left.GetDouble(), base::INT32_BITS);
            opNumber1 =
                right.IsInt() ? right.GetInt() : base::NumberHelper::DoubleToInt(right.GetDouble(), base::INT32_BITS);
        } else {
            // slow path
            SAVE_ACC();
            JSTaggedValue taggedNumber0 = SlowRuntimeStub::ToJSTaggedValueWithInt32(thread, left);
            INTERPRETER_RETURN_IF_ABRUPT(taggedNumber0);
            RESTORE_ACC();
            right = GET_ACC();  // Maybe moved by GC
            JSTaggedValue taggedNumber1 = SlowRuntimeStub::ToJSTaggedValueWithInt32(thread, right);
            INTERPRETER_RETURN_IF_ABRUPT(taggedNumber1);
            opNumber0 = taggedNumber0.GetInt();
            opNumber1 = taggedNumber1.GetInt();
        }
        // NOLINT(hicpp-signed-bitwise)
        auto ret = static_cast<uint32_t>(opNumber0) & static_cast<uint32_t>(opNumber1);
        SET_ACC(JSTaggedValue(static_cast<uint32_t>(ret)))
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_OR2DYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();

        LOG_INST() << "intrinsics::or2dyn"
                   << " v" << v0;
        int32_t opNumber0;
        int32_t opNumber1;
        JSTaggedValue left = GET_VREG_VALUE(v0);
        JSTaggedValue right = GET_ACC();
        // both number, fast path
        if (left.IsInt() && right.IsInt()) {
            opNumber0 = left.GetInt();
            opNumber1 = right.GetInt();
        } else if (left.IsNumber() && right.IsNumber()) {
            opNumber0 =
                left.IsInt() ? left.GetInt() : base::NumberHelper::DoubleToInt(left.GetDouble(), base::INT32_BITS);
            opNumber1 =
                right.IsInt() ? right.GetInt() : base::NumberHelper::DoubleToInt(right.GetDouble(), base::INT32_BITS);
        } else {
            // slow path
            SAVE_ACC();
            JSTaggedValue taggedNumber0 = SlowRuntimeStub::ToJSTaggedValueWithInt32(thread, left);
            INTERPRETER_RETURN_IF_ABRUPT(taggedNumber0);
            RESTORE_ACC();
            right = GET_ACC();  // Maybe moved by GC
            JSTaggedValue taggedNumber1 = SlowRuntimeStub::ToJSTaggedValueWithInt32(thread, right);
            INTERPRETER_RETURN_IF_ABRUPT(taggedNumber1);
            opNumber0 = taggedNumber0.GetInt();
            opNumber1 = taggedNumber1.GetInt();
        }
        // NOLINT(hicpp-signed-bitwise)
        auto ret = static_cast<uint32_t>(opNumber0) | static_cast<uint32_t>(opNumber1);
        SET_ACC(JSTaggedValue(static_cast<uint32_t>(ret)))
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_XOR2DYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();

        LOG_INST() << "intrinsics::xor2dyn"
                   << " v" << v0;
        int32_t opNumber0;
        int32_t opNumber1;
        JSTaggedValue left = GET_VREG_VALUE(v0);
        JSTaggedValue right = GET_ACC();
        // both number, fast path
        if (left.IsInt() && right.IsInt()) {
            opNumber0 = left.GetInt();
            opNumber1 = right.GetInt();
        } else if (left.IsNumber() && right.IsNumber()) {
            opNumber0 =
                left.IsInt() ? left.GetInt() : base::NumberHelper::DoubleToInt(left.GetDouble(), base::INT32_BITS);
            opNumber1 =
                right.IsInt() ? right.GetInt() : base::NumberHelper::DoubleToInt(right.GetDouble(), base::INT32_BITS);
        } else {
            // slow path
            SAVE_ACC();
            JSTaggedValue taggedNumber0 = SlowRuntimeStub::ToJSTaggedValueWithInt32(thread, left);
            INTERPRETER_RETURN_IF_ABRUPT(taggedNumber0);
            RESTORE_ACC();
            right = GET_ACC();  // Maybe moved by GC
            JSTaggedValue taggedNumber1 = SlowRuntimeStub::ToJSTaggedValueWithInt32(thread, right);
            INTERPRETER_RETURN_IF_ABRUPT(taggedNumber1);
            opNumber0 = taggedNumber0.GetInt();
            opNumber1 = taggedNumber1.GetInt();
        }
        // NOLINT(hicpp-signed-bitwise)
        auto ret = static_cast<uint32_t>(opNumber0) ^ static_cast<uint32_t>(opNumber1);
        SET_ACC(JSTaggedValue(static_cast<uint32_t>(ret)))
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_DELOBJPROP_PREF_V8_V8) {
        uint16_t v0 = READ_INST_8_1();
        uint16_t v1 = READ_INST_8_2();
        LOG_INST() << "intrinsics::delobjprop"
                   << " v0" << v0 << " v1" << v1;

        JSTaggedValue obj = GET_VREG_VALUE(v0);
        JSTaggedValue prop = GET_VREG_VALUE(v1);
        JSTaggedValue res = SlowRuntimeStub::DelObjProp(thread, obj, prop);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);

        DISPATCH(BytecodeInstruction::Format::PREF_V8_V8);
    }
    HANDLE_OPCODE(HANDLE_DEFINEFUNCDYN_PREF_ID16_IMM16_V8) {
        uint16_t methodId = READ_INST_16_1();
        uint16_t length = READ_INST_16_3();
        uint16_t v0 = READ_INST_8_5();
        LOG_INST() << "intrinsics::definefuncDyn length: " << length
                   << " v" << v0;
        JSFunction *result = JSFunction::Cast(constpool->GetObjectFromCache(methodId).GetTaggedObject());
        ASSERT(result != nullptr);
        if (result->IsResolved()) {
            auto res = SlowRuntimeStub::DefinefuncDyn(thread, result);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            result = JSFunction::Cast(res.GetTaggedObject());
            result->SetConstantPool(thread, JSTaggedValue(constpool));
        } else {
            result->SetResolved(thread);
        }

        result->SetPropertyInlinedProps(thread, JSFunction::LENGTH_INLINE_PROPERTY_INDEX, JSTaggedValue(length));
        JSTaggedValue envHandle = GET_VREG_VALUE(v0);
        result->SetLexicalEnv(thread, envHandle);
        SET_ACC(JSTaggedValue(result))

        DISPATCH(BytecodeInstruction::Format::PREF_ID16_IMM16_V8);
    }
    HANDLE_OPCODE(HANDLE_DEFINENCFUNCDYN_PREF_ID16_IMM16_V8) {
        uint16_t methodId = READ_INST_16_1();
        uint16_t length = READ_INST_16_3();
        uint16_t v0 = READ_INST_8_5();
        JSTaggedValue homeObject = GET_ACC();
        LOG_INST() << "intrinsics::definencfuncDyn length: " << length
                   << " v" << v0;
        JSFunction *result = JSFunction::Cast(constpool->GetObjectFromCache(methodId).GetTaggedObject());
        ASSERT(result != nullptr);
        if (result->IsResolved()) {
            auto res = SlowRuntimeStub::DefineNCFuncDyn(thread, result);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            result = JSFunction::Cast(res.GetTaggedObject());
            result->SetConstantPool(thread, JSTaggedValue(constpool));
        } else {
            result->SetResolved(thread);
        }

        result->SetPropertyInlinedProps(thread, JSFunction::LENGTH_INLINE_PROPERTY_INDEX, JSTaggedValue(length));
        JSTaggedValue env = GET_VREG_VALUE(v0);
        result->SetLexicalEnv(thread, env);
        result->SetHomeObject(thread, homeObject);
        SET_ACC(JSTaggedValue(result));

        DISPATCH(BytecodeInstruction::Format::PREF_ID16_IMM16_V8);
    }
    HANDLE_OPCODE(HANDLE_DEFINEMETHOD_PREF_ID16_IMM16_V8) {
        uint16_t methodId = READ_INST_16_1();
        uint16_t length = READ_INST_16_3();
        uint16_t v0 = READ_INST_8_5();
        JSTaggedValue homeObject = GET_ACC();
        LOG_INST() << "intrinsics::definemethod length: " << length
                   << " v" << v0;
        JSFunction *result = JSFunction::Cast(constpool->GetObjectFromCache(methodId).GetTaggedObject());
        ASSERT(result != nullptr);
        if (result->IsResolved()) {
            auto res = SlowRuntimeStub::DefineMethod(thread, result, homeObject);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            result = JSFunction::Cast(res.GetTaggedObject());
            result->SetConstantPool(thread, JSTaggedValue(constpool));
        } else {
            result->SetHomeObject(thread, homeObject);
            result->SetResolved(thread);
        }

        result->SetPropertyInlinedProps(thread, JSFunction::LENGTH_INLINE_PROPERTY_INDEX, JSTaggedValue(length));
        JSTaggedValue taggedCurEnv = GET_VREG_VALUE(v0);
        result->SetLexicalEnv(thread, taggedCurEnv);
        SET_ACC(JSTaggedValue(result));

        DISPATCH(BytecodeInstruction::Format::PREF_ID16_IMM16_V8);
    }
    HANDLE_OPCODE(HANDLE_NEWOBJDYNRANGE_PREF_IMM16_V8) {
        uint16_t numArgs = READ_INST_16_1();
        uint16_t firstArgRegIdx = READ_INST_8_3();
        LOG_INST() << "intrinsics::newobjDynrange " << numArgs << " v" << firstArgRegIdx;

        constexpr uint16_t firstArgOffset = 2;
        JSTaggedValue func = GET_VREG_VALUE(firstArgRegIdx);
        JSTaggedValue newTarget = GET_VREG_VALUE(firstArgRegIdx + 1);
        // Exclude func and newTarget
        uint16_t firstArgIdx = firstArgRegIdx + firstArgOffset;
        uint16_t length = numArgs - firstArgOffset;

        JSTaggedValue res = SlowRuntimeStub::NewObjDynRange(thread, func, newTarget, firstArgIdx, length);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_IMM16_V8);
    }
    HANDLE_OPCODE(HANDLE_EXPDYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();
        LOG_INST() << "intrinsics::expdyn"
                   << " v" << v0;
        JSTaggedValue base = GET_VREG_VALUE(v0);
        JSTaggedValue exponent = GET_ACC();
        if (base.IsNumber() && exponent.IsNumber()) {
            // fast path
            double doubleBase = base.IsInt() ? base.GetInt() : base.GetDouble();
            double doubleExponent = exponent.IsInt() ? exponent.GetInt() : exponent.GetDouble();
            if (std::abs(doubleBase) == 1 && std::isinf(doubleExponent)) {
                SET_ACC(JSTaggedValue(base::NAN_VALUE));
            }
            if ((doubleBase == 0 &&
                ((bit_cast<uint64_t>(doubleBase)) & base::DOUBLE_SIGN_MASK) == base::DOUBLE_SIGN_MASK) &&
                std::isfinite(doubleExponent) && base::NumberHelper::TruncateDouble(doubleExponent) == doubleExponent &&
                base::NumberHelper::TruncateDouble(doubleExponent / 2) + base::HALF ==  // 2 : half
                (doubleExponent / 2)) {  // 2 : half
                if (doubleExponent > 0) {
                    SET_ACC(JSTaggedValue(-0.0));
                }
                if (doubleExponent < 0) {
                    SET_ACC(JSTaggedValue(-base::POSITIVE_INFINITY));
                }
            }
            SET_ACC(JSTaggedValue(std::pow(doubleBase, doubleExponent)));
        } else {
            // slow path
            JSTaggedValue res = SlowRuntimeStub::ExpDyn(thread, base, exponent);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            SET_ACC(res);
        }
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_ISINDYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();
        LOG_INST() << "intrinsics::isindyn"
                   << " v" << v0;
        JSTaggedValue prop = GET_VREG_VALUE(v0);
        JSTaggedValue obj = GET_ACC();
        JSTaggedValue res = SlowRuntimeStub::IsInDyn(thread, prop, obj);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_INSTANCEOFDYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();
        LOG_INST() << "intrinsics::instanceofdyn"
                   << " v" << v0;
        JSTaggedValue obj = GET_VREG_VALUE(v0);
        JSTaggedValue target = GET_ACC();
        JSTaggedValue res = SlowRuntimeStub::InstanceofDyn(thread, obj, target);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_STRICTNOTEQDYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();
        LOG_INST() << "intrinsics::strictnoteq"
                   << " v" << v0;
        JSTaggedValue left = GET_VREG_VALUE(v0);
        JSTaggedValue right = GET_ACC();
        bool res = FastRuntimeStub::FastStrictEqual(left, right);
        SET_ACC(JSTaggedValue(!res));
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_STRICTEQDYN_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();
        LOG_INST() << "intrinsics::stricteq"
                   << " v" << v0;
        JSTaggedValue left = GET_VREG_VALUE(v0);
        JSTaggedValue right = GET_ACC();
        bool res = FastRuntimeStub::FastStrictEqual(left, right);
        SET_ACC(JSTaggedValue(res));
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_LDLEXVARDYN_PREF_IMM16_IMM16) {
        uint16_t level = READ_INST_16_1();
        uint16_t slot = READ_INST_16_3();

        LOG_INST() << "intrinsics::ldlexvardyn"
                   << " level:" << level << " slot:" << slot;
        FrameState *state = GET_FRAME(sp);
        JSTaggedValue currentLexenv = state->env;
        JSTaggedValue env(currentLexenv);
        for (int i = 0; i < level; i++) {
            JSTaggedValue taggedParentEnv = LexicalEnv::Cast(env.GetTaggedObject())->GetParentEnv();
            ASSERT(!taggedParentEnv.IsUndefined());
            env = taggedParentEnv;
        }
        SET_ACC(LexicalEnv::Cast(env.GetTaggedObject())->GetProperties(slot));
        DISPATCH(BytecodeInstruction::Format::PREF_IMM16_IMM16);
    }
    HANDLE_OPCODE(HANDLE_LDLEXVARDYN_PREF_IMM8_IMM8) {
        uint16_t level = READ_INST_8_1();
        uint16_t slot = READ_INST_8_2();

        LOG_INST() << "intrinsics::ldlexvardyn"
                   << " level:" << level << " slot:" << slot;
        FrameState *state = GET_FRAME(sp);
        JSTaggedValue currentLexenv = state->env;
        JSTaggedValue env(currentLexenv);
        for (int i = 0; i < level; i++) {
            JSTaggedValue taggedParentEnv = LexicalEnv::Cast(env.GetTaggedObject())->GetParentEnv();
            ASSERT(!taggedParentEnv.IsUndefined());
            env = taggedParentEnv;
        }
        SET_ACC(LexicalEnv::Cast(env.GetTaggedObject())->GetProperties(slot));
        DISPATCH(BytecodeInstruction::Format::PREF_IMM8_IMM8);
    }
    HANDLE_OPCODE(HANDLE_LDLEXVARDYN_PREF_IMM4_IMM4) {
        uint16_t level = READ_INST_4_2();
        uint16_t slot = READ_INST_4_3();

        LOG_INST() << "intrinsics::ldlexvardyn"
                   << " level:" << level << " slot:" << slot;
        FrameState *state = GET_FRAME(sp);
        JSTaggedValue currentLexenv = state->env;
        JSTaggedValue env(currentLexenv);
        for (int i = 0; i < level; i++) {
            JSTaggedValue taggedParentEnv = LexicalEnv::Cast(env.GetTaggedObject())->GetParentEnv();
            ASSERT(!taggedParentEnv.IsUndefined());
            env = taggedParentEnv;
        }
        SET_ACC(LexicalEnv::Cast(env.GetTaggedObject())->GetProperties(slot));
        DISPATCH(BytecodeInstruction::Format::PREF_IMM4_IMM4);
    }
    HANDLE_OPCODE(HANDLE_STLEXVARDYN_PREF_IMM16_IMM16_V8) {
        uint16_t level = READ_INST_16_1();
        uint16_t slot = READ_INST_16_3();
        uint16_t v0 = READ_INST_8_5();
        LOG_INST() << "intrinsics::stlexvardyn"
                   << " level:" << level << " slot:" << slot << " v" << v0;

        JSTaggedValue value = GET_VREG_VALUE(v0);
        FrameState *state = GET_FRAME(sp);
        JSTaggedValue env = state->env;
        for (int i = 0; i < level; i++) {
            JSTaggedValue taggedParentEnv = LexicalEnv::Cast(env.GetTaggedObject())->GetParentEnv();
            ASSERT(!taggedParentEnv.IsUndefined());
            env = taggedParentEnv;
        }
        LexicalEnv::Cast(env.GetTaggedObject())->SetProperties(thread, slot, value);

        DISPATCH(BytecodeInstruction::Format::PREF_IMM16_IMM16_V8);
    }
    HANDLE_OPCODE(HANDLE_STLEXVARDYN_PREF_IMM8_IMM8_V8) {
        uint16_t level = READ_INST_8_1();
        uint16_t slot = READ_INST_8_2();
        uint16_t v0 = READ_INST_8_3();
        LOG_INST() << "intrinsics::stlexvardyn"
                   << " level:" << level << " slot:" << slot << " v" << v0;

        JSTaggedValue value = GET_VREG_VALUE(v0);
        FrameState *state = GET_FRAME(sp);
        JSTaggedValue env = state->env;
        for (int i = 0; i < level; i++) {
            JSTaggedValue taggedParentEnv = LexicalEnv::Cast(env.GetTaggedObject())->GetParentEnv();
            ASSERT(!taggedParentEnv.IsUndefined());
            env = taggedParentEnv;
        }
        LexicalEnv::Cast(env.GetTaggedObject())->SetProperties(thread, slot, value);

        DISPATCH(BytecodeInstruction::Format::PREF_IMM8_IMM8_V8);
    }
    HANDLE_OPCODE(HANDLE_STLEXVARDYN_PREF_IMM4_IMM4_V8) {
        uint16_t level = READ_INST_4_2();
        uint16_t slot = READ_INST_4_3();
        uint16_t v0 = READ_INST_8_2();
        LOG_INST() << "intrinsics::stlexvardyn"
                   << " level:" << level << " slot:" << slot << " v" << v0;

        JSTaggedValue value = GET_VREG_VALUE(v0);
        FrameState *state = GET_FRAME(sp);
        JSTaggedValue env = state->env;
        for (int i = 0; i < level; i++) {
            JSTaggedValue taggedParentEnv = LexicalEnv::Cast(env.GetTaggedObject())->GetParentEnv();
            ASSERT(!taggedParentEnv.IsUndefined());
            env = taggedParentEnv;
        }
        LexicalEnv::Cast(env.GetTaggedObject())->SetProperties(thread, slot, value);

        DISPATCH(BytecodeInstruction::Format::PREF_IMM4_IMM4_V8);
    }
    HANDLE_OPCODE(HANDLE_NEWLEXENVDYN_PREF_IMM16) {
        uint16_t numVars = READ_INST_16_1();
        LOG_INST() << "intrinsics::newlexenvdyn"
                   << " imm " << numVars;

        JSTaggedValue res = FastRuntimeStub::NewLexicalEnvDyn(thread, factory, numVars);
        if (res.IsHole()) {
            res = SlowRuntimeStub::NewLexicalEnvDyn(thread, numVars);
            INTERPRETER_RETURN_IF_ABRUPT(res);
        }
        SET_ACC(res);
        GET_FRAME(sp)->env = res;
        DISPATCH(BytecodeInstruction::Format::PREF_IMM16);
    }
    HANDLE_OPCODE(HANDLE_POPLEXENVDYN_PREF) {
        FrameState *state = GET_FRAME(sp);
        JSTaggedValue currentLexenv = state->env;
        JSTaggedValue parentLexenv = LexicalEnv::Cast(currentLexenv.GetTaggedObject())->GetParentEnv();
        GET_FRAME(sp)->env = parentLexenv;
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_CREATEITERRESULTOBJ_PREF_V8_V8) {
        uint16_t v0 = READ_INST_8_1();
        uint16_t v1 = READ_INST_8_2();
        LOG_INST() << "intrinsics::createiterresultobj"
                   << " v" << v0 << " v" << v1;
        JSTaggedValue value = GET_VREG_VALUE(v0);
        JSTaggedValue flag = GET_VREG_VALUE(v1);
        JSTaggedValue res = SlowRuntimeStub::CreateIterResultObj(thread, value, flag);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_V8_V8);
    }
    HANDLE_OPCODE(HANDLE_SUSPENDGENERATOR_PREF_V8_V8) {
        uint16_t v0 = READ_INST_8_1();
        uint16_t v1 = READ_INST_8_2();
        LOG_INST() << "intrinsics::suspendgenerator"
                   << " v" << v0 << " v" << v1;
        JSTaggedValue genObj = GET_VREG_VALUE(v0);
        JSTaggedValue value = GET_VREG_VALUE(v1);
        // suspend will record bytecode offset
        SAVE_PC();
        SAVE_ACC();
        JSTaggedValue res = SlowRuntimeStub::SuspendGenerator(thread, genObj, value);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);

        FrameState *state = GET_FRAME(sp);
        [[maybe_unused]] auto fistPC = state->method->GetInstructions();
        UPDATE_HOTNESS_COUNTER(-(pc - fistPC));
        LOG(DEBUG, INTERPRETER) << "Exit: SuspendGenerator " << std::hex << reinterpret_cast<uintptr_t>(sp) << " "
                                << std::hex << reinterpret_cast<uintptr_t>(state->pc);
        sp = state->prev;
        ASSERT(sp != nullptr);
        FrameState *prevState = GET_FRAME(sp);
        pc = prevState->pc;

        // break frame
        if (pc == nullptr) {
            state->acc = acc;
            return;
        }
        thread->SetCurrentSPFrame(sp);
        constpool = prevState->constpool;

        size_t jumpSize = GetJumpSizeAfterCall(pc);
        DISPATCH_OFFSET(jumpSize);
    }
    HANDLE_OPCODE(HANDLE_ASYNCFUNCTIONAWAITUNCAUGHT_PREF_V8_V8) {
        uint16_t v0 = READ_INST_8_1();
        uint16_t v1 = READ_INST_8_2();
        LOG_INST() << "intrinsics::asyncfunctionawaituncaught"
                   << " v" << v0 << " v" << v1;
        JSTaggedValue asyncFuncObj = GET_VREG_VALUE(v0);
        JSTaggedValue value = GET_VREG_VALUE(v1);
        JSTaggedValue res = SlowRuntimeStub::AsyncFunctionAwaitUncaught(thread, asyncFuncObj, value);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_V8_V8);
    }
    HANDLE_OPCODE(HANDLE_ASYNCFUNCTIONRESOLVE_PREF_V8_V8_V8) {
        uint16_t v0 = READ_INST_8_1();
        [[maybe_unused]] uint16_t v1 = READ_INST_8_2();
        uint16_t v2 = READ_INST_8_3();
        LOG_INST() << "intrinsics::asyncfunctionresolve"
                   << " v" << v0 << " v" << v1 << " v" << v2;

        JSTaggedValue asyncFuncObj = GET_VREG_VALUE(v0);
        JSTaggedValue value = GET_VREG_VALUE(v2);
        JSTaggedValue res = SlowRuntimeStub::AsyncFunctionResolveOrReject(thread, asyncFuncObj, value, true);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_V8_V8_V8);
    }
    HANDLE_OPCODE(HANDLE_ASYNCFUNCTIONREJECT_PREF_V8_V8_V8) {
        uint16_t v0 = READ_INST_8_1();
        [[maybe_unused]] uint16_t v1 = READ_INST_8_2();
        uint16_t v2 = READ_INST_8_3();
        LOG_INST() << "intrinsics::asyncfunctionreject"
                   << " v" << v0 << " v" << v1 << " v" << v2;

        JSTaggedValue asyncFuncObj = GET_VREG_VALUE(v0);
        JSTaggedValue value = GET_VREG_VALUE(v2);
        SAVE_ACC();
        JSTaggedValue res = SlowRuntimeStub::AsyncFunctionResolveOrReject(thread, asyncFuncObj, value, false);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        RESTORE_ACC();
        DISPATCH(BytecodeInstruction::Format::PREF_V8_V8_V8);
    }
    HANDLE_OPCODE(HANDLE_NEWOBJSPREADDYN_PREF_V8_V8) {
        uint16_t v0 = READ_INST_8_1();
        uint16_t v1 = READ_INST_8_2();
        LOG_INST() << "intrinsic::newobjspearddyn"
                   << " v" << v0 << " v" << v1;
        JSTaggedValue func = GET_VREG_VALUE(v0);
        JSTaggedValue newTarget = GET_VREG_VALUE(v1);
        JSTaggedValue array = GET_ACC();
        JSTaggedValue res = SlowRuntimeStub::NewObjSpreadDyn(thread, func, newTarget, array);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_V8_V8);
    }
    HANDLE_OPCODE(HANDLE_THROWUNDEFINEDIFHOLE_PREF_V8_V8) {
        uint16_t v0 = READ_INST_8_1();
        uint16_t v1 = READ_INST_8_2();
        LOG_INST() << "intrinsic::throwundefinedifhole"
                   << " v" << v0 << " v" << v1;
        JSTaggedValue hole = GET_VREG_VALUE(v0);
        if (!hole.IsHole()) {
            DISPATCH(BytecodeInstruction::Format::PREF_V8_V8);
        }
        JSTaggedValue obj = GET_VREG_VALUE(v1);
        ASSERT(obj.IsString());
        SlowRuntimeStub::ThrowUndefinedIfHole(thread, obj);
        INTERPRETER_GOTO_EXCEPTION_HANDLER();
    }
    HANDLE_OPCODE(HANDLE_STOWNBYNAME_PREF_ID32_V8) {
        uint32_t stringId = READ_INST_32_1();
        uint32_t v0 = READ_INST_8_5();
        LOG_INST() << "intrinsics::stownbyname "
                   << "v" << v0 << " stringId:" << stringId;

        JSTaggedValue receiver = GET_VREG_VALUE(v0);
        if (receiver.IsJSObject() && !receiver.IsClassConstructor() && !receiver.IsClassPrototype()) {
            JSTaggedValue propKey = constpool->GetObjectFromCache(stringId);
            JSTaggedValue value = GET_ACC();
            // fast path
            SAVE_ACC();
            JSTaggedValue res = FastRuntimeStub::SetPropertyByName<true>(thread, receiver, propKey, value);
            if (!res.IsHole()) {
                INTERPRETER_RETURN_IF_ABRUPT(res);
                RESTORE_ACC();
                DISPATCH(BytecodeInstruction::Format::PREF_ID32_V8);
            }
            RESTORE_ACC();
        }

        SAVE_ACC();
        receiver = GET_VREG_VALUE(v0);                           // Maybe moved by GC
        auto propKey = constpool->GetObjectFromCache(stringId);  // Maybe moved by GC
        auto value = GET_ACC();                                  // Maybe moved by GC
        JSTaggedValue res = SlowRuntimeStub::StOwnByName(thread, receiver, propKey, value);
        RESTORE_ACC();
        INTERPRETER_RETURN_IF_ABRUPT(res);
        DISPATCH(BytecodeInstruction::Format::PREF_ID32_V8);
    }
    HANDLE_OPCODE(HANDLE_CREATEEMPTYARRAY_PREF) {
        LOG_INST() << "intrinsics::createemptyarray";
        JSTaggedValue res = SlowRuntimeStub::CreateEmptyArray(thread, factory, globalEnv);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_CREATEEMPTYOBJECT_PREF) {
        LOG_INST() << "intrinsics::createemptyobject";
        JSTaggedValue res = SlowRuntimeStub::CreateEmptyObject(thread, factory, globalEnv);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_CREATEOBJECTWITHBUFFER_PREF_IMM16) {
        uint16_t imm = READ_INST_16_1();
        LOG_INST() << "intrinsics::createobjectwithbuffer"
                   << " imm:" << imm;
        JSObject *result = JSObject::Cast(constpool->GetObjectFromCache(imm).GetTaggedObject());

        JSTaggedValue res = SlowRuntimeStub::CreateObjectWithBuffer(thread, factory, result);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_IMM16);
    }
    HANDLE_OPCODE(HANDLE_SETOBJECTWITHPROTO_PREF_V8_V8) {
        uint16_t v0 = READ_INST_8_1();
        uint16_t v1 = READ_INST_8_2();
        LOG_INST() << "intrinsics::setobjectwithproto"
                   << " v" << v0 << " v" << v1;
        JSTaggedValue proto = GET_VREG_VALUE(v0);
        JSTaggedValue obj = GET_VREG_VALUE(v1);
        SAVE_ACC();
        JSTaggedValue res = SlowRuntimeStub::SetObjectWithProto(thread, proto, obj);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        RESTORE_ACC();
        DISPATCH(BytecodeInstruction::Format::PREF_V8_V8);
    }
    HANDLE_OPCODE(HANDLE_CREATEARRAYWITHBUFFER_PREF_IMM16) {
        uint16_t imm = READ_INST_16_1();
        LOG_INST() << "intrinsics::createarraywithbuffer"
                   << " imm:" << imm;
        JSArray *result = JSArray::Cast(constpool->GetObjectFromCache(imm).GetTaggedObject());
        JSTaggedValue res = SlowRuntimeStub::CreateArrayWithBuffer(thread, factory, result);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_IMM16);
    }
    HANDLE_OPCODE(HANDLE_IMPORTMODULE_PREF_ID32) {
        uint32_t stringId = READ_INST_32_1();
        auto prop = constpool->GetObjectFromCache(stringId);

        LOG_INST() << "intrinsics::importmodule "
                   << "stringId:" << stringId << ", " << ConvertToString(EcmaString::Cast(prop.GetTaggedObject()));

        JSTaggedValue moduleRef = SlowRuntimeStub::ImportModule(thread, prop);
        SET_ACC(moduleRef);
        DISPATCH(BytecodeInstruction::Format::PREF_ID32);
    }
    HANDLE_OPCODE(HANDLE_STMODULEVAR_PREF_ID32) {
        uint32_t stringId = READ_INST_32_1();
        auto prop = constpool->GetObjectFromCache(stringId);

        LOG_INST() << "intrinsics::stmodulevar "
                   << "stringId:" << stringId << ", " << ConvertToString(EcmaString::Cast(prop.GetTaggedObject()));
        JSTaggedValue value = GET_ACC();

        SAVE_ACC();
        SlowRuntimeStub::StModuleVar(thread, prop, value);
        RESTORE_ACC();
        DISPATCH(BytecodeInstruction::Format::PREF_ID32);
    }
    HANDLE_OPCODE(HANDLE_COPYMODULE_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();
        JSTaggedValue srcModule = GET_VREG_VALUE(v0);

        LOG_INST() << "intrinsics::copymodule ";

        SAVE_ACC();
        SlowRuntimeStub::CopyModule(thread, srcModule);
        RESTORE_ACC();
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_LDMODVARBYNAME_PREF_ID32_V8) {
        uint32_t stringId = READ_INST_32_1();
        uint32_t v0 = READ_INST_8_5();

        JSTaggedValue itemName = constpool->GetObjectFromCache(stringId);
        JSTaggedValue moduleObj = GET_VREG_VALUE(v0);
        LOG_INST() << "intrinsics::ldmodvarbyname "
                   << "string_id:" << stringId << ", "
                   << "itemName: " << ConvertToString(EcmaString::Cast(itemName.GetTaggedObject()));

        JSTaggedValue moduleVar = SlowRuntimeStub::LdModvarByName(thread, moduleObj, itemName);
        SET_ACC(moduleVar);
        DISPATCH(BytecodeInstruction::Format::PREF_ID32_V8);
    }
    HANDLE_OPCODE(HANDLE_CREATEREGEXPWITHLITERAL_PREF_ID32_IMM8) {
        uint32_t stringId = READ_INST_32_1();
        JSTaggedValue pattern = constpool->GetObjectFromCache(stringId);
        uint8_t flags = READ_INST_8_5();
        LOG_INST() << "intrinsics::createregexpwithliteral "
                   << "stringId:" << stringId << ", " << ConvertToString(EcmaString::Cast(pattern.GetTaggedObject()))
                   << ", flags:" << flags;
        JSTaggedValue res = SlowRuntimeStub::CreateRegExpWithLiteral(thread, pattern, flags);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_ID32_IMM8);
    }
    HANDLE_OPCODE(HANDLE_GETTEMPLATEOBJECT_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();
        LOG_INST() << "intrinsic::gettemplateobject"
                   << " v" << v0;

        JSTaggedValue literal = GET_VREG_VALUE(v0);
        JSTaggedValue res = SlowRuntimeStub::GetTemplateObject(thread, literal);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_GETNEXTPROPNAME_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();
        LOG_INST() << "intrinsic::getnextpropname"
                   << " v" << v0;
        JSTaggedValue iter = GET_VREG_VALUE(v0);
        JSTaggedValue res = SlowRuntimeStub::GetNextPropName(thread, iter);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_COPYDATAPROPERTIES_PREF_V8_V8) {
        uint16_t v0 = READ_INST_8_1();
        uint16_t v1 = READ_INST_8_2();
        LOG_INST() << "intrinsic::copydataproperties"
                   << " v" << v0 << " v" << v1;
        JSTaggedValue dst = GET_VREG_VALUE(v0);
        JSTaggedValue src = GET_VREG_VALUE(v1);
        JSTaggedValue res = SlowRuntimeStub::CopyDataProperties(thread, dst, src);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_V8_V8);
    }
    HANDLE_OPCODE(HANDLE_STOWNBYINDEX_PREF_V8_IMM32) {
        uint32_t v0 = READ_INST_8_1();
        uint32_t index = READ_INST_32_2();
        LOG_INST() << "intrinsics::stownbyindex"
                   << " v" << v0 << " imm" << index;
        JSTaggedValue receiver = GET_VREG_VALUE(v0);
        // fast path
        if (receiver.IsHeapObject() && !receiver.IsClassConstructor() && !receiver.IsClassPrototype()) {
            SAVE_ACC();
            JSTaggedValue value = GET_ACC();
            // fast path
            JSTaggedValue res =
                FastRuntimeStub::SetPropertyByIndex<true>(thread, receiver, index, value);
            if (!res.IsHole()) {
                INTERPRETER_RETURN_IF_ABRUPT(res);
                RESTORE_ACC();
                DISPATCH(BytecodeInstruction::Format::PREF_V8_IMM32);
            }
            RESTORE_ACC();
        }
        SAVE_ACC();
        receiver = GET_VREG_VALUE(v0);  // Maybe moved by GC
        auto value = GET_ACC();         // Maybe moved by GC
        JSTaggedValue res = SlowRuntimeStub::StOwnByIndex(thread, receiver, index, value);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        RESTORE_ACC();
        DISPATCH(BytecodeInstruction::Format::PREF_V8_IMM32);
    }
    HANDLE_OPCODE(HANDLE_STOWNBYVALUE_PREF_V8_V8) {
        uint32_t v0 = READ_INST_8_1();
        uint32_t v1 = READ_INST_8_2();
        LOG_INST() << "intrinsics::stownbyvalue"
                   << " v" << v0 << " v" << v1;

        JSTaggedValue receiver = GET_VREG_VALUE(v0);
        if (receiver.IsHeapObject() && !receiver.IsClassConstructor() && !receiver.IsClassPrototype()) {
            SAVE_ACC();
            JSTaggedValue propKey = GET_VREG_VALUE(v1);
            JSTaggedValue value = GET_ACC();
            // fast path
            JSTaggedValue res = FastRuntimeStub::SetPropertyByValue<true>(thread, receiver, propKey, value);

            // SetPropertyByValue maybe gc need update the value
            RESTORE_ACC();
            propKey = GET_VREG_VALUE(v1);
            value = GET_ACC();
            if (!res.IsHole()) {
                INTERPRETER_RETURN_IF_ABRUPT(res);
                if (value.IsJSFunction()) {
                    JSFunction::SetFunctionNameNoPrefix(thread, JSFunction::Cast(value.GetTaggedObject()), propKey);
                }
                RESTORE_ACC();
                DISPATCH(BytecodeInstruction::Format::PREF_V8_V8);
            }
        }

        // slow path
        SAVE_ACC();
        receiver = GET_VREG_VALUE(v0);      // Maybe moved by GC
        auto propKey = GET_VREG_VALUE(v1);  // Maybe moved by GC
        auto value = GET_ACC();             // Maybe moved by GC
        JSTaggedValue res = SlowRuntimeStub::StOwnByValue(thread, receiver, propKey, value);
        RESTORE_ACC();
        INTERPRETER_RETURN_IF_ABRUPT(res);
        DISPATCH(BytecodeInstruction::Format::PREF_V8_V8);
    }
    HANDLE_OPCODE(HANDLE_CREATEOBJECTWITHEXCLUDEDKEYS_PREF_IMM16_V8_V8) {
        uint16_t numKeys = READ_INST_16_1();
        uint16_t v0 = READ_INST_8_3();
        uint16_t firstArgRegIdx = READ_INST_8_4();
        LOG_INST() << "intrinsics::createobjectwithexcludedkeys " << numKeys << " v" << firstArgRegIdx;

        JSTaggedValue obj = GET_VREG_VALUE(v0);

        JSTaggedValue res = SlowRuntimeStub::CreateObjectWithExcludedKeys(thread, numKeys, obj, firstArgRegIdx);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_IMM16_V8_V8);
    }
    HANDLE_OPCODE(HANDLE_DEFINEGENERATORFUNC_PREF_ID16_IMM16_V8) {
        uint16_t methodId = READ_INST_16_1();
        uint16_t length = READ_INST_16_3();
        uint16_t v0 = READ_INST_8_5();
        LOG_INST() << "define gengerator function length: " << length
                   << " v" << v0;
        JSFunction *result = JSFunction::Cast(constpool->GetObjectFromCache(methodId).GetTaggedObject());
        ASSERT(result != nullptr);
        if (result->IsResolved()) {
            auto res = SlowRuntimeStub::DefineGeneratorFunc(thread, result);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            result = JSFunction::Cast(res.GetTaggedObject());
            result->SetConstantPool(thread, JSTaggedValue(constpool));
        } else {
            result->SetResolved(thread);
        }

        result->SetPropertyInlinedProps(thread, JSFunction::LENGTH_INLINE_PROPERTY_INDEX, JSTaggedValue(length));
        JSTaggedValue env = GET_VREG_VALUE(v0);
        result->SetLexicalEnv(thread, env);
        SET_ACC(JSTaggedValue(result))
        DISPATCH(BytecodeInstruction::Format::PREF_ID16_IMM16_V8);
    }
    HANDLE_OPCODE(HANDLE_DEFINEASYNCFUNC_PREF_ID16_IMM16_V8) {
        uint16_t methodId = READ_INST_16_1();
        uint16_t length = READ_INST_16_3();
        uint16_t v0 = READ_INST_8_5();
        LOG_INST() << "define async function length: " << length
                   << " v" << v0;
        JSFunction *result = JSFunction::Cast(constpool->GetObjectFromCache(methodId).GetTaggedObject());
        ASSERT(result != nullptr);
        if (result->IsResolved()) {
            auto res = SlowRuntimeStub::DefineAsyncFunc(thread, result);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            result = JSFunction::Cast(res.GetTaggedObject());
            result->SetConstantPool(thread, JSTaggedValue(constpool));
        } else {
            result->SetResolved(thread);
        }

        result->SetPropertyInlinedProps(thread, JSFunction::LENGTH_INLINE_PROPERTY_INDEX, JSTaggedValue(length));
        JSTaggedValue env = GET_VREG_VALUE(v0);
        result->SetLexicalEnv(thread, env);
        SET_ACC(JSTaggedValue(result))
        DISPATCH(BytecodeInstruction::Format::PREF_ID16_IMM16_V8);
    }
    HANDLE_OPCODE(HANDLE_LDHOLE_PREF) {
        LOG_INST() << "intrinsic::ldhole";
        SET_ACC(JSTaggedValue::Hole());
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_COPYRESTARGS_PREF_IMM16) {
        uint16_t restIdx = READ_INST_16_1();
        LOG_INST() << "intrinsics::copyrestargs"
                   << " index: " << restIdx;

        FrameState *state = GET_FRAME(sp);
        uint32_t numVregs = state->method->GetNumVregs();
        // Exclude func, newTarget and "this"
        int32_t actualNumArgs = state->numActualArgs - NUM_MANDATORY_JSFUNC_ARGS;
        int32_t tmp = actualNumArgs - restIdx;
        uint32_t restNumArgs = (tmp > 0) ? tmp : 0;
        uint32_t startIdx = numVregs + NUM_MANDATORY_JSFUNC_ARGS + restIdx;

        JSTaggedValue res = SlowRuntimeStub::CopyRestArgs(thread, sp, restNumArgs, startIdx);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_IMM16);
    }
    HANDLE_OPCODE(HANDLE_DEFINEGETTERSETTERBYVALUE_PREF_V8_V8_V8_V8) {
        uint16_t v0 = READ_INST_8_1();
        uint16_t v1 = READ_INST_8_2();
        uint16_t v2 = READ_INST_8_3();
        uint16_t v3 = READ_INST_8_4();
        LOG_INST() << "intrinsics::definegettersetterbyvalue"
                   << " v" << v0 << " v" << v1 << " v" << v2 << " v" << v3;

        JSTaggedValue obj = GET_VREG_VALUE(v0);
        JSTaggedValue prop = GET_VREG_VALUE(v1);
        JSTaggedValue getter = GET_VREG_VALUE(v2);
        JSTaggedValue setter = GET_VREG_VALUE(v3);
        JSTaggedValue flag = GET_ACC();
        JSTaggedValue res =
            SlowRuntimeStub::DefineGetterSetterByValue(thread, obj, prop, getter, setter, flag.ToBoolean());
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_V8_V8_V8_V8);
    }
    HANDLE_OPCODE(HANDLE_LDOBJBYINDEX_PREF_V8_IMM32) {
        uint16_t v0 = READ_INST_8_1();
        uint32_t idx = READ_INST_32_2();
        LOG_INST() << "intrinsics::ldobjbyindex"
                   << " v" << v0 << " imm" << idx;

        JSTaggedValue receiver = GET_VREG_VALUE(v0);
        // fast path
        if (LIKELY(receiver.IsHeapObject())) {
            JSTaggedValue res = FastRuntimeStub::GetPropertyByIndex(thread, receiver, idx);
            if (!res.IsHole()) {
                INTERPRETER_RETURN_IF_ABRUPT(res);
                SET_ACC(res);
                DISPATCH(BytecodeInstruction::Format::PREF_V8_IMM32);
            }
        }
        // not meet fast condition or fast path return hole, walk slow path
        // slow stub not need receiver
        JSTaggedValue res = SlowRuntimeStub::LdObjByIndex(thread, receiver, idx, false, JSTaggedValue::Undefined());
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_V8_IMM32);
    }
    HANDLE_OPCODE(HANDLE_STOBJBYINDEX_PREF_V8_IMM32) {
        uint16_t v0 = READ_INST_8_1();
        uint32_t index = READ_INST_32_2();
        LOG_INST() << "intrinsics::stobjbyindex"
                   << " v" << v0 << " imm" << index;

        JSTaggedValue receiver = GET_VREG_VALUE(v0);
        if (receiver.IsHeapObject()) {
            SAVE_ACC();
            JSTaggedValue value = GET_ACC();
            // fast path
            JSTaggedValue res = FastRuntimeStub::SetPropertyByIndex(thread, receiver, index, value);
            if (!res.IsHole()) {
                INTERPRETER_RETURN_IF_ABRUPT(res);
                RESTORE_ACC();
                DISPATCH(BytecodeInstruction::Format::PREF_V8_IMM32);
            }
            RESTORE_ACC();
        }
        // slow path
        SAVE_ACC();
        receiver = GET_VREG_VALUE(v0);    // Maybe moved by GC
        JSTaggedValue value = GET_ACC();  // Maybe moved by GC
        JSTaggedValue res = SlowRuntimeStub::StObjByIndex(thread, receiver, index, value);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        RESTORE_ACC();
        DISPATCH(BytecodeInstruction::Format::PREF_V8_IMM32);
    }
    HANDLE_OPCODE(HANDLE_LDOBJBYVALUE_PREF_V8_V8) {
        uint32_t v0 = READ_INST_8_1();
        uint32_t v1 = READ_INST_8_2();
        LOG_INST() << "intrinsics::Ldobjbyvalue"
                   << " v" << v0 << " v" << v1;

        JSTaggedValue receiver = GET_VREG_VALUE(v0);
        JSTaggedValue propKey = GET_VREG_VALUE(v1);

#if ECMASCRIPT_ENABLE_IC
        auto profileTypeInfo = GetRuntimeProfileTypeInfo(sp);
        if (!profileTypeInfo.IsUndefined()) {
            uint16_t slotId = READ_INST_8_0();
            auto profileTypeArray = ProfileTypeInfo::Cast(profileTypeInfo.GetTaggedObject());
            JSTaggedValue firstValue = profileTypeArray->Get(slotId);
            JSTaggedValue res = JSTaggedValue::Hole();

            if (LIKELY(firstValue.IsHeapObject())) {
                JSTaggedValue secondValue = profileTypeArray->Get(slotId + 1);
                res = ICRuntimeStub::TryLoadICByValue(thread, receiver, propKey, firstValue, secondValue);
            } else if (firstValue.IsUndefined()) {
                res = ICRuntimeStub::LoadICByValue(thread,
                                                   profileTypeArray,
                                                   receiver, propKey, slotId);
            }

            if (LIKELY(!res.IsHole())) {
                INTERPRETER_RETURN_IF_ABRUPT(res);
                SET_ACC(res);
                DISPATCH(BytecodeInstruction::Format::PREF_V8_V8);
            }
        }
#endif
        // fast path
        if (LIKELY(receiver.IsHeapObject())) {
            JSTaggedValue res = FastRuntimeStub::GetPropertyByValue(thread, receiver, propKey);
            if (!res.IsHole()) {
                ASSERT(!res.IsAccessor());
                INTERPRETER_RETURN_IF_ABRUPT(res);
                SET_ACC(res);
                DISPATCH(BytecodeInstruction::Format::PREF_V8_V8);
            }
        }
        // slow path
        JSTaggedValue res = SlowRuntimeStub::LdObjByValue(thread, receiver, propKey, false, JSTaggedValue::Undefined());
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_V8_V8);
    }
    HANDLE_OPCODE(HANDLE_STOBJBYVALUE_PREF_V8_V8) {
        uint32_t v0 = READ_INST_8_1();
        uint32_t v1 = READ_INST_8_2();

        LOG_INST() << "intrinsics::stobjbyvalue"
                   << " v" << v0 << " v" << v1;

        JSTaggedValue receiver = GET_VREG_VALUE(v0);
#if ECMASCRIPT_ENABLE_IC
        auto profileTypeInfo = GetRuntimeProfileTypeInfo(sp);
        if (!profileTypeInfo.IsUndefined()) {
            uint16_t slotId = READ_INST_8_0();
            auto profileTypeArray = ProfileTypeInfo::Cast(profileTypeInfo.GetTaggedObject());
            JSTaggedValue firstValue = profileTypeArray->Get(slotId);
            JSTaggedValue propKey = GET_VREG_VALUE(v1);
            JSTaggedValue value = GET_ACC();
            JSTaggedValue res = JSTaggedValue::Hole();
            SAVE_ACC();

            if (LIKELY(firstValue.IsHeapObject())) {
                JSTaggedValue secondValue = profileTypeArray->Get(slotId + 1);
                res = ICRuntimeStub::TryStoreICByValue(thread, receiver, propKey, firstValue, secondValue, value);
            } else if (firstValue.IsUndefined()) {
                res = ICRuntimeStub::StoreICByValue(thread,
                                                    profileTypeArray,
                                                    receiver, propKey, value, slotId);
            }

            if (LIKELY(!res.IsHole())) {
                INTERPRETER_RETURN_IF_ABRUPT(res);
                RESTORE_ACC();
                DISPATCH(BytecodeInstruction::Format::PREF_V8_V8);
            }
        }
#endif
        if (receiver.IsHeapObject()) {
            SAVE_ACC();
            JSTaggedValue propKey = GET_VREG_VALUE(v1);
            JSTaggedValue value = GET_ACC();
            // fast path
            JSTaggedValue res = FastRuntimeStub::SetPropertyByValue(thread, receiver, propKey, value);
            if (!res.IsHole()) {
                INTERPRETER_RETURN_IF_ABRUPT(res);
                RESTORE_ACC();
                DISPATCH(BytecodeInstruction::Format::PREF_V8_V8);
            }
            RESTORE_ACC();
        }
        {
            // slow path
            SAVE_ACC();
            receiver = GET_VREG_VALUE(v0);  // Maybe moved by GC
            JSTaggedValue propKey = GET_VREG_VALUE(v1);   // Maybe moved by GC
            JSTaggedValue value = GET_ACC();              // Maybe moved by GC
            JSTaggedValue res = SlowRuntimeStub::StObjByValue(thread, receiver, propKey, value);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            RESTORE_ACC();
        }
        DISPATCH(BytecodeInstruction::Format::PREF_V8_V8);
    }
    HANDLE_OPCODE(HANDLE_LDSUPERBYVALUE_PREF_V8_V8) {
        uint32_t v0 = READ_INST_8_1();
        uint32_t v1 = READ_INST_8_2();
        LOG_INST() << "intrinsics::Ldsuperbyvalue"
                   << " v" << v0 << " v" << v1;

        JSTaggedValue receiver = GET_VREG_VALUE(v0);
        JSTaggedValue propKey = GET_VREG_VALUE(v1);

        // slow path
        JSTaggedValue thisFunc = GetThisFunction(sp);
        JSTaggedValue res = SlowRuntimeStub::LdSuperByValue(thread, receiver, propKey, thisFunc);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_V8_V8);
    }
    HANDLE_OPCODE(HANDLE_STSUPERBYVALUE_PREF_V8_V8) {
        uint32_t v0 = READ_INST_8_1();
        uint32_t v1 = READ_INST_8_2();

        LOG_INST() << "intrinsics::stsuperbyvalue"
                   << " v" << v0 << " v" << v1;
        JSTaggedValue receiver = GET_VREG_VALUE(v0);
        JSTaggedValue propKey = GET_VREG_VALUE(v1);
        JSTaggedValue value = GET_ACC();

        // slow path
        SAVE_ACC();
        JSTaggedValue thisFunc = GetThisFunction(sp);
        JSTaggedValue res = SlowRuntimeStub::StSuperByValue(thread, receiver, propKey, value, thisFunc);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        RESTORE_ACC();
        DISPATCH(BytecodeInstruction::Format::PREF_V8_V8);
    }
    HANDLE_OPCODE(HANDLE_TRYLDGLOBALBYNAME_PREF_ID32) {
        uint32_t stringId = READ_INST_32_1();
        auto prop = constpool->GetObjectFromCache(stringId);

        LOG_INST() << "intrinsics::tryldglobalbyname "
                   << "stringId:" << stringId << ", " << ConvertToString(EcmaString::Cast(prop.GetTaggedObject()));

#if ECMASCRIPT_ENABLE_IC
        auto profileTypeInfo = GetRuntimeProfileTypeInfo(sp);
        if (!profileTypeInfo.IsUndefined()) {
            uint16_t slotId = READ_INST_8_0();
            JSTaggedValue res = ICRuntimeStub::LoadGlobalICByName(thread,
                                                                  ProfileTypeInfo::Cast(
                                                                      profileTypeInfo.GetTaggedObject()),
                                                                  globalObj, prop, slotId);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            SET_ACC(res);
            DISPATCH(BytecodeInstruction::Format::PREF_ID32);
        }
#endif

        bool found = false;
        // order: 1. global record 2. global object
        JSTaggedValue result = SlowRuntimeStub::LdGlobalRecord(thread, prop, &found);
        if (found) {
            SET_ACC(result);
        } else {
            JSTaggedValue globalResult = FastRuntimeStub::GetGlobalOwnProperty(globalObj, prop, &found);
            if (found) {
                SET_ACC(globalResult);
            } else {
                // slow path
                JSTaggedValue res = SlowRuntimeStub::TryLdGlobalByName(thread, globalObj, prop);
                INTERPRETER_RETURN_IF_ABRUPT(res);
                SET_ACC(res);
            }
        }

        DISPATCH(BytecodeInstruction::Format::PREF_ID32);
    }
    HANDLE_OPCODE(HANDLE_TRYSTGLOBALBYNAME_PREF_ID32) {
        uint32_t stringId = READ_INST_32_1();
        JSTaggedValue propKey = constpool->GetObjectFromCache(stringId);
        LOG_INST() << "intrinsics::trystglobalbyname"
                   << " stringId:" << stringId << ", " << ConvertToString(EcmaString::Cast(propKey.GetTaggedObject()));

#if ECMASCRIPT_ENABLE_IC
        auto profileTypeInfo = GetRuntimeProfileTypeInfo(sp);
        if (!profileTypeInfo.IsUndefined()) {
            uint16_t slotId = READ_INST_8_0();
            JSTaggedValue value = GET_ACC();
            SAVE_ACC();
            JSTaggedValue res = ICRuntimeStub::StoreGlobalICByName(thread,
                                                                   ProfileTypeInfo::Cast(
                                                                       profileTypeInfo.GetTaggedObject()),
                                                                   globalObj, propKey, value, slotId);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            RESTORE_ACC();
            DISPATCH(BytecodeInstruction::Format::PREF_ID32);
        }
#endif

        bool found = false;
        SlowRuntimeStub::LdGlobalRecord(thread, propKey, &found);
        // 1. find from global record
        if (found) {
            JSTaggedValue value = GET_ACC();
            SAVE_ACC();
            JSTaggedValue res = SlowRuntimeStub::TryUpdateGlobalRecord(thread, propKey, value);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            RESTORE_ACC();
        } else {
            // 2. find from global object
            FastRuntimeStub::GetGlobalOwnProperty(globalObj, propKey, &found);
            if (!found) {
                auto result = SlowRuntimeStub::ThrowReferenceError(thread, propKey, " is not defined");
                INTERPRETER_RETURN_IF_ABRUPT(result);
            }
            JSTaggedValue value = GET_ACC();
            SAVE_ACC();
            JSTaggedValue res = SlowRuntimeStub::StGlobalVar(thread, propKey, value);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            RESTORE_ACC();
        }
        DISPATCH(BytecodeInstruction::Format::PREF_ID32);
    }

    HANDLE_OPCODE(HANDLE_STCONSTTOGLOBALRECORD_PREF_ID32) {
        uint32_t stringId = READ_INST_32_1();
        JSTaggedValue propKey = constpool->GetObjectFromCache(stringId);
        LOG_INST() << "intrinsics::stconsttoglobalrecord"
                   << " stringId:" << stringId << ", " << ConvertToString(EcmaString::Cast(propKey.GetTaggedObject()));

        JSTaggedValue value = GET_ACC();
        SAVE_ACC();
        JSTaggedValue res = SlowRuntimeStub::StGlobalRecord(thread, propKey, value, true);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        RESTORE_ACC();
        DISPATCH(BytecodeInstruction::Format::PREF_ID32);
    }

    HANDLE_OPCODE(HANDLE_STLETTOGLOBALRECORD_PREF_ID32) {
        uint32_t stringId = READ_INST_32_1();
        JSTaggedValue propKey = constpool->GetObjectFromCache(stringId);
        LOG_INST() << "intrinsics::stlettoglobalrecord"
                   << " stringId:" << stringId << ", " << ConvertToString(EcmaString::Cast(propKey.GetTaggedObject()));

        JSTaggedValue value = GET_ACC();
        SAVE_ACC();
        JSTaggedValue res = SlowRuntimeStub::StGlobalRecord(thread, propKey, value, false);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        RESTORE_ACC();
        DISPATCH(BytecodeInstruction::Format::PREF_ID32);
    }

    HANDLE_OPCODE(HANDLE_STCLASSTOGLOBALRECORD_PREF_ID32) {
        uint32_t stringId = READ_INST_32_1();
        JSTaggedValue propKey = constpool->GetObjectFromCache(stringId);
        LOG_INST() << "intrinsics::stclasstoglobalrecord"
                   << " stringId:" << stringId << ", " << ConvertToString(EcmaString::Cast(propKey.GetTaggedObject()));

        JSTaggedValue value = GET_ACC();
        SAVE_ACC();
        JSTaggedValue res = SlowRuntimeStub::StGlobalRecord(thread, propKey, value, false);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        RESTORE_ACC();
        DISPATCH(BytecodeInstruction::Format::PREF_ID32);
    }

    HANDLE_OPCODE(HANDLE_LDGLOBALVAR_PREF_ID32) {
        uint32_t stringId = READ_INST_32_1();
        JSTaggedValue propKey = constpool->GetObjectFromCache(stringId);

#if ECMASCRIPT_ENABLE_IC
        auto profileTypeInfo = GetRuntimeProfileTypeInfo(sp);
        if (!profileTypeInfo.IsUndefined()) {
            uint16_t slotId = READ_INST_8_0();
            JSTaggedValue res = ICRuntimeStub::LoadGlobalICByName(thread,
                                                                  ProfileTypeInfo::Cast(
                                                                      profileTypeInfo.GetTaggedObject()),
                                                                  globalObj, propKey, slotId);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            SET_ACC(res);
            DISPATCH(BytecodeInstruction::Format::PREF_ID32);
        }
#endif
        bool found = false;
        JSTaggedValue result = FastRuntimeStub::GetGlobalOwnProperty(globalObj, propKey, &found);
        if (found) {
            SET_ACC(result);
        } else {
            // slow path
            JSTaggedValue res = SlowRuntimeStub::LdGlobalVar(thread, globalObj, propKey);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            SET_ACC(res);
        }
        DISPATCH(BytecodeInstruction::Format::PREF_ID32);
    }
    HANDLE_OPCODE(HANDLE_LDOBJBYNAME_PREF_ID32_V8) {
        uint32_t v0 = READ_INST_8_5();
        JSTaggedValue receiver = GET_VREG_VALUE(v0);

#if ECMASCRIPT_ENABLE_IC
        auto profileTypeInfo = GetRuntimeProfileTypeInfo(sp);
        if (!profileTypeInfo.IsUndefined()) {
            uint16_t slotId = READ_INST_8_0();
            auto profileTypeArray = ProfileTypeInfo::Cast(profileTypeInfo.GetTaggedObject());
            JSTaggedValue firstValue = profileTypeArray->Get(slotId);
            JSTaggedValue res = JSTaggedValue::Hole();

            if (LIKELY(firstValue.IsHeapObject())) {
                JSTaggedValue secondValue = profileTypeArray->Get(slotId + 1);
                res = ICRuntimeStub::TryLoadICByName(thread, receiver, firstValue, secondValue);
            } else if (firstValue.IsUndefined()) {
                uint32_t stringId = READ_INST_32_1();
                JSTaggedValue propKey = constpool->GetObjectFromCache(stringId);
                res = ICRuntimeStub::LoadICByName(thread,
                                                  profileTypeArray,
                                                  receiver, propKey, slotId);
            }

            if (LIKELY(!res.IsHole())) {
                INTERPRETER_RETURN_IF_ABRUPT(res);
                SET_ACC(res);
                DISPATCH(BytecodeInstruction::Format::PREF_ID32_V8);
            }
        }
#endif
        uint32_t stringId = READ_INST_32_1();
        JSTaggedValue propKey = constpool->GetObjectFromCache(stringId);
        LOG_INST() << "intrinsics::ldobjbyname "
                   << "v" << v0 << " stringId:" << stringId << ", "
                   << ConvertToString(EcmaString::Cast(propKey.GetTaggedObject())) << ", obj:" << receiver.GetRawData();

        if (LIKELY(receiver.IsHeapObject())) {
            // fast path
            JSTaggedValue res = FastRuntimeStub::GetPropertyByName(thread, receiver, propKey);
            if (!res.IsHole()) {
                ASSERT(!res.IsAccessor());
                INTERPRETER_RETURN_IF_ABRUPT(res);
                SET_ACC(res);
                DISPATCH(BytecodeInstruction::Format::PREF_ID32_V8);
            }
        }
        // not meet fast condition or fast path return hole, walk slow path
        // slow stub not need receiver
        JSTaggedValue res = SlowRuntimeStub::LdObjByName(thread, receiver, propKey, false, JSTaggedValue::Undefined());
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_ID32_V8);
    }
    HANDLE_OPCODE(HANDLE_STOBJBYNAME_PREF_ID32_V8) {
        uint32_t v0 = READ_INST_8_5();
        JSTaggedValue receiver = GET_VREG_VALUE(v0);
        JSTaggedValue value = GET_ACC();
#if ECMASCRIPT_ENABLE_IC
        auto profileTypeInfo = GetRuntimeProfileTypeInfo(sp);
        if (!profileTypeInfo.IsUndefined()) {
            uint16_t slotId = READ_INST_8_0();
            auto profileTypeArray = ProfileTypeInfo::Cast(profileTypeInfo.GetTaggedObject());
            JSTaggedValue firstValue = profileTypeArray->Get(slotId);
            JSTaggedValue res = JSTaggedValue::Hole();
            SAVE_ACC();

            if (LIKELY(firstValue.IsHeapObject())) {
                JSTaggedValue secondValue = profileTypeArray->Get(slotId + 1);
                res = ICRuntimeStub::TryStoreICByName(thread, receiver, firstValue, secondValue, value);
            } else if (firstValue.IsUndefined()) {
                uint32_t stringId = READ_INST_32_1();
                JSTaggedValue propKey = constpool->GetObjectFromCache(stringId);
                res = ICRuntimeStub::StoreICByName(thread,
                                                   profileTypeArray,
                                                   receiver, propKey, value, slotId);
            }

            if (LIKELY(!res.IsHole())) {
                INTERPRETER_RETURN_IF_ABRUPT(res);
                RESTORE_ACC();
                DISPATCH(BytecodeInstruction::Format::PREF_ID32_V8);
            }
        }
#endif
        uint32_t stringId = READ_INST_32_1();
        LOG_INST() << "intrinsics::stobjbyname "
                   << "v" << v0 << " stringId:" << stringId;
        if (receiver.IsHeapObject()) {
            JSTaggedValue propKey = constpool->GetObjectFromCache(stringId);
            value = GET_ACC();
            // fast path
            SAVE_ACC();
            JSTaggedValue res = FastRuntimeStub::SetPropertyByName(thread, receiver, propKey, value);
            if (!res.IsHole()) {
                INTERPRETER_RETURN_IF_ABRUPT(res);
                RESTORE_ACC();
                DISPATCH(BytecodeInstruction::Format::PREF_ID32_V8);
            }
            RESTORE_ACC();
        }
        // slow path
        SAVE_ACC();
        receiver = GET_VREG_VALUE(v0);                           // Maybe moved by GC
        auto propKey = constpool->GetObjectFromCache(stringId);  // Maybe moved by GC
        value = GET_ACC();                                  // Maybe moved by GC
        JSTaggedValue res = SlowRuntimeStub::StObjByName(thread, receiver, propKey, value);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        RESTORE_ACC();
        DISPATCH(BytecodeInstruction::Format::PREF_ID32_V8);
    }
    HANDLE_OPCODE(HANDLE_LDSUPERBYNAME_PREF_ID32_V8) {
        uint32_t stringId = READ_INST_32_1();
        uint32_t v0 = READ_INST_8_5();
        JSTaggedValue obj = GET_VREG_VALUE(v0);
        JSTaggedValue propKey = constpool->GetObjectFromCache(stringId);

        LOG_INST() << "intrinsics::ldsuperbyname"
                   << "v" << v0 << " stringId:" << stringId << ", "
                   << ConvertToString(EcmaString::Cast(propKey.GetTaggedObject())) << ", obj:" << obj.GetRawData();

        JSTaggedValue thisFunc = GetThisFunction(sp);
        JSTaggedValue res = SlowRuntimeStub::LdSuperByValue(thread, obj, propKey, thisFunc);

        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_ID32_V8);
    }
    HANDLE_OPCODE(HANDLE_STSUPERBYNAME_PREF_ID32_V8) {
        uint32_t stringId = READ_INST_32_1();
        uint32_t v0 = READ_INST_8_5();

        JSTaggedValue obj = GET_VREG_VALUE(v0);
        JSTaggedValue propKey = constpool->GetObjectFromCache(stringId);
        JSTaggedValue value = GET_ACC();

        LOG_INST() << "intrinsics::stsuperbyname"
                   << "v" << v0 << " stringId:" << stringId << ", "
                   << ConvertToString(EcmaString::Cast(propKey.GetTaggedObject())) << ", obj:" << obj.GetRawData()
                   << ", value:" << value.GetRawData();

        // slow path
        SAVE_ACC();
        JSTaggedValue thisFunc = GetThisFunction(sp);
        JSTaggedValue res = SlowRuntimeStub::StSuperByValue(thread, obj, propKey, value, thisFunc);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        RESTORE_ACC();
        DISPATCH(BytecodeInstruction::Format::PREF_ID32_V8);
    }
    HANDLE_OPCODE(HANDLE_STGLOBALVAR_PREF_ID32) {
        uint32_t stringId = READ_INST_32_1();
        JSTaggedValue prop = constpool->GetObjectFromCache(stringId);
        JSTaggedValue value = GET_ACC();

        LOG_INST() << "intrinsics::stglobalvar "
                   << "stringId:" << stringId << ", " << ConvertToString(EcmaString::Cast(prop.GetTaggedObject()))
                   << ", value:" << value.GetRawData();
#if ECMASCRIPT_ENABLE_IC
        auto profileTypeInfo = GetRuntimeProfileTypeInfo(sp);
        if (!profileTypeInfo.IsUndefined()) {
            uint16_t slotId = READ_INST_8_0();
            SAVE_ACC();
            JSTaggedValue res = ICRuntimeStub::StoreGlobalICByName(thread,
                                                                   ProfileTypeInfo::Cast(
                                                                       profileTypeInfo.GetTaggedObject()),
                                                                   globalObj, prop, value, slotId);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            RESTORE_ACC();
            DISPATCH(BytecodeInstruction::Format::PREF_ID32);
        }
#endif
        SAVE_ACC();
        JSTaggedValue res = SlowRuntimeStub::StGlobalVar(thread, prop, value);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        RESTORE_ACC();
        DISPATCH(BytecodeInstruction::Format::PREF_ID32);
    }
    HANDLE_OPCODE(HANDLE_CREATEGENERATOROBJ_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();
        LOG_INST() << "intrinsics::creategeneratorobj"
                   << " v" << v0;
        JSTaggedValue genFunc = GET_VREG_VALUE(v0);
        JSTaggedValue res = SlowRuntimeStub::CreateGeneratorObj(thread, genFunc);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_STARRAYSPREAD_PREF_V8_V8) {
        uint16_t v0 = READ_INST_8_1();
        uint16_t v1 = READ_INST_8_2();
        LOG_INST() << "ecmascript::intrinsics::starrayspread"
                   << " v" << v0 << " v" << v1 << "acc";
        JSTaggedValue dst = GET_VREG_VALUE(v0);
        JSTaggedValue index = GET_VREG_VALUE(v1);
        JSTaggedValue src = GET_ACC();
        JSTaggedValue res = SlowRuntimeStub::StArraySpread(thread, dst, index, src);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_V8_V8);
    }
    HANDLE_OPCODE(HANDLE_GETITERATORNEXT_PREF_V8_V8) {
        uint16_t v0 = READ_INST_8_1();
        uint16_t v1 = READ_INST_8_2();
        LOG_INST() << "intrinsic::getiteratornext"
                   << " v" << v0 << " v" << v1;
        JSTaggedValue obj = GET_VREG_VALUE(v0);
        JSTaggedValue method = GET_VREG_VALUE(v1);
        JSTaggedValue res = SlowRuntimeStub::GetIteratorNext(thread, obj, method);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_V8_V8);
    }
    HANDLE_OPCODE(HANDLE_DEFINECLASSWITHBUFFER_PREF_ID16_IMM16_IMM16_V8_V8) {
        uint16_t methodId = READ_INST_16_1();
        uint16_t imm = READ_INST_16_3();
        uint16_t length = READ_INST_16_5();
        uint16_t v0 = READ_INST_8_7();
        uint16_t v1 = READ_INST_8_8();
        LOG_INST() << "intrinsics::defineclasswithbuffer"
                   << " method id:" << methodId << " literal id:" << imm << " lexenv: v" << v0 << " parent: v" << v1;
        JSFunction *cls = JSFunction::Cast(constpool->GetObjectFromCache(methodId).GetTaggedObject());
        ASSERT(cls != nullptr);
        if (cls->IsResolved()) {
            auto res = SlowRuntimeStub::NewClassFunc(thread, cls);
            INTERPRETER_RETURN_IF_ABRUPT(res);
            cls = JSFunction::Cast(res.GetTaggedObject());
            cls->SetConstantPool(thread, JSTaggedValue(constpool));
        } else {
            cls->SetResolved(thread);
        }

        cls->SetPropertyInlinedProps(thread, JSFunction::LENGTH_INLINE_PROPERTY_INDEX, JSTaggedValue(length));
        JSTaggedValue lexenv = GET_VREG_VALUE(v0);
        cls->SetLexicalEnv(thread, lexenv);

        TaggedArray *literalBuffer = TaggedArray::Cast(constpool->GetObjectFromCache(imm).GetTaggedObject());
        JSTaggedValue proto = GET_VREG_VALUE(v1);
        JSTaggedValue res = SlowRuntimeStub::DefineClass(thread, cls, literalBuffer, proto, lexenv, constpool);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_ID16_IMM16_IMM16_V8_V8);
    }
    HANDLE_OPCODE(HANDLE_SUPERCALL_PREF_IMM16_V8) {
        uint16_t range = READ_INST_16_1();
        uint16_t v0 = READ_INST_8_3();
        LOG_INST() << "intrinsics::supercall"
                   << " range: " << range << " v" << v0;

        FrameState *state = GET_FRAME(sp);
        auto numVregs = state->method->GetNumVregs();
        JSTaggedValue thisFunc = GET_ACC();
        JSTaggedValue newTarget = GET_VREG_VALUE(numVregs + 1);

        JSTaggedValue res = SlowRuntimeStub::SuperCall(thread, thisFunc, newTarget, v0, range);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_IMM16_V8);
    }
    HANDLE_OPCODE(HANDLE_SUPERCALLSPREAD_PREF_V8) {
        uint16_t v0 = READ_INST_8_1();
        LOG_INST() << "intrinsic::supercallspread"
                   << " array: v" << v0;

        FrameState *state = GET_FRAME(sp);
        auto numVregs = state->method->GetNumVregs();
        JSTaggedValue thisFunc = GET_ACC();
        JSTaggedValue newTarget = GET_VREG_VALUE(numVregs + 1);
        JSTaggedValue array = GET_VREG_VALUE(v0);

        JSTaggedValue res = SlowRuntimeStub::SuperCallSpread(thread, thisFunc, newTarget, array);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_V8);
    }
    HANDLE_OPCODE(HANDLE_CREATEOBJECTHAVINGMETHOD_PREF_IMM16) {
        uint16_t imm = READ_INST_16_1();
        LOG_INST() << "intrinsics::createobjecthavingmethod"
                   << " imm:" << imm;
        JSObject *result = JSObject::Cast(constpool->GetObjectFromCache(imm).GetTaggedObject());
        JSTaggedValue env = GET_ACC();

        JSTaggedValue res = SlowRuntimeStub::CreateObjectHavingMethod(thread, factory, result, env, constpool);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        SET_ACC(res);
        DISPATCH(BytecodeInstruction::Format::PREF_IMM16);
    }
    HANDLE_OPCODE(HANDLE_THROWIFSUPERNOTCORRECTCALL_PREF_IMM16) {
        uint16_t imm = READ_INST_16_1();
        JSTaggedValue thisValue = GET_ACC();
        LOG_INST() << "intrinsic::throwifsupernotcorrectcall"
                   << " imm:" << imm;
        JSTaggedValue res = SlowRuntimeStub::ThrowIfSuperNotCorrectCall(thread, imm, thisValue);
        INTERPRETER_RETURN_IF_ABRUPT(res);
        DISPATCH(BytecodeInstruction::Format::PREF_IMM16);
    }
    HANDLE_OPCODE(HANDLE_LDHOMEOBJECT_PREF) {
        LOG_INST() << "intrinsics::ldhomeobject";

        JSTaggedValue thisFunc = GetThisFunction(sp);
        JSTaggedValue homeObject = JSFunction::Cast(thisFunc.GetTaggedObject())->GetHomeObject();

        SET_ACC(homeObject);
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_THROWDELETESUPERPROPERTY_PREF) {
        LOG_INST() << "throwdeletesuperproperty";

        SlowRuntimeStub::ThrowDeleteSuperProperty(thread);
        INTERPRETER_GOTO_EXCEPTION_HANDLER();
    }
    HANDLE_OPCODE(HANDLE_DEBUGGER_PREF) {
        LOG_INST() << "intrinsics::debugger";
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_ISTRUE_PREF) {
        LOG_INST() << "intrinsics::istrue";
        if (GET_ACC().ToBoolean()) {
            SET_ACC(JSTaggedValue::True());
        } else {
            SET_ACC(JSTaggedValue::False());
        }
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(HANDLE_ISFALSE_PREF) {
        LOG_INST() << "intrinsics::isfalse";
        if (!GET_ACC().ToBoolean()) {
            SET_ACC(JSTaggedValue::True());
        } else {
            SET_ACC(JSTaggedValue::False());
        }
        DISPATCH(BytecodeInstruction::Format::PREF_NONE);
    }
    HANDLE_OPCODE(EXCEPTION_HANDLER) {
        auto exception = thread->GetException();

        EcmaFrameHandler frameHandler(sp);
        uint32_t pcOffset = panda_file::INVALID_OFFSET;
        for (; frameHandler.HasFrame(); frameHandler.PrevFrame()) {
            auto method = frameHandler.GetMethod();
            pcOffset = FindCatchBlock(method, frameHandler.GetBytecodeOffset());
            if (pcOffset != panda_file::INVALID_OFFSET) {
                sp = frameHandler.GetSp();
                constpool = frameHandler.GetConstpool();
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                pc = JSMethod::Cast(method)->GetBytecodeArray() + pcOffset;
                break;
            }
        }
        if (pcOffset == panda_file::INVALID_OFFSET) {
            return;
        }

        // set exception to acc
        if (exception.IsObjectWrapper()) {
            SET_ACC(ObjectWrapper::Cast(exception.GetTaggedObject())->GetValue());
        } else {
            SET_ACC(exception);
        }
        thread->ClearException();
        thread->SetCurrentSPFrame(sp);
        DISPATCH_OFFSET(0);
    }
    HANDLE_OPCODE(HANDLE_OVERFLOW) {
        LOG(FATAL, INTERPRETER) << "opcode overflow";
    }
#include "templates/debugger_instruction_handler.inl"
}

void EcmaInterpreter::InitStackFrame(JSThread *thread)
{
    uint64_t *prevSp = const_cast<uint64_t *>(thread->GetCurrentSPFrame());
    FrameState *state = GET_FRAME(prevSp);
    state->pc = nullptr;
    state->sp = nullptr;
    state->method = nullptr;
    state->acc = JSTaggedValue::Hole();
    state->constpool = nullptr;
    state->profileTypeInfo = JSTaggedValue::Undefined();
    state->prev = nullptr;
    state->numActualArgs = 0;
}

uint32_t EcmaInterpreter::FindCatchBlock(JSMethod *caller, uint32_t pc)
{
    auto *pandaFile = caller->GetPandaFile();
    panda_file::MethodDataAccessor mda(*pandaFile, caller->GetFileId());
    panda_file::CodeDataAccessor cda(*pandaFile, mda.GetCodeId().value());

    uint32_t pcOffset = panda_file::INVALID_OFFSET;
    cda.EnumerateTryBlocks([&pcOffset, pc](panda_file::CodeDataAccessor::TryBlock &try_block) {
        if ((try_block.GetStartPc() <= pc) && ((try_block.GetStartPc() + try_block.GetLength()) > pc)) {
            try_block.EnumerateCatchBlocks([&](panda_file::CodeDataAccessor::CatchBlock &catch_block) {
                pcOffset = catch_block.GetHandlerPc();
                return false;
            });
        }
        return pcOffset == panda_file::INVALID_OFFSET;
    });
    return pcOffset;
}

void EcmaInterpreter::InterpreterFrameCopyArgs(JSTaggedType *newSp, uint32_t numVregs, uint32_t numActualArgs,
                                               uint32_t numDeclaredArgs)
{
    for (size_t i = 0; i < numVregs; i++) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        newSp[i] = JSTaggedValue::VALUE_UNDEFINED;
    }
    for (size_t i = numActualArgs; i < numDeclaredArgs; i++) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        newSp[numVregs + i] = JSTaggedValue::VALUE_UNDEFINED;
    }
}

JSTaggedValue EcmaInterpreter::GetThisFunction(JSTaggedType *sp)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    FrameState *state = reinterpret_cast<FrameState *>(sp) - 1;
    auto numVregs = state->method->GetNumVregs();
    return GET_VREG_VALUE(numVregs);
}

size_t EcmaInterpreter::GetJumpSizeAfterCall(const uint8_t *prevPc)
{
    uint8_t op = *prevPc;
    size_t jumpSize;
    switch (op) {
        case (EcmaOpcode::CALLARG0DYN_PREF_V8):
            jumpSize = BytecodeInstruction::Size(BytecodeInstruction::Format::PREF_V8);
            break;
        case (EcmaOpcode::CALLARG1DYN_PREF_V8_V8):
            jumpSize = BytecodeInstruction::Size(BytecodeInstruction::Format::PREF_V8_V8);
            break;
        case (EcmaOpcode::CALLARGS2DYN_PREF_V8_V8_V8):
            jumpSize = BytecodeInstruction::Size(BytecodeInstruction::Format::PREF_V8_V8_V8);
            break;
        case (EcmaOpcode::CALLARGS3DYN_PREF_V8_V8_V8_V8):
            jumpSize = BytecodeInstruction::Size(BytecodeInstruction::Format::PREF_V8_V8_V8_V8);
            break;
        case (EcmaOpcode::CALLIRANGEDYN_PREF_IMM16_V8):
            jumpSize = BytecodeInstruction::Size(BytecodeInstruction::Format::PREF_IMM16_V8);
            break;
        case (EcmaOpcode::CALLITHISRANGEDYN_PREF_IMM16_V8):
            jumpSize = BytecodeInstruction::Size(BytecodeInstruction::Format::PREF_IMM16_V8);
            break;
        default:
            UNREACHABLE();
    }

    return jumpSize;
}

JSTaggedValue EcmaInterpreter::GetRuntimeProfileTypeInfo(TaggedType *sp)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    FrameState *state = reinterpret_cast<FrameState *>(sp) - 1;
    return state->profileTypeInfo;
}

bool EcmaInterpreter::UpdateHotnessCounter(JSThread* thread, TaggedType *sp, JSTaggedValue acc, int32_t offset)
{
    FrameState *state = GET_FRAME(sp);
    auto method = state->method;
    auto hotnessCounter = static_cast<int32_t>(method->GetHotnessCounter());

    hotnessCounter += offset;
    if (UNLIKELY(hotnessCounter <= 0)) {
        if (state->profileTypeInfo == JSTaggedValue::Undefined()) {
            state->acc = acc;
            auto numVregs = method->GetNumVregs();
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            auto thisFunc = JSTaggedValue(sp[numVregs]);
            auto res = SlowRuntimeStub::NotifyInlineCache(
                thread, JSFunction::Cast(thisFunc.GetHeapObject()), method);
            state->profileTypeInfo = res;
            method->SetHotnessCounter(std::numeric_limits<int32_t>::max());
            return true;
        } else {
            hotnessCounter = std::numeric_limits<int32_t>::max();
        }
    }
    method->SetHotnessCounter(static_cast<uint32_t>(hotnessCounter));
    return false;
}
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
}  // namespace panda::ecmascript
#endif  // ECMASCRIPT_INTERPRETER_INTERPRETER_INL_H
