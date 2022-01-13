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

#include "runtime_trampolines.h"
#include "ecmascript/accessor_data.h"
#include "ecmascript/ecma_macros.h"
#include "ecmascript/frames.h"
#include "ecmascript/ic/ic_runtime.h"
#include "ecmascript/ic/profile_type_info.h"
#include "ecmascript/ic/properties_cache.h"
#include "ecmascript/interpreter/interpreter-inl.h"
#include "ecmascript/interpreter/interpreter_assembly.h"
#include "ecmascript/js_object.h"
#include "ecmascript/js_proxy.h"
#include "ecmascript/layout_info.h"
#include "ecmascript/mem/space-inl.h"
#include "ecmascript/message_string.h"
#include "ecmascript/object_factory.h"
#include "ecmascript/tagged_dictionary.h"
#include "libpandabase/utils/string_helpers.h"

namespace panda::ecmascript {
bool RuntimeTrampolines::AddElementInternal(uintptr_t argGlue, JSTaggedType argReceiver, uint32_t argIndex,
                                            JSTaggedType argValue, uint32_t argAttr)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    JSHandle<JSObject> receiver(thread, reinterpret_cast<TaggedObject *>(argReceiver));
    JSHandle<JSTaggedValue> value(thread, JSTaggedValue(reinterpret_cast<TaggedObject *>(argValue)));
    auto attr = static_cast<PropertyAttributes>(argAttr);
    return JSObject::AddElementInternal(thread, receiver, argIndex, value, attr);
}

bool RuntimeTrampolines::CallSetter(uintptr_t argGlue, JSTaggedType argSetter, JSTaggedType argReceiver,
                                    JSTaggedType argValue, bool argMayThrow)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    JSHandle<JSTaggedValue> receiver(thread, JSTaggedValue(reinterpret_cast<TaggedObject *>(argReceiver)));
    JSHandle<JSTaggedValue> value(thread, JSTaggedValue(reinterpret_cast<TaggedObject *>(argValue)));
    auto setter = AccessorData::Cast((reinterpret_cast<TaggedObject *>(argSetter)));
    return JSObject::CallSetter(thread, *setter, receiver, value, argMayThrow);
}

JSTaggedType RuntimeTrampolines::CallSetter2(uintptr_t argGlue, JSTaggedType argReceiver, JSTaggedType argValue,
                                             JSTaggedType argAccessor)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    [[maybe_unused]] EcmaHandleScope handleScope(thread);

    JSHandle<JSTaggedValue> objHandle(thread, JSTaggedValue(reinterpret_cast<TaggedObject *>(argReceiver)));
    JSHandle<JSTaggedValue> valueHandle(thread, JSTaggedValue(reinterpret_cast<TaggedObject *>(argValue)));

    auto accessor = AccessorData::Cast(JSTaggedValue(argAccessor).GetTaggedObject());
    bool success = JSObject::CallSetter(thread, *accessor, objHandle, valueHandle, true);
    return success ? JSTaggedValue::Undefined().GetRawData() : JSTaggedValue::Exception().GetRawData();
}

JSTaggedType RuntimeTrampolines::CallGetter2(uintptr_t argGlue, JSTaggedType argReceiver, JSTaggedType argHolder,
                                             JSTaggedType argAccessor)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    AccessorData *accessor = AccessorData::Cast(JSTaggedValue(argAccessor).GetTaggedObject());
    if (UNLIKELY(accessor->IsInternal())) {
        JSHandle<JSObject> objHandle(thread, JSTaggedValue(argHolder));
        return accessor->CallInternalGet(thread, objHandle).GetRawData();
    }
    JSHandle<JSTaggedValue> objHandle(thread, JSTaggedValue(argReceiver));
    return JSObject::CallGetter(thread, accessor, objHandle).GetRawData();
}

