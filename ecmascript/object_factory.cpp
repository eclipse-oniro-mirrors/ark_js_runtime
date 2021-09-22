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

#include "ecma_string_table.h"
#include "ecma_vm.h"
#include "ecmascript/accessor_data.h"
#include "ecmascript/base/error_helper.h"
#include "ecmascript/builtins.h"
#include "ecmascript/builtins/builtins_errors.h"
#include "ecmascript/builtins/builtins_global.h"
#include "ecmascript/class_linker/program_object.h"
#include "ecmascript/ecma_macros.h"
#include "ecmascript/ecma_module.h"
#include "ecmascript/free_object.h"
#include "ecmascript/global_env.h"
#include "ecmascript/global_env_constants-inl.h"
#include "ecmascript/global_env_constants.h"
#include "ecmascript/ic/ic_handler.h"
#include "ecmascript/ic/profile_type_info.h"
#include "ecmascript/ic/property_box.h"
#include "ecmascript/ic/proto_change_details.h"
#include "ecmascript/internal_call_params.h"
#include "ecmascript/jobs/micro_job_queue.h"
#include "ecmascript/jobs/pending_job.h"
#include "ecmascript/js_arguments.h"
#include "ecmascript/js_array.h"
#include "ecmascript/js_array_iterator.h"
#include "ecmascript/js_arraybuffer.h"
#include "ecmascript/js_async_function.h"
#include "ecmascript/js_dataview.h"
#include "ecmascript/js_date.h"
#include "ecmascript/js_for_in_iterator.h"
#include "ecmascript/js_generator_object.h"
#include "ecmascript/js_hclass-inl.h"
#include "ecmascript/js_hclass.h"
#include "ecmascript/js_iterator.h"
#include "ecmascript/js_map.h"
#include "ecmascript/js_map_iterator.h"
#include "ecmascript/js_object-inl.h"
#include "ecmascript/js_primitive_ref.h"
#include "ecmascript/js_promise.h"
#include "ecmascript/js_proxy.h"
#include "ecmascript/js_realm.h"
#include "ecmascript/js_regexp.h"
#include "ecmascript/js_set.h"
#include "ecmascript/js_set_iterator.h"
#include "ecmascript/js_string_iterator.h"
#include "ecmascript/js_symbol.h"
#include "ecmascript/js_tagged_value-inl.h"
#include "ecmascript/js_thread.h"
#include "ecmascript/js_typed_array.h"
#include "ecmascript/js_weak_container.h"
#include "ecmascript/layout_info-inl.h"
#include "ecmascript/linked_hash_table-inl.h"
#include "ecmascript/mem/ecma_heap_manager.h"
#include "ecmascript/mem/heap-inl.h"
#include "ecmascript/mem/space.h"
#include "ecmascript/record.h"
#include "ecmascript/symbol_table-inl.h"
#include "ecmascript/template_map.h"

