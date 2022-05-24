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
#include "extend_assembler.h"
#include "ecmascript/frames.h"

namespace panda::ecmascript::aarch64 {
void ExtendedAssembler::CalleeSave()
{
    Register sp(SP);
    Stp(Register(X27), Register(X28), MemoryOperand(sp, -16, PREINDEX));
    Stp(Register(X25), Register(X26), MemoryOperand(sp, -16, PREINDEX));
    Stp(Register(X23), Register(X24), MemoryOperand(sp, -16, PREINDEX));
    Stp(Register(X21), Register(X22), MemoryOperand(sp, -16, PREINDEX));
    Stp(Register(X19), Register(X20), MemoryOperand(sp, -16, PREINDEX));

    Stp(VectorRegister(v14), VectorRegister(v15), MemoryOperand(sp, -16, PREINDEX));
    Stp(VectorRegister(v12), VectorRegister(v13), MemoryOperand(sp, -16, PREINDEX));
    Stp(VectorRegister(v10), VectorRegister(v11), MemoryOperand(sp, -16, PREINDEX));
    Stp(VectorRegister(v8), VectorRegister(v9), MemoryOperand(sp, -16, PREINDEX));
}

void ExtendedAssembler::CalleeRestore()
{
    Register sp(SP);
    Ldp(VectorRegister(v8), VectorRegister(v9), MemoryOperand(sp, 16, POSTINDEX));
    Ldp(VectorRegister(v10), VectorRegister(v11), MemoryOperand(sp, 16, POSTINDEX));
    Ldp(VectorRegister(v12), VectorRegister(v13), MemoryOperand(sp, 16, POSTINDEX));
    Ldp(VectorRegister(v14), VectorRegister(v15), MemoryOperand(sp, 16, POSTINDEX));
    Ldp(Register(X19), Register(X20), MemoryOperand(sp, 16, POSTINDEX));
    Ldp(Register(X21), Register(X22), MemoryOperand(sp, 16, POSTINDEX));
    Ldp(Register(X23), Register(X24), MemoryOperand(sp, 16, POSTINDEX));
    Ldp(Register(X25), Register(X26), MemoryOperand(sp, 16, POSTINDEX));
    Ldp(Register(X27), Register(X28), MemoryOperand(sp, 16, POSTINDEX));
}

void ExtendedAssembler::CallAssemblerStub(int id, bool isTail)
{
    Label *target = module_->GetFunctionLabel(id);
    isTail ? B(target) : Bl(target);
}

void ExtendedAssembler::BindAssemblerStub(int id)
{
    Label *target = module_->GetFunctionLabel(id);
    Bind(target);
}

void ExtendedAssembler::SaveFpAndLr()
{
    Register sp(SP);
    Stp(Register(X29), Register(X30), MemoryOperand(sp, -16, PREINDEX));
    Mov(Register(X29), Register(SP));
}

void ExtendedAssembler::RestoreFpAndLr()
{
    Register sp(SP);
    Ldp(Register(X29), Register(X30), MemoryOperand(sp, 16, POSTINDEX));
}
}  // namespace panda::ecmascript::aarch64