void RuntimeTrampolines::ThrowTypeError(uintptr_t argGlue, int argMessageStringId)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    std::string message = MessageString::GetMessageString(argMessageStringId);
    ObjectFactory *factory = JSThread::Cast(thread)->GetEcmaVM()->GetFactory();
    JSHandle<JSObject> error = factory->GetJSError(ErrorType::TYPE_ERROR, message.c_str());
    THROW_NEW_ERROR_AND_RETURN(thread, error.GetTaggedValue());
}

bool RuntimeTrampolines::JSProxySetProperty(uintptr_t argGlue, JSTaggedType argProxy, JSTaggedType argKey,
                                            JSTaggedType argValue, JSTaggedType argReceiver, bool argMayThrow)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    JSHandle<JSProxy> proxy(thread, JSTaggedValue(reinterpret_cast<TaggedObject *>(argProxy)));
    JSHandle<JSTaggedValue> index(thread, JSTaggedValue(reinterpret_cast<TaggedObject *>(argKey)));
    JSHandle<JSTaggedValue> value(thread, JSTaggedValue(reinterpret_cast<TaggedObject *>(argValue)));
    JSHandle<JSTaggedValue> receiver(thread, JSTaggedValue(reinterpret_cast<TaggedObject *>(argReceiver)));

    return JSProxy::SetProperty(thread, proxy, index, value, receiver, argMayThrow);
}

uint32_t RuntimeTrampolines::GetHash32(uintptr_t key, uint32_t len)
{
    auto pkey = reinterpret_cast<uint8_t *>(key);
    return panda::GetHash32(pkey, static_cast<size_t>(len));
}

JSTaggedType RuntimeTrampolines::CallGetter(uintptr_t argGlue, JSTaggedType argGetter, JSTaggedType argReceiver)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    auto accessor = AccessorData::Cast(reinterpret_cast<TaggedObject *>(argGetter));
    JSHandle<JSTaggedValue> objHandle(thread, JSTaggedValue(reinterpret_cast<TaggedObject *>(argReceiver)));
    return JSObject::CallGetter(thread, accessor, objHandle).GetRawData();
}

JSTaggedType RuntimeTrampolines::CallInternalGetter(uintptr_t argGlue, JSTaggedType argAccessor,
                                                    JSTaggedType argReceiver)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    auto accessor = AccessorData::Cast(reinterpret_cast<TaggedObject *>(argAccessor));
    JSHandle<JSObject> objHandle(thread, JSTaggedValue(reinterpret_cast<TaggedObject *>(argReceiver)));
    return accessor->CallInternalGet(thread, objHandle).GetRawData();
}

int32_t RuntimeTrampolines::FindElementWithCache(uintptr_t argGlue, JSTaggedType hClass, JSTaggedType key,
                                                 int32_t num)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    auto cls  = reinterpret_cast<JSHClass *>(hClass);
    auto layoutInfo = LayoutInfo::Cast(cls->GetLayout().GetTaggedObject());
    PropertiesCache *cache = thread->GetPropertiesCache();
    int index = cache->Get(cls, JSTaggedValue(key));
    if (index == PropertiesCache::NOT_FOUND) {
        index = layoutInfo->BinarySearch(JSTaggedValue(key), num);
        cache->Set(cls, JSTaggedValue(key), index);
    }
    return index;
}

uint32_t RuntimeTrampolines::StringGetHashCode(JSTaggedType ecmaString)
{
    auto string = reinterpret_cast<EcmaString *>(ecmaString);
    return string->GetHashcode();
}

void RuntimeTrampolines::PrintHeapReginInfo(uintptr_t argGlue)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    thread->GetEcmaVM()->GetHeap()->GetNewSpace()->EnumerateRegions([](Region *current) {
        LOG_ECMA(INFO) << "semispace region: " << current << std::endl;
    });
    thread->GetEcmaVM()->GetHeap()->GetOldSpace()->EnumerateRegions([](Region *current) {
        LOG_ECMA(INFO) << "GetOldSpace region: " << current << std::endl;
    });
    thread->GetEcmaVM()->GetHeap()->GetNonMovableSpace()->EnumerateRegions([](Region *current) {
        LOG_ECMA(INFO) << "GetNonMovableSpace region: " << current << std::endl;
    });
    thread->GetEcmaVM()->GetHeap()->GetMachineCodeSpace()->EnumerateRegions([](Region *current) {
        LOG_ECMA(INFO) << "GetMachineCodeSpace region: " << current << std::endl;
    });
}