namespace panda::ecmascript {
using Error = builtins::BuiltinsError;
using RangeError = builtins::BuiltinsRangeError;
using ReferenceError = builtins::BuiltinsReferenceError;
using TypeError = builtins::BuiltinsTypeError;
using URIError = builtins::BuiltinsURIError;
using SyntaxError = builtins::BuiltinsSyntaxError;
using EvalError = builtins::BuiltinsEvalError;
using ErrorType = base::ErrorType;
using ErrorHelper = base::ErrorHelper;

ObjectFactory::ObjectFactory(JSThread *thread, Heap *heap)
    : thread_(thread), heapHelper_(heap), vm_(thread->GetEcmaVM()), heap_(heap)
{
}

JSHandle<JSHClass> ObjectFactory::NewEcmaDynClass(JSHClass *hclass, uint32_t size, JSType type)
{
    NewObjectHook();
    uint32_t classSize = JSHClass::SIZE;
    auto *newClass = static_cast<JSHClass *>(heapHelper_.AllocateNonMovableOrHugeObject(hclass, classSize));
    newClass->Initialize(thread_, size, type, JSTaggedValue::Null());

    return JSHandle<JSHClass>(thread_, newClass);
}

JSHandle<JSHClass> ObjectFactory::NewEcmaDynClass(uint32_t size, JSType type)
{
    return NewEcmaDynClass(hclassClass_, size, type);
}

void ObjectFactory::ObtainRootClass([[maybe_unused]] const JSHandle<GlobalEnv> &globalEnv)
{
    const GlobalEnvConstants *globalConst = thread_->GlobalConstants();
    hclassClass_ = JSHClass::Cast(globalConst->GetHClassClass().GetTaggedObject());
    stringClass_ = JSHClass::Cast(globalConst->GetStringClass().GetTaggedObject());
    arrayClass_ = JSHClass::Cast(globalConst->GetArrayClass().GetTaggedObject());
    dictionaryClass_ = JSHClass::Cast(globalConst->GetDictionaryClass().GetTaggedObject());
    jsNativePointerClass_ = JSHClass::Cast(globalConst->GetJSNativePointerClass().GetTaggedObject());
    freeObjectWithNoneFieldClass_ = JSHClass::Cast(globalConst->GetFreeObjectWithNoneFieldClass().GetTaggedObject());
    freeObjectWithOneFieldClass_ = JSHClass::Cast(globalConst->GetFreeObjectWithOneFieldClass().GetTaggedObject());
    freeObjectWithTwoFieldClass_ = JSHClass::Cast(globalConst->GetFreeObjectWithTwoFieldClass().GetTaggedObject());

    completionRecordClass_ = JSHClass::Cast(globalConst->GetCompletionRecordClass().GetTaggedObject());
    generatorContextClass_ = JSHClass::Cast(globalConst->GetGeneratorContextClass().GetTaggedObject());
    programClass_ = JSHClass::Cast(globalConst->GetProgramClass().GetTaggedObject());
    ecmaModuleClass_ = JSHClass::Cast(globalConst->GetEcmaModuleClass().GetTaggedObject());
    envClass_ = JSHClass::Cast(globalConst->GetEnvClass().GetTaggedObject());
    symbolClass_ = JSHClass::Cast(globalConst->GetSymbolClass().GetTaggedObject());
    accessorDataClass_ = JSHClass::Cast(globalConst->GetAccessorDataClass().GetTaggedObject());
    internalAccessorClass_ = JSHClass::Cast(globalConst->GetInternalAccessorClass().GetTaggedObject());
    capabilityRecordClass_ = JSHClass::Cast(globalConst->GetCapabilityRecordClass().GetTaggedObject());
    reactionsRecordClass_ = JSHClass::Cast(globalConst->GetReactionsRecordClass().GetTaggedObject());
    promiseIteratorRecordClass_ = JSHClass::Cast(globalConst->GetPromiseIteratorRecordClass().GetTaggedObject());
    microJobQueueClass_ = JSHClass::Cast(globalConst->GetMicroJobQueueClass().GetTaggedObject());
    pendingJobClass_ = JSHClass::Cast(globalConst->GetPendingJobClass().GetTaggedObject());
    jsProxyOrdinaryClass_ = JSHClass::Cast(globalConst->GetJSProxyOrdinaryClass().GetTaggedObject());
    jsProxyCallableClass_ = JSHClass::Cast(globalConst->GetJSProxyCallableClass().GetTaggedObject());
    jsProxyConstructClass_ = JSHClass::Cast(globalConst->GetJSProxyConstructClass().GetTaggedObject());
    objectWrapperClass_ = JSHClass::Cast(globalConst->GetObjectWrapperClass().GetTaggedObject());
    PropertyBoxClass_ = JSHClass::Cast(globalConst->GetPropertyBoxClass().GetTaggedObject());
    protoChangeMarkerClass_ = JSHClass::Cast(globalConst->GetProtoChangeMarkerClass().GetTaggedObject());
    protoChangeDetailsClass_ = JSHClass::Cast(globalConst->GetProtoChangeDetailsClass().GetTaggedObject());
    promiseRecordClass_ = JSHClass::Cast(globalConst->GetPromiseRecordClass().GetTaggedObject());
    promiseResolvingFunctionsRecord_ =
        JSHClass::Cast(globalConst->GetPromiseResolvingFunctionsRecordClass().GetTaggedObject());
    transitionHandlerClass_ = JSHClass::Cast(globalConst->GetTransitionHandlerClass().GetTaggedObject());
    prototypeHandlerClass_ = JSHClass::Cast(globalConst->GetPrototypeHandlerClass().GetTaggedObject());
    functionExtraInfo_ = JSHClass::Cast(globalConst->GetFunctionExtraInfoClass().GetTaggedObject());
    jsRealmClass_ = JSHClass::Cast(globalConst->GetJSRealmClass().GetTaggedObject());
    machineCodeClass_ = JSHClass::Cast(globalConst->GetMachineCodeClass().GetTaggedObject());
}

void ObjectFactory::InitObjectFields(const TaggedObject *object)
{
    auto *klass = object->GetClass();
    auto objBodySize = klass->GetObjectSize() - TaggedObject::TaggedObjectSize();
    ASSERT(objBodySize % JSTaggedValue::TaggedTypeSize() == 0);
    int numOfFields = static_cast<int>(objBodySize / JSTaggedValue::TaggedTypeSize());
    size_t addr = reinterpret_cast<uintptr_t>(object) + TaggedObject::TaggedObjectSize();
    for (int i = 0; i < numOfFields; i++) {
        auto *fieldAddr = reinterpret_cast<JSTaggedType *>(addr + i * JSTaggedValue::TaggedTypeSize());
        *fieldAddr = JSTaggedValue::Undefined().GetRawData();
    }
}

void ObjectFactory::NewJSArrayBufferData(const JSHandle<JSArrayBuffer> &array, int32_t length)
{
    if (length == 0) {
        return;
    }

    JSTaggedValue data = array->GetArrayBufferData();
    if (data != JSTaggedValue::Undefined()) {
        auto *pointer = JSNativePointer::Cast(data.GetTaggedObject());
        auto newData = vm_->GetRegionFactory()->AllocateBuffer(length * sizeof(uint8_t));
        if (memset_s(newData, length, 0, length) != EOK) {
            LOG_ECMA(FATAL) << "memset_s failed";
            UNREACHABLE();
        }
        pointer->ResetExternalPointer(newData);
        return;
    }

    auto newData = vm_->GetRegionFactory()->AllocateBuffer(length * sizeof(uint8_t));
    if (memset_s(newData, length, 0, length) != EOK) {
        LOG_ECMA(FATAL) << "memset_s failed";
        UNREACHABLE();
    }
    JSHandle<JSNativePointer> pointer = NewJSNativePointer(newData, RegionFactory::FreeBufferFunc,
                                                           vm_->GetRegionFactory());
    array->SetArrayBufferData(thread_, pointer.GetTaggedValue());
    vm_->PushToArrayDataList(*pointer);
}

JSHandle<JSArrayBuffer> ObjectFactory::NewJSArrayBuffer(int32_t length)
{
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();

    JSHandle<JSFunction> constructor(env->GetArrayBufferFunction());
    JSHandle<JSTaggedValue> newTarget(constructor);
    JSHandle<JSArrayBuffer> arrayBuffer(NewJSObjectByConstructor(constructor, newTarget));
    arrayBuffer->SetArrayBufferByteLength(thread_, JSTaggedValue(length));
    if (length > 0) {
        auto newData = vm_->GetRegionFactory()->AllocateBuffer(length);
        if (memset_s(newData, length, 0, length) != EOK) {
            LOG_ECMA(FATAL) << "memset_s failed";
            UNREACHABLE();
        }
        JSHandle<JSNativePointer> pointer = NewJSNativePointer(newData, RegionFactory::FreeBufferFunc,
                                                               vm_->GetRegionFactory());
        arrayBuffer->SetArrayBufferData(thread_, pointer.GetTaggedValue());
        arrayBuffer->SetShared(thread_, JSTaggedValue::False());
        vm_->PushToArrayDataList(*pointer);
    }
    return arrayBuffer;
}

JSHandle<JSArrayBuffer> ObjectFactory::NewJSArrayBuffer(void *buffer, int32_t length, const DeleteEntryPoint &deleter,
                                                        void *data, bool share)
{
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();

    JSHandle<JSFunction> constructor(env->GetArrayBufferFunction());
    JSHandle<JSTaggedValue> newTarget(constructor);
    JSHandle<JSArrayBuffer> arrayBuffer(NewJSObjectByConstructor(constructor, newTarget));
    length = buffer == nullptr ? 0 : length;
    arrayBuffer->SetArrayBufferByteLength(thread_, JSTaggedValue(length));
    if (length > 0) {
        JSHandle<JSNativePointer> pointer = NewJSNativePointer(buffer, deleter, data);
        arrayBuffer->SetArrayBufferData(thread_, pointer.GetTaggedValue());
        arrayBuffer->SetShared(thread_, JSTaggedValue(share));
        vm_->PushToArrayDataList(*pointer);
    }
    return arrayBuffer;
}

JSHandle<JSDataView> ObjectFactory::NewJSDataView(JSHandle<JSArrayBuffer> buffer, int32_t offset, int32_t length)
{
    JSTaggedValue arrayLength = buffer->GetArrayBufferByteLength();
    if (!arrayLength.IsNumber()) {
        THROW_TYPE_ERROR_AND_RETURN(thread_, "ArrayBuffer length error",
                                    JSHandle<JSDataView>(thread_, JSTaggedValue::Undefined()));
    }
    if (offset + length > arrayLength.GetNumber()) {
        THROW_TYPE_ERROR_AND_RETURN(thread_, "offset or length error",
                                    JSHandle<JSDataView>(thread_, JSTaggedValue::Undefined()));
    }
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();

    JSHandle<JSFunction> constructor(env->GetDataViewFunction());
    JSHandle<JSTaggedValue> newTarget(constructor);
    JSHandle<JSDataView> arrayBuffer(NewJSObjectByConstructor(constructor, newTarget));
    arrayBuffer->SetDataView(thread_, JSTaggedValue::True());
    arrayBuffer->SetViewedArrayBuffer(thread_, buffer.GetTaggedValue());
    arrayBuffer->SetByteLength(thread_, JSTaggedValue(length));
    arrayBuffer->SetByteOffset(thread_, JSTaggedValue(offset));
    return arrayBuffer;
}

void ObjectFactory::NewJSRegExpByteCodeData(const JSHandle<JSRegExp> &regexp, void *buffer, size_t size)
{
    if (buffer == nullptr) {
        return;
    }

    auto newBuffer = vm_->GetRegionFactory()->AllocateBuffer(size);
    if (memcpy_s(newBuffer, size, buffer, size) != EOK) {
        LOG_ECMA(FATAL) << "memcpy_s failed";
        UNREACHABLE();
    }
    JSTaggedValue data = regexp->GetByteCodeBuffer();
    if (data != JSTaggedValue::Undefined()) {
        JSNativePointer *native = JSNativePointer::Cast(data.GetTaggedObject());
        native->ResetExternalPointer(newBuffer);
        return;
    }
    JSHandle<JSNativePointer> pointer = NewJSNativePointer(newBuffer, RegionFactory::FreeBufferFunc,
                                                           vm_->GetRegionFactory());
    regexp->SetByteCodeBuffer(thread_, pointer.GetTaggedValue());
    regexp->SetLength(thread_, JSTaggedValue(static_cast<uint32_t>(size)));

    // push uint8_t* to ecma array_data_list
    vm_->PushToArrayDataList(*pointer);
}

JSHandle<JSHClass> ObjectFactory::NewEcmaDynClass(uint32_t size, JSType type, const JSHandle<JSTaggedValue> &prototype)
{
    NewObjectHook();
    JSHandle<JSHClass> newClass = NewEcmaDynClass(size, type);
    newClass->SetPrototype(thread_, prototype.GetTaggedValue());
    return newClass;
}

JSHandle<JSObject> ObjectFactory::NewJSObject(const JSHandle<JSHClass> &jshclass)
{
    NewObjectHook();
    JSHandle<JSObject> obj(thread_, JSObject::Cast(NewDynObject(jshclass, JSHClass::DEFAULT_CAPACITY_OF_IN_OBJECTS)));
    JSHandle<TaggedArray> emptyArray = EmptyArray();
    obj->InitializeHash();
    obj->SetElements(thread_, emptyArray, SKIP_BARRIER);
    obj->SetProperties(thread_, emptyArray, SKIP_BARRIER);
    return obj;
}

JSHandle<TaggedArray> ObjectFactory::CloneProperties(const JSHandle<TaggedArray> &old)
{
    array_size_t newLength = old->GetLength();
    if (newLength == 0) {
        return EmptyArray();
    }
    NewObjectHook();
    auto klass = old->GetClass();
    size_t size = TaggedArray::ComputeSize(JSTaggedValue::TaggedTypeSize(), newLength);
    auto header = heapHelper_.AllocateYoungGenerationOrHugeObject(klass, size);
    JSHandle<TaggedArray> newArray(thread_, header);
    newArray->SetLength(newLength);

    for (array_size_t i = 0; i < newLength; i++) {
        JSTaggedValue value = old->Get(i);
        newArray->Set(thread_, i, value);
    }
    return newArray;
}

JSHandle<JSObject> ObjectFactory::CloneObjectLiteral(JSHandle<JSObject> object)
{
    NewObjectHook();
    auto klass = object->GetClass();

    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(klass);
    JSHandle<JSObject> cloneObject(thread_, JSObject::Cast(header));

    JSHandle<TaggedArray> elements(thread_, object->GetElements());
    auto newElements = CloneProperties(elements);
    cloneObject->SetElements(thread_, newElements.GetTaggedValue());

    JSHandle<TaggedArray> properties(thread_, object->GetProperties());
    auto newProperties = CloneProperties(properties);
    cloneObject->SetProperties(thread_, newProperties.GetTaggedValue());

    for (int i = 0; i < JSHClass::DEFAULT_CAPACITY_OF_IN_OBJECTS; i++) {
        cloneObject->SetPropertyInlinedProps(thread_, i, object->GetPropertyInlinedProps(i));
    }
    return cloneObject;
}

JSHandle<JSArray> ObjectFactory::CloneArrayLiteral(JSHandle<JSArray> object)
{
    NewObjectHook();
    auto klass = object->GetClass();

    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(klass);
    JSHandle<JSArray> cloneObject(thread_, JSObject::Cast(header));

    JSHandle<TaggedArray> elements(thread_, object->GetElements());
    auto newElements = CopyArray(elements, elements->GetLength(), elements->GetLength());
    cloneObject->SetElements(thread_, newElements.GetTaggedValue());

    JSHandle<TaggedArray> properties(thread_, object->GetProperties());
    auto newProperties = CopyArray(properties, properties->GetLength(), properties->GetLength());
    cloneObject->SetProperties(thread_, newProperties.GetTaggedValue());

    cloneObject->SetArrayLength(thread_, object->GetArrayLength());
    for (int i = 0; i < JSHClass::DEFAULT_CAPACITY_OF_IN_OBJECTS; i++) {
        cloneObject->SetPropertyInlinedProps(thread_, i, object->GetPropertyInlinedProps(i));
    }
    return cloneObject;
}

JSHandle<TaggedArray> ObjectFactory::CloneProperties(const JSHandle<TaggedArray> &old,
                                                     const JSHandle<JSTaggedValue> &env, const JSHandle<JSObject> &obj,
                                                     const JSHandle<JSTaggedValue> &constpool)
{
    array_size_t newLength = old->GetLength();
    if (newLength == 0) {
        return EmptyArray();
    }
    NewObjectHook();
    auto klass = old->GetClass();
    size_t size = TaggedArray::ComputeSize(JSTaggedValue::TaggedTypeSize(), newLength);
    auto header = heapHelper_.AllocateYoungGenerationOrHugeObject(klass, size);
    JSHandle<TaggedArray> newArray(thread_, header);
    newArray->SetLength(newLength);

    for (array_size_t i = 0; i < newLength; i++) {
        JSTaggedValue value = old->Get(i);
        if (!value.IsJSFunction()) {
            newArray->Set(thread_, i, value);
        } else {
            JSHandle<JSFunction> valueHandle(thread_, value);
            JSHandle<JSFunction> newFunc = CloneJSFuction(valueHandle, valueHandle->GetFunctionKind());
            newFunc->SetLexicalEnv(thread_, env);
            newFunc->SetHomeObject(thread_, obj);
            newFunc->SetConstantPool(thread_, constpool);
            newArray->Set(thread_, i, newFunc);
        }
    }
    return newArray;
}

JSHandle<JSObject> ObjectFactory::CloneObjectLiteral(JSHandle<JSObject> object, const JSHandle<JSTaggedValue> &env,
                                                     const JSHandle<JSTaggedValue> &constpool)
{
    NewObjectHook();
    auto klass = object->GetClass();

    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(klass);
    JSHandle<JSObject> cloneObject(thread_, JSObject::Cast(header));

    JSHandle<TaggedArray> elements(thread_, object->GetElements());
    auto newElements = CloneProperties(elements, env, cloneObject, constpool);
    cloneObject->SetElements(thread_, newElements.GetTaggedValue());

    JSHandle<TaggedArray> properties(thread_, object->GetProperties());
    auto newProperties = CloneProperties(properties, env, cloneObject, constpool);
    cloneObject->SetProperties(thread_, newProperties.GetTaggedValue());

    for (int i = 0; i < JSHClass::DEFAULT_CAPACITY_OF_IN_OBJECTS; i++) {
        JSTaggedValue value = object->GetPropertyInlinedProps(i);
        if (!value.IsJSFunction()) {
            cloneObject->SetPropertyInlinedProps(thread_, i, value);
        } else {
            JSHandle<JSFunction> valueHandle(thread_, value);
            JSHandle<JSFunction> newFunc = CloneJSFuction(valueHandle, valueHandle->GetFunctionKind());
            newFunc->SetLexicalEnv(thread_, env);
            newFunc->SetHomeObject(thread_, cloneObject);
            newFunc->SetConstantPool(thread_, constpool);
            cloneObject->SetPropertyInlinedProps(thread_, i, newFunc.GetTaggedValue());
        }
    }
    return cloneObject;
}

JSHandle<JSFunction> ObjectFactory::CloneJSFuction(JSHandle<JSFunction> obj, FunctionKind kind)
{
    NewObjectHook();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSHClass> jshclass(thread_, obj->GetJSHClass());
    JSHandle<JSFunction> cloneFunc = NewJSFunctionByDynClass(obj->GetCallTarget(), jshclass, kind);
    if (kind == FunctionKind::GENERATOR_FUNCTION) {
        JSHandle<JSTaggedValue> objFun = env->GetObjectFunction();
        JSHandle<JSObject> initialGeneratorFuncPrototype =
            NewJSObjectByConstructor(JSHandle<JSFunction>(objFun), objFun);
        JSObject::SetPrototype(thread_, initialGeneratorFuncPrototype, env->GetGeneratorPrototype());
        cloneFunc->SetProtoOrDynClass(thread_, initialGeneratorFuncPrototype);
    }

    JSTaggedValue length = obj->GetPropertyInlinedProps(JSFunction::LENGTH_INLINE_PROPERTY_INDEX);
    cloneFunc->SetPropertyInlinedProps(thread_, JSFunction::LENGTH_INLINE_PROPERTY_INDEX, length);
    return cloneFunc;
}

JSHandle<JSObject> ObjectFactory::NewNonMovableJSObject(const JSHandle<JSHClass> &jshclass)
{
    NewObjectHook();
    JSHandle<JSObject> obj(thread_,
                           JSObject::Cast(NewNonMovableDynObject(jshclass, JSHClass::DEFAULT_CAPACITY_OF_IN_OBJECTS)));
    obj->SetElements(thread_, EmptyArray(), SKIP_BARRIER);
    obj->SetProperties(thread_, EmptyArray(), SKIP_BARRIER);
    return obj;
}

JSHandle<JSPrimitiveRef> ObjectFactory::NewJSPrimitiveRef(const JSHandle<JSHClass> &dynKlass,
                                                          const JSHandle<JSTaggedValue> &object)
{
    NewObjectHook();
    JSHandle<JSPrimitiveRef> obj = JSHandle<JSPrimitiveRef>::Cast(NewJSObject(dynKlass));
    obj->SetValue(thread_, object);
    return obj;
}

JSHandle<JSArray> ObjectFactory::NewJSArray()
{
    NewObjectHook();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSTaggedValue> function = env->GetArrayFunction();

    return JSHandle<JSArray>(NewJSObjectByConstructor(JSHandle<JSFunction>(function), function));
}

JSHandle<JSForInIterator> ObjectFactory::NewJSForinIterator(const JSHandle<JSTaggedValue> &obj)
{
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSHClass> dynclass(env->GetForinIteratorClass());

    JSHandle<JSForInIterator> it = JSHandle<JSForInIterator>::Cast(NewJSObject(dynclass));
    it->SetObject(thread_, obj);
    it->SetWasVisited(thread_, JSTaggedValue::False());
    it->SetVisitedKeys(thread_, env->GetEmptyTaggedQueue());
    it->SetRemainingKeys(thread_, env->GetEmptyTaggedQueue());
    return it;
}

JSHandle<JSHClass> ObjectFactory::CreateJSRegExpInstanceClass(JSHandle<JSTaggedValue> proto)
{
    const GlobalEnvConstants *globalConst = thread_->GlobalConstants();
    JSHandle<JSHClass> regexpDynclass = NewEcmaDynClass(JSRegExp::SIZE, JSType::JS_REG_EXP, proto);

    uint32_t fieldOrder = 0;
    JSHandle<LayoutInfo> layoutInfoHandle = CreateLayoutInfo(1);
    {
        PropertyAttributes attributes = PropertyAttributes::Default(true, false, false);
        attributes.SetIsInlinedProps(true);
        attributes.SetRepresentation(Representation::MIXED);
        attributes.SetOffset(fieldOrder++);
        layoutInfoHandle->AddKey(thread_, 0, globalConst->GetLastIndexString(), attributes);
    }

    {
        regexpDynclass->SetAttributes(thread_, layoutInfoHandle);
        uint32_t unusedInlinedProps = regexpDynclass->GetUnusedInlinedProps();
        ASSERT(unusedInlinedProps >= fieldOrder);
        regexpDynclass->SetUnusedInlinedProps(unusedInlinedProps - fieldOrder);
    }

    return regexpDynclass;
}

JSHandle<JSHClass> ObjectFactory::CreateJSArrayInstanceClass(JSHandle<JSTaggedValue> proto)
{
    const GlobalEnvConstants *globalConst = thread_->GlobalConstants();
    JSHandle<JSHClass> arrayDynclass = NewEcmaDynClass(JSArray::SIZE, JSType::JS_ARRAY, proto);

    uint32_t fieldOrder = 0;
    ASSERT(JSArray::LENGTH_INLINE_PROPERTY_INDEX == fieldOrder);
    JSHandle<LayoutInfo> layoutInfoHandle = CreateLayoutInfo(1);
    {
        PropertyAttributes attributes = PropertyAttributes::DefaultAccessor(true, false, false);
        attributes.SetIsInlinedProps(true);
        attributes.SetRepresentation(Representation::MIXED);
        attributes.SetOffset(fieldOrder++);
        layoutInfoHandle->AddKey(thread_, 0, globalConst->GetLengthString(), attributes);
    }

    {
        arrayDynclass->SetAttributes(thread_, layoutInfoHandle);
        uint32_t unusedInlinedProps = arrayDynclass->GetUnusedInlinedProps();
        ASSERT(unusedInlinedProps >= fieldOrder);
        arrayDynclass->SetUnusedInlinedProps(unusedInlinedProps - fieldOrder);
    }
    arrayDynclass->SetIsStableElements(true);
    arrayDynclass->SetHasConstructor(false);

    return arrayDynclass;
}

JSHandle<JSHClass> ObjectFactory::CreateJSArguments()
{
    JSHandle<GlobalEnv> env = thread_->GetEcmaVM()->GetGlobalEnv();
    const GlobalEnvConstants *globalConst = thread_->GlobalConstants();
    JSHandle<JSTaggedValue> proto = env->GetObjectFunctionPrototype();

    JSHandle<JSHClass> argumentsDynclass = NewEcmaDynClass(JSArguments::SIZE, JSType::JS_ARGUMENTS, proto);

    uint32_t fieldOrder = 0;
    ASSERT(JSArguments::LENGTH_INLINE_PROPERTY_INDEX == fieldOrder);
    JSHandle<LayoutInfo> layoutInfoHandle = CreateLayoutInfo(JSArguments::LENGTH_OF_INLINE_PROPERTIES);
    {
        PropertyAttributes attributes = PropertyAttributes::Default(true, false, true);
        attributes.SetIsInlinedProps(true);
        attributes.SetRepresentation(Representation::MIXED);
        attributes.SetOffset(fieldOrder++);
        layoutInfoHandle->AddKey(thread_, JSArguments::LENGTH_INLINE_PROPERTY_INDEX, globalConst->GetLengthString(),
                                 attributes);
    }

    ASSERT(JSArguments::ITERATOR_INLINE_PROPERTY_INDEX == fieldOrder);
    {
        PropertyAttributes attributes = PropertyAttributes::Default(true, false, true);
        attributes.SetIsInlinedProps(true);
        attributes.SetRepresentation(Representation::MIXED);
        attributes.SetOffset(fieldOrder++);
        layoutInfoHandle->AddKey(thread_, JSArguments::ITERATOR_INLINE_PROPERTY_INDEX,
                                 env->GetIteratorSymbol().GetTaggedValue(), attributes);
    }

    {
        ASSERT(JSArguments::CALLER_INLINE_PROPERTY_INDEX == fieldOrder);
        PropertyAttributes attributes = PropertyAttributes::Default(false, false, false);
        attributes.SetIsInlinedProps(true);
        attributes.SetIsAccessor(true);
        attributes.SetRepresentation(Representation::MIXED);
        attributes.SetOffset(fieldOrder++);
        layoutInfoHandle->AddKey(thread_, JSArguments::CALLER_INLINE_PROPERTY_INDEX,
                                 thread_->GlobalConstants()->GetHandledCallerString().GetTaggedValue(), attributes);
    }

    {
        ASSERT(JSArguments::CALLEE_INLINE_PROPERTY_INDEX == fieldOrder);
        PropertyAttributes attributes = PropertyAttributes::Default(false, false, false);
        attributes.SetIsInlinedProps(true);
        attributes.SetIsAccessor(true);
        attributes.SetRepresentation(Representation::MIXED);
        attributes.SetOffset(fieldOrder++);
        layoutInfoHandle->AddKey(thread_, JSArguments::CALLEE_INLINE_PROPERTY_INDEX,
                                 thread_->GlobalConstants()->GetHandledCalleeString().GetTaggedValue(), attributes);
    }

    {
        argumentsDynclass->SetAttributes(thread_, layoutInfoHandle);
        uint32_t unusedInlinedProps = argumentsDynclass->GetUnusedInlinedProps();
        ASSERT(unusedInlinedProps >= fieldOrder);
        argumentsDynclass->SetUnusedInlinedProps(unusedInlinedProps - fieldOrder);
    }
    argumentsDynclass->SetIsStableElements(true);
    return argumentsDynclass;
}

JSHandle<JSArguments> ObjectFactory::NewJSArguments()
{
    NewObjectHook();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSHClass> dynclass = JSHandle<JSHClass>::Cast(env->GetArgumentsClass());
    JSHandle<JSArguments> obj = JSHandle<JSArguments>::Cast(NewJSObject(dynclass));
    return obj;
}

JSHandle<JSObject> ObjectFactory::GetJSError(const ErrorType &errorType, const char *data)
{
    ASSERT_PRINT(errorType == ErrorType::ERROR || errorType == ErrorType::EVAL_ERROR ||
                     errorType == ErrorType::RANGE_ERROR || errorType == ErrorType::REFERENCE_ERROR ||
                     errorType == ErrorType::SYNTAX_ERROR || errorType == ErrorType::TYPE_ERROR ||
                     errorType == ErrorType::URI_ERROR,
                 "The error type is not in the valid range.");
    NewObjectHook();
    if (data != nullptr) {
        JSHandle<EcmaString> handleMsg = NewFromString(data);
        return NewJSError(errorType, handleMsg);
    }
    JSHandle<EcmaString> emptyString(thread_->GlobalConstants()->GetHandledEmptyString());
    return NewJSError(errorType, emptyString);
}

JSHandle<JSObject> ObjectFactory::NewJSError(const ErrorType &errorType, const JSHandle<EcmaString> &message)
{
    NewObjectHook();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    const GlobalEnvConstants *globalConst = thread_->GlobalConstants();
    JSHandle<JSTaggedValue> nativeConstructor;
    switch (errorType) {
        case ErrorType::RANGE_ERROR:
            nativeConstructor = env->GetRangeErrorFunction();
            break;
        case ErrorType::EVAL_ERROR:
            nativeConstructor = env->GetEvalErrorFunction();
            break;
        case ErrorType::REFERENCE_ERROR:
            nativeConstructor = env->GetReferenceErrorFunction();
            break;
        case ErrorType::TYPE_ERROR:
            nativeConstructor = env->GetTypeErrorFunction();
            break;
        case ErrorType::URI_ERROR:
            nativeConstructor = env->GetURIErrorFunction();
            break;
        case ErrorType::SYNTAX_ERROR:
            nativeConstructor = env->GetSyntaxErrorFunction();
            break;
        default:
            nativeConstructor = env->GetErrorFunction();
            break;
    }
    JSHandle<JSFunction> nativeFunc = JSHandle<JSFunction>::Cast(nativeConstructor);
    JSHandle<JSTaggedValue> nativePrototype(thread_, nativeFunc->GetFunctionPrototype());
    JSHandle<JSTaggedValue> ctorKey = globalConst->GetHandledConstructorString();

    InternalCallParams *arguments = thread_->GetInternalCallParams();
    arguments->MakeArgv(message.GetTaggedValue());
    JSTaggedValue obj = JSFunction::Invoke(thread_, nativePrototype, ctorKey, 1, arguments->GetArgv());
    JSHandle<JSObject> handleNativeInstanceObj(thread_, obj);
    return handleNativeInstanceObj;
}

JSHandle<JSObject> ObjectFactory::NewJSObjectByConstructor(const JSHandle<JSFunction> &constructor,
                                                           const JSHandle<JSTaggedValue> &newTarget)
{
    NewObjectHook();
    JSHandle<JSHClass> jshclass;
    if (!constructor->HasFunctionPrototype() ||
        (constructor->GetProtoOrDynClass().IsHeapObject() && constructor->GetFunctionPrototype().IsECMAObject())) {
        jshclass = JSFunction::GetInstanceJSHClass(thread_, constructor, newTarget);
    } else {
        JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
        jshclass = JSFunction::GetInstanceJSHClass(thread_, JSHandle<JSFunction>(env->GetObjectFunction()), newTarget);
    }
    RETURN_HANDLE_IF_ABRUPT_COMPLETION(JSObject, thread_);

    JSHandle<JSObject> obj = NewJSObject(jshclass);
    {
        JSType type = jshclass->GetObjectType();
        switch (type) {
            case JSType::JS_OBJECT:
            case JSType::JS_ERROR:
            case JSType::JS_EVAL_ERROR:
            case JSType::JS_RANGE_ERROR:
            case JSType::JS_REFERENCE_ERROR:
            case JSType::JS_TYPE_ERROR:
            case JSType::JS_URI_ERROR:
            case JSType::JS_SYNTAX_ERROR:
            case JSType::JS_ITERATOR:
            case JSType::JS_INTL:
            case JSType::JS_LOCALE:
            case JSType::JS_DATE_TIME_FORMAT:
            case JSType::JS_NUMBER_FORMAT:
            case JSType::JS_RELATIVE_TIME_FORMAT:
            case JSType::JS_COLLATOR:
            case JSType::JS_PLURAL_RULES:
                break;
            case JSType::JS_ARRAY: {
                JSArray::Cast(*obj)->SetLength(thread_, JSTaggedValue(0));
                auto accessor = thread_->GlobalConstants()->GetArrayLengthAccessor();
                JSArray::Cast(*obj)->SetPropertyInlinedProps(thread_, JSArray::LENGTH_INLINE_PROPERTY_INDEX, accessor);
                break;
            }
            case JSType::JS_DATE:
                JSDate::Cast(*obj)->SetTimeValue(thread_, JSTaggedValue(0.0));
                JSDate::Cast(*obj)->SetLocalOffset(thread_, JSTaggedValue(JSDate::MAX_DOUBLE));
                break;
            case JSType::JS_INT8_ARRAY:
            case JSType::JS_UINT8_ARRAY:
            case JSType::JS_UINT8_CLAMPED_ARRAY:
            case JSType::JS_INT16_ARRAY:
            case JSType::JS_UINT16_ARRAY:
            case JSType::JS_INT32_ARRAY:
            case JSType::JS_UINT32_ARRAY:
            case JSType::JS_FLOAT32_ARRAY:
            case JSType::JS_FLOAT64_ARRAY:
                JSTypedArray::Cast(*obj)->SetViewedArrayBuffer(thread_, JSTaggedValue::Undefined());
                JSTypedArray::Cast(*obj)->SetTypedArrayName(thread_, JSTaggedValue::Undefined());
                JSTypedArray::Cast(*obj)->SetByteLength(thread_, JSTaggedValue(0));
                JSTypedArray::Cast(*obj)->SetByteOffset(thread_, JSTaggedValue(0));
                JSTypedArray::Cast(*obj)->SetArrayLength(thread_, JSTaggedValue(0));
                break;
            case JSType::JS_REG_EXP:
                JSRegExp::Cast(*obj)->SetByteCodeBuffer(thread_, JSTaggedValue::Undefined());
                JSRegExp::Cast(*obj)->SetOriginalSource(thread_, JSTaggedValue::Undefined());
                JSRegExp::Cast(*obj)->SetOriginalFlags(thread_, JSTaggedValue(0));
                JSRegExp::Cast(*obj)->SetLength(thread_, JSTaggedValue(0));
                break;
            case JSType::JS_PRIMITIVE_REF:
                JSPrimitiveRef::Cast(*obj)->SetValue(thread_, JSTaggedValue::Undefined());
                break;
            case JSType::JS_SET:
                JSSet::Cast(*obj)->SetLinkedSet(thread_, JSTaggedValue::Undefined());
                break;
            case JSType::JS_MAP:
                JSMap::Cast(*obj)->SetLinkedMap(thread_, JSTaggedValue::Undefined());
                break;
            case JSType::JS_WEAK_MAP:
                JSWeakMap::Cast(*obj)->SetLinkedMap(thread_, JSTaggedValue::Undefined());
                break;
            case JSType::JS_WEAK_SET:
                JSWeakSet::Cast(*obj)->SetLinkedSet(thread_, JSTaggedValue::Undefined());
                break;
            case JSType::JS_GENERATOR_OBJECT:
                JSGeneratorObject::Cast(*obj)->SetGeneratorState(thread_, JSTaggedValue::Undefined());
                JSGeneratorObject::Cast(*obj)->SetGeneratorContext(thread_, JSTaggedValue::Undefined());
                JSGeneratorObject::Cast(*obj)->SetResumeResult(thread_, JSTaggedValue::Undefined());
                JSGeneratorObject::Cast(*obj)->SetResumeMode(thread_, JSTaggedValue::Undefined());
                break;
            case JSType::JS_STRING_ITERATOR:
                JSStringIterator::Cast(*obj)->SetStringIteratorNextIndex(thread_, JSTaggedValue(0));
                JSStringIterator::Cast(*obj)->SetIteratedString(thread_, JSTaggedValue::Undefined());
                break;
            case JSType::JS_ARRAY_BUFFER:
                JSArrayBuffer::Cast(*obj)->SetArrayBufferByteLength(thread_, JSTaggedValue(0));
                JSArrayBuffer::Cast(*obj)->SetArrayBufferData(thread_, JSTaggedValue::Undefined());
                break;
            case JSType::JS_PROMISE:
                JSPromise::Cast(*obj)->SetPromiseState(thread_,
                                                       JSTaggedValue(static_cast<int32_t>(PromiseStatus::PENDING)));
                JSPromise::Cast(*obj)->SetPromiseResult(thread_, JSTaggedValue::Undefined());
                JSPromise::Cast(*obj)->SetPromiseRejectReactions(thread_, GetEmptyTaggedQueue().GetTaggedValue());
                JSPromise::Cast(*obj)->SetPromiseFulfillReactions(thread_, GetEmptyTaggedQueue().GetTaggedValue());

                JSPromise::Cast(*obj)->SetPromiseIsHandled(thread_, JSTaggedValue::Undefined());
                break;
            case JSType::JS_DATA_VIEW:
                JSDataView::Cast(*obj)->SetDataView(thread_, JSTaggedValue(false));
                JSDataView::Cast(*obj)->SetViewedArrayBuffer(thread_, JSTaggedValue::Undefined());
                JSDataView::Cast(*obj)->SetByteLength(thread_, JSTaggedValue(0));
                JSDataView::Cast(*obj)->SetByteOffset(thread_, JSTaggedValue(0));
                break;
            case JSType::JS_FUNCTION:
            case JSType::JS_GENERATOR_FUNCTION:
            case JSType::JS_FORIN_ITERATOR:
            case JSType::JS_MAP_ITERATOR:
            case JSType::JS_SET_ITERATOR:
            case JSType::JS_ARRAY_ITERATOR:
            default:
                UNREACHABLE();
        }
    }
    return obj;
}

FreeObject *ObjectFactory::FillFreeObject(uintptr_t address, size_t size, RemoveSlots removeSlots)
{
    FreeObject *object = nullptr;
    if (size >= FreeObject::SIZE_OFFSET && size < FreeObject::SIZE) {
        object = reinterpret_cast<FreeObject *>(address);
        object->SetClass(freeObjectWithOneFieldClass_);
    } else if (size >= FreeObject::SIZE) {
        object = reinterpret_cast<FreeObject *>(address);
        object->SetClass(freeObjectWithTwoFieldClass_);
        object->SetAvailable(size);
    } else if (size == FreeObject::NEXT_OFFSET) {
        object = reinterpret_cast<FreeObject *>(address);
        object->SetClass(freeObjectWithNoneFieldClass_);
    } else {
        LOG_ECMA(INFO) << "Fill free object size is smaller";
    }

    if (removeSlots == RemoveSlots::YES) {
        Region *region = Region::ObjectAddressToRange(object);
        if (!region->InYoungGeneration()) {
            heap_->ClearSlotsRange(region, address, address + size);
        }
    }
    return object;
}

TaggedObject *ObjectFactory::NewDynObject(const JSHandle<JSHClass> &dynclass, int inobjPropCount)
{
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(*dynclass);
    if (inobjPropCount > 0) {
        InitializeExtraProperties(dynclass, header, inobjPropCount);
    }
    return header;
}

TaggedObject *ObjectFactory::NewNonMovableDynObject(const JSHandle<JSHClass> &dynclass, int inobjPropCount)
{
    TaggedObject *header = heapHelper_.AllocateNonMovableOrHugeObject(*dynclass);
    if (inobjPropCount > 0) {
        InitializeExtraProperties(dynclass, header, inobjPropCount);
    }
    return header;
}

void ObjectFactory::InitializeExtraProperties(const JSHandle<JSHClass> &dynclass, TaggedObject *obj, int inobjPropCount)
{
    ASSERT(inobjPropCount * JSTaggedValue::TaggedTypeSize() < dynclass->GetObjectSize());
    auto paddr = reinterpret_cast<uintptr_t>(obj) + dynclass->GetObjectSize();
    JSTaggedType initVal = JSTaggedValue::Undefined().GetRawData();
    for (int i = 0; i < inobjPropCount; ++i) {
        paddr -= JSTaggedValue::TaggedTypeSize();
        *reinterpret_cast<JSTaggedType *>(paddr) = initVal;
    }
}

JSHandle<JSObject> ObjectFactory::OrdinaryNewJSObjectCreate(const JSHandle<JSTaggedValue> &proto)
{
    NewObjectHook();
    JSHandle<JSTaggedValue> protoValue(proto);
    JSHandle<JSHClass> protoDyn = NewEcmaDynClass(JSObject::SIZE, JSType::JS_OBJECT, protoValue);
    JSHandle<JSObject> newObj = NewJSObject(protoDyn);
    newObj->GetJSHClass()->SetExtensible(true);
    return newObj;
}

JSHandle<JSFunction> ObjectFactory::NewJSFunction(const JSHandle<GlobalEnv> &env, const void *nativeFunc,
                                                  FunctionKind kind)
{
    JSMethod *target = vm_->GetMethodForNativeFunction(nativeFunc);
    return NewJSFunction(env, target, kind);
}

JSHandle<JSFunction> ObjectFactory::NewJSFunction(const JSHandle<GlobalEnv> &env, JSMethod *method, FunctionKind kind)
{
    NewObjectHook();
    JSHandle<JSHClass> dynclass;
    if (kind == FunctionKind::BASE_CONSTRUCTOR) {
        dynclass = JSHandle<JSHClass>::Cast(env->GetFunctionClassWithProto());
    } else if (JSFunction::IsConstructorKind(kind)) {
        dynclass = JSHandle<JSHClass>::Cast(env->GetConstructorFunctionClass());
    } else {
        dynclass = JSHandle<JSHClass>::Cast(env->GetNormalFunctionClass());
    }

    return NewJSFunctionByDynClass(method, dynclass, kind);
}

JSHandle<JSHClass> ObjectFactory::CreateFunctionClass(FunctionKind kind, uint32_t size, JSType type,
                                                      const JSHandle<JSTaggedValue> &prototype)
{
    const GlobalEnvConstants *globalConst = thread_->GlobalConstants();
    JSHandle<JSHClass> functionClass = NewEcmaDynClass(size, type, prototype);
    {
        functionClass->SetCallable(true);
        // FunctionKind = BASE_CONSTRUCTOR
        if (JSFunction::IsConstructorKind(kind)) {
            functionClass->SetConstructor(true);
        }
        functionClass->SetExtensible(true);
    }

    uint32_t fieldOrder = 0;
    ASSERT(JSFunction::LENGTH_INLINE_PROPERTY_INDEX == fieldOrder);
    JSHandle<LayoutInfo> layoutInfoHandle = CreateLayoutInfo(JSFunction::LENGTH_OF_INLINE_PROPERTIES);
    {
        PropertyAttributes attributes = PropertyAttributes::Default(false, false, true);
        attributes.SetIsInlinedProps(true);
        attributes.SetRepresentation(Representation::MIXED);
        attributes.SetOffset(fieldOrder);
        layoutInfoHandle->AddKey(thread_, fieldOrder, globalConst->GetLengthString(), attributes);
        fieldOrder++;
    }

    ASSERT(JSFunction::NAME_INLINE_PROPERTY_INDEX == fieldOrder);
    // not set name in-object property on class which may have a name() method
    if (!JSFunction::IsClassConstructor(kind)) {
        PropertyAttributes attributes = PropertyAttributes::DefaultAccessor(false, false, true);
        attributes.SetIsInlinedProps(true);
        attributes.SetRepresentation(Representation::MIXED);
        attributes.SetOffset(fieldOrder);
        layoutInfoHandle->AddKey(thread_, fieldOrder,
                                 thread_->GlobalConstants()->GetHandledNameString().GetTaggedValue(), attributes);
        fieldOrder++;
    }

    if (JSFunction::HasPrototype(kind) && !JSFunction::IsClassConstructor(kind)) {
        ASSERT(JSFunction::PROTOTYPE_INLINE_PROPERTY_INDEX == fieldOrder);
        PropertyAttributes attributes = PropertyAttributes::DefaultAccessor(true, false, false);
        attributes.SetIsInlinedProps(true);
        attributes.SetRepresentation(Representation::MIXED);
        attributes.SetOffset(fieldOrder);
        layoutInfoHandle->AddKey(thread_, fieldOrder, globalConst->GetPrototypeString(), attributes);
        fieldOrder++;
    } else if (JSFunction::IsClassConstructor(kind)) {
        ASSERT(JSFunction::CLASS_PROTOTYPE_INLINE_PROPERTY_INDEX == fieldOrder);
        PropertyAttributes attributes = PropertyAttributes::DefaultAccessor(false, false, false);
        attributes.SetIsInlinedProps(true);
        attributes.SetRepresentation(Representation::MIXED);
        attributes.SetOffset(fieldOrder);
        layoutInfoHandle->AddKey(thread_, fieldOrder, globalConst->GetPrototypeString(), attributes);
        fieldOrder++;
    }

    {
        functionClass->SetAttributes(thread_, layoutInfoHandle);
        uint32_t unusedInlinedProps = functionClass->GetUnusedInlinedProps();
        ASSERT(unusedInlinedProps >= fieldOrder);
        functionClass->SetUnusedInlinedProps(unusedInlinedProps - fieldOrder);
    }
    return functionClass;
}

JSHandle<JSFunction> ObjectFactory::NewJSFunctionByDynClass(JSMethod *method, const JSHandle<JSHClass> &clazz,
                                                            FunctionKind kind)
{
    NewObjectHook();
    JSHandle<JSFunction> function = JSHandle<JSFunction>::Cast(NewJSObject(clazz));
    clazz->SetCallable(true);
    clazz->SetExtensible(true);
    JSFunction::InitializeJSFunction(thread_, function, kind);
    function->SetCallTarget(thread_, method);
    return function;
}

JSHandle<JSFunction> ObjectFactory::NewJSNativeErrorFunction(const JSHandle<GlobalEnv> &env, const void *nativeFunc)
{
    NewObjectHook();
    JSMethod *target = vm_->GetMethodForNativeFunction(nativeFunc);
    JSHandle<JSHClass> dynclass = JSHandle<JSHClass>::Cast(env->GetNativeErrorFunctionClass());
    return NewJSFunctionByDynClass(target, dynclass, FunctionKind::BUILTIN_CONSTRUCTOR);
}

JSHandle<JSFunction> ObjectFactory::NewSpecificTypedArrayFunction(const JSHandle<GlobalEnv> &env,
                                                                  const void *nativeFunc)
{
    NewObjectHook();
    JSMethod *target = vm_->GetMethodForNativeFunction(nativeFunc);
    JSHandle<JSHClass> dynclass = JSHandle<JSHClass>::Cast(env->GetSpecificTypedArrayFunctionClass());
    return NewJSFunctionByDynClass(target, dynclass, FunctionKind::BUILTIN_CONSTRUCTOR);
}

JSHandle<JSBoundFunction> ObjectFactory::NewJSBoundFunction(const JSHandle<JSFunctionBase> &target,
                                                            const JSHandle<JSTaggedValue> &boundThis,
                                                            const JSHandle<TaggedArray> &args)
{
    NewObjectHook();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSTaggedValue> proto = env->GetFunctionPrototype();
    JSHandle<JSHClass> dynclass = NewEcmaDynClass(JSBoundFunction::SIZE, JSType::JS_BOUND_FUNCTION, proto);

    JSHandle<JSBoundFunction> bundleFunction = JSHandle<JSBoundFunction>::Cast(NewJSObject(dynclass));
    bundleFunction->SetBoundTarget(thread_, target);
    bundleFunction->SetBoundThis(thread_, boundThis);
    bundleFunction->SetBoundArguments(thread_, args);
    dynclass->SetCallable(true);
    if (target.GetTaggedValue().IsConstructor()) {
        bundleFunction->SetConstructor(true);
    }
    JSMethod *method =
        vm_->GetMethodForNativeFunction(reinterpret_cast<void *>(builtins::BuiltinsGlobal::CallJsBoundFunction));
    bundleFunction->SetCallTarget(thread_, method);
    return bundleFunction;
}

JSHandle<JSIntlBoundFunction> ObjectFactory::NewJSIntlBoundFunction(const void *nativeFunc, int functionLength)
{
    NewObjectHook();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSHClass> dynclass = JSHandle<JSHClass>::Cast(env->GetJSIntlBoundFunctionClass());

    JSHandle<JSIntlBoundFunction> intlBoundFunc = JSHandle<JSIntlBoundFunction>::Cast(NewJSObject(dynclass));
    JSMethod *method = vm_->GetMethodForNativeFunction(nativeFunc);
    intlBoundFunc->SetCallTarget(thread_, method);
    JSHandle<JSFunction> function = JSHandle<JSFunction>::Cast(intlBoundFunc);
    JSFunction::InitializeJSFunction(thread_, function, FunctionKind::NORMAL_FUNCTION);
    JSFunction::SetFunctionLength(thread_, function, JSTaggedValue(functionLength));
    const GlobalEnvConstants *globalConst = thread_->GlobalConstants();
    JSHandle<JSTaggedValue> emptyString = globalConst->GetHandledEmptyString();
    JSHandle<JSTaggedValue> nameKey = globalConst->GetHandledNameString();
    PropertyDescriptor nameDesc(thread_, emptyString, false, false, true);
    JSTaggedValue::DefinePropertyOrThrow(thread_, JSHandle<JSTaggedValue>::Cast(function), nameKey, nameDesc);
    return intlBoundFunc;
}

JSHandle<JSProxyRevocFunction> ObjectFactory::NewJSProxyRevocFunction(const JSHandle<JSProxy> &proxy,
                                                                      const void *nativeFunc)
{
    NewObjectHook();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    const GlobalEnvConstants *globalConst = thread_->GlobalConstants();
    JSHandle<JSHClass> dynclass = JSHandle<JSHClass>::Cast(env->GetProxyRevocFunctionClass());

    JSHandle<JSProxyRevocFunction> revocFunction = JSHandle<JSProxyRevocFunction>::Cast(NewJSObject(dynclass));
    revocFunction->SetRevocableProxy(thread_, proxy);

    JSMethod *target = vm_->GetMethodForNativeFunction(nativeFunc);
    revocFunction->SetCallTarget(thread_, target);
    JSHandle<JSFunction> function = JSHandle<JSFunction>::Cast(revocFunction);
    JSFunction::InitializeJSFunction(thread_, function, FunctionKind::NORMAL_FUNCTION);
    JSFunction::SetFunctionLength(thread_, function, JSTaggedValue(0));
    JSHandle<JSTaggedValue> emptyString = globalConst->GetHandledEmptyString();
    JSHandle<JSTaggedValue> nameKey = globalConst->GetHandledNameString();
    PropertyDescriptor nameDesc(thread_, emptyString, false, false, true);
    JSTaggedValue::DefinePropertyOrThrow(thread_, JSHandle<JSTaggedValue>::Cast(function), nameKey, nameDesc);
    return revocFunction;
}

JSHandle<JSAsyncAwaitStatusFunction> ObjectFactory::NewJSAsyncAwaitStatusFunction(const void *nativeFunc)
{
    NewObjectHook();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSHClass> dynclass = JSHandle<JSHClass>::Cast(env->GetAsyncAwaitStatusFunctionClass());

    JSHandle<JSAsyncAwaitStatusFunction> awaitFunction =
        JSHandle<JSAsyncAwaitStatusFunction>::Cast(NewJSObject(dynclass));

    JSFunction::InitializeJSFunction(thread_, JSHandle<JSFunction>::Cast(awaitFunction));
    JSMethod *target = vm_->GetMethodForNativeFunction(nativeFunc);
    awaitFunction->SetCallTarget(thread_, target);
    return awaitFunction;
}

JSHandle<JSFunction> ObjectFactory::NewJSGeneratorFunction(JSMethod *method)
{
    NewObjectHook();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();

    JSHandle<JSHClass> dynclass = JSHandle<JSHClass>::Cast(env->GetGeneratorFunctionClass());
    JSHandle<JSFunction> generatorFunc = JSHandle<JSFunction>::Cast(NewJSObject(dynclass));
    JSFunction::InitializeJSFunction(thread_, generatorFunc, FunctionKind::GENERATOR_FUNCTION);
    generatorFunc->SetCallTarget(thread_, method);
    return generatorFunc;
}

JSHandle<JSGeneratorObject> ObjectFactory::NewJSGeneratorObject(JSHandle<JSTaggedValue> generatorFunction)
{
    NewObjectHook();
    JSHandle<JSTaggedValue> proto(thread_, JSHandle<JSFunction>::Cast(generatorFunction)->GetProtoOrDynClass());
    if (!proto->IsECMAObject()) {
        JSHandle<GlobalEnv> realmHandle = JSObject::GetFunctionRealm(thread_, generatorFunction);
        proto = realmHandle->GetGeneratorPrototype();
    }
    JSHandle<JSHClass> dynclass = NewEcmaDynClass(JSGeneratorObject::SIZE, JSType::JS_GENERATOR_OBJECT, proto);
    JSHandle<JSGeneratorObject> generatorObject = JSHandle<JSGeneratorObject>::Cast(NewJSObject(dynclass));
    return generatorObject;
}

JSHandle<JSAsyncFunction> ObjectFactory::NewAsyncFunction(JSMethod *method)
{
    NewObjectHook();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSHClass> dynclass = JSHandle<JSHClass>::Cast(env->GetAsyncFunctionClass());
    JSHandle<JSAsyncFunction> asyncFunction = JSHandle<JSAsyncFunction>::Cast(NewJSObject(dynclass));
    JSFunction::InitializeJSFunction(thread_, JSHandle<JSFunction>::Cast(asyncFunction));
    asyncFunction->SetCallTarget(thread_, method);
    return asyncFunction;
}

JSHandle<JSAsyncFuncObject> ObjectFactory::NewJSAsyncFuncObject()
{
    NewObjectHook();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSTaggedValue> proto = env->GetInitialGenerator();
    JSHandle<JSHClass> dynclass = NewEcmaDynClass(JSAsyncFuncObject::SIZE, JSType::JS_ASYNC_FUNC_OBJECT, proto);
    JSHandle<JSAsyncFuncObject> asyncFuncObject = JSHandle<JSAsyncFuncObject>::Cast(NewJSObject(dynclass));
    return asyncFuncObject;
}

JSHandle<CompletionRecord> ObjectFactory::NewCompletionRecord(uint8_t type, JSHandle<JSTaggedValue> value)
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(completionRecordClass_);
    JSHandle<CompletionRecord> obj(thread_, header);
    obj->SetType(thread_, JSTaggedValue(static_cast<int32_t>(type)));
    obj->SetValue(thread_, value);
    return obj;
}

