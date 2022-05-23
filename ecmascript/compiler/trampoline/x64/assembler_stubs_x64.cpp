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

#include "assembler_stubs_x64.h"

#include "ecmascript/compiler/assembler/assembler.h"
#include "ecmascript/compiler/common_stubs.h"
#include "ecmascript/compiler/rt_call_signature.h"
#include "ecmascript/ecma_runtime_call_info.h"
#include "ecmascript/frames.h"
#include "ecmascript/js_function.h"
#include "ecmascript/js_method.h"
#include "ecmascript/js_thread.h"
#include "ecmascript/message_string.h"
#include "ecmascript/runtime_call_id.h"
#include "libpandafile/bytecode_instruction-inl.h"

namespace panda::ecmascript::x64 {
#define __ assembler->
void AssemblerStubsX64::CallRuntime(ExtendedAssemblerX64 *assembler)
{
    __ Pushq(rbp);
    __ Movq(rsp, rbp);
    __ Movq(rsp, Operand(rax, JSThread::GlueData::GetLeaveFrameOffset(false)));
    __ Pushq(static_cast<int32_t>(FrameType::LEAVE_FRAME));

    __ Pushq(r10);
    __ Pushq(rdx);
    __ Pushq(rax);

    __ Movq(rbp, rdx);
    // 16: rbp & return address
    __ Addq(16, rdx);

    __ Movq(Operand(rdx, 0), r10);
    __ Movq(Operand(rax, r10, Times8, JSThread::GlueData::GetRTStubEntriesOffset(false)), r10);
    __ Movq(rax, rdi);
    // 8: argc
    __ Movq(Operand(rdx, 8), rsi);
    // 16: argv
    __ Addq(16, rdx);
    __ Callq(r10);

    // 8: skip rax
    __ Addq(8, rsp);
    __ Popq(rdx);
    __ Popq(r10);

    // 8: skip frame type
    __ Addq(8, rsp);
    __ Popq(rbp);
    __ Ret();
}

// uint64_t JSFunctionEntry(uintptr_t glue, uintptr_t prevFp, uint32_t expectedNumArgs,
//     uint32_t actualNumArgs, const JSTaggedType argV[], uintptr_t codeAddr)
// Input:
// %rdi - glue
// %rsi - prevFp
// %rdx - expectedNumArgs
// %ecx - actualNumArgs
// %r8  - argV
// %r9  - codeAddr
void AssemblerStubsX64::JSFunctionEntry(ExtendedAssemblerX64 *assembler)
{
    Register glueReg = rdi;
    Register prevFpReg = rsi;
    Register expectedNumArgsReg = rdx;
    Register actualNumArgsReg = rcx;
    Register argvReg = r8;
    Register codeAddrReg = r9;

    Label lAlign16Bytes;
    Label lCopyExtraAument;
    Label lCopyArguments;
    Label lCopyLoop;
    Label lPopFrame;

    __ PushCppCalleeSaveRegisters();
    __ Pushq(glueReg); // caller save
    // construct the frame
    __ Pushq(rbp);
    __ Movq(rsp, rbp);
    __ Pushq(static_cast<int32_t>(FrameType::OPTIMIZED_FRAME));
    __ Pushq(prevFpReg);

    // 16 bytes align check
    __ Movl(expectedNumArgsReg, r14);
    __ Testb(1, r14);
    __ Jne(&lAlign16Bytes);
    __ Pushq(0); // push zero to align 16 bytes stack

    __ Bind(&lAlign16Bytes);
    // expectedNumArgs > actualNumArgs
    __ Movl(expectedNumArgsReg, rbx); // save expectedNumArgs
    __ Cmpl(actualNumArgsReg, expectedNumArgsReg);
    __ Jbe(&lCopyArguments);
    __ Movl(actualNumArgsReg, rax);
    __ Movl(rbx, expectedNumArgsReg);

    __ Bind(&lCopyExtraAument); // copy undefined value to stack
    __ Pushq(JSTaggedValue::VALUE_UNDEFINED);
    __ Addq(-1, expectedNumArgsReg);
    __ Cmpq(rax, expectedNumArgsReg);
    __ Ja(&lCopyExtraAument);

    __ Bind(&lCopyArguments);
    __ Cmpl(actualNumArgsReg, rbx);
    __ CMovbe(rbx, actualNumArgsReg);
    __ Movl(actualNumArgsReg, rax); // rax -> actualNumArgsReg

    __ Bind(&lCopyLoop);
    __ Movq(Operand(argvReg, rax, Scale::Times8, -8), actualNumArgsReg); // -8 : disp
    __ Pushq(actualNumArgsReg);
    __ Addq(-1, rax);
    __ Jne(&lCopyLoop);

    __ Pushq(r14);
    __ Movq(glueReg, rax); // mov glue to rax
    __ Callq(codeAddrReg); // then call jsFunction
    __ Leaq(Operand(r14, Scale::Times8, 0), actualNumArgsReg); // Note: fixed for 3 extra arguments
    __ Addq(actualNumArgsReg, rsp);
    __ Addq(8, rsp); // 8: skip r14
    __ Testb(1, r14); // stack 16bytes align check
    __ Jne(&lPopFrame);
    __ Addq(8, rsp); // 8: align byte

    __ Bind(&lPopFrame);
    __ Popq(prevFpReg);
    __ Addq(8, rsp); // 8: frame type
    __ Popq(rbp);
    __ Popq(glueReg); // caller restore
    __ PopCppCalleeSaveRegisters(); // callee restore
    __ Movq(prevFpReg, Operand(glueReg, JSThread::GlueData::GetLeaveFrameOffset(false)));
    __ Ret();
}

// uint64_t OptimizedCallOptimized(uintptr_t glue, uint32_t expectedNumArgs,
//                                uint32_t actualNumArgs, uintptr_t codeAddr, uintptr_t argv)
// Input:
// %rdi - glue
// %rsi - expectedNumArgs
// %rdx - actualNumArgs
// %rcx - codeAddr
// %r8  - argv

// sp[0 * 8]  -  argc
// sp[1 * 8]  -  argv[0]
// sp[2 * 8]  -  argv[1]
// .....
// sp[(N -3) * 8] - argv[N - 1]
// Output:
// stack as followsn from high address to lowAdress
//  sp       -      argv[N - 1]
// sp[-8]    -      argv[N -2]
// ...........................
// sp[- 8(N - 1)] - arg[0]
void AssemblerStubsX64::OptimizedCallOptimized(ExtendedAssemblerX64 *assembler)
{
    Register glueReg = rdi;
    Register expectedNumArgsReg = rsi;
    Register actualNumArgsReg = rdx;
    Register codeAddrReg = rcx;
    Register argvReg = r8;

    Label lAlign16Bytes1;
    Label lCopyExtraAument1;
    Label lCopyArguments1;
    Label lCopyLoop1;
    Label lPopFrame1;
    __ Pushq(rbp);
    __ Movq(rsp, rbp);
    __ Pushq(static_cast<int32_t>(FrameType::OPTIMIZED_FRAME));
    // callee save
    __ Pushq(r14);
    __ Pushq(rbx);
    __ Pushq(rax);

    // 16 bytes align check
    __ Movl(expectedNumArgsReg, r14);
    __ Testb(1, r14);
    __ Jne(&lAlign16Bytes1);
    __ Pushq(0);

    __ Bind(&lAlign16Bytes1);
    // expectedNumArgs > actualNumArgs
    __ Movl(expectedNumArgsReg, rbx);
    __ Cmpl(actualNumArgsReg, expectedNumArgsReg); // save expectedNumArgs
    __ Jbe(&lCopyArguments1);
    __ Movl(actualNumArgsReg, rax);
    __ Movl(rbx, expectedNumArgsReg);

    __ Bind(&lCopyExtraAument1); // copy undefined value to stack
    __ Pushq(JSTaggedValue::VALUE_UNDEFINED);
    __ Addq(-1, expectedNumArgsReg);
    __ Cmpq(rax, expectedNumArgsReg);
    __ Ja(&lCopyExtraAument1);

    __ Bind(&lCopyArguments1);
    __ Cmpl(actualNumArgsReg, rbx);
    __ CMovbe(rbx, actualNumArgsReg);
    __ Movl(actualNumArgsReg, rax); // rax = actualNumArgsReg

    __ Bind(&lCopyLoop1);
    __ Movq(Operand(argvReg, rax, Scale::Times8, -8), rbx); // -8: stack index
    __ Pushq(rbx);
    __ Addq(-1, rax);
    __ Jne(&lCopyLoop1);
    __ Pushq(actualNumArgsReg); // actual argc
    __ Movq(glueReg, rax); // mov glue to rax
    __ Callq(codeAddrReg); // then call jsFunction
    __ Leaq(Operand(r14, Scale::Times8, 0), codeAddrReg);
    __ Addq(codeAddrReg, rsp);
    __ Addq(8, rsp); // 8: skip actualNumArgsReg
    __ Testb(1, r14); // stack 16bytes align check
    __ Jne(&lPopFrame1);
    __ Addq(8, rsp); // 8: align byte

    __ Bind(&lPopFrame1);
    __ Addq(8, rsp); // 8: skip rax
    __ Popq(rbx);
    __ Popq(r14);
    __ Addq(8, rsp); // 8: skip frame type
    __ Pop(rbp);
    __ Ret();
}

// uint64_t CallNativeTrampoline(uintptr_t glue, uintptr_t codeAddress, uint32_t argc, ...)
// webkit_jscc calling convention call runtime_id's runtion function(c-abi)
// Input:
// %rax - glue
// stack layout:
// sp + N*8 argvN
// ........
// sp + 24: argv1
// sp + 16: argv0
// sp + 8:  actualArgc
// sp:      codeAddress
// construct Native Leave Frame:
//   +--------------------------+
//   |       argv0              | calltarget , newTarget, this, ....
//   +--------------------------+ ---
//   |       argc               |   ^
//   |--------------------------|  Fixed
//   |       codeAddress        | OptimizedLeaveFrame
//   |--------------------------|   |
//   |       returnAddr         |   |
//   |--------------------------|   |
//   |       callsiteFp         |   |
//   |--------------------------|   |
//   |       frameType          |   v
//   +--------------------------+ ---

// Output:
//  sp - 8 : pc
//  sp - 16: rbp <---------current rbp & current sp
//  current sp - 8:  type
void AssemblerStubsX64::CallNativeTrampoline(ExtendedAssemblerX64 *assembler)
{
    Register glueReg = rax;
    __ Pushq(rbp);
    __ Movq(rsp, rbp);
    __ Movq(rbp, Operand(glueReg, JSThread::GlueData::GetLeaveFrameOffset(false))); // save to thread->leaveFrame_
    __ Pushq(static_cast<int32_t>(FrameType::LEAVE_FRAME));

    // callee save
    __ Pushq(r10);
    __ Pushq(rbx);

    __ Movq(rbp, rdx);
    __ Addq(16, rdx); // 16 : for rbp & return address
    // load native pointer address
    __ Movq(Operand(rdx, 0), r10);  // r10 -> argv[0]
    __ Subq(8, rsp); // 8: align 16 bytes
    __ Subq(sizeof(EcmaRuntimeCallInfo), rsp);

    // construct ecma_runtime_call_info
    // get thread
    Register threadReg = rax;
    __ Subq(JSThread::GetGlueDataOffset(), threadReg);
    __ Movq(threadReg, Operand(rsp, 0));   // thread_
    // get numArgs
    __ Movq(0, rax);
    __ Movl(Operand(rdx, 8), rax); // 8: sp + 8 actualArgc
    __ Subl(3, rax); // 3: size
    __ Movq(rax, Operand(rsp, EcmaRuntimeCallInfo::GetNumArgsOffset())); // actualArgc
    // get gpr data
    __ Movq(rdx, rbx);
    __ Addq(16, rbx); // 16: argv[0]
    __ Movq(rbx, Operand(rsp, EcmaRuntimeCallInfo::GetStackArgsOffset())); // argv0
    __ Movq(0, Operand(rsp, EcmaRuntimeCallInfo::GetDataOffset())); // argv1

    __ Movq(rsp, rdi);
    __ Callq(r10);
    __ Addq(ASM_GLUE_ECMA_RUNTIME_CALLINFO_SIZE, rsp);
    __ Addq(8, rsp); //  8: sp + 8 align 16bytes
    __ Popq(rbx);
    __ Pop(r10);
    __ Addq(8, rsp); // 8: sp + 8
    __ Popq(rbp);
    Register pcReg = rdx;
    __ Popq(pcReg); // load pc
    __ Addq(8, rsp); // 8: skip code address
    __ Pushq(pcReg); // save pc
    __ Ret();
}

// uint64_t JSCallWithArgV(uintptr_t glue, uint32_t argc, JSTaggedType calltarget, uintptr_t argv[])
// c++ calling convention call js function
// Input:
// %rdi - glue
// %rsi - argc
// %rdx - calltarget
// %rcx - argV (calltarget, newtarget, thisObj, ...)
void AssemblerStubsX64::JSCallWithArgv(ExtendedAssemblerX64 *assembler)
{
    Label jsCall;
    Label lJSCallStart;
    Label lNotJSFunction;
    Label lNonCallable;
    Label lJSFunctionCall;
    Label lJSBoundFunction;
    Label lJSProxy;
    Label lCallOptimziedMethod;
    Label lDirectCallCodeEntry;
    Label lCallNativeMethod;
    Label optimizedCallOptimized;
    Label callNativeTrampoline;
    Label lAlign16Bytes2;
    Label lCopyBoundArgument;
    Label lCopyArgument2;
    Label lPushCallTarget;
    Label lCopyBoundArgumentLoop;
    Label lPopFrame2;
    Register glueReg = rdi;
    Register callTarget = rdx;
    Register argvReg = rcx;
    __ Movq(callTarget, Operand(argvReg, 0));
    __ Movq(callTarget, rax);
    __ Jmp(&lJSCallStart);
    __ Bind(&jsCall);
    {
        __ Movq(glueReg, rdi);
        glueReg = rdi;
        __ Movq(Operand(rsp, 16), rax); // 16: get jsFunc
    }
    __ Bind(&lJSCallStart);
    Register jsFuncReg = rax;
    {
        __ Movabs(JSTaggedValue::TAG_INT, rdx); // IsTaggedInt
        __ And(jsFuncReg, rdx);
        __ Cmp(0x0, rdx);
        __ Jne(&lNonCallable);
        __ Cmp(0x0, jsFuncReg); // IsHole
        __ Je(&lNonCallable);
        __ Movabs(JSTaggedValue::TAG_SPECIAL_VALUE, rdx);
        __ And(jsFuncReg, rdx);  // IsSpecial
        __ Cmp(0x0, rdx);
        __ Jne(&lNonCallable);

        __ Movq(jsFuncReg, r8); // save jsFunc
        __ Movq(Operand(jsFuncReg, 0), rax); // get jsHclass
        Register jsHclassReg = rax;
        __ Movl(Operand(jsHclassReg, JSHClass::BIT_FIELD_OFFSET), rax);
        __ Btl(JSHClass::BIT_FIELD1_OFFSET, rax); // IsCallable
        __ Jnb(&lNonCallable);

        __ Shll(24, rax); // objectType << 24
        __ Leal(Operand(rax, -50331649), rdx); // -50331649: disp
        __ Cmpl(0x9FFFFFF, rdx); // 0x9FFFFFF: is Jsfunction
        __ Jae(&lNotJSFunction); // objecttype in (0x04 ~ 0x0c)
        __ Jmp(&lJSFunctionCall);
    }

    __ Bind(&lNotJSFunction);
    {
        __ Cmpl(0xd000000, rax); // 0xd000000: IsJsBoundFunction
        __ Je(&lJSBoundFunction);
        __ Cmpl(0x4f000000, rax); // 0x4f000000: IsJsProxy
        __ Je(&lJSProxy);
    }

    __ Bind(&lNonCallable);
    {
        __ Pushq(rbp);
        __ Movq(rsp, rbp); // set frame pointer
        __ Pushq(static_cast<int32_t>(FrameType::OPTIMIZED_FRAME)); // set frame type
        __ Movq(MessageString::Message_NonCallable, rax);
        __ Pushq(rax); // message id
        __ Pushq(1); // argc
        __ Pushq(EcmaRuntimeCallerId::RUNTIME_ID_ThrowTypeError); // runtime id
        __ Movq(rcx, rax); // glue
        __ Movq(kungfu::RuntimeStubCSigns::ID_CallRuntime, r10);
        __ Movq(Operand(rax, r10, Times8, JSThread::GlueData::GetRTStubEntriesOffset(false)), r10);
        __ Callq(r10); // call CallRuntime
        __ Movabs(JSTaggedValue::VALUE_EXCEPTION, rax); // return exception
        __ Addq(32, rsp); // 32: sp + 32 argv
        __ Pop(rbp);
        __ Ret();
    }

    __ Bind(&lJSFunctionCall);
    jsFuncReg = r8;
    Register argc = rdx;
    Register methodCallField = rax;
    Register jsMethod = rsi;
    {
        __ Mov(Operand(jsFuncReg, JSFunctionBase::METHOD_OFFSET), jsMethod); // get method
        __ Movl(Operand(rsp, 8), argc); // 8: sp + 8 actual argc
        __ Mov(Operand(jsMethod, JSMethod::GetCallFieldOffset(false)), methodCallField); // get call field
        __ Btq(JSMethod::IsNativeBit::SIZE, methodCallField); // is native
        __ Jb(&lCallNativeMethod);
        __ Btq(JSMethod::IsAotCodeBit::SIZE, methodCallField); // is aot
        __ Jb(&lCallOptimziedMethod);
        __ Int3();
        __ Ret();
    }

    __ Bind(&lCallOptimziedMethod);
    Register codeAddrReg = rcx;
    Register expectedNumArgsReg = rsi;
    {
        __ Mov(Operand(jsFuncReg, JSFunctionBase::CODE_ENTRY_OFFSET), codeAddrReg); // get codeAddress
        __ Shr(JSMethod::NumArgsBits::START_BIT, methodCallField);
        __ Andl(((1LU <<  JSMethod::NumArgsBits::SIZE) - 1), methodCallField);
        __ Addl(NUM_MANDATORY_JSFUNC_ARGS, methodCallField); // add mandatory argument
        __ Mov(methodCallField, expectedNumArgsReg); // expected numbers
        __ Movq(rsp, r8);
        argvReg = r8;
        __ Addq(16, argvReg); // 16: sp + 8 argv
        __ Cmpl(expectedNumArgsReg, argc); // expectedNumArgs <= actualNumArgs
        __ Jg(&lDirectCallCodeEntry);
        __ Jmp(&optimizedCallOptimized);
    }
    __ Bind(&optimizedCallOptimized);
    OptimizedCallOptimized(assembler);

    __ Bind(&lDirectCallCodeEntry);
    {
        __ Movq(glueReg, rax); // rax = glue
        __ Jmp(codeAddrReg);
    }

    __ Bind(&lCallNativeMethod);
    {
        __ Pop(rax); // pc
        __ Mov(Operand(jsFuncReg, JSFunctionBase::METHOD_OFFSET), jsMethod); // Get Method
        Register nativePointer = rsi;
        __ Mov(Operand(jsMethod, JSMethod::GetBytecodeArrayOffset(false)), nativePointer); // get native pointer
        __ Push(nativePointer); // native code address
        __ Push(rax); // pc
        __ Movq(glueReg, rax);
        __ Jmp(&callNativeTrampoline);
    }

    __ Bind(&callNativeTrampoline);
    CallNativeTrampoline(assembler);

    __ Bind(&lJSBoundFunction);
    {
        __ Pushq(rbp);
        __ Movq(rsp, rbp);
        __ Pushq(static_cast<int32_t>(FrameType::OPTIMIZED_FRAME));
        __ Pushq(r10); // callee save
        __ Movq(rsp, rdx);
        __ Addq(32, rdx); // 32: sp + 32 argv
        __ Mov(Operand(rdx, 0), rax); // get origin argc
        __ Movq(rax, r10);
        // get bound target
        __ Mov(Operand(r8, JSBoundFunction::BOUND_ARGUMENTS_OFFSET), rcx);
        // get bound length
        __ Mov(Operand(rcx, TaggedArray::LENGTH_OFFSET), rcx);
        __ Addq(rcx, r10);

        // 16 bytes align check
        __ Testb(1, r10);
        __ Jne(&lAlign16Bytes2);
        __ PushAlignBytes(); // push zero to align 16 bytes stack
    }

    __ Bind(&lAlign16Bytes2);
    {
        __ Subq(NUM_MANDATORY_JSFUNC_ARGS, rax);
        __ Cmp(0, rax);
        __ Je(&lCopyBoundArgument);
    }

    __ Bind(&lCopyArgument2);
    {
        __ Movq(Operand(rdx, rax, Scale::Times8, 24), rcx); // 24: slot size
        __ Pushq(rcx);
        __ Addq(-1, rax);
        __ Jne(&lCopyArgument2);

        // get bound target
        __ Mov(Operand(r8, JSBoundFunction::BOUND_ARGUMENTS_OFFSET), rdx);
        // get bound length
        __ Mov(Operand(rdx, TaggedArray::LENGTH_OFFSET), rax);
        __ Addq(TaggedArray::DATA_OFFSET, rdx);
    }
    __ Bind(&lCopyBoundArgument);
    {
        __ Cmp(0, rax);
        __ Je(&lPushCallTarget);
    }
    __ Bind(&lCopyBoundArgumentLoop);
    {
        __ Addq(-1, rax);
        __ Movq(Operand(rdx, rax, Scale::Times8, 0), rcx);
        __ Pushq(rcx);
        __ Jne(&lCopyBoundArgumentLoop);
    }
    __ Bind(&lPushCallTarget);
    {
        __ Mov(Operand(r8, JSBoundFunction::BOUND_THIS_OFFSET), rax); // thisObj
        __ Pushq(rax);
        __ Pushq(JSTaggedValue::VALUE_UNDEFINED); // newTarget
        __ Mov(Operand(r8, JSBoundFunction::BOUND_TARGET_OFFSET), rax); // callTarget
        __ Pushq(rax);
        __ Pushq(r10); // push actual arguments
        __ Movq(rdi, rax);
        __ Callq(&jsCall); // call JSCall
        __ Leaq(Operand(r10, Scale::Times8, 8), rcx); // 8: offset
        __ Addq(rcx, rsp);
        __ Testb(1, r10);  // stack 16bytes align check
        __ Jne(&lPopFrame2);
        __ Addq(8, rsp); // 8: sp + 8
    }

    __ Bind(&lPopFrame2);
    {
        __ Pop(r10);
        __ Addq(8, rsp); // 8: sp + 8
        __ Pop(rbp);
        __ Ret();
    }
    __ Bind(&lJSProxy);
    __ Movq(rsp, rcx);
    __ Addq(8, rcx); // 8: sp + 8
    __ Mov(Operand(rcx, 0), rsi); // get origin argc
    __ Movq(r8, rdx);
    __ Addq(8, rcx); // 8: sp + 8 argv
    __ Movq(kungfu::CommonStubCSigns::JsProxyCallInternal, r9);
    __ Movq(Operand(rdi, r9, Scale::Times8, JSThread::GlueData::GetCOStubEntriesOffset(false)), r8);
    __ Jmp(r8);
    __ Ret();
}

// uint64_t JSCall(uintptr_t glue, uint32_t argc, JSTaggedType calltarget, JSTaggedType new, JSTaggedType this, ...)
// webkit_jscc calling convention call js function()
// Input:
// %rax - glue
// stack layout:
// sp + N*8 argvN
// ........
// sp + 24: argc
// sp + 16: this
// sp + 8:  new
// sp:      jsfunc
//   +--------------------------+
//   |       ...                |
//   +--------------------------+
//   |       arg0               |
//   +--------------------------+
//   |       this               |
//   +--------------------------+
//   |       new                |
//   +--------------------------+ ---
//   |       jsfunction         |   ^
//   |--------------------------|  Fixed
//   |       argc               | OptimizedFrame
//   |--------------------------|   |
//   |       returnAddr         |   |
//   |--------------------------|   |
//   |       callsiteFp         |   |
//   |--------------------------|   |
//   |       frameType          |   v
//   +--------------------------+ ---
void AssemblerStubsX64::JSCall(ExtendedAssemblerX64 *assembler)
{
    Label jsCall;
    Label lJSCallStart;
    Label lNotJSFunction;
    Label lNonCallable;
    Label lJSFunctionCall;
    Label lJSBoundFunction;
    Label lJSProxy;
    Label lCallOptimziedMethod;
    Label lDirectCallCodeEntry;
    Label lCallNativeMethod;
    Label optimizedCallOptimized;
    Label callNativeTrampoline;
    Label lAlign16Bytes2;
    Label lCopyBoundArgument;
    Label lCopyArgument2;
    Label lPushCallTarget;
    Label lCopyBoundArgumentLoop;
    Label lPopFrame2;
    Register glueReg = rax;
    __ Bind(&jsCall);
    {
        __ Movq(glueReg, rdi);
        glueReg = rdi;
        __ Movq(Operand(rsp, 16), rax); // 16: sp + 16 get jsFunc
    }
    __ Bind(&lJSCallStart);
    Register jsFuncReg = rax;
    {
        __ Movabs(JSTaggedValue::TAG_INT, rdx); // IsTaggedInt
        __ And(jsFuncReg, rdx);
        __ Cmp(0x0, rdx);
        __ Jne(&lNonCallable);
        __ Cmp(0x0, jsFuncReg); // IsHole
        __ Je(&lNonCallable);
        __ Movabs(JSTaggedValue::TAG_SPECIAL_VALUE, rdx);
        __ And(jsFuncReg, rdx);  // IsSpecial
        __ Cmp(0x0, rdx);
        __ Jne(&lNonCallable);

        __ Movq(jsFuncReg, r8); // save jsFunc
        __ Movq(Operand(jsFuncReg, 0), rax); // get jsHclass
        Register jsHclassReg = rax;
        __ Movl(Operand(jsHclassReg, JSHClass::BIT_FIELD_OFFSET), rax);
        __ Btl(JSHClass::BIT_FIELD1_OFFSET, rax); // IsCallable
        __ Jnb(&lNonCallable);

        __ Shll(24, rax); // objectType << 24
        __ Leal(Operand(rax, -50331649), rdx); // -50331649: disp
        __ Cmpl(0x9FFFFFF, rdx); // 0x9FFFFFF: is jsfunction
        __ Jae(&lNotJSFunction); // objecttype in (0x04 ~ 0x0c)
        __ Jmp(&lJSFunctionCall);
    }

    __ Bind(&lNotJSFunction);
    {
        __ Cmpl(0xd000000, rax); // 0xd000000: IsJsBoundFunction
        __ Je(&lJSBoundFunction);
        __ Cmpl(0x4f000000, rax); // 0x4f000000: IsJsProxy
        __ Je(&lJSProxy);
    }

    __ Bind(&lNonCallable);
    {
        __ Pushq(rbp);
        __ Movq(rsp, rbp); // set frame pointer
        __ Pushq(static_cast<int32_t>(FrameType::OPTIMIZED_FRAME)); // set frame type
        __ Movq(MessageString::Message_NonCallable, rax);
        __ Pushq(rax); // message id
        __ Pushq(1); // argc
        __ Pushq(EcmaRuntimeCallerId::RUNTIME_ID_ThrowTypeError); // runtime id
        __ Movq(rcx, rax); // glue
        __ Movq(kungfu::RuntimeStubCSigns::ID_CallRuntime, r10);
        __ Movq(Operand(rax, r10, Times8, JSThread::GlueData::GetRTStubEntriesOffset(false)), r10);
        __ Callq(r10); // call CallRuntime
        __ Movabs(JSTaggedValue::VALUE_EXCEPTION, rax); // return exception
        __ Addq(32, rsp); // 32: sp + 32 argv
        __ Pop(rbp);
        __ Ret();
    }

    __ Bind(&lJSFunctionCall);
    jsFuncReg = r8;
    Register argc = rdx;
    Register methodCallField = rax;
    Register jsMethod = rsi;
    {
        __ Mov(Operand(jsFuncReg, JSFunctionBase::METHOD_OFFSET), jsMethod); // get method
        __ Movl(Operand(rsp, 8), argc); // 8: sp + 8 actual argc
        __ Mov(Operand(jsMethod, JSMethod::GetCallFieldOffset(false)), methodCallField); // get call field
        __ Btq(JSMethod::IsNativeBit::SIZE, methodCallField); // is native
        __ Jb(&lCallNativeMethod);
        __ Btq(JSMethod::IsAotCodeBit::SIZE, methodCallField); // is aot
        __ Jb(&lCallOptimziedMethod);
        __ Int3();
        __ Ret();
    }

    __ Bind(&lCallOptimziedMethod);
    Register codeAddrReg = rcx;
    Register expectedNumArgsReg = rsi;
    {
        __ Mov(Operand(jsFuncReg, JSFunctionBase::CODE_ENTRY_OFFSET), codeAddrReg); // get codeAddress
        __ Shr(JSMethod::NumArgsBits::START_BIT, methodCallField);
        __ Andl(((1LU <<  JSMethod::NumArgsBits::SIZE) - 1), methodCallField);
        __ Addl(NUM_MANDATORY_JSFUNC_ARGS, methodCallField); // add mandatory argument
        __ Mov(methodCallField, expectedNumArgsReg); // expected numbers
        __ Movq(rsp, r8);
        Register argvReg = r8;
        __ Addq(16, argvReg); // 16: sp + 16 argv
        __ Cmpl(expectedNumArgsReg, argc); // expectedNumArgs <= actualNumArgs
        __ Jg(&lDirectCallCodeEntry);
        __ Jmp(&optimizedCallOptimized);
    }
    __ Bind(&optimizedCallOptimized);
    OptimizedCallOptimized(assembler);

    __ Bind(&lDirectCallCodeEntry);
    {
        __ Movq(glueReg, rax); // rax = glue
        __ Jmp(codeAddrReg);
    }

    __ Bind(&lCallNativeMethod);
    {
        __ Pop(rax); // pc
        __ Mov(Operand(jsFuncReg, JSFunctionBase::METHOD_OFFSET), jsMethod); // Get Method
        Register nativePointer = rsi;
        __ Mov(Operand(jsMethod, JSMethod::GetBytecodeArrayOffset(false)), nativePointer); // get native pointer
        __ Push(nativePointer); // native code address
        __ Push(rax); // pc
        __ Movq(glueReg, rax);
        __ Jmp(&callNativeTrampoline);
    }

    __ Bind(&callNativeTrampoline);
    CallNativeTrampoline(assembler);

    __ Bind(&lJSBoundFunction);
    {
        __ Pushq(rbp);
        __ Movq(rsp, rbp);
        __ Pushq(static_cast<int32_t>(FrameType::OPTIMIZED_FRAME));
        __ Pushq(r10); // callee save
        __ Movq(rsp, rdx);
        __ Addq(32, rdx); // 32: sp + 32 argv
        __ Mov(Operand(rdx, 0), rax); // get origin argc
        __ Movq(rax, r10);
        // get bound target
        __ Mov(Operand(r8, JSBoundFunction::BOUND_ARGUMENTS_OFFSET), rcx);
        // get bound length
        __ Mov(Operand(rcx, TaggedArray::LENGTH_OFFSET), rcx);
        __ Addq(rcx, r10);

        // 16 bytes align check
        __ Testb(1, r10);
        __ Jne(&lAlign16Bytes2);
        __ PushAlignBytes(); // push zero to align 16 bytes stack
    }

    __ Bind(&lAlign16Bytes2);
    {
        __ Subq(NUM_MANDATORY_JSFUNC_ARGS, rax);
        __ Cmp(0, rax);
        __ Je(&lCopyBoundArgument);
    }

    __ Bind(&lCopyArgument2);
    {
        __ Movq(Operand(rdx, rax, Scale::Times8, 24), rcx); // 24 : disp
        __ Pushq(rcx);
        __ Addq(-1, rax);
        __ Jne(&lCopyArgument2);

        // get bound target
        __ Mov(Operand(r8, JSBoundFunction::BOUND_ARGUMENTS_OFFSET), rdx);
        // get bound length
        __ Mov(Operand(rdx, TaggedArray::LENGTH_OFFSET), rax);
        __ Addq(TaggedArray::DATA_OFFSET, rdx);
    }
    __ Bind(&lCopyBoundArgument);
    {
        __ Cmp(0, rax);
        __ Je(&lPushCallTarget);
    }
    __ Bind(&lCopyBoundArgumentLoop);
    {
        __ Addq(-1, rax);
        __ Movq(Operand(rdx, rax, Scale::Times8, 0), rcx);
        __ Pushq(rcx);
        __ Jne(&lCopyBoundArgumentLoop);
    }
    __ Bind(&lPushCallTarget);
    {
        __ Mov(Operand(r8, JSBoundFunction::BOUND_THIS_OFFSET), rax); // thisObj
        __ Pushq(rax);
        __ Pushq(JSTaggedValue::VALUE_UNDEFINED); // newTarget
        __ Mov(Operand(r8, JSBoundFunction::BOUND_TARGET_OFFSET), rax); // callTarget
        __ Pushq(rax);
        __ Pushq(r10); // push actual arguments
        __ Movq(rdi, rax);
        __ Callq(&jsCall); // call JSCall
        __ Leaq(Operand(r10, Scale::Times8, 8), rcx); // 8: disp
        __ Addq(rcx, rsp);
        __ Testb(1, r10);  // stack 16bytes align check
        __ Jne(&lPopFrame2);
        __ Addq(8, rsp); // 8: align byte
    }

    __ Bind(&lPopFrame2);
    {
        __ Pop(r10);
        __ Addq(8, rsp); // 8: sp + 8
        __ Pop(rbp);
        __ Ret();
    }
    __ Bind(&lJSProxy);
    __ Movq(rsp, rcx);
    __ Addq(8, rcx); // 8: sp + 8
    __ Mov(Operand(rcx, 0), rsi); // get origin argc
    __ Movq(r8, rdx); // 8: slot size
    __ Addq(8, rcx); // argv
    __ Movq(kungfu::CommonStubCSigns::JsProxyCallInternal, r9);
    __ Movq(Operand(rdi, r9, Scale::Times8, JSThread::GlueData::GetCOStubEntriesOffset(false)), r8);
    __ Jmp(r8);
    __ Ret();
}


// uint64_t CallRuntimeWithArgv(uintptr_t glue, uint64_t runtime_id, uint64_t argc, uintptr_t argv)
// cc calling convention call runtime_id's runtion function(c-abi)
// JSTaggedType (*)(uintptr_t argGlue, uint64_t argc, JSTaggedType argv[])
// Input:
// %rdi - glue
// %rsi - runtime_id
// %edx - argc
// %rcx - argv
// stack layout:
//   +--------------------------+
//   |       return addr        |
//   +--------------------------+

// %r8  - argV
// %r9  - codeAddr

// Output:
// construct Leave Frame:
//   +--------------------------+
//   |       returnAddr         |
//   +--------------------------+
//   |       argv[]             |
//   +--------------------------+ ---
//   |       argc               |   ^
//   |--------------------------|  Fixed
//   |       RuntimeId          | OptimizedLeaveFrame
//   |--------------------------|   |
//   |       returnAddr         |   |
//   |--------------------------|   |
//   |       callsiteFp         |   |
//   |--------------------------|   |
//   |       frameType          |   v
//   +--------------------------+ ---
void AssemblerStubsX64::CallRuntimeWithArgv(ExtendedAssemblerX64 *assembler)
{
    Register glueReg = rdi;
    Register runtimeIdReg = rsi;
    Register argcReg = rdx;
    Register argvReg = rcx;

    __ Movq(rsp, r8);
    Register returnAddrReg = r9;
    __ Movq(Operand(rsp, 0), returnAddrReg);
    __ Pushq(argvReg); // argv[]
    __ Pushq(argcReg); // argc
    __ Pushq(runtimeIdReg); // runtime_id
    __ Pushq(returnAddrReg); // returnAddr

    // construct leave frame
    __ Pushq(rbp);
    __ Movq(rsp, rbp); // set frame pointer
    __ Movq(rbp, Operand(glueReg, JSThread::GlueData::GetLeaveFrameOffset(false))); // save to thread->leaveFrame_
    __ Pushq(static_cast<int32_t>(FrameType::LEAVE_FRAME_WITH_ARGV));

    __ Movq(Operand(glueReg, runtimeIdReg, Scale::Times8, JSThread::GlueData::GetRTStubEntriesOffset(false)), r9);
    __ Movq(argcReg, rsi); // argc
    __ Movq(argvReg, rdx); // argv
    __ Pushq(r8);
    __ Callq(r9);
    __ Popq(r8);
    __ Addq(8, rsp); // 8: skip type
    __ Popq(rbp);
    __ Movq(r8, rsp);
    __ Ret();
}

// Generate code for Entering asm interpreter
// c++ calling convention
// Input:
// %rdi - glue
// %rsi - argc
// %rdx - argv(<callTarget, newTarget, this> are at the beginning of argv)
void AssemblerStubsX64::AsmInterpreterEntry(ExtendedAssemblerX64 *assembler)
{
    __ PushCppCalleeSaveRegisters();
    __ Pushq(rdi);  // caller save register

    __ Pushq(rbp);
    // construct asm interpreter entry frame
    __ Movq(rsp, rbp);
    __ Pushq(static_cast<int64_t>(FrameType::ASM_INTERPRETER_ENTRY_FRAME));
    __ Movq(Operand(rdi, JSThread::GlueData::GetLeaveFrameOffset(false)), rbx);
    __ Pushq(rbx);  // prev managed fp is leave frame or nullptr(the first frame)
    __ Pushq(0);    // pc

    __ Movq(kungfu::RuntimeStubCSigns::ID_JSCallDispatch, r12);
    __ Movq(Operand(rdi, r12, Scale::Times8, JSThread::GlueData::GetRTStubEntriesOffset(false)), r11);
    __ Callq(r11);

    __ Addq(8, rsp);  // 8: skip pc
    __ Popq(rbx);
    __ Addq(8, rsp);  // 8: skip frame type
    __ Popq(rbp);
    __ Popq(rdi);
    __ Movq(rbx, Operand(rdi, JSThread::GlueData::GetLeaveFrameOffset(false)));
    __ PopCppCalleeSaveRegisters();
    __ Ret();
}

// Input:
// glueRegister   - %rdi
// argcRegister   - %rsi
// argvRegister   - %rdx(<callTarget, newTarget, this> are at the beginning of argv)
// prevSpRegister - %rbp
void AssemblerStubsX64::JSCallDispatch(ExtendedAssemblerX64 *assembler)
{
    Label notJSFunction;
    Label callNativeEntry;
    Label callJSFunctionEntry;
    Label pushArgsSlowPath;
    Label callJSProxyEntry;
    Label notCallable;
    Register glueRegister = rdi;
    Register argcRegister = rsi;
    Register argvRegister = rdx;
    Register prevSpRegister = rbp;

    Register callTargetRegister = r9;
    Register methodRegister = rcx;
    Register bitFieldRegister = r12;
    Register tempRegister = r11;  // can not be used to store any variable
    __ Movq(Operand(rdx, 0), callTargetRegister);
    __ Movq(Operand(callTargetRegister, 0), tempRegister);  // hclass
    __ Movq(Operand(tempRegister, JSHClass::BIT_FIELD_OFFSET), bitFieldRegister);
    __ Cmpb(static_cast<int32_t>(JSType::JS_FUNCTION_BEGIN), bitFieldRegister);
    __ Jb(&notJSFunction);
    __ Cmpb(static_cast<int32_t>(JSType::JS_FUNCTION_END), bitFieldRegister);
    __ Jbe(&callJSFunctionEntry);
    __ Bind(&notJSFunction);
    {
        __ Testq(static_cast<int64_t>(1ULL << JSHClass::CallableBit::START_BIT), bitFieldRegister);
        __ Jz(&notCallable);
        __ Cmpb(static_cast<int32_t>(JSType::JS_PROXY), bitFieldRegister);
        __ Je(&callJSProxyEntry);
        // bound function branch, default native
        __ Movq(Operand(callTargetRegister, JSFunctionBase::METHOD_OFFSET), methodRegister);
        __ Jmp(&callNativeEntry);
    }
    __ Bind(&callNativeEntry);
    CallNativeEntry(assembler);
    __ Bind(&callJSProxyEntry);
    {
        __ Movq(Operand(callTargetRegister, JSProxy::METHOD_OFFSET), methodRegister);
        __ Jmp(&callNativeEntry);
    }
    __ Bind(&callJSFunctionEntry);
    {
        __ Movq(Operand(callTargetRegister, JSFunctionBase::METHOD_OFFSET), methodRegister);
        Register callFieldRegister = r14;
        __ Movq(Operand(methodRegister, JSMethod::GetCallFieldOffset(false)), callFieldRegister);
        __ Btq(JSMethod::IsNativeBit::START_BIT, callFieldRegister);
        __ Jb(&callNativeEntry);
        Register declaredNumArgsRegister = r11;
        GetDeclaredNumArgsFromCallField(assembler, callFieldRegister, declaredNumArgsRegister);
        __ Cmpq(declaredNumArgsRegister, argcRegister);
        __ Jne(&pushArgsSlowPath);
        // fast path
        Register fpRegister = r10;
        __ Movq(rsp, fpRegister);
        PushArgsFastPath(assembler, glueRegister, argcRegister, argvRegister, callTargetRegister, methodRegister,
            prevSpRegister, fpRegister, callFieldRegister);
        __ Bind(&pushArgsSlowPath);
        PushArgsSlowPath(assembler, glueRegister, declaredNumArgsRegister, argcRegister, argvRegister,
            callTargetRegister, methodRegister, prevSpRegister, callFieldRegister);
    }
    __ Bind(&notCallable);
    {
        __ Movq(glueRegister, rax);  // glue
        __ Pushq(0);                 // argc
        Register runtimeIdRegister = r12;
        __ Movq(kungfu::RuntimeStubCSigns::ID_ThrowNotCallableException, runtimeIdRegister);
        __ Pushq(runtimeIdRegister);  // runtimeId
        Register trampolineIdRegister = r12;
        Register trampolineRegister = r10;
        __ Movq(kungfu::RuntimeStubCSigns::ID_CallRuntime, trampolineIdRegister);
        __ Movq(Operand(rax, trampolineIdRegister, Times8, JSThread::GlueData::GetRTStubEntriesOffset(false)),
            trampolineRegister);
        __ Callq(trampolineRegister);
        __ Addq(16, rsp);  // 16: skip argc and runtime_id
        __ Ret();
    }
}

// Input:
// glueRegister       - %rdi
// argcRegister       - %rsi
// argvRegister       - %rdx(<callTarget, newTarget, this> are at the beginning of argv)
// callTargetRegister - %r9
// methodRegister     - %rcx
// prevSpRegister     - %rbp
// fpRegister         - %r10
// callFieldRegister  - %r14
void AssemblerStubsX64::PushArgsFastPath(ExtendedAssemblerX64 *assembler, Register glueRegister,
    Register argcRegister, Register argvRegister, Register callTargetRegister, Register methodRegister,
    Register prevSpRegister, Register fpRegister, Register callFieldRegister)
{
    Label pushCallThis;
    Label pushNewTarget;
    Label pushCallTarget;
    Label pushVregs;
    Label pushFrameState;
    Label alignedJSCallEntry;
    Label returnOfCallJSFunction;

    __ Cmpq(0, argcRegister);
    __ Jbe(&pushCallThis);  // skip push args
    Register argvOnlyHaveArgsRegister = r12;
    __ Leaq(Operand(argvRegister, BuiltinFrame::RESERVED_CALL_ARGCOUNT * JSTaggedValue::TaggedTypeSize()),
        argvOnlyHaveArgsRegister);
    Register tempRegister = r11;
    __ PushArgsWithArgv(argcRegister, argvOnlyHaveArgsRegister, tempRegister);  // args

    __ Bind(&pushCallThis);
    __ Testb(CALL_TYPE_MASK, callFieldRegister);
    __ Jz(&pushVregs);
    __ Testq(JSMethod::HaveThisBit::Mask(), callFieldRegister);
    __ Jz(&pushNewTarget);
    __ Movq(Operand(argvRegister, 16), tempRegister);  // 16: skip callTarget, newTarget
    __ Pushq(tempRegister);  // this

    __ Bind(&pushNewTarget);
    __ Testq(JSMethod::HaveNewTargetBit::Mask(), callFieldRegister);
    __ Jz(&pushCallTarget);
    __ Movq(Operand(argvRegister, 8), tempRegister);  // 8: skip callTarget
    __ Pushq(tempRegister);  // newTarget

    __ Bind(&pushCallTarget);
    __ Testq(JSMethod::HaveFuncBit::Mask(), callFieldRegister);
    __ Jz(&pushVregs);
    __ Pushq(callTargetRegister);  // callTarget

    __ Bind(&pushVregs);
    {
        Register numVregsRegister = r11;
        GetNumVregsFromCallField(assembler, callFieldRegister, numVregsRegister);
        __ Cmpq(0, numVregsRegister);
        __ Jz(&pushFrameState);
        PushUndefinedWithArgc(assembler, numVregsRegister);
    }

    __ Bind(&pushFrameState);
    Register newSpRegister = r8;
    __ Movq(rsp, newSpRegister);

    StackOverflowCheck(assembler);

    Register pcRegister = r12;  // reuse r12
    PushFrameState(assembler, prevSpRegister, fpRegister, callTargetRegister, methodRegister, pcRegister, tempRegister);

    // align 16 bytes
    __ Testb(15, rsp);  // 15: 0x1111
    __ Jnz(&alignedJSCallEntry);
    __ PushAlignBytes();
    __ Bind(&alignedJSCallEntry);
    {
        __ Pushq(newSpRegister);  // caller save newSp register to restore rsp after call
        // prepare call entry
        __ Movq(glueRegister, r13);  // %r13 - glue
        __ Movq(newSpRegister, rbp); // %rbp - sp
                                     // %r12 - pc
        __ Movq(Operand(callTargetRegister, JSFunction::CONSTANT_POOL_OFFSET), rbx);       // rbx - constantpool
        __ Movq(Operand(callTargetRegister, JSFunction::PROFILE_TYPE_INFO_OFFSET), r14);   // r14 - profileTypeInfo
        __ Movq(JSTaggedValue::Hole().GetRawData(), rsi);                                  // rsi - acc
        __ Movzwq(Operand(methodRegister, JSMethod::GetHotnessCounterOffset(false)), rdi); // rdi - hotnessCounter

        // call the first bytecode handler
        __ Movzbq(Operand(pcRegister, 0), rax);
        __ Movq(Operand(r13, rax, Times8, JSThread::GlueData::GetBCStubEntriesOffset(false)), r11);
        __ Callq(r11);
        __ Jmp(&returnOfCallJSFunction);
    }

    __ Bind(&returnOfCallJSFunction);
    {
        Register frameStateBaseRegister = r11;
        __ Popq(frameStateBaseRegister);
        __ Subq(sizeof(AsmInterpretedFrame), frameStateBaseRegister);
        __ Movq(Operand(frameStateBaseRegister, AsmInterpretedFrame::GetAccOffset(false)), rax);  // return value
        __ Movq(Operand(frameStateBaseRegister, AsmInterpretedFrame::GetFpOffset(false)), rsp);   // resume rsp
        __ Ret();
    }
}

// Input:
// glueRegister       - %rdi
// declaredNumArgsRegister - %r11
// argcRegister       - %rsi
// argvRegister       - %rdx(<callTarget, newTarget, this> are at the beginning of argv)
// callTargetRegister - %r9
// methodRegister     - %rcx
// prevSpRegister     - %rbp
// callFieldRegister  - %r14
void AssemblerStubsX64::PushArgsSlowPath(ExtendedAssemblerX64 *assembler, Register glueRegister,
    Register declaredNumArgsRegister, Register argcRegister, Register argvRegister, Register callTargetRegister,
    Register methodRegister, Register prevSpRegister, Register callFieldRegister)
{
    Label jumpToFastPath;
    Label haveExtra;
    Label pushUndefined;
    Register fpRegister = r10;
    Register tempRegister = r12;
    __ Movq(rsp, fpRegister);
    __ Testq(JSMethod::HaveExtraBit::Mask(), callFieldRegister);
    __ Jnz(&haveExtra);
    __ Movq(declaredNumArgsRegister, tempRegister);
    __ Subq(argcRegister, declaredNumArgsRegister);
    __ Cmpq(0, declaredNumArgsRegister);
    __ Jg(&pushUndefined);
    __ Movq(tempRegister, argcRegister);  // std::min(declare, actual)
    __ Jmp(&jumpToFastPath);
    // fall through
    __ Bind(&haveExtra);
    {
        __ Pushq(argcRegister);
        __ Subq(argcRegister, declaredNumArgsRegister);
        __ Cmpq(0, declaredNumArgsRegister);
        __ Jle(&jumpToFastPath);
        // fall through
    }
    __ Bind(&pushUndefined);
    {
        PushUndefinedWithArgc(assembler, declaredNumArgsRegister);
        // fall through
    }
    __ Bind(&jumpToFastPath);
    PushArgsFastPath(assembler, glueRegister, argcRegister, argvRegister, callTargetRegister, methodRegister,
        prevSpRegister, fpRegister, callFieldRegister);
}

void AssemblerStubsX64::PushFrameState(ExtendedAssemblerX64 *assembler, Register prevSpRegister, Register fpRegister,
    Register callTargetRegister, Register methodRegister, Register pcRegister, Register operatorRegister)
{
    __ Pushq(static_cast<int32_t>(FrameType::ASM_INTERPRETER_FRAME));  // frame type
    __ Pushq(prevSpRegister);                                      // prevSp
    __ Movq(Operand(methodRegister, JSMethod::GetBytecodeArrayOffset(false)), pcRegister);
    __ Pushq(pcRegister);                                          // pc
    __ Pushq(fpRegister);                                          // fp
    __ Pushq(0);                                                   // jumpSizeAfterCall
    __ Movq(Operand(callTargetRegister, JSFunction::LEXICAL_ENV_OFFSET), operatorRegister);
    __ Pushq(operatorRegister);                                    // env
    __ Pushq(JSTaggedValue::Hole().GetRawData());                  // acc
    __ Pushq(callTargetRegister);                                  // callTarget
}

void AssemblerStubsX64::GlueToThread(ExtendedAssemblerX64 *assembler, Register glueRegister, Register threadRegister)
{
    if (glueRegister != threadRegister) {
        __ Movq(glueRegister, threadRegister);
    }
    __ Subq(JSThread::GetGlueDataOffset(), threadRegister);
}

void AssemblerStubsX64::ConstructEcmaRuntimeCallInfo(ExtendedAssemblerX64 *assembler, Register threadRegister,
    Register numArgsRegister, Register stackArgsRegister)
{
    __ Subq(sizeof(EcmaRuntimeCallInfo), rsp);
    __ Movq(threadRegister, Operand(rsp, EcmaRuntimeCallInfo::GetThreadOffset()));
    __ Movq(numArgsRegister, Operand(rsp, EcmaRuntimeCallInfo::GetNumArgsOffset()));
    __ Movq(stackArgsRegister, Operand(rsp, EcmaRuntimeCallInfo::GetStackArgsOffset()));
    __ Movq(0, Operand(rsp, EcmaRuntimeCallInfo::GetDataOffset()));
}

void AssemblerStubsX64::GetDeclaredNumArgsFromCallField(ExtendedAssemblerX64 *assembler, Register callFieldRegister,
    Register declaredNumArgsRegister)
{
    __ Movq(callFieldRegister, declaredNumArgsRegister);
    __ Shrq(JSMethod::NumArgsBits::START_BIT, declaredNumArgsRegister);
    __ Andq(JSMethod::NumArgsBits::Mask() >> JSMethod::NumArgsBits::START_BIT, declaredNumArgsRegister);
}

void AssemblerStubsX64::GetNumVregsFromCallField(ExtendedAssemblerX64 *assembler, Register callFieldRegister,
    Register numVregsRegister)
{
    __ Movq(callFieldRegister, numVregsRegister);
    __ Shrq(JSMethod::NumVregsBits::START_BIT, numVregsRegister);
    __ Andq(JSMethod::NumVregsBits::Mask() >> JSMethod::NumVregsBits::START_BIT, numVregsRegister);
}

// void PushCallArgsxAndDispatch(uintptr_t glue, uintptr_t sp, uint64_t callTarget, uintptr_t method,
//     uint64_t callField, ...)
// GHC calling convention
// Input1: for callarg0/1/2/3         Input2: for callrange
// %r13 - glue                        // %r13 - glue
// %rbp - sp                          // %rbp - sp
// %r12 - callTarget                  // %r12 - callTarget
// %rbx - method                      // %rbx - method
// %r14 - callField                   // %r14 - callField
// %rsi - arg0                        // %rsi - actualArgc
// %rdi - arg1                        // %rdi - argv
// %r8  - arg2
void AssemblerStubsX64::PushCallIThisRangeAndDispatch(ExtendedAssemblerX64 *assembler)
{
    Register jumpSizeRegister = rdx;
    Register fpRegister = r10;
    __ Movq(BytecodeInstruction::Size(BytecodeInstruction::Format::PREF_IMM16_V8), jumpSizeRegister);
    __ Movq(rsp, fpRegister);
    CallIThisRangeEntry(assembler);
}

void AssemblerStubsX64::PushCallIRangeAndDispatch(ExtendedAssemblerX64 *assembler)
{
    Register jumpSizeRegister = rdx;
    Register fpRegister = r10;
    __ Movq(BytecodeInstruction::Size(BytecodeInstruction::Format::PREF_IMM16_V8), jumpSizeRegister);
    __ Movq(rsp, fpRegister);
    CallIRangeEntry(assembler);
}

void AssemblerStubsX64::PushCallArgs3AndDispatch(ExtendedAssemblerX64 *assembler)
{
    Register jumpSizeRegister = rdx;
    Register fpRegister = r10;
    __ Movq(BytecodeInstruction::Size(BytecodeInstruction::Format::PREF_V8_V8_V8_V8), jumpSizeRegister);
    __ Movq(rsp, fpRegister);
    Callargs3Entry(assembler);
}

void AssemblerStubsX64::PushCallArgs2AndDispatch(ExtendedAssemblerX64 *assembler)
{
    Register jumpSizeRegister = rdx;
    Register fpRegister = r10;
    __ Movq(BytecodeInstruction::Size(BytecodeInstruction::Format::PREF_V8_V8_V8), jumpSizeRegister);
    __ Movq(rsp, fpRegister);
    Callargs2Entry(assembler);
}

void AssemblerStubsX64::PushCallArgs1AndDispatch(ExtendedAssemblerX64 *assembler)
{
    Register jumpSizeRegister = rdx;
    Register fpRegister = r10;
    __ Movq(BytecodeInstruction::Size(BytecodeInstruction::Format::PREF_V8_V8), jumpSizeRegister);
    __ Movq(rsp, fpRegister);
    Callarg1Entry(assembler);
}

void AssemblerStubsX64::PushCallArgs0AndDispatch(ExtendedAssemblerX64 *assembler)
{
    Register jumpSizeRegister = rdx;
    Register fpRegister = r10;
    __ Movq(BytecodeInstruction::Size(BytecodeInstruction::Format::PREF_V8), jumpSizeRegister);
    __ Movq(rsp, fpRegister);
    PushCallThisUndefined(assembler);  // Callarg0Entry
}

// void PushCallArgsxAndDispatchSlowPath(uintptr_t glue, uintptr_t sp, uint64_t callTarget, uintptr_t method,
//       uint64_t callField, ...)
// GHC calling convention
// Input1: for callarg0/1/2/3         Input2: for callrange
// %r13 - glue                        // %r13 - glue
// %rbp - sp                          // %rbp - sp
// %r12 - callTarget                  // %r12 - callTarget
// %rbx - method                      // %rbx - method
// %r14 - callField                   // %r14 - callField
// %rsi - arg0                        // %rsi - actualArgc
// %rdi - arg1                        // %rdi - argv
// %r8  - arg2
void AssemblerStubsX64::PushCallIThisRangeAndDispatchSlowPath(ExtendedAssemblerX64 *assembler)
{
    Register callFieldRegister = r14;
    Register argcRegister = rsi;
    Label haveExtraEntry;
    Label pushArgsNoExtraEntry;
    Label pushArgsEntry;

    Register jumpSizeRegister = rdx;
    Register fpRegister = r10;
    __ Movq(BytecodeInstruction::Size(BytecodeInstruction::Format::PREF_IMM16_V8), jumpSizeRegister);
    __ Movq(rsp, fpRegister);
    Register declaredNumArgsRegister = r11;
    GetDeclaredNumArgsFromCallField(assembler, callFieldRegister, declaredNumArgsRegister);
    __ Testq(JSMethod::HaveExtraBit::Mask(), callFieldRegister);
    __ Jnz(&haveExtraEntry);
    Register diffRegister = r15;
    __ Movq(declaredNumArgsRegister, diffRegister);
    __ Subq(argcRegister, diffRegister);
    __ Cmpq(0, diffRegister);
    __ Jle(&pushArgsNoExtraEntry);
    PushUndefinedWithArgc(assembler, diffRegister);
    __ Jmp(&pushArgsNoExtraEntry);

    __ Bind(&haveExtraEntry);
    {
        __ Pushq(argcRegister);
        __ Movq(declaredNumArgsRegister, diffRegister);
        __ Subq(argcRegister, diffRegister);
        __ Cmpq(0, diffRegister);
        __ Jle(&pushArgsEntry);
        PushUndefinedWithArgc(assembler, diffRegister);
        __ Jmp(&pushArgsEntry);
    }

    __ Bind(&pushArgsNoExtraEntry);
    {
        CallIThisRangeNoExtraEntry(assembler, declaredNumArgsRegister);
    }
    __ Bind(&pushArgsEntry);
    {
        CallIThisRangeEntry(assembler);
    }
}

void AssemblerStubsX64::PushCallIRangeAndDispatchSlowPath(ExtendedAssemblerX64 *assembler)
{
    Register callFieldRegister = r14;
    Register argcRegister = rsi;
    Label haveExtraEntry;
    Label pushArgsNoExtraEntry;
    Label pushArgsEntry;

    Register jumpSizeRegister = rdx;
    Register fpRegister = r10;
    __ Movq(BytecodeInstruction::Size(BytecodeInstruction::Format::PREF_IMM16_V8), jumpSizeRegister);
    __ Movq(rsp, fpRegister);
    Register declaredNumArgsRegister = r11;
    GetDeclaredNumArgsFromCallField(assembler, callFieldRegister, declaredNumArgsRegister);
    __ Testq(JSMethod::HaveExtraBit::Mask(), callFieldRegister);
    __ Jnz(&haveExtraEntry);
    Register diffRegister = r15;
    __ Movq(declaredNumArgsRegister, diffRegister);
    __ Subq(argcRegister, diffRegister);
    __ Cmpq(0, diffRegister);
    __ Jle(&pushArgsNoExtraEntry);
    PushUndefinedWithArgc(assembler, diffRegister);
    __ Jmp(&pushArgsNoExtraEntry);

    __ Bind(&haveExtraEntry);
    {
        __ Pushq(argcRegister);
        __ Movq(declaredNumArgsRegister, diffRegister);
        __ Subq(argcRegister, diffRegister);
        __ Cmpq(0, diffRegister);
        __ Jle(&pushArgsEntry);
        PushUndefinedWithArgc(assembler, diffRegister);
        __ Jmp(&pushArgsEntry);
    }

    __ Bind(&pushArgsNoExtraEntry);
    {
        CallIRangeNoExtraEntry(assembler, declaredNumArgsRegister);
    }
    __ Bind(&pushArgsEntry);
    {
        CallIRangeEntry(assembler);
    }
}

void AssemblerStubsX64::PushCallArgs3AndDispatchSlowPath(ExtendedAssemblerX64 *assembler)
{
    Register callFieldRegister = r14;
    constexpr int32_t argc = 3;
    Label haveExtraEntry;
    Label pushArgsNoExtraEntry;
    Label pushArgsEntry;

    Register jumpSizeRegister = rdx;
    Register fpRegister = r10;
    __ Movq(BytecodeInstruction::Size(BytecodeInstruction::Format::PREF_V8_V8_V8_V8), jumpSizeRegister);
    __ Movq(rsp, fpRegister);
    Register declaredNumArgsRegister = r11;
    GetDeclaredNumArgsFromCallField(assembler, callFieldRegister, declaredNumArgsRegister);
    __ Testq(JSMethod::HaveExtraBit::Mask(), callFieldRegister);
    __ Jnz(&haveExtraEntry);
    Register diffRegister = r15;
    __ Movq(declaredNumArgsRegister, diffRegister);
    __ Subq(argc, diffRegister);
    __ Cmpq(0, diffRegister);
    __ Jle(&pushArgsNoExtraEntry);
    PushUndefinedWithArgc(assembler, diffRegister);
    __ Jmp(&pushArgsNoExtraEntry);

    __ Bind(&haveExtraEntry);
    {
        __ Pushq(argc);
        __ Movq(declaredNumArgsRegister, diffRegister);
        __ Subq(argc, diffRegister);
        __ Cmpq(0, diffRegister);
        __ Jle(&pushArgsEntry);
        PushUndefinedWithArgc(assembler, diffRegister);
        __ Jmp(&pushArgsEntry);
    }

    __ Bind(&pushArgsNoExtraEntry);
    {
        Callargs3NoExtraEntry(assembler, declaredNumArgsRegister);
    }
    __ Bind(&pushArgsEntry);
    {
        Callargs3Entry(assembler);
    }
}

void AssemblerStubsX64::PushCallArgs2AndDispatchSlowPath(ExtendedAssemblerX64 *assembler)
{
    Register callFieldRegister = r14;
    constexpr int32_t argc = 2;
    Label haveExtraEntry;
    Label pushArgsNoExtraEntry;
    Label pushArgsEntry;

    Register jumpSizeRegister = rdx;
    Register fpRegister = r10;
    __ Movq(BytecodeInstruction::Size(BytecodeInstruction::Format::PREF_V8_V8_V8), jumpSizeRegister);
    __ Movq(rsp, fpRegister);
    Register declaredNumArgsRegister = r11;
    GetDeclaredNumArgsFromCallField(assembler, callFieldRegister, declaredNumArgsRegister);
    __ Testq(JSMethod::HaveExtraBit::Mask(), callFieldRegister);
    __ Jnz(&haveExtraEntry);
    Register diffRegister = r15;
    __ Movq(declaredNumArgsRegister, diffRegister);
    __ Subq(argc, diffRegister);
    __ Cmpq(0, diffRegister);
    __ Jle(&pushArgsNoExtraEntry);
    PushUndefinedWithArgc(assembler, diffRegister);
    __ Jmp(&pushArgsNoExtraEntry);

    __ Bind(&haveExtraEntry);
    {
        __ Pushq(argc);
        __ Movq(declaredNumArgsRegister, diffRegister);
        __ Subq(argc, diffRegister);
        __ Cmpq(0, diffRegister);
        __ Jle(&pushArgsEntry);
        PushUndefinedWithArgc(assembler, diffRegister);
        __ Jmp(&pushArgsEntry);
    }

    __ Bind(&pushArgsNoExtraEntry);
    {
        Callargs2NoExtraEntry(assembler, declaredNumArgsRegister);
    }
    __ Bind(&pushArgsEntry);
    {
        Callargs2Entry(assembler);
    }
}

void AssemblerStubsX64::PushCallArgs1AndDispatchSlowPath(ExtendedAssemblerX64 *assembler)
{
    Register callFieldRegister = r14;
    constexpr int32_t argc = 1;
    Label haveExtraEntry;
    Label pushArgsNoExtraEntry;
    Label pushArgsEntry;

    Register jumpSizeRegister = rdx;
    Register fpRegister = r10;
    __ Movq(BytecodeInstruction::Size(BytecodeInstruction::Format::PREF_V8_V8), jumpSizeRegister);
    __ Movq(rsp, fpRegister);
    Register declaredNumArgsRegister = r11;
    GetDeclaredNumArgsFromCallField(assembler, callFieldRegister, declaredNumArgsRegister);
    __ Testq(JSMethod::HaveExtraBit::Mask(), callFieldRegister);
    __ Jnz(&haveExtraEntry);
    Register diffRegister = r15;
    __ Movq(declaredNumArgsRegister, diffRegister);
    __ Subq(argc, diffRegister);
    __ Cmpq(0, diffRegister);
    __ Jle(&pushArgsNoExtraEntry);
    PushUndefinedWithArgc(assembler, diffRegister);
    __ Jmp(&pushArgsNoExtraEntry);

    __ Bind(&haveExtraEntry);
    {
        __ Pushq(argc);
        __ Movq(declaredNumArgsRegister, diffRegister);
        __ Subq(argc, diffRegister);
        __ Cmpq(0, diffRegister);
        __ Jle(&pushArgsEntry);
        PushUndefinedWithArgc(assembler, diffRegister);
        __ Jmp(&pushArgsEntry);
    }

    __ Bind(&pushArgsNoExtraEntry);
    {
        Callargs1NoExtraEntry(assembler, declaredNumArgsRegister);
    }
    __ Bind(&pushArgsEntry);
    {
        Callarg1Entry(assembler);
    }
}

void AssemblerStubsX64::PushCallArgs0AndDispatchSlowPath(ExtendedAssemblerX64 *assembler)
{
    Register callFieldRegister = r14;
    constexpr int32_t argc = 0;
    Label haveExtraEntry;
    Label pushArgsNoExtraEntry;
    Label pushArgsEntry;

    Register jumpSizeRegister = rdx;
    Register fpRegister = r10;
    __ Movq(BytecodeInstruction::Size(BytecodeInstruction::Format::PREF_V8), jumpSizeRegister);
    __ Movq(rsp, fpRegister);
    Register declaredNumArgsRegister = r11;
    GetDeclaredNumArgsFromCallField(assembler, callFieldRegister, declaredNumArgsRegister);
    __ Testq(JSMethod::HaveExtraBit::Mask(), callFieldRegister);
    __ Jnz(&haveExtraEntry);
    Register diffRegister = r15;
    __ Movq(declaredNumArgsRegister, diffRegister);
    __ Subq(argc, diffRegister);
    __ Cmpq(0, diffRegister);
    __ Jle(&pushArgsNoExtraEntry);
    PushUndefinedWithArgc(assembler, diffRegister);
    __ Jmp(&pushArgsNoExtraEntry);

    __ Bind(&haveExtraEntry);
    {
        __ Pushq(argc);
        __ Movq(declaredNumArgsRegister, diffRegister);
        __ Subq(argc, diffRegister);
        __ Cmpq(0, diffRegister);
        __ Jle(&pushArgsEntry);
        PushUndefinedWithArgc(assembler, diffRegister);
        __ Jmp(&pushArgsEntry);
    }

    __ Bind(&pushArgsNoExtraEntry);
    {
        Callargs0NoExtraEntry(assembler);
    }
    __ Bind(&pushArgsEntry);
    {
        PushCallThisUndefined(assembler);
    }
}

// Input:
// %rsi - actualArgc
// %rdi - argv
void AssemblerStubsX64::CallIThisRangeNoExtraEntry(ExtendedAssemblerX64 *assembler, Register declaredNumArgsRegister)
{
    Register argcRegister = rsi;
    Register argvRegister = rdi;

    Label prepareLoop;
    Label pushCallThis;
    __ Cmpq(declaredNumArgsRegister, argcRegister);
    __ Jae(&prepareLoop);
    Register numRegister = declaredNumArgsRegister;
    __ Movq(argcRegister, numRegister);
    __ Bind(&prepareLoop);
    {
        __ Cmpq(0, numRegister);
        __ Jbe(&pushCallThis);
        Register opRegister = r8;
        __ PushArgsWithArgv(numRegister, argvRegister, opRegister);
        __ Jmp(&pushCallThis);
    }
    __ Bind(&pushCallThis);
    {
        PushCallThis(assembler);
    }
}

// Input:
// %rsi - actualArgc
// %rdi - argv
void AssemblerStubsX64::CallIRangeNoExtraEntry(ExtendedAssemblerX64 *assembler, Register declaredNumArgsRegister)
{
    Register argcRegister = rsi;
    Register argvRegister = rdi;

    Label prepareLoop;
    Label pushCallThisUndefined;
    __ Cmpq(declaredNumArgsRegister, argcRegister);
    __ Jae(&prepareLoop);
    Register numRegister = declaredNumArgsRegister;
    __ Movq(argcRegister, numRegister);
    __ Bind(&prepareLoop);
    {
        __ Cmpq(0, numRegister);
        __ Jbe(&pushCallThisUndefined);
        Register opRegister = r8;
        __ PushArgsWithArgv(numRegister, argvRegister, opRegister);
        __ Jmp(&pushCallThisUndefined);
    }
    __ Bind(&pushCallThisUndefined);
    {
        PushCallThisUndefined(assembler);
    }
}

// Input:
// %rsi - arg0
// %rdi - arg1
// %r8  - arg2
void AssemblerStubsX64::Callargs3NoExtraEntry(ExtendedAssemblerX64 *assembler, Register declaredNumArgsRegister)
{
    constexpr int32_t argc = 3;
    Label callargs2NoExtraEntry;
    __ Cmpq(argc, declaredNumArgsRegister);
    __ Jb(&callargs2NoExtraEntry);
    __ Pushq(r8);  // arg2
    // fall through
    __ Bind(&callargs2NoExtraEntry);
    Callargs2NoExtraEntry(assembler, declaredNumArgsRegister);
}

// Input:
// %rsi - arg0
// %rdi - arg1
void AssemblerStubsX64::Callargs2NoExtraEntry(ExtendedAssemblerX64 *assembler, Register declaredNumArgsRegister)
{
    constexpr int32_t argc = 2;
    Label callargs1NoExtraEntry;
    __ Cmpq(argc, declaredNumArgsRegister);
    __ Jb(&callargs1NoExtraEntry);
    __ Pushq(rdi);  // arg1
    // fall through
    __ Bind(&callargs1NoExtraEntry);
    Callargs1NoExtraEntry(assembler, declaredNumArgsRegister);
}

// Input:
// %rsi - arg0
void AssemblerStubsX64::Callargs1NoExtraEntry(ExtendedAssemblerX64 *assembler, Register declaredNumArgsRegister)
{
    constexpr int32_t argc = 1;
    Label callargs0NoExtraEntry;
    __ Cmpq(argc, declaredNumArgsRegister);
    __ Jb(&callargs0NoExtraEntry);
    __ Pushq(rsi);  // arg0
    // fall through
    __ Bind(&callargs0NoExtraEntry);
    Callargs0NoExtraEntry(assembler);
}

void AssemblerStubsX64::Callargs0NoExtraEntry(ExtendedAssemblerX64 *assembler)
{
    PushCallThisUndefined(assembler);
}

// Input:
// %rsi - actualArgc
// %rdi - argv
void AssemblerStubsX64::CallIThisRangeEntry(ExtendedAssemblerX64 *assembler)
{
    Register argcRegister = rsi;
    Register argvRegister = rdi;

    Label pushCallThis;

    Register numRegister = r11;
    __ Movq(argcRegister, numRegister);
    __ Cmpq(0, numRegister);
    __ Jbe(&pushCallThis);
    // fall through
    Register opRegister = r8;
    __ PushArgsWithArgv(numRegister, argvRegister, opRegister);
    __ Bind(&pushCallThis);
    PushCallThis(assembler);
}

// Input:
// %r14 - callField
// %rdi - argv
void AssemblerStubsX64::PushCallThis(ExtendedAssemblerX64 *assembler)
{
    Register callFieldRegister = r14;
    Register argvRegister = rdi;

    Label pushVregs;
    Label pushNewTarget;
    __ Testb(CALL_TYPE_MASK, callFieldRegister);
    __ Jz(&pushVregs);
    // fall through
    __ Testq(JSMethod::HaveThisBit::Mask(), callFieldRegister);
    __ Jz(&pushNewTarget);
    // push this
    Register tempRegister = r11;
    __ Movq(Operand(argvRegister, -8), tempRegister);  // 8: this is just before the argv list
    __ Pushq(tempRegister);
    // fall through
    __ Bind(&pushNewTarget);
    {
        PushNewTarget(assembler);
    }
    __ Bind(&pushVregs);
    {
        PushVregs(assembler);
    }
}

// Input:
// %rsi - actualArgc
// %rdi - argv
void AssemblerStubsX64::CallIRangeEntry(ExtendedAssemblerX64 *assembler)
{
    Register argcRegister = rsi;
    Register argvRegister = rdi;

    Label pushCallThisUndefined;

    Register numRegister = r11;
    __ Movq(argcRegister, numRegister);
    __ Cmpq(0, numRegister);
    __ Jbe(&pushCallThisUndefined);
    // fall through
    Register opRegister = r8;
    __ PushArgsWithArgv(numRegister, argvRegister, opRegister);
    __ Bind(&pushCallThisUndefined);
    PushCallThisUndefined(assembler);
}

// Input:
// %rsi - arg0
// %rdi - arg1
// %r8  - arg2
void AssemblerStubsX64::Callargs3Entry(ExtendedAssemblerX64 *assembler)
{
    __ Pushq(r8);  // arg2
    Callargs2Entry(assembler);
}

// Input:
// %rsi - arg0
// %rdi - arg1
void AssemblerStubsX64::Callargs2Entry(ExtendedAssemblerX64 *assembler)
{
    __ Pushq(rdi);  // arg1
    Callarg1Entry(assembler);
}

// Input:
// %rsi - arg0
void AssemblerStubsX64::Callarg1Entry(ExtendedAssemblerX64 *assembler)
{
    __ Pushq(rsi);  // arg0
    PushCallThisUndefined(assembler);
}

// Input:
// %r14 - callField
void AssemblerStubsX64::PushCallThisUndefined(ExtendedAssemblerX64 *assembler)
{
    Register callFieldRegister = r14;

    Label pushVregs;
    Label pushNewTarget;
    __ Testb(CALL_TYPE_MASK, callFieldRegister);
    __ Jz(&pushVregs);
    // fall through
    __ Testq(JSMethod::HaveThisBit::Mask(), callFieldRegister);
    __ Jz(&pushNewTarget);
    // push undefined
    __ Pushq(JSTaggedValue::Undefined().GetRawData());
    // fall through
    __ Bind(&pushNewTarget);
    {
        PushNewTarget(assembler);
    }
    __ Bind(&pushVregs);
    {
        PushVregs(assembler);
    }
}

// Input:
// %r14 - callField
void AssemblerStubsX64::PushNewTarget(ExtendedAssemblerX64 *assembler)
{
    Register callFieldRegister = r14;

    Label pushCallTarget;
    __ Testq(JSMethod::HaveNewTargetBit::Mask(), callFieldRegister);
    __ Jz(&pushCallTarget);
    // push undefined
    __ Pushq(JSTaggedValue::Undefined().GetRawData());
    __ Bind(&pushCallTarget);
    PushCallTarget(assembler);
}

// Input:
// %r12 - callTarget
// %r14 - callField
void AssemblerStubsX64::PushCallTarget(ExtendedAssemblerX64 *assembler)
{
    Register callTargetRegister = r12;
    Register callFieldRegister = r14;

    Label pushVregs;
    __ Testq(JSMethod::HaveFuncBit::Mask(), callFieldRegister);
    __ Jz(&pushVregs);
    // push undefined
    __ Pushq(callTargetRegister);
    // fall through
    __ Bind(&pushVregs);
    PushVregs(assembler);
}

// Input:
// %rbp - sp
// %r12 - callTarget
// %rbx - method
// %r14 - callField
// %rdx - jumpSizeAfterCall
// %r10 - fp
void AssemblerStubsX64::PushVregs(ExtendedAssemblerX64 *assembler)
{
    Register prevSpRegister = rbp;
    Register callTargetRegister = r12;
    Register methodRegister = rbx;
    Register callFieldRegister = r14;
    Register jumpSizeRegister = rdx;
    Register fpRegister = r10;

    Label pushFrameState;
    Label dispatchCall;
    Register pcRegister = rcx;
    Register newSpRegister = r15;
    Register numVregsRegister = r11;
    GetNumVregsFromCallField(assembler, callFieldRegister, numVregsRegister);
    __ Cmpq(0, numVregsRegister);
    __ Jz(&pushFrameState);
    PushUndefinedWithArgc(assembler, numVregsRegister);
    // fall through
    __ Bind(&pushFrameState);
    {
        __ Movq(rsp, newSpRegister);

        StackOverflowCheck(assembler);

        Register tempRegister = r11;  // reuse
        __ Movq(prevSpRegister, tempRegister);
        __ Subq(sizeof(AsmInterpretedFrame), tempRegister);
        __ Movq(jumpSizeRegister, Operand(tempRegister, AsmInterpretedFrame::GetCallSizeOffset(false)));
        PushFrameState(assembler, prevSpRegister, fpRegister, callTargetRegister, methodRegister, pcRegister,
            tempRegister);
        // align 16 bytes
        __ Testq(15, rsp);  // 15: low 4 bits must be 0b0000
        __ Jnz(&dispatchCall);
        __ PushAlignBytes();
        // fall through
    }
    __ Bind(&dispatchCall);
    {
        DispatchCall(assembler, pcRegister, newSpRegister);
    }
}

// Input:
// %r13 - glue
// %rbp - sp
// %r12 - callTarget
// %rbx - method
void AssemblerStubsX64::DispatchCall(ExtendedAssemblerX64 *assembler, Register pcRegister, Register newSpRegister)
{
    Register glueRegister = r13;
    Register callTargetRegister = r12;
    Register methodRegister = rbx;
    __ Movzwq(Operand(methodRegister, JSMethod::GetHotnessCounterOffset(false)), rdi);  // hotnessCounter: rdi
    __ Movq(JSTaggedValue::Hole().GetRawData(), rsi);                                   // acc: rsi
    __ Movq(Operand(callTargetRegister, JSFunction::PROFILE_TYPE_INFO_OFFSET), r14);    // profileTypeInfo: r14
    __ Movq(Operand(callTargetRegister, JSFunction::CONSTANT_POOL_OFFSET), rbx);        // constantPool: rbx
    __ Movq(pcRegister, r12);                                                           // pc: r12
    __ Movq(newSpRegister, rbp);                                                        // sp: rbp
                                                                                        // glue: r13

    Register bcIndexRegister = rax;
    Register tempRegister = r11;
    __ Movzbq(Operand(pcRegister, 0), bcIndexRegister);
    __ Movq(Operand(glueRegister, bcIndexRegister, Times8, JSThread::GlueData::GetBCStubEntriesOffset(false)),
        tempRegister);
    __ Jmp(tempRegister);
}

// uint64_t PushCallIRangeAndDispatchNative(uintptr_t glue, uint32_t argc, JSTaggedType calltarget, uintptr_t argv[])
// c++ calling convention call js function
// Input:
// %rdi - glue
// %rsi - nativeCode
// %rdx - func
// %rcx - thisValue
// %r8  - argc
// %r9  - argV (...)
void AssemblerStubsX64::PushCallIRangeAndDispatchNative(ExtendedAssemblerX64 *assembler)
{
    Register glue = rdi;
    Register nativeCode = rsi;
    Register func = rdx;
    Register thisValue = rcx;
    Register numArgs = r8;
    Register stackArgs = r9;
    Register temporary = rax;
    Register opNumArgs = r10;
    Label aligned;

    PushBuiltinFrame(assembler, glue, FrameType::BUILTIN_FRAME_WITH_ARGV);

    StackOverflowCheck(assembler);
    __ Push(numArgs);
    __ Movq(numArgs, opNumArgs);
    __ PushArgsWithArgv(opNumArgs, stackArgs, temporary);
    __ Push(thisValue);
    // new.target
    __ Pushq(JSTaggedValue::Undefined().GetRawData());
    __ Push(func);
    __ Movq(rsp, stackArgs);

    __ Testq(0xf, rsp);  // 0xf: 0x1111
    __ Jz(&aligned, Distance::NEAR);
    __ PushAlignBytes();

    __ Bind(&aligned);
    CallNativeInternal(assembler, glue, numArgs, stackArgs, nativeCode);
    __ Ret();
}

void AssemblerStubsX64::CallNativeEntry(ExtendedAssemblerX64 *assembler)
{
    Register glue = rdi;
    Register argc = rsi;
    Register argv = rdx;
    Register method = rcx;
    Register function = r9;
    Register nativeCode = rbx;

    // skip nativeCode & argc
    __ Addq(16, rsp);
    __ Push(function);
    PushBuiltinFrame(assembler, glue, FrameType::BUILTIN_ENTRY_FRAME);
    __ Movq(Operand(method, JSMethod::GetBytecodeArrayOffset(false)), nativeCode); // get native pointer
    CallNativeInternal(assembler, glue, argc, argv, nativeCode);

    // 24: skip function
    __ Addq(24, rsp);
    __ Ret();
}

// uint64_t PushCallArgsAndDispatchNative(uintptr_t glue, uintptr_t codeAddress, uint32_t argc, ...)
// webkit_jscc calling convention call runtime_id's runtion function(c-abi)
// Input:
// %rax - glue
// stack layout:
// sp + N*8 argvN
// ........
// sp + 24: argv1
// sp + 16: argv0
// sp + 8:  actualArgc
// sp:      codeAddress
// construct Native Leave Frame:
//   +--------------------------+
//   |       argv0              | calltarget , newTarget, this, ....
//   +--------------------------+ ---
//   |       argc               |   ^
//   |--------------------------|  Fixed
//   |       codeAddress        | BuiltinFrame
//   |--------------------------|   |
//   |       returnAddr         |   |
//   |--------------------------|   |
//   |       callsiteFp         |   |
//   |--------------------------|   |
//   |       frameType          |   v
//   +--------------------------+ ---
void AssemblerStubsX64::PushCallArgsAndDispatchNative(ExtendedAssemblerX64 *assembler)
{
    Register glue = rax;
    Register numArgs = rdx;
    Register stackArgs = rcx;
    Register nativeCode = rbx;

    PushBuiltinFrame(assembler, glue, FrameType::BUILTIN_FRAME);
    __ Movq(Operand(rbp, BuiltinFrame::GetNativeCodeToFpDelta(false)), nativeCode);
    // 8: offset of numArgs
    __ Movq(Operand(rbp, BuiltinFrame::GetNumArgsToFpDelta(false)), numArgs);
    // 8: offset of &argv[0]
    __ Leaq(Operand(rbp, BuiltinFrame::GetStackArgsToFpDelta(false)), stackArgs);

    __ PushAlignBytes();

    CallNativeInternal(assembler, glue, numArgs, stackArgs, nativeCode);
    __ Ret();
}

void AssemblerStubsX64::PushBuiltinFrame(ExtendedAssemblerX64 *assembler,
                                         Register glue, FrameType type)
{
    __ Pushq(rbp);
    __ Movq(rsp, rbp);
    __ Movq(rbp, Operand(glue, JSThread::GlueData::GetLeaveFrameOffset(false)));
    __ Pushq(static_cast<int32_t>(type));
}

void AssemblerStubsX64::CallNativeInternal(ExtendedAssemblerX64 *assembler,
    Register glue, Register numArgs, Register stackArgs, Register nativeCode)
{
    GlueToThread(assembler, glue, glue);
    ConstructEcmaRuntimeCallInfo(assembler, glue, numArgs, stackArgs);

    // rsp is ecma callinfo base
    __ Movq(rsp, rdi);
    __ Callq(nativeCode);
    // resume rsp
    __ Movq(rbp, rsp);
    __ Pop(rbp);
}

// ResumeRspAndDispatch(uintptr_t glue, uintptr_t sp, uintptr_t pc, uintptr_t constantPool,
//     uint64_t profileTypeInfo, uint64_t acc, uint32_t hotnessCounter, size_t jumpSize)
// GHC calling convention
// %r13 - glue
// %rbp - sp
// %r12 - pc
// %rbx - constantPool
// %r14 - profileTypeInfo
// %rsi - acc
// %rdi - hotnessCounter
// %r8  - jumpSizeAfterCall
void AssemblerStubsX64::ResumeRspAndDispatch(ExtendedAssemblerX64 *assembler)
{
    Register glueRegister = r13;
    Register spRegister = rbp;
    Register pcRegister = r12;
    Register jumpSizeRegister = r8;

    Register frameStateBaseRegister = r11;
    __ Movq(spRegister, frameStateBaseRegister);
    __ Subq(sizeof(AsmInterpretedFrame), frameStateBaseRegister);
    __ Movq(Operand(frameStateBaseRegister, AsmInterpretedFrame::GetFpOffset(false)), rsp);   // resume rsp
    __ Movq(Operand(frameStateBaseRegister, AsmInterpretedFrame::GetBaseOffset(false)), spRegister);  // update sp

    __ Addq(jumpSizeRegister, pcRegister);  // newPc
    Register opcodeRegister = rax;
    __ Movzbq(Operand(pcRegister, 0), opcodeRegister);
    Register bcStubRegister = r11;
    __ Movq(Operand(glueRegister, opcodeRegister, Times8, JSThread::GlueData::GetBCStubEntriesOffset(false)),
        bcStubRegister);
    __ Jmp(bcStubRegister);
}

// ResumeRspAndReturn(uintptr_t glue, uintptr_t sp)
// GHC calling convention
// %r13 - glue
// %rbp - sp
void AssemblerStubsX64::ResumeRspAndReturn([[maybe_unused]] ExtendedAssemblerX64 *assembler)
{
    __ Ret();
}

// ResumeCaughtFrameAndDispatch(uintptr_t glue, uintptr_t sp, uintptr_t pc, uintptr_t constantPool,
//     uint64_t profileTypeInfo, uint64_t acc, uint32_t hotnessCounter)
// GHC calling convention
// %r13 - glue
// %rbp - sp
// %r12 - pc
// %rbx - constantPool
// %r14 - profileTypeInfo
// %rsi - acc
// %rdi - hotnessCounter
void AssemblerStubsX64::ResumeCaughtFrameAndDispatch(ExtendedAssemblerX64 *assembler)
{
    Register glueRegister = r13;
    Register pcRegister = r12;

    Label dispatch;
    Register fpRegister = r11;
    __ Movq(Operand(glueRegister, JSThread::GlueData::GetLastFpOffset(false)), fpRegister);
    __ Cmpq(0, fpRegister);
    __ Jz(&dispatch);
    __ Movq(fpRegister, rsp);  // resume rsp
    __ Bind(&dispatch);
    {
        Register opcodeRegister = rax;
        __ Movzbq(Operand(pcRegister, 0), opcodeRegister);
        Register bcStubRegister = r11;
        __ Movq(Operand(glueRegister, opcodeRegister, Times8, JSThread::GlueData::GetBCStubEntriesOffset(false)),
            bcStubRegister);
        __ Jmp(bcStubRegister);
    }
}

void AssemblerStubsX64::PushUndefinedWithArgc(ExtendedAssemblerX64 *assembler, Register argc)
{
    Label loopBeginning;
    __ Bind(&loopBeginning);
    __ Pushq(JSTaggedValue::Undefined().GetRawData());
    __ Subq(1, argc);
    __ Ja(&loopBeginning);
}

void AssemblerStubsX64::HasPendingException([[maybe_unused]] ExtendedAssemblerX64 *assembler,
    [[maybe_unused]] Register threadRegister)
{
}

void AssemblerStubsX64::StackOverflowCheck([[maybe_unused]] ExtendedAssemblerX64 *assembler)
{
}
#undef __
}  // namespace panda::ecmascript::x64