JSTaggedType RuntimeTrampolines::GetTaggedArrayPtrTest(uintptr_t argGlue)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    // this case static static JSHandle<TaggedArray> arr don't free in first call
    // second call trigger gc.
    // don't call EcmaHandleScope handleScope(thread);
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    static int i = 0;
    static JSHandle<TaggedArray> arr = factory->NewTaggedArray(2);
    if (i == 0) {
        arr->Set(thread, 0, JSTaggedValue(3)); // 3: first element
        arr->Set(thread, 1, JSTaggedValue(4)); // 4: second element
    }
#ifndef NDEBUG
    PrintHeapReginInfo(argGlue);
#endif
    if (i != 0) {
        thread->GetEcmaVM()->CollectGarbage(TriggerGCType::COMPRESS_FULL_GC);
    }
    LOG_ECMA(INFO) << " arr->GetData() " << std::hex << "  " << arr->GetData();
    i++;
    return arr.GetTaggedValue().GetRawData();
}

JSTaggedType RuntimeTrampolines::Execute(uintptr_t argGlue, JSTaggedType argFunc, JSTaggedType thisArg,
                                         uint32_t argc, uintptr_t argArgv)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    auto func = reinterpret_cast<ECMAObject *>(argFunc);
    auto argv = reinterpret_cast<const TaggedType *>(argArgv);
    CallParams params;
    params.callTarget = func;
    params.newTarget = JSTaggedValue::VALUE_UNDEFINED;
    params.thisArg = thisArg;
    params.argc = argc;
    params.argv = argv;

    return EcmaInterpreter::Execute(thread, params).GetRawData();
}

void RuntimeTrampolines::SetValueWithBarrier([[maybe_unused]] uintptr_t argGlue, JSTaggedType argAddr,
                                             size_t argOffset, JSTaggedType argValue)
{
    auto addr = reinterpret_cast<void *>(argAddr);
    auto offset = static_cast<size_t>(argOffset);
    auto value = static_cast<JSTaggedValue>(argValue);
    if (value.IsHeapObject()) {
        WriteBarrier(addr, offset, value.GetRawData());
    }
}

double RuntimeTrampolines::FloatMod(double left, double right)
{
    return std::fmod(left, right);
}

JSTaggedType RuntimeTrampolines::NewInternalString(uintptr_t argGlue, JSTaggedType argKey)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    JSHandle<JSTaggedValue> keyHandle(thread, JSTaggedValue(reinterpret_cast<TaggedObject *>(argKey)));
    return JSTaggedValue(thread->GetEcmaVM()->GetFactory()->InternString(keyHandle)).GetRawData();
}

JSTaggedType RuntimeTrampolines::NewTaggedArray(uintptr_t argGlue, uint32_t length)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    return factory->NewTaggedArray(length).GetTaggedValue().GetRawData();
}

JSTaggedType RuntimeTrampolines::CopyArray(uintptr_t argGlue, JSTaggedType argArray, uint32_t length, uint32_t capacity)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    JSHandle<TaggedArray> array(thread, JSTaggedValue(reinterpret_cast<TaggedObject *>(argArray)));
    return factory->CopyArray(array, length, capacity).GetTaggedValue().GetRawData();
}