JSHandle<GeneratorContext> ObjectFactory::NewGeneratorContext()
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(generatorContextClass_);
    JSHandle<GeneratorContext> obj(thread_, header);
    obj->SetRegsArray(thread_, JSTaggedValue::Undefined());
    obj->SetMethod(thread_, JSTaggedValue::Undefined());
    obj->SetAcc(thread_, JSTaggedValue::Undefined());
    obj->SetNRegs(thread_, JSTaggedValue::Undefined());
    obj->SetBCOffset(thread_, JSTaggedValue::Undefined());
    obj->SetGeneratorObject(thread_, JSTaggedValue::Undefined());
    obj->SetLexicalEnv(thread_, JSTaggedValue::Undefined());
    return obj;
}

JSHandle<JSPrimitiveRef> ObjectFactory::NewJSPrimitiveRef(const JSHandle<JSFunction> &function,
                                                          const JSHandle<JSTaggedValue> &object)
{
    NewObjectHook();

    JSHandle<JSPrimitiveRef> obj(NewJSObjectByConstructor(function, JSHandle<JSTaggedValue>(function)));
    obj->SetValue(thread_, object);

    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    const GlobalEnvConstants *globalConst = thread_->GlobalConstants();
    if (function.GetTaggedValue() == env->GetStringFunction().GetTaggedValue()) {
        JSHandle<JSTaggedValue> lengthStr = globalConst->GetHandledLengthString();

        int32_t length = EcmaString::Cast(object.GetTaggedValue().GetTaggedObject())->GetLength();
        PropertyDescriptor desc(thread_, JSHandle<JSTaggedValue>(thread_, JSTaggedValue(length)), false, false, false);
        JSTaggedValue::DefinePropertyOrThrow(thread_, JSHandle<JSTaggedValue>(obj), lengthStr, desc);
    }

    return obj;
}

