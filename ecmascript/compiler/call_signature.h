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
#ifndef ECMASCRIPT_COMPILER_CALL_SIGNATURE_H
#define ECMASCRIPT_COMPILER_CALL_SIGNATURE_H

#include <array>
#include <functional>
#include <memory>

#include "ecmascript/compiler/variable_type.h"
#include "libpandabase/macros.h"
#include "libpandabase/utils/bit_field.h"

namespace panda::ecmascript::kungfu {
class Stub;
class Circuit;

enum class ArgumentsOrder {
    DEFAULT_ORDER,  // Push Arguments in stack from right -> left
};

class CallSignature {
public:
    using TargetConstructor = std::function<void *(void *)>;
    using VariableArgsBits = panda::BitField<bool, 0, 1>;  // 1 variable argument
    enum class TargetKind : uint8_t {
        COMMON_STUB = 0,
        RUNTIME_STUB,
        RUNTIME_STUB_NO_GC,
        BYTECODE_HANDLER,
        JSFUNCTION,

        STUB_BEGIN = COMMON_STUB,
        STUB_END = BYTECODE_HANDLER,
        BCHANDLER_BEGIN = BYTECODE_HANDLER,
        BCHANDLER_END = JSFUNCTION
    };

    explicit CallSignature(std::string name, int flags, int paramCounter, ArgumentsOrder order, VariableType returnType)
        : name_(name), flags_(flags), paramCounter_(paramCounter), order_(order), returnType_(returnType)
    {
    }

    CallSignature() = default;

    ~CallSignature() = default;

    CallSignature(CallSignature const &other)
    {
        name_ = other.name_;
        flags_ = other.flags_;
        paramCounter_ = other.paramCounter_;
        order_ = other.order_;
        kind_ = other.kind_;
        id_ = other.id_;
        returnType_ = other.returnType_;
        constructor_ = other.constructor_;
        if (paramCounter_ > 0 && other.paramsType_ != nullptr) {
            paramsType_ = std::make_unique<std::vector<VariableType>>(paramCounter_);
            for (int i = 0; i < paramCounter_; i++) {
                (*paramsType_)[i] = other.GetParametersType()[i];
            }
        }
    }

    CallSignature &operator=(CallSignature const &other)
    {
        name_ = other.name_;
        flags_ = other.flags_;
        paramCounter_ = other.paramCounter_;
        order_ = other.order_;
        kind_ = other.kind_;
        id_ = other.id_;
        returnType_ = other.returnType_;
        constructor_ = other.constructor_;
        if (paramCounter_ > 0 && other.paramsType_ != nullptr) {
            paramsType_ = std::make_unique<std::vector<VariableType>>(paramCounter_);
            for (int i = 0; i < paramCounter_; i++) {
                (*paramsType_)[i] = other.GetParametersType()[i];
            }
        }
        return *this;
    }

    bool IsCommonStub() const
    {
        return (kind_ == TargetKind::COMMON_STUB);
    }

    bool IsStub() const
    {
        return TargetKind::STUB_BEGIN <= kind_ && kind_ < TargetKind::STUB_END;
    }

    bool IsBCHandler() const
    {
        return TargetKind::BCHANDLER_BEGIN <= kind_ && kind_ < TargetKind::BCHANDLER_END;
    }

    void SetParameters(VariableType *paramsType)
    {
        if (paramCounter_ > 0 && paramsType_ == nullptr) {
            paramsType_ = std::make_unique<std::vector<VariableType>>(paramCounter_);
            for (int i = 0; i < paramCounter_; i++) {
                (*paramsType_)[i] = paramsType[i];
            }
        }
    }

    VariableType *GetParametersType() const
    {
        if (paramsType_ != nullptr) {
            return paramsType_->data();
        } else {
            return nullptr;
        }
    }

    int GetParametersCount() const
    {
        return paramCounter_;
    }

    VariableType GetReturnType() const
    {
        return returnType_;
    }

    ArgumentsOrder GetArgumentsOrder() const
    {
        return order_;
    }

    int GetFlags() const
    {
        return flags_;
    }

    void SetFlags(int flag)
    {
        flags_ = flag;
    }

    bool GetVariableArgs() const
    {
        return VariableArgsBits::Decode(flags_);
    }

    void SetVariableArgs(bool variable)
    {
        uint64_t newVal = VariableArgsBits::Update(flags_, variable);
        SetFlags(newVal);
    }

    TargetKind GetTargetKind() const
    {
        return kind_;
    }

    void SetTargetKind(TargetKind kind)
    {
        kind_ = kind;
    }

    const std::string &GetName()
    {
        return name_;
    }

    void SetConstructor(TargetConstructor ctor)
    {
        constructor_ = ctor;
    }

    TargetConstructor GetConstructor() const
    {
        return constructor_;
    }

    bool HasConstructor() const
    {
        return constructor_ != nullptr;
    }

    int GetID() const
    {
        return id_;
    }

    void SetID(int id)
    {
        id_ = id;
    }

private:
    std::string name_;
    TargetKind kind_ {TargetKind::COMMON_STUB};
    int flags_ {0};
    int paramCounter_ {0};
    int id_ {-1};
    ArgumentsOrder order_ {ArgumentsOrder::DEFAULT_ORDER};

    VariableType returnType_ {VariableType::VOID()};
    std::unique_ptr<std::vector<VariableType>> paramsType_ {nullptr};

    TargetConstructor constructor_ {nullptr};
};

#define EXPLICIT_CALL_SIGNATURE_LIST(V)     \
    V(Add)                                  \
    V(Sub)                                  \
    V(Mul)                                  \
    V(MulGCTest)                            \
    V(Div)                                  \
    V(Mod)                                  \
    V(TypeOf)                               \
    V(Equal)                                \
    V(SetPropertyByName)                    \
    V(SetPropertyByNameWithOwn)             \
    V(SetPropertyByValue)                   \
    V(GetPropertyByName)                    \
    V(GetPropertyByIndex)                   \
    V(SetPropertyByIndex)                   \
    V(GetPropertyByValue)                   \
    V(TryLoadICByName)                      \
    V(TryLoadICByValue)                     \
    V(TryStoreICByName)                     \
    V(TryStoreICByValue)                    \
    V(TestAbsoluteAddressRelocation)        \
    V(GetTaggedArrayPtrTest)                \
    V(BytecodeHandler)                      \
    V(SingleStepDebugging)                  \
    V(AsmInterpreterEntry)                  \
    V(RuntimeCallTrampolineInterpreterAsm)  \
    V(RuntimeCallTrampolineAot)             \
    V(AotCallAotTrampoline)                 \
    V(DebugPrint)                           \
    V(InsertOldToNewRememberedSet)          \
    V(DoubleToInt)                          \
    V(MarkingBarrier)                       \
    V(CallArg0Dyn)                          \
    V(CallArg1Dyn)                          \
    V(CallArgs2Dyn)                         \
    V(CallArgs3Dyn)                         \
    V(CallIThisRangeDyn)                    \
    V(CallIRangeDyn)

#define DECL_CALL_SIGNATURE(name)                                  \
class name##CallSignature final {                                  \
    public:                                                        \
        static void Initialize(CallSignature *descriptor);         \
    };
EXPLICIT_CALL_SIGNATURE_LIST(DECL_CALL_SIGNATURE)
}  // namespace panda::ecmascript::kungfu
#endif  // ECMASCRIPT_COMPILER_CALL_SIGNATURE_H