JSTaggedType RuntimeTrampolines::NameDictPutIfAbsent(uintptr_t argGlue, JSTaggedType receiver, JSTaggedType array,
    JSTaggedType key, JSTaggedType value, uint32_t attr, bool needTransToDict)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    JSHandle<JSTaggedValue> keyHandle(thread, JSTaggedValue(reinterpret_cast<TaggedObject *>(key)));
    JSHandle<JSTaggedValue> valueHandle(thread, JSTaggedValue(reinterpret_cast<TaggedObject *>(value)));
    PropertyAttributes propAttr(attr);
    if (needTransToDict) {
        JSHandle<JSObject> objHandle(thread, JSTaggedValue(reinterpret_cast<TaggedObject *>(receiver)));
        JSHandle<NameDictionary> dictHandle(JSObject::TransitionToDictionary(thread, objHandle));
        return NameDictionary::
            PutIfAbsent(thread, dictHandle, keyHandle, valueHandle, propAttr).GetTaggedValue().GetRawData();
    } else {
        JSHandle<NameDictionary> dictHandle(thread, JSTaggedValue(reinterpret_cast<TaggedObject *>(array)));
        return NameDictionary::
            PutIfAbsent(thread, dictHandle, keyHandle, valueHandle, propAttr).GetTaggedValue().GetRawData();
    }
}

void RuntimeTrampolines::PropertiesSetValue(uintptr_t argGlue, JSTaggedType argReceiver, JSTaggedType argValue,
                                            JSTaggedType argArray, uint32_t capacity, uint32_t index)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    JSHandle<TaggedArray> properties;
    JSHandle<JSObject> objHandle(thread, reinterpret_cast<JSObject *>(argReceiver));
    JSHandle<JSTaggedValue> valueHandle(thread, JSTaggedValue(argValue));
    if (capacity == 0) {
        capacity = JSObject::MIN_PROPERTIES_LENGTH;
        properties = factory->NewTaggedArray(capacity);
    } else {
        auto arrayHandle = JSHandle<TaggedArray>(thread, reinterpret_cast<TaggedArray *>(argArray));
        properties = factory->CopyArray(arrayHandle, capacity,
                                        JSObject::ComputePropertyCapacity(capacity));
    }
    properties->Set(thread, index, valueHandle);
    objHandle->SetProperties(thread, properties);
}

JSTaggedType RuntimeTrampolines::TaggedArraySetValue(uintptr_t argGlue, JSTaggedType argReceiver,
                                                     JSTaggedType argValue, JSTaggedType argElement,
                                                     uint32_t elementIndex, uint32_t capacity)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    auto elements = reinterpret_cast<TaggedArray *>(argElement);
    JSTaggedValue value(argValue);
    if (elementIndex >= capacity) {
        if (JSObject::ShouldTransToDict(capacity, elementIndex)) {
            return JSTaggedValue::Hole().GetRawData();
        }
        [[maybe_unused]] EcmaHandleScope handleScope(thread);
        JSHandle<JSObject> receiverHandle(thread, reinterpret_cast<JSObject *>(argReceiver));
        JSHandle<JSTaggedValue> valueHandle(thread, value);
        elements = *JSObject::GrowElementsCapacity(thread, receiverHandle,
                                                   JSObject::ComputeElementCapacity(elementIndex + 1));
        receiverHandle->SetElements(thread, JSTaggedValue(elements));
        elements->Set(thread, elementIndex, valueHandle);
        return JSTaggedValue::Undefined().GetRawData();
    }
    elements->Set(thread, elementIndex, value);
    return JSTaggedValue::Undefined().GetRawData();
}

JSTaggedType RuntimeTrampolines::NewEcmaDynClass(uintptr_t argGlue, uint32_t size, uint32_t type, uint32_t inlinedProps)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    return (thread->GetEcmaVM()->GetFactory()->NewEcmaDynClass(size, JSType(type), inlinedProps)).
        GetTaggedValue().GetRawData();
}