JSHandle<JSPrimitiveRef> ObjectFactory::NewJSPrimitiveRef(PrimitiveType type, const JSHandle<JSTaggedValue> &object)
{
    NewObjectHook();
    ObjectFactory *factory = vm_->GetFactory();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSTaggedValue> function;
    switch (type) {
        case PrimitiveType::PRIMITIVE_NUMBER:
            function = env->GetNumberFunction();
            break;
        case PrimitiveType::PRIMITIVE_STRING:
            function = env->GetStringFunction();
            break;
        case PrimitiveType::PRIMITIVE_SYMBOL:
            function = env->GetSymbolFunction();
            break;
        case PrimitiveType::PRIMITIVE_BOOLEAN:
            function = env->GetBooleanFunction();
            break;
        default:
            break;
    }
    JSHandle<JSFunction> funcHandle(function);
    return factory->NewJSPrimitiveRef(funcHandle, object);
}

JSHandle<JSPrimitiveRef> ObjectFactory::NewJSString(const JSHandle<JSTaggedValue> &str)
{
    NewObjectHook();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSTaggedValue> stringFunc = env->GetStringFunction();

    JSHandle<JSPrimitiveRef> obj =
        JSHandle<JSPrimitiveRef>::Cast(NewJSObjectByConstructor(JSHandle<JSFunction>(stringFunc), stringFunc));
    obj->SetValue(thread_, str, SKIP_BARRIER);
    return obj;
}

JSHandle<GlobalEnv> ObjectFactory::NewGlobalEnv(JSHClass *globalEnvClass)
{
    NewObjectHook();
    // Note: Global env must be allocated in non-movable heap, since its getters will directly return
    //       the offsets of the properties as the address of Handles.
    TaggedObject *header = heapHelper_.AllocateNonMovableOrHugeObject(globalEnvClass);
    InitObjectFields(header);
    return JSHandle<GlobalEnv>(thread_, GlobalEnv::Cast(header));
}

JSHandle<LexicalEnv> ObjectFactory::NewLexicalEnv(int numSlots)
{
    NewObjectHook();
    size_t size = LexicalEnv::ComputeSize(numSlots);
    auto header = heapHelper_.AllocateYoungGenerationOrHugeObject(envClass_, size);
    JSHandle<LexicalEnv> array(thread_, header);
    array->InitializeWithSpecialValue(JSTaggedValue::Undefined(), numSlots + LexicalEnv::RESERVED_ENV_LENGTH);
    return array;
}

JSHandle<JSSymbol> ObjectFactory::NewJSSymbol()
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(symbolClass_);
    JSHandle<JSSymbol> obj(thread_, JSSymbol::Cast(header));
    obj->SetDescription(thread_, JSTaggedValue::Undefined());
    obj->SetFlags(thread_, JSTaggedValue(0));
    auto result = JSTaggedValue(static_cast<int>(SymbolTable::Hash(obj.GetTaggedValue())));
    obj->SetHashField(thread_, result);
    return obj;
}

JSHandle<JSSymbol> ObjectFactory::NewPrivateSymbol()
{
    JSHandle<JSSymbol> obj = NewJSSymbol();
    obj->SetPrivate(thread_);
    return obj;
}

JSHandle<JSSymbol> ObjectFactory::NewPrivateNameSymbol(const JSHandle<JSTaggedValue> &name)
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(symbolClass_);
    JSHandle<JSSymbol> obj(thread_, JSSymbol::Cast(header));
    obj->SetFlags(thread_, JSTaggedValue(0));
    obj->SetPrivateNameSymbol(thread_);
    obj->SetDescription(thread_, name);
    auto result = JSTaggedValue(static_cast<int>(SymbolTable::Hash(name.GetTaggedValue())));
    obj->SetHashField(thread_, result);
    return obj;
}

JSHandle<JSSymbol> ObjectFactory::NewWellKnownSymbol(const JSHandle<JSTaggedValue> &name)
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(symbolClass_);
    JSHandle<JSSymbol> obj(thread_, JSSymbol::Cast(header));
    obj->SetFlags(thread_, JSTaggedValue(0));
    obj->SetWellKnownSymbol(thread_);
    obj->SetDescription(thread_, name);
    auto result = JSTaggedValue(static_cast<int>(SymbolTable::Hash(name.GetTaggedValue())));
    obj->SetHashField(thread_, result);
    return obj;
}

JSHandle<JSSymbol> ObjectFactory::NewPublicSymbol(const JSHandle<JSTaggedValue> &name)
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(symbolClass_);
    JSHandle<JSSymbol> obj(thread_, JSSymbol::Cast(header));
    obj->SetFlags(thread_, JSTaggedValue(0));
    obj->SetDescription(thread_, name);
    auto result = JSTaggedValue(static_cast<int>(SymbolTable::Hash(name.GetTaggedValue())));
    obj->SetHashField(thread_, result);
    return obj;
}

JSHandle<JSSymbol> ObjectFactory::NewSymbolWithTable(const JSHandle<JSTaggedValue> &name)
{
    NewObjectHook();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<SymbolTable> tableHandle(env->GetRegisterSymbols());
    if (tableHandle->ContainsKey(thread_, name.GetTaggedValue())) {
        JSTaggedValue objValue = tableHandle->GetSymbol(name.GetTaggedValue());
        return JSHandle<JSSymbol>(thread_, objValue);
    }

    JSHandle<JSSymbol> obj = NewPublicSymbol(name);
    JSHandle<JSTaggedValue> valueHandle(obj);
    JSHandle<JSTaggedValue> keyHandle(name);
    SymbolTable *table = SymbolTable::Insert(thread_, tableHandle, keyHandle, valueHandle);
    env->SetRegisterSymbols(thread_, JSTaggedValue(table));
    return obj;
}

JSHandle<JSSymbol> ObjectFactory::NewPrivateNameSymbolWithChar(const char *description)
{
    NewObjectHook();
    JSHandle<EcmaString> string = NewFromString(description);
    return NewPrivateNameSymbol(JSHandle<JSTaggedValue>(string));
}

JSHandle<JSSymbol> ObjectFactory::NewWellKnownSymbolWithChar(const char *description)
{
    NewObjectHook();
    JSHandle<EcmaString> string = NewFromString(description);
    return NewWellKnownSymbol(JSHandle<JSTaggedValue>(string));
}

JSHandle<JSSymbol> ObjectFactory::NewPublicSymbolWithChar(const char *description)
{
    NewObjectHook();
    JSHandle<EcmaString> string = NewFromString(description);
    return NewPublicSymbol(JSHandle<JSTaggedValue>(string));
}

JSHandle<JSSymbol> ObjectFactory::NewSymbolWithTableWithChar(const char *description)
{
    NewObjectHook();
    JSHandle<EcmaString> string = NewFromString(description);
    return NewSymbolWithTable(JSHandle<JSTaggedValue>(string));
}

JSHandle<AccessorData> ObjectFactory::NewAccessorData()
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(accessorDataClass_);
    JSHandle<AccessorData> acc(thread_, AccessorData::Cast(header));
    acc->SetGetter(thread_, JSTaggedValue::Undefined());
    acc->SetSetter(thread_, JSTaggedValue::Undefined());
    return acc;
}

JSHandle<AccessorData> ObjectFactory::NewInternalAccessor(void *setter, void *getter)
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateNonMovableOrHugeObject(internalAccessorClass_);
    JSHandle<AccessorData> obj(thread_, AccessorData::Cast(header));
    if (setter != nullptr) {
        JSHandle<JSNativePointer> setFunc = NewJSNativePointer(setter, true);
        obj->SetSetter(thread_, setFunc.GetTaggedValue());
    } else {
        JSTaggedValue setFunc = JSTaggedValue::Undefined();
        obj->SetSetter(thread_, setFunc);
        ASSERT(!obj->HasSetter());
    }
    JSHandle<JSNativePointer> getFunc = NewJSNativePointer(getter, true);
    obj->SetGetter(thread_, getFunc);
    return obj;
}