void RuntimeTrampolines::UpdateLayOutAndAddTransition(uintptr_t argGlue, JSTaggedType oldHClass,
                                                      JSTaggedType newHClass, JSTaggedType key, uint32_t attr)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    auto factory = thread->GetEcmaVM()->GetFactory();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    JSHandle<JSHClass> oldHClassHandle(thread, reinterpret_cast<JSHClass *>(oldHClass));
    JSHandle<JSHClass> newHClassHandle(thread, reinterpret_cast<JSHClass *>(newHClass));
    JSHandle<JSTaggedValue> keyHandle(thread, JSTaggedValue(reinterpret_cast<TaggedObject *>(key)));
    PropertyAttributes attrValue(attr);
    int offset = attrValue.GetOffset();
    newHClassHandle->IncNumberOfProps();

    {
        JSMutableHandle<LayoutInfo> layoutInfoHandle(thread, newHClassHandle->GetLayout());

        if (layoutInfoHandle->NumberOfElements() != offset) {
            layoutInfoHandle.Update(factory->CopyAndReSort(layoutInfoHandle, offset, offset + 1));
            newHClassHandle->SetLayout(thread, layoutInfoHandle);
        } else if (layoutInfoHandle->GetPropertiesCapacity() <= offset) {  // need to Grow
            layoutInfoHandle.Update(
                factory->ExtendLayoutInfo(layoutInfoHandle, LayoutInfo::ComputeGrowCapacity(offset)));
            newHClassHandle->SetLayout(thread, layoutInfoHandle);
        }
        layoutInfoHandle->AddKey(thread, offset, keyHandle.GetTaggedValue(), attrValue);
    }

    // 5. Add newDynclass to old dynclass's transitions.
    JSHClass::AddTransitions(thread, oldHClassHandle, newHClassHandle, keyHandle, attrValue);
    return;
}

void RuntimeTrampolines::DebugPrint(int fmtMessageId, ...)
{
    std::string format = MessageString::GetMessageString(fmtMessageId);
    va_list args;
    va_start(args, fmtMessageId);
    std::string result = panda::helpers::string::Vformat(format.c_str(), args);
    std::cerr << result << std::endl;
    va_end(args);
}

void RuntimeTrampolines::NoticeThroughChainAndRefreshUser(uintptr_t argGlue, JSTaggedType argoldHClass,
                                                          JSTaggedType argnewHClass)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    JSHandle<JSHClass> oldHClassHandle(thread, reinterpret_cast<JSHClass *>(argoldHClass));
    JSHandle<JSHClass> newHClassHandle(thread, reinterpret_cast<JSHClass *>(argnewHClass));

    JSHClass::NoticeThroughChain(thread, oldHClassHandle);
    JSHClass::RefreshUsers(thread, oldHClassHandle, newHClassHandle);
}

uintptr_t RuntimeTrampolines::JumpToCInterpreter(uintptr_t argGlue, uintptr_t pc, uintptr_t sp,
    JSTaggedType constpool, JSTaggedType profileTypeInfo, JSTaggedType acc, int32_t hotnessCounter)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    const uint8_t* currentPc = reinterpret_cast<const uint8_t*>(pc);
    JSTaggedType* currentSp = reinterpret_cast<JSTaggedType*>(sp);

    uint8_t opcode = currentPc[0];
    asmDispatchTable[opcode](thread, currentPc, currentSp, JSTaggedValue(constpool),
        JSTaggedValue(profileTypeInfo), JSTaggedValue(acc), hotnessCounter);
    sp = reinterpret_cast<uintptr_t>(thread->GetCurrentSPFrame());
    InterpretedFrame *frame = GET_FRAME(sp);
    return reinterpret_cast<uintptr_t>(frame->pc);
}

JSTaggedType RuntimeTrampolines::IncDyn(uintptr_t argGlue, JSTaggedType value)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    return SlowRuntimeStub::IncDyn(thread, JSTaggedValue(value)).GetRawData();
}

JSTaggedType RuntimeTrampolines::DecDyn(uintptr_t argGlue, JSTaggedType value)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    return SlowRuntimeStub::DecDyn(thread, JSTaggedValue(value)).GetRawData();
}

JSTaggedType RuntimeTrampolines::ExpDyn(uintptr_t argGlue, JSTaggedType base, JSTaggedType exponent)
{
    JSTaggedValue baseValue(base);
    JSTaggedValue exponentValue(exponent);
    if (baseValue.IsNumber() && exponentValue.IsNumber()) {
        // fast path
        double doubleBase = baseValue.IsInt() ? baseValue.GetInt() : baseValue.GetDouble();
        double doubleExponent = exponentValue.IsInt() ? exponentValue.GetInt() : exponentValue.GetDouble();
        if (std::abs(doubleBase) == 1 && std::isinf(doubleExponent)) {
            return JSTaggedValue(base::NAN_VALUE).GetRawData();
        }
        if ((doubleBase == 0 &&
            ((bit_cast<uint64_t>(doubleBase)) & base::DOUBLE_SIGN_MASK) == base::DOUBLE_SIGN_MASK) &&
            std::isfinite(doubleExponent) && base::NumberHelper::TruncateDouble(doubleExponent) == doubleExponent &&
            base::NumberHelper::TruncateDouble(doubleExponent / 2) + base::HALF ==  // 2 : half
            (doubleExponent / 2)) {  // 2 : half
            if (doubleExponent > 0) {
                return JSTaggedValue(-0.0).GetRawData();
            }
            if (doubleExponent < 0) {
                return JSTaggedValue(-base::POSITIVE_INFINITY).GetRawData();
            }
        }
        return JSTaggedValue(std::pow(doubleBase, doubleExponent)).GetRawData();
    }
    // slow path
    auto thread = JSThread::GlueToJSThread(argGlue);
    return SlowRuntimeStub::ExpDyn(thread, baseValue, exponentValue).GetRawData();
}

JSTaggedType RuntimeTrampolines::IsInDyn(uintptr_t argGlue, JSTaggedType prop, JSTaggedType obj)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    return SlowRuntimeStub::IsInDyn(thread, JSTaggedValue(prop), JSTaggedValue(obj)).GetRawData();
}

JSTaggedType RuntimeTrampolines::InstanceOfDyn(uintptr_t argGlue, JSTaggedType obj, JSTaggedType target)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    return SlowRuntimeStub::InstanceofDyn(thread, JSTaggedValue(obj), JSTaggedValue(target)).GetRawData();
}

JSTaggedType RuntimeTrampolines::FastStrictNotEqual(JSTaggedType left, JSTaggedType right)
{
    bool result = FastRuntimeStub::FastStrictEqual(JSTaggedValue(left), JSTaggedValue(right));
    if (result) {
        return JSTaggedValue::False().GetRawData();
    }
    return JSTaggedValue::True().GetRawData();
}

JSTaggedType RuntimeTrampolines::FastStrictEqual(JSTaggedType left, JSTaggedType right)
{
    bool result = FastRuntimeStub::FastStrictEqual(JSTaggedValue(left), JSTaggedValue(right));
    if (result) {
        return JSTaggedValue::True().GetRawData();
    }
    return JSTaggedValue::False().GetRawData();
}

JSTaggedType RuntimeTrampolines::CreateGeneratorObj(uintptr_t argGlue, JSTaggedType genFunc)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    return SlowRuntimeStub::CreateGeneratorObj(thread, JSTaggedValue(genFunc)).GetRawData();
}

JSTaggedType RuntimeTrampolines::StGlobalRecord(uintptr_t argGlue, JSTaggedType prop, JSTaggedType value, bool isConst)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    return SlowRuntimeStub::StGlobalRecord(thread, JSTaggedValue(prop), JSTaggedValue(value), isConst).GetRawData();
}

JSTaggedType RuntimeTrampolines::NegDyn(uintptr_t argGlue, JSTaggedType value)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    return SlowRuntimeStub::NegDyn(thread, JSTaggedValue(value)).GetRawData();
}

JSTaggedType RuntimeTrampolines::ResolveClass(uintptr_t argGlue, JSTaggedType ctor, JSTaggedType literal,
    JSTaggedType base, JSTaggedType lexenv, JSTaggedType constpool)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    return SlowRuntimeStub::ResolveClass(thread, JSTaggedValue(ctor), reinterpret_cast<TaggedArray *>(literal),
                JSTaggedValue(base), JSTaggedValue(lexenv), reinterpret_cast<ConstantPool *>(constpool)).GetRawData();
}