JSHandle<PromiseCapability> ObjectFactory::NewPromiseCapability()
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(capabilityRecordClass_);
    JSHandle<PromiseCapability> obj(thread_, header);
    obj->SetPromise(thread_, JSTaggedValue::Undefined());
    obj->SetResolve(thread_, JSTaggedValue::Undefined());
    obj->SetReject(thread_, JSTaggedValue::Undefined());
    return obj;
}

JSHandle<PromiseReaction> ObjectFactory::NewPromiseReaction()
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(reactionsRecordClass_);
    JSHandle<PromiseReaction> obj(thread_, header);
    obj->SetPromiseCapability(thread_, JSTaggedValue::Undefined());
    obj->SetHandler(thread_, JSTaggedValue::Undefined());
    obj->SetType(thread_, JSTaggedValue::Undefined());
    return obj;
}

JSHandle<PromiseIteratorRecord> ObjectFactory::NewPromiseIteratorRecord(const JSHandle<JSTaggedValue> &itor,
                                                                        const JSHandle<JSTaggedValue> &done)
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(promiseIteratorRecordClass_);
    JSHandle<PromiseIteratorRecord> obj(thread_, header);
    obj->SetIterator(thread_, itor.GetTaggedValue());
    obj->SetDone(thread_, done.GetTaggedValue());
    return obj;
}

JSHandle<job::MicroJobQueue> ObjectFactory::NewMicroJobQueue()
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateNonMovableOrHugeObject(microJobQueueClass_);
    JSHandle<job::MicroJobQueue> obj(thread_, header);
    obj->SetPromiseJobQueue(thread_, GetEmptyTaggedQueue().GetTaggedValue());
    obj->SetScriptJobQueue(thread_, GetEmptyTaggedQueue().GetTaggedValue());
    return obj;
}

JSHandle<job::PendingJob> ObjectFactory::NewPendingJob(const JSHandle<JSFunction> &func,
                                                       const JSHandle<TaggedArray> &argv)
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(pendingJobClass_);
    JSHandle<job::PendingJob> obj(thread_, header);
    obj->SetJob(thread_, func.GetTaggedValue());
    obj->SetArguments(thread_, argv.GetTaggedValue());
    return obj;
}

JSHandle<JSFunctionExtraInfo> ObjectFactory::NewFunctionExtraInfo(const JSHandle<JSNativePointer> &callBack,
                                                                  const JSHandle<JSNativePointer> &data)
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(functionExtraInfo_);
    JSHandle<JSFunctionExtraInfo> obj(thread_, header);
    obj->SetCallback(thread_, callBack.GetTaggedValue());
    obj->SetData(thread_, data.GetTaggedValue());
    return obj;
}

JSHandle<JSProxy> ObjectFactory::NewJSProxy(const JSHandle<JSTaggedValue> &target,
                                            const JSHandle<JSTaggedValue> &handler)
{
    NewObjectHook();
    TaggedObject *header = nullptr;
    if (target->IsCallable()) {
        header = target->IsConstructor() ? heapHelper_.AllocateYoungGenerationOrHugeObject(jsProxyConstructClass_)
                                         : heapHelper_.AllocateYoungGenerationOrHugeObject(jsProxyCallableClass_);
    } else {
        header = heapHelper_.AllocateYoungGenerationOrHugeObject(jsProxyOrdinaryClass_);
    }

    JSHandle<JSProxy> proxy(thread_, header);
    JSMethod *method = nullptr;
    if (target->IsCallable()) {
        JSMethod *nativeMethod =
            vm_->GetMethodForNativeFunction(reinterpret_cast<void *>(builtins::BuiltinsGlobal::CallJsProxy));
        proxy->SetCallTarget(thread_, nativeMethod);
    }
    proxy->SetMethod(method);

    proxy->SetTarget(thread_, target.GetTaggedValue());
    proxy->SetHandler(thread_, handler.GetTaggedValue());
    return proxy;
}

JSHandle<JSRealm> ObjectFactory::NewJSRealm()
{
    NewObjectHook();

    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSHClass> dynClassClassHandle = NewEcmaDynClass(nullptr, JSHClass::SIZE, JSType::HCLASS);
    JSHClass *dynclass = reinterpret_cast<JSHClass *>(dynClassClassHandle.GetTaggedValue().GetTaggedObject());
    dynclass->SetClass(dynclass);
    JSHandle<JSHClass> realmEnvClass = NewEcmaDynClass(*dynClassClassHandle, GlobalEnv::SIZE, JSType::GLOBAL_ENV);
    JSHandle<GlobalEnv> realmEnvHandle = NewGlobalEnv(*realmEnvClass);

    ObtainRootClass(env);
    realmEnvHandle->SetEmptyArray(thread_, NewEmptyArray());
    realmEnvHandle->SetEmptyTaggedQueue(thread_, NewTaggedQueue(0));
    realmEnvHandle->SetTemplateMap(thread_, JSTaggedValue(TemplateMap::Create(thread_)));

    Builtins builtins;
    builtins.Initialize(realmEnvHandle, thread_);
    JSHandle<JSTaggedValue> protoValue = thread_->GlobalConstants()->GetHandledJSRealmClass();
    JSHandle<JSHClass> dynHandle = NewEcmaDynClass(JSRealm::SIZE, JSType::JS_REALM, protoValue);
    JSHandle<JSRealm> realm(NewJSObject(dynHandle));
    realm->SetGlobalEnv(thread_, realmEnvHandle.GetTaggedValue());

    JSHandle<JSTaggedValue> realmObj = realmEnvHandle->GetJSGlobalObject();
    JSHandle<JSTaggedValue> realmkey(thread_->GlobalConstants()->GetHandledGlobalString());
    PropertyDescriptor realmDesc(thread_, JSHandle<JSTaggedValue>::Cast(realmObj), true, false, true);
    [[maybe_unused]] bool status =
        JSObject::DefineOwnProperty(thread_, JSHandle<JSObject>::Cast(realm), realmkey, realmDesc);
    ASSERT_PRINT(status == true, "Realm defineOwnProperty failed");

    return realm;
}

JSHandle<TaggedArray> ObjectFactory::NewEmptyArray()
{
    NewObjectHook();

    auto header = heapHelper_.AllocateNonMovableOrHugeObject(arrayClass_, sizeof(TaggedArray));
    JSHandle<TaggedArray> array(thread_, header);
    array->SetLength(0);
    return array;
}

JSHandle<TaggedArray> ObjectFactory::NewTaggedArray(array_size_t length, JSTaggedValue initVal, bool nonMovable)
{
    if (nonMovable) {
        return NewTaggedArray(length, initVal, MemSpaceType::NON_MOVABLE);
    }
    return NewTaggedArray(length, initVal, MemSpaceType::SEMI_SPACE);
}

JSHandle<TaggedArray> ObjectFactory::NewTaggedArray(array_size_t length, JSTaggedValue initVal, MemSpaceType spaceType)
{
    NewObjectHook();
    if (length == 0) {
        return EmptyArray();
    }

    size_t size = TaggedArray::ComputeSize(JSTaggedValue::TaggedTypeSize(), length);
    TaggedObject *header = nullptr;
    switch (spaceType) {
        case MemSpaceType::SEMI_SPACE:
            header = heapHelper_.AllocateYoungGenerationOrHugeObject(arrayClass_, size);
            break;
        case MemSpaceType::OLD_SPACE:
            header = heapHelper_.AllocateOldGenerationOrHugeObject(arrayClass_, size);
            break;
        case MemSpaceType::NON_MOVABLE:
            header = heapHelper_.AllocateNonMovableOrHugeObject(arrayClass_, size);
            break;
        default:
            UNREACHABLE();
    }

    JSHandle<TaggedArray> array(thread_, header);
    array->InitializeWithSpecialValue(initVal, length);
    return array;
}

JSHandle<TaggedArray> ObjectFactory::NewTaggedArray(array_size_t length, JSTaggedValue initVal)
{
    NewObjectHook();
    if (length == 0) {
        return EmptyArray();
    }

    size_t size = TaggedArray::ComputeSize(JSTaggedValue::TaggedTypeSize(), length);
    auto header = heapHelper_.AllocateYoungGenerationOrHugeObject(arrayClass_, size);
    JSHandle<TaggedArray> array(thread_, header);
    array->InitializeWithSpecialValue(initVal, length);
    return array;
}

JSHandle<TaggedArray> ObjectFactory::NewDictionaryArray(array_size_t length)
{
    NewObjectHook();

    ASSERT(length > 0);

    size_t size = TaggedArray::ComputeSize(JSTaggedValue::TaggedTypeSize(), length);
    auto header = heapHelper_.AllocateYoungGenerationOrHugeObject(dictionaryClass_, size);
    JSHandle<TaggedArray> array(thread_, header);
    array->InitializeWithSpecialValue(JSTaggedValue::Undefined(), length);

    return array;
}

JSHandle<TaggedArray> ObjectFactory::ExtendArray(const JSHandle<TaggedArray> &old, array_size_t length,
                                                 JSTaggedValue initVal)
{
    ASSERT(length > old->GetLength());

    NewObjectHook();
    size_t size = TaggedArray::ComputeSize(JSTaggedValue::TaggedTypeSize(), length);
    auto header = heapHelper_.AllocateYoungGenerationOrHugeObject(arrayClass_, size);
    JSHandle<TaggedArray> newArray(thread_, header);
    newArray->SetLength(length);

    array_size_t oldLength = old->GetLength();
    for (array_size_t i = 0; i < oldLength; i++) {
        JSTaggedValue value = old->Get(i);
        newArray->Set(thread_, i, value);
    }

    for (array_size_t i = oldLength; i < length; i++) {
        newArray->Set(thread_, i, initVal);
    }

    return newArray;
}

JSHandle<TaggedArray> ObjectFactory::CopyPartArray(const JSHandle<TaggedArray> &old, array_size_t start,
                                                   array_size_t end)
{
    ASSERT(start <= end);
    ASSERT(end <= old->GetLength());

    array_size_t newLength = end - start;
    if (newLength == 0) {
        return EmptyArray();
    }

    NewObjectHook();
    size_t size = TaggedArray::ComputeSize(JSTaggedValue::TaggedTypeSize(), newLength);
    auto header = heapHelper_.AllocateYoungGenerationOrHugeObject(arrayClass_, size);
    JSHandle<TaggedArray> newArray(thread_, header);
    newArray->SetLength(newLength);

    for (array_size_t i = 0; i < newLength; i++) {
        JSTaggedValue value = old->Get(i + start);
        if (value.IsHole()) {
            break;
        }
        newArray->Set(thread_, i, value);
    }
    return newArray;
}

JSHandle<TaggedArray> ObjectFactory::CopyArray(const JSHandle<TaggedArray> &old,
                                               [[maybe_unused]] array_size_t oldLength, array_size_t newLength,
                                               JSTaggedValue initVal)
{
    if (newLength == 0) {
        return EmptyArray();
    }
    if (newLength > oldLength) {
        return ExtendArray(old, newLength, initVal);
    }

    NewObjectHook();
    size_t size = TaggedArray::ComputeSize(JSTaggedValue::TaggedTypeSize(), newLength);
    auto header = heapHelper_.AllocateYoungGenerationOrHugeObject(arrayClass_, size);
    JSHandle<TaggedArray> newArray(thread_, header);
    newArray->SetLength(newLength);

    for (array_size_t i = 0; i < newLength; i++) {
        JSTaggedValue value = old->Get(i);
        newArray->Set(thread_, i, value);
    }

    return newArray;
}

JSHandle<LayoutInfo> ObjectFactory::CreateLayoutInfo(int properties, JSTaggedValue initVal)
{
    int arrayLength = LayoutInfo::ComputeArrayLength(LayoutInfo::ComputeGrowCapacity(properties));
    JSHandle<LayoutInfo> layoutInfoHandle = JSHandle<LayoutInfo>::Cast(NewTaggedArray(arrayLength, initVal));
    layoutInfoHandle->SetNumberOfElements(thread_, 0);
    return layoutInfoHandle;
}

JSHandle<LayoutInfo> ObjectFactory::ExtendLayoutInfo(const JSHandle<LayoutInfo> &old, int properties,
                                                     JSTaggedValue initVal)
{
    ASSERT(properties > old->NumberOfElements());
    NewObjectHook();
    int arrayLength = LayoutInfo::ComputeArrayLength(LayoutInfo::ComputeGrowCapacity(properties));
    return JSHandle<LayoutInfo>(ExtendArray(JSHandle<TaggedArray>(old), arrayLength, initVal));
}

JSHandle<LayoutInfo> ObjectFactory::CopyLayoutInfo(const JSHandle<LayoutInfo> &old)
{
    NewObjectHook();
    int newLength = old->GetLength();
    return JSHandle<LayoutInfo>(CopyArray(JSHandle<TaggedArray>::Cast(old), newLength, newLength));
}

JSHandle<LayoutInfo> ObjectFactory::CopyAndReSort(const JSHandle<LayoutInfo> &old, int end, int capacity)
{
    NewObjectHook();
    ASSERT(capacity >= end);
    JSHandle<LayoutInfo> newArr = CreateLayoutInfo(capacity);
    Span<struct Properties> sp(old->GetProperties(), end);
    int i = 0;
    for (; i < end; i++) {
        newArr->AddKey(thread_, i, sp[i].key_, PropertyAttributes(sp[i].attr_));
    }

    return newArr;
}