JSTaggedType RuntimeTrampolines::CloneClassFromTemplate(uintptr_t argGlue, JSTaggedType ctor, JSTaggedType base,
    JSTaggedType lexenv, JSTaggedType constpool)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    return SlowRuntimeStub::CloneClassFromTemplate(thread, JSTaggedValue(ctor), JSTaggedValue(base),
        JSTaggedValue(lexenv), reinterpret_cast<ConstantPool *>(constpool)).GetRawData();
}

JSTaggedType RuntimeTrampolines::SetClassConstructorLength(uintptr_t argGlue, JSTaggedType ctor, JSTaggedType length)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    return SlowRuntimeStub::SetClassConstructorLength(thread, JSTaggedValue(ctor), JSTaggedValue(length)).GetRawData();
}

JSTaggedType RuntimeTrampolines::UpdateHotnessCounter(uintptr_t argGlue, uintptr_t sp)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    InterpretedFrame *state = GET_FRAME(sp);
    thread->CheckSafepoint();
    if (state->profileTypeInfo == JSTaggedValue::Undefined()) {
        auto thisFunc = state->function;
        auto method = ECMAObject::Cast(thisFunc.GetTaggedObject())->GetCallTarget();
        auto res = SlowRuntimeStub::NotifyInlineCache(
            thread, JSFunction::Cast(thisFunc.GetHeapObject()), method);
            state->profileTypeInfo = res;
    }
    return state->profileTypeInfo.GetRawData();
}

JSTaggedType RuntimeTrampolines::SetPropertyByValue(uintptr_t argGlue, JSTaggedType argRreceiver, JSTaggedType argKey, JSTaggedType argValue)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    return FastRuntimeStub::SetPropertyByValue(thread, JSTaggedValue(argRreceiver), JSTaggedValue(argKey), JSTaggedValue(argValue)).GetRawData();
}

void RuntimeTrampolines::SetFunctionNameNoPrefix(uintptr_t argGlue, JSTaggedType argFunc, JSTaggedType argName)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    JSFunction::SetFunctionNameNoPrefix(thread, reinterpret_cast<JSFunction *>(argFunc), JSTaggedValue(argName));
}

JSTaggedType RuntimeTrampolines::StOwnByValueWithNameSet(uintptr_t argGlue, JSTaggedType obj, JSTaggedType key, JSTaggedType value)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    return SlowRuntimeStub::StOwnByValueWithNameSet(thread, JSTaggedValue(obj), JSTaggedValue(key), JSTaggedValue(value)).GetRawData();
}

JSTaggedType RuntimeTrampolines::StOwnByNameWithNameSet(uintptr_t argGlue, JSTaggedType obj, JSTaggedType prop, JSTaggedType value)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    return SlowRuntimeStub::StOwnByValueWithNameSet(thread, JSTaggedValue(obj), JSTaggedValue(prop), JSTaggedValue(value)).GetRawData();
}

JSTaggedType RuntimeTrampolines::ImportModule(uintptr_t argGlue, JSTaggedType moduleName)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    return SlowRuntimeStub::ImportModule(thread, JSTaggedValue(moduleName)).GetRawData();
}

void RuntimeTrampolines::StModuleVar(uintptr_t argGlue, JSTaggedType exportName, JSTaggedType exportObj)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    SlowRuntimeStub::StModuleVar(thread, JSTaggedValue(exportName), JSTaggedValue(exportObj));
}

JSTaggedType RuntimeTrampolines::LdModvarByName(uintptr_t argGlue, JSTaggedType moduleObj, JSTaggedType itemName)
{
    auto thread = JSThread::GlueToJSThread(argGlue);
    return SlowRuntimeStub::LdModvarByName(thread, JSTaggedValue(moduleObj), JSTaggedValue(itemName)).GetRawData();
}
}  // namespace panda::ecmascript