JSHandle<ConstantPool> ObjectFactory::NewConstantPool(uint32_t capacity)
{
    NewObjectHook();
    if (capacity == 0) {
        return JSHandle<ConstantPool>::Cast(EmptyArray());
    }
    size_t size = TaggedArray::ComputeSize(JSTaggedValue::TaggedTypeSize(), capacity);
    auto header = heapHelper_.AllocateNonMovableOrHugeObject(arrayClass_, size);
    JSHandle<ConstantPool> array(thread_, header);
    array->InitializeWithSpecialValue(JSTaggedValue::Undefined(), capacity);
    return array;
}

JSHandle<Program> ObjectFactory::NewProgram()
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(programClass_);
    JSHandle<Program> p(thread_, header);
    p->SetLocation(thread_, JSTaggedValue::Undefined());
    p->SetConstantPool(thread_, JSTaggedValue::Undefined());
    p->SetMainFunction(thread_, JSTaggedValue::Undefined());
    p->SetMethodsData(nullptr);
    return p;
}

JSHandle<EcmaModule> ObjectFactory::NewEmptyEcmaModule()
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(ecmaModuleClass_);
    JSHandle<EcmaModule> module(thread_, header);
    module->SetNameDictionary(thread_, JSTaggedValue::Undefined());
    return module;
}

JSHandle<EcmaString> ObjectFactory::GetEmptyString() const
{
    return JSHandle<EcmaString>(thread_->GlobalConstants()->GetHandledEmptyString());
}

JSHandle<TaggedArray> ObjectFactory::EmptyArray() const
{
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    return JSHandle<TaggedArray>(env->GetEmptyArray());
}

JSHandle<ObjectWrapper> ObjectFactory::NewObjectWrapper(const JSHandle<JSTaggedValue> &value)
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(objectWrapperClass_);
    ObjectWrapper *obj = ObjectWrapper::Cast(header);
    obj->SetValue(thread_, value);
    return JSHandle<ObjectWrapper>(thread_, obj);
}

JSHandle<EcmaString> ObjectFactory::GetStringFromStringTable(const uint8_t *utf8Data, uint32_t utf8Len,
                                                             bool canBeCompress) const
{
    if (utf8Len == 0) {
        return GetEmptyString();
    }
    auto stringTable = vm_->GetEcmaStringTable();
    return JSHandle<EcmaString>(thread_, stringTable->GetOrInternString(utf8Data, utf8Len, canBeCompress));
}

JSHandle<EcmaString> ObjectFactory::GetStringFromStringTable(const uint16_t *utf16Data, uint32_t utf16Len,
                                                             bool canBeCompress) const
{
    if (utf16Len == 0) {
        return GetEmptyString();
    }
    auto stringTable = vm_->GetEcmaStringTable();
    return JSHandle<EcmaString>(thread_, stringTable->GetOrInternString(utf16Data, utf16Len, canBeCompress));
}

JSHandle<EcmaString> ObjectFactory::GetStringFromStringTable(EcmaString *string) const
{
    ASSERT(string != nullptr);
    if (string->GetLength() == 0) {
        return GetEmptyString();
    }
    auto stringTable = vm_->GetEcmaStringTable();
    return JSHandle<EcmaString>(thread_, stringTable->GetOrInternString(string));
}

// NB! don't do special case for C0 80, it means '\u0000', so don't convert to UTF-8
EcmaString *ObjectFactory::GetRawStringFromStringTable(const uint8_t *mutf8Data, uint32_t utf16Len) const
{
    if (UNLIKELY(utf16Len == 0)) {
        return *GetEmptyString();
    }

    if (utf::IsMUtf8OnlySingleBytes(mutf8Data)) {
        return EcmaString::Cast(vm_->GetEcmaStringTable()->GetOrInternString(mutf8Data, utf16Len, true));
    }

    CVector<uint16_t> utf16Data(utf16Len);
    auto len = utf::ConvertRegionMUtf8ToUtf16(mutf8Data, utf16Data.data(), utf::Mutf8Size(mutf8Data), utf16Len, 0);
    return EcmaString::Cast(vm_->GetEcmaStringTable()->GetOrInternString(utf16Data.data(), len, false));
}

JSHandle<PropertyBox> ObjectFactory::NewPropertyBox(const JSHandle<JSTaggedValue> &value)
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(PropertyBoxClass_);
    JSHandle<PropertyBox> box(thread_, header);
    box->SetValue(thread_, value);
    return box;
}

JSHandle<ProtoChangeMarker> ObjectFactory::NewProtoChangeMarker()
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(protoChangeMarkerClass_);
    JSHandle<ProtoChangeMarker> marker(thread_, header);
    marker->SetHasChanged(false);
    return marker;
}

JSHandle<ProtoChangeDetails> ObjectFactory::NewProtoChangeDetails()
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(protoChangeDetailsClass_);
    JSHandle<ProtoChangeDetails> protoInfo(thread_, header);
    protoInfo->SetChangeListener(thread_, JSTaggedValue(0));
    protoInfo->SetRegisterIndex(thread_, JSTaggedValue(ProtoChangeDetails::UNREGISTERED));
    return protoInfo;
}

JSHandle<ProfileTypeInfo> ObjectFactory::NewProfileTypeInfo(uint32_t length)
{
    NewObjectHook();
    ASSERT(length > 0);

    size_t size = TaggedArray::ComputeSize(JSTaggedValue::TaggedTypeSize(), length);
    auto header = heapHelper_.AllocateYoungGenerationOrHugeObject(arrayClass_, size);
    JSHandle<ProfileTypeInfo> array(thread_, header);
    array->InitializeWithSpecialValue(JSTaggedValue::Undefined(), length);

    return array;
}

// static
void ObjectFactory::NewObjectHook()
{
#ifndef NDEBUG
    if (!isTriggerGc_) {
        return;
    }
    if (vm_->IsInitialized()) {
        if (triggerSemiGC_) {
            vm_->CollectGarbage(TriggerGCType::SEMI_GC);
        } else {
            vm_->CollectGarbage(TriggerGCType::COMPRESS_FULL_GC);
        }
    }
#endif
}

void ObjectFactory::SetTriggerGc(bool flag, bool triggerSemiGC)
{
    isTriggerGc_ = flag;
    triggerSemiGC_ = triggerSemiGC;
}

JSHandle<TaggedQueue> ObjectFactory::NewTaggedQueue(array_size_t length)
{
    NewObjectHook();

    array_size_t queueLength = TaggedQueue::QueueToArrayIndex(length);
    auto queue = JSHandle<TaggedQueue>::Cast(NewTaggedArray(queueLength, JSTaggedValue::Hole()));
    queue->SetStart(thread_, JSTaggedValue(0));  // equal to 0 when add 1.
    queue->SetEnd(thread_, JSTaggedValue(0));
    queue->SetCapacity(thread_, JSTaggedValue(length));

    return queue;
}

JSHandle<TaggedQueue> ObjectFactory::GetEmptyTaggedQueue() const
{
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    return JSHandle<TaggedQueue>(env->GetEmptyTaggedQueue());
}

JSHandle<JSSetIterator> ObjectFactory::NewJSSetIterator(const JSHandle<JSSet> &set, IterationKind kind)
{
    NewObjectHook();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSTaggedValue> protoValue = env->GetSetIteratorPrototype();
    JSHandle<JSHClass> dynHandle = NewEcmaDynClass(JSSetIterator::SIZE, JSType::JS_SET_ITERATOR, protoValue);
    JSHandle<JSSetIterator> iter(NewJSObject(dynHandle));
    iter->GetJSHClass()->SetExtensible(true);
    iter->SetIteratedSet(thread_, set->GetLinkedSet());
    iter->SetNextIndex(thread_, JSTaggedValue(0));
    iter->SetIterationKind(thread_, JSTaggedValue(static_cast<int>(kind)));
    return iter;
}

JSHandle<JSMapIterator> ObjectFactory::NewJSMapIterator(const JSHandle<JSMap> &map, IterationKind kind)
{
    NewObjectHook();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSTaggedValue> protoValue = env->GetMapIteratorPrototype();
    JSHandle<JSHClass> dynHandle = NewEcmaDynClass(JSMapIterator::SIZE, JSType::JS_MAP_ITERATOR, protoValue);
    JSHandle<JSMapIterator> iter(NewJSObject(dynHandle));
    iter->GetJSHClass()->SetExtensible(true);
    iter->SetIteratedMap(thread_, map->GetLinkedMap());
    iter->SetNextIndex(thread_, JSTaggedValue(0));
    iter->SetIterationKind(thread_, JSTaggedValue(static_cast<int>(kind)));
    return iter;
}

JSHandle<JSArrayIterator> ObjectFactory::NewJSArrayIterator(const JSHandle<JSObject> &array, IterationKind kind)
{
    NewObjectHook();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSTaggedValue> protoValue = env->GetArrayIteratorPrototype();
    JSHandle<JSHClass> dynHandle = NewEcmaDynClass(JSArrayIterator::SIZE, JSType::JS_ARRAY_ITERATOR, protoValue);
    JSHandle<JSArrayIterator> iter(NewJSObject(dynHandle));
    iter->GetJSHClass()->SetExtensible(true);
    iter->SetIteratedArray(thread_, array);
    iter->SetNextIndex(thread_, JSTaggedValue(0));
    iter->SetIterationKind(thread_, JSTaggedValue(static_cast<int>(kind)));
    return iter;
}

JSHandle<JSPromiseReactionsFunction> ObjectFactory::CreateJSPromiseReactionsFunction(const void *nativeFunc)
{
    NewObjectHook();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSHClass> dynclass = JSHandle<JSHClass>::Cast(env->GetPromiseReactionFunctionClass());

    JSHandle<JSPromiseReactionsFunction> reactionsFunction =
        JSHandle<JSPromiseReactionsFunction>::Cast(NewJSObject(dynclass));
    reactionsFunction->SetPromise(thread_, JSTaggedValue::Hole());
    reactionsFunction->SetAlreadyResolved(thread_, JSTaggedValue::Hole());
    JSMethod *method = vm_->GetMethodForNativeFunction(nativeFunc);
    reactionsFunction->SetCallTarget(thread_, method);
    JSHandle<JSFunction> function = JSHandle<JSFunction>::Cast(reactionsFunction);
    JSFunction::InitializeJSFunction(thread_, function);
    JSFunction::SetFunctionLength(thread_, function, JSTaggedValue(1));
    return reactionsFunction;
}

JSHandle<JSPromiseExecutorFunction> ObjectFactory::CreateJSPromiseExecutorFunction(const void *nativeFunc)
{
    NewObjectHook();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSHClass> dynclass = JSHandle<JSHClass>::Cast(env->GetPromiseExecutorFunctionClass());
    JSHandle<JSPromiseExecutorFunction> executorFunction =
        JSHandle<JSPromiseExecutorFunction>::Cast(NewJSObject(dynclass));
    executorFunction->SetCapability(thread_, JSTaggedValue::Hole());
    JSMethod *method = vm_->GetMethodForNativeFunction(nativeFunc);
    executorFunction->SetCallTarget(thread_, method);
    executorFunction->SetCapability(thread_, JSTaggedValue::Undefined());
    JSHandle<JSFunction> function = JSHandle<JSFunction>::Cast(executorFunction);
    JSFunction::InitializeJSFunction(thread_, function, FunctionKind::NORMAL_FUNCTION);
    JSFunction::SetFunctionLength(thread_, function, JSTaggedValue(FunctionLength::TWO));
    return executorFunction;
}

JSHandle<JSPromiseAllResolveElementFunction> ObjectFactory::NewJSPromiseAllResolveElementFunction(
    const void *nativeFunc)
{
    NewObjectHook();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSHClass> dynclass = JSHandle<JSHClass>::Cast(env->GetPromiseAllResolveElementFunctionClass());
    JSHandle<JSPromiseAllResolveElementFunction> function =
        JSHandle<JSPromiseAllResolveElementFunction>::Cast(NewJSObject(dynclass));
    JSFunction::InitializeJSFunction(thread_, JSHandle<JSFunction>::Cast(function));
    JSMethod *method = vm_->GetMethodForNativeFunction(nativeFunc);
    function->SetCallTarget(thread_, method);
    JSFunction::SetFunctionLength(thread_, JSHandle<JSFunction>::Cast(function), JSTaggedValue(1));
    return function;
}

EcmaString *ObjectFactory::InternString(const JSHandle<JSTaggedValue> &key)
{
    EcmaString *str = EcmaString::Cast(key->GetTaggedObject());
    if (str->IsInternString()) {
        return str;
    }

    EcmaStringTable *stringTable = vm_->GetEcmaStringTable();
    return stringTable->GetOrInternString(str);
}

JSHandle<TransitionHandler> ObjectFactory::NewTransitionHandler()
{
    NewObjectHook();
    TransitionHandler *handler =
        TransitionHandler::Cast(heapHelper_.AllocateYoungGenerationOrHugeObject(transitionHandlerClass_));
    return JSHandle<TransitionHandler>(thread_, handler);
}

JSHandle<PrototypeHandler> ObjectFactory::NewPrototypeHandler()
{
    NewObjectHook();
    PrototypeHandler *header =
        PrototypeHandler::Cast(heapHelper_.AllocateYoungGenerationOrHugeObject(prototypeHandlerClass_));
    JSHandle<PrototypeHandler> handler(thread_, header);
    handler->SetHandlerInfo(thread_, JSTaggedValue::Undefined());
    handler->SetProtoCell(thread_, JSTaggedValue::Undefined());
    handler->SetHolder(thread_, JSTaggedValue::Undefined());
    return handler;
}

JSHandle<PromiseRecord> ObjectFactory::NewPromiseRecord()
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(promiseRecordClass_);
    JSHandle<PromiseRecord> obj(thread_, header);
    obj->SetValue(thread_, JSTaggedValue::Undefined());
    return obj;
}

JSHandle<ResolvingFunctionsRecord> ObjectFactory::NewResolvingFunctionsRecord()
{
    NewObjectHook();
    TaggedObject *header = heapHelper_.AllocateYoungGenerationOrHugeObject(promiseResolvingFunctionsRecord_);
    JSHandle<ResolvingFunctionsRecord> obj(thread_, header);
    obj->SetResolveFunction(thread_, JSTaggedValue::Undefined());
    obj->SetRejectFunction(thread_, JSTaggedValue::Undefined());
    return obj;
}

JSHandle<JSHClass> ObjectFactory::CreateObjectClass(const JSHandle<TaggedArray> &keys,
                                                    const JSHandle<TaggedArray> &values)
{
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSTaggedValue> proto = env->GetObjectFunctionPrototype();

    JSHandle<JSHClass> objClass = NewEcmaDynClass(JSObject::SIZE, JSType::JS_OBJECT, proto);

    uint32_t fieldOrder = 0;
    JSMutableHandle<JSTaggedValue> key(thread_, JSTaggedValue::Undefined());
    JSMutableHandle<JSTaggedValue> value(thread_, JSTaggedValue::Undefined());
    uint32_t length = keys->GetLength();
    JSHandle<LayoutInfo> layoutInfoHandle = CreateLayoutInfo(length);
    while (fieldOrder < length) {
        key.Update(keys->Get(fieldOrder));
        value.Update(values->Get(fieldOrder));
        ASSERT_PRINT(JSTaggedValue::IsPropertyKey(key), "Key is not a property key");
        PropertyAttributes attributes = PropertyAttributes::Default(true, true, true);

        if (values->Get(fieldOrder).IsAccessorData()) {
            attributes.SetIsAccessor(true);
        }

        if (fieldOrder >= JSObject::MIN_PROPERTIES_LENGTH) {
            attributes.SetIsInlinedProps(false);
        } else {
            attributes.SetIsInlinedProps(true);
        }

        attributes.SetRepresentation(Representation::MIXED);
        attributes.SetOffset(fieldOrder);
        layoutInfoHandle->AddKey(thread_, fieldOrder, key.GetTaggedValue(), attributes);
        fieldOrder++;
    }

    {
        objClass->SetExtensible(true);
        objClass->SetIsLiteral(true);
        objClass->SetAttributes(thread_, layoutInfoHandle);

        if (fieldOrder > JSObject::MIN_PROPERTIES_LENGTH) {
            uint32_t unusedNonInlinedProps = objClass->GetUnusedNonInlinedProps();
            ASSERT(unusedNonInlinedProps >= (fieldOrder - JSObject::MIN_PROPERTIES_LENGTH));
            objClass->SetUnusedNonInlinedProps(unusedNonInlinedProps - (fieldOrder - JSObject::MIN_PROPERTIES_LENGTH));
            objClass->SetUnusedInlinedProps(0);
        } else {
            uint32_t unusedInlinedProps = objClass->GetUnusedInlinedProps();
            ASSERT(unusedInlinedProps >= fieldOrder);
            objClass->SetUnusedInlinedProps(unusedInlinedProps - fieldOrder);
        }
    }
    return objClass;
}

JSHandle<JSHClass> ObjectFactory::CreateObjectClass(const JSHandle<TaggedArray> &properties, size_t length)
{
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSTaggedValue> proto = env->GetObjectFunctionPrototype();
    JSHandle<JSHClass> objClass = NewEcmaDynClass(JSObject::SIZE, JSType::JS_OBJECT, proto);

    uint32_t fieldOrder = 0;
    JSMutableHandle<JSTaggedValue> key(thread_, JSTaggedValue::Undefined());
    JSHandle<LayoutInfo> layoutInfoHandle = CreateLayoutInfo(length);
    while (fieldOrder < length) {
        key.Update(properties->Get(fieldOrder * 2));  // 2: Meaning to double
        ASSERT_PRINT(JSTaggedValue::IsPropertyKey(key), "Key is not a property key");
        PropertyAttributes attributes = PropertyAttributes::Default(true, true, true);

        if (properties->Get(fieldOrder * 2 + 1).IsAccessor()) {  // 2: Meaning to double
            attributes.SetIsAccessor(true);
        }

        if (fieldOrder >= JSObject::MIN_PROPERTIES_LENGTH) {
            attributes.SetIsInlinedProps(false);
        } else {
            attributes.SetIsInlinedProps(true);
        }

        attributes.SetRepresentation(Representation::MIXED);
        attributes.SetOffset(fieldOrder);
        layoutInfoHandle->AddKey(thread_, fieldOrder, key.GetTaggedValue(), attributes);
        fieldOrder++;
    }

    {
        objClass->SetExtensible(true);
        objClass->SetIsLiteral(true);
        objClass->SetAttributes(thread_, layoutInfoHandle);
        if (fieldOrder > JSObject::MIN_PROPERTIES_LENGTH) {
            uint32_t unusedNonInlinedProps = objClass->GetUnusedNonInlinedProps();
            ASSERT(unusedNonInlinedProps >= (fieldOrder - JSObject::MIN_PROPERTIES_LENGTH));
            objClass->SetUnusedNonInlinedProps(unusedNonInlinedProps - (fieldOrder - JSObject::MIN_PROPERTIES_LENGTH));
            objClass->SetUnusedInlinedProps(0);
        } else {
            uint32_t unusedInlinedProps = objClass->GetUnusedInlinedProps();
            ASSERT(unusedInlinedProps >= fieldOrder);
            objClass->SetUnusedInlinedProps(unusedInlinedProps - fieldOrder);
        }
    }
    return objClass;
}

JSHandle<JSObject> ObjectFactory::NewJSObjectByClass(const JSHandle<TaggedArray> &keys,
                                                     const JSHandle<TaggedArray> &values)
{
    NewObjectHook();
    JSHandle<JSHClass> dynclass = CreateObjectClass(keys, values);
    JSHandle<JSObject> obj = NewJSObject(dynclass);
    return obj;
}

JSHandle<JSObject> ObjectFactory::NewJSObjectByClass(const JSHandle<TaggedArray> &properties, size_t length)
{
    NewObjectHook();
    JSHandle<JSHClass> dynclass = CreateObjectClass(properties, length);
    JSHandle<JSObject> obj = NewJSObject(dynclass);
    return obj;
}

JSHandle<JSObject> ObjectFactory::NewEmptyJSObject()
{
    NewObjectHook();
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSTaggedValue> builtinObj = env->GetObjectFunction();
    return NewJSObjectByConstructor(JSHandle<JSFunction>(builtinObj), builtinObj);
}

EcmaString *ObjectFactory::ResolveString(uint32_t stringId)
{
    JSMethod *caller = EcmaFrameHandler(thread_).GetMethod();
    auto *pf = caller->GetPandaFile();
    auto id = panda_file::File::EntityId(stringId);
    auto foundStr = pf->GetStringData(id);

    return GetRawStringFromStringTable(foundStr.data, foundStr.utf16_length);
}

uintptr_t ObjectFactory::NewSpaceBySnapShotAllocator(size_t size)
{
    return heapHelper_.AllocateSnapShotSpace(size);
}

JSHandle<JSNativeObject> ObjectFactory::NewJSNativeObject(void *externalPointer)
{
    return NewJSNativeObject(externalPointer, nullptr, nullptr);
}

JSHandle<JSNativeObject> ObjectFactory::NewJSNativeObject(void *externalPointer, DeleteEntryPoint callback, void *data)
{
    JSHandle<GlobalEnv> env = vm_->GetGlobalEnv();
    JSHandle<JSHClass> dynclass(env->GetJSNativeObjectClass());
    JSHandle<JSNativeObject> nativeObject = JSHandle<JSNativeObject>::Cast(NewJSObject(dynclass));
    JSHandle<JSNativePointer> pointer = NewJSNativePointer(externalPointer, callback, data);
    nativeObject->SetJSNativePointer(thread_, pointer.GetTaggedValue());
    vm_->PushToArrayDataList(*pointer);
    return nativeObject;
}

JSHandle<MachineCode> ObjectFactory::NewMachineCodeObject(size_t length, const uint8_t *data)
{
    NewObjectHook();
    TaggedObject *obj = heapHelper_.AllocateMachineCodeSpaceObject(machineCodeClass_, length + sizeof(MachineCode));
    MachineCode *code = MachineCode::Cast(obj);
    code->SetInstructionSizeInBytes(thread_, JSTaggedValue(static_cast<int>(length)));
    if (data != nullptr) {
        code->SetData(data, length);
    }
    JSHandle<MachineCode> codeObj(thread_, code);
    return codeObj;
}

// ----------------------------------- new string ----------------------------------------
JSHandle<EcmaString> ObjectFactory::NewFromString(const CString &data)
{
    NewObjectHook();
    auto utf8Data = reinterpret_cast<const uint8_t *>(data.c_str());
    bool canBeCompress = EcmaString::CanBeCompressed(utf8Data);
    return GetStringFromStringTable(utf8Data, data.length(), canBeCompress);
}

JSHandle<EcmaString> ObjectFactory::NewFromCanBeCompressString(const CString &data)
{
    NewObjectHook();
    auto utf8Data = reinterpret_cast<const uint8_t *>(data.c_str());
    ASSERT(EcmaString::CanBeCompressed(utf8Data));
    return GetStringFromStringTable(utf8Data, data.length(), true);
}

JSHandle<EcmaString> ObjectFactory::NewFromStdString(const std::string &data)
{
    NewObjectHook();
    auto utf8Data = reinterpret_cast<const uint8_t *>(data.c_str());
    bool canBeCompress = EcmaString::CanBeCompressed(utf8Data);
    return GetStringFromStringTable(utf8Data, data.size(), canBeCompress);
}

JSHandle<EcmaString> ObjectFactory::NewFromStdStringUnCheck(const std::string &data, bool canBeCompress)
{
    NewObjectHook();
    auto utf8Data = reinterpret_cast<const uint8_t *>(data.c_str());
    return GetStringFromStringTable(utf8Data, data.size(), canBeCompress);
}

JSHandle<EcmaString> ObjectFactory::NewFromUtf8(const uint8_t *utf8Data, uint32_t utf8Len)
{
    NewObjectHook();
    bool canBeCompress = EcmaString::CanBeCompressed(utf8Data);
    return GetStringFromStringTable(utf8Data, utf8Len, canBeCompress);
}

JSHandle<EcmaString> ObjectFactory::NewFromUtf8UnCheck(const uint8_t *utf8Data, uint32_t utf8Len, bool canBeCompress)
{
    NewObjectHook();
    return GetStringFromStringTable(utf8Data, utf8Len, canBeCompress);
}

JSHandle<EcmaString> ObjectFactory::NewFromUtf16(const uint16_t *utf16Data, uint32_t utf16Len)
{
    NewObjectHook();
    bool canBeCompress = EcmaString::CanBeCompressed(utf16Data, utf16Len);
    return GetStringFromStringTable(utf16Data, utf16Len, canBeCompress);
}

JSHandle<EcmaString> ObjectFactory::NewFromUtf16UnCheck(const uint16_t *utf16Data, uint32_t utf16Len,
                                                        bool canBeCompress)
{
    NewObjectHook();
    return GetStringFromStringTable(utf16Data, utf16Len, canBeCompress);
}

JSHandle<EcmaString> ObjectFactory::NewFromUtf8Literal(const uint8_t *utf8Data, uint32_t utf8Len)
{
    NewObjectHook();
    bool canBeCompress = EcmaString::CanBeCompressed(utf8Data);
    return JSHandle<EcmaString>(thread_, EcmaString::CreateFromUtf8(utf8Data, utf8Len, vm_, canBeCompress));
}

JSHandle<EcmaString> ObjectFactory::NewFromUtf8LiteralUnCheck(const uint8_t *utf8Data, uint32_t utf8Len,
                                                              bool canBeCompress)
{
    NewObjectHook();
    return JSHandle<EcmaString>(thread_, EcmaString::CreateFromUtf8(utf8Data, utf8Len, vm_, canBeCompress));
}

JSHandle<EcmaString> ObjectFactory::NewFromUtf16Literal(const uint16_t *utf16Data, uint32_t utf16Len)
{
    NewObjectHook();
    bool canBeCompress = EcmaString::CanBeCompressed(utf16Data, utf16Len);
    return JSHandle<EcmaString>(thread_, EcmaString::CreateFromUtf16(utf16Data, utf16Len, vm_, canBeCompress));
}

JSHandle<EcmaString> ObjectFactory::NewFromUtf16LiteralUnCheck(const uint16_t *utf16Data, uint32_t utf16Len,
                                                               bool canBeCompress)
{
    NewObjectHook();
    return JSHandle<EcmaString>(thread_, EcmaString::CreateFromUtf16(utf16Data, utf16Len, vm_, canBeCompress));
}

JSHandle<EcmaString> ObjectFactory::NewFromString(EcmaString *str)
{
    NewObjectHook();
    return GetStringFromStringTable(str);
}

JSHandle<EcmaString> ObjectFactory::ConcatFromString(const JSHandle<EcmaString> &prefix,
                                                     const JSHandle<EcmaString> &suffix)
{
    NewObjectHook();
    EcmaString *concatString = EcmaString::Concat(prefix, suffix, vm_);
    return GetStringFromStringTable(concatString);
}
}  // namespace panda::ecmascript
