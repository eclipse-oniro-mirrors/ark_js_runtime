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

#include "ecmascript/containers/containers_private.h"

#include "containers_arraylist.h"
#include "containers_treemap.h"
#include "containers_treeset.h"
#include "ecmascript/global_env.h"
#include "ecmascript/global_env_constants.h"
#include "ecmascript/interpreter/fast_runtime_stub-inl.h"
#include "ecmascript/js_api_tree_map.h"
#include "ecmascript/js_api_tree_map_iterator.h"
#include "ecmascript/js_api_tree_set.h"
#include "ecmascript/js_api_tree_set_iterator.h"
#include "ecmascript/js_api_arraylist.h"
#include "ecmascript/js_api_arraylist_iterator.h"
#include "ecmascript/js_function.h"

namespace panda::ecmascript::containers {
JSTaggedValue ContainersPrivate::Load(EcmaRuntimeCallInfo *msg)
{
    ASSERT(msg);
    JSThread *thread = msg->GetThread();
    [[maybe_unused]] EcmaHandleScope handleScope(thread);
    JSHandle<JSTaggedValue> argv = GetCallArg(msg, 0);
    JSHandle<JSObject> thisValue(GetThis(msg));

    uint32_t tag = 0;
    if (!JSTaggedValue::ToElementIndex(argv.GetTaggedValue(), &tag) || tag >= ContainerTag::END) {
        THROW_TYPE_ERROR_AND_RETURN(thread, "Incorrect input parameters", JSTaggedValue::Exception());
    }

    JSTaggedValue res = JSTaggedValue::Undefined();
    switch (tag) {
        case ContainerTag::ArrayList: {
            res = InitializeContainer(thread, thisValue, InitializeArrayList, "ArrayListConstructor");
            break;
        }
        case ContainerTag::TreeMap: {
            res = InitializeContainer(thread, thisValue, InitializeTreeMap, "TreeMapConstructor");
            break;
        }
        case ContainerTag::TreeSet: {
            res = InitializeContainer(thread, thisValue, InitializeTreeSet, "TreeSetConstructor");
            break;
        }
        case ContainerTag::Queue:
        case ContainerTag::Deque:
        case ContainerTag::Stack:
        case ContainerTag::Vector:
        case ContainerTag::List:
        case ContainerTag::LinkedList:
        case ContainerTag::HashMap:
        case ContainerTag::HashSet:
        case ContainerTag::LightWeightMap:
        case ContainerTag::LightWeightSet:
        case ContainerTag::PlainArray:
        case ContainerTag::END:
            break;
        default:
            UNREACHABLE();
    }

    return res;
}

JSTaggedValue ContainersPrivate::InitializeContainer(JSThread *thread, const JSHandle<JSObject> &obj,
                                                     InitializeFunction func, const char *name)
{
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    JSHandle<JSTaggedValue> key(factory->NewFromCanBeCompressString(name));
    JSTaggedValue value =
        FastRuntimeStub::GetPropertyByName<true>(thread, obj.GetTaggedValue(), key.GetTaggedValue());
    if (value != JSTaggedValue::Undefined()) {
        return value;
    }
    JSHandle<JSTaggedValue> map = func(thread);
    SetFrozenConstructor(thread, obj, name, map);
    return map.GetTaggedValue();
}

JSHandle<JSFunction> ContainersPrivate::NewContainerConstructor(JSThread *thread, const JSHandle<JSObject> &prototype,
                                                                EcmaEntrypoint ctorFunc, const char *name, int length)
{
    JSHandle<GlobalEnv> env = thread->GetEcmaVM()->GetGlobalEnv();
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    JSHandle<JSFunction> ctor =
        factory->NewJSFunction(env, reinterpret_cast<void *>(ctorFunc), FunctionKind::BUILTIN_CONSTRUCTOR);

    const GlobalEnvConstants *globalConst = thread->GlobalConstants();
    JSFunction::SetFunctionLength(thread, ctor, JSTaggedValue(length));
    JSHandle<JSTaggedValue> nameString(factory->NewFromCanBeCompressString(name));
    JSFunction::SetFunctionName(thread, JSHandle<JSFunctionBase>(ctor), nameString,
                                globalConst->GetHandledUndefined());
    JSHandle<JSTaggedValue> constructorKey = globalConst->GetHandledConstructorString();
    PropertyDescriptor descriptor1(thread, JSHandle<JSTaggedValue>::Cast(ctor), true, false, true);
    JSObject::DefineOwnProperty(thread, prototype, constructorKey, descriptor1);

    /* set "prototype" in constructor */
    ctor->SetFunctionPrototype(thread, prototype.GetTaggedValue());

    return ctor;
}

void ContainersPrivate::SetFrozenFunction(JSThread *thread, const JSHandle<JSObject> &obj, const char *key,
                                          EcmaEntrypoint func, int length)
{
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    JSHandle<JSTaggedValue> keyString(factory->NewFromCanBeCompressString(key));
    JSHandle<JSFunction> function = NewFunction(thread, keyString, func, length);
    PropertyDescriptor descriptor(thread, JSHandle<JSTaggedValue>(function), false, false, false);
    JSObject::DefineOwnProperty(thread, obj, keyString, descriptor);
}

void ContainersPrivate::SetFrozenConstructor(JSThread *thread, const JSHandle<JSObject> &obj, const char *keyChar,
                                             JSHandle<JSTaggedValue> &value)
{
    JSObject::PreventExtensions(thread, JSHandle<JSObject>::Cast(value));
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    JSHandle<JSTaggedValue> key(factory->NewFromCanBeCompressString(keyChar));
    PropertyDescriptor descriptor(thread, value, false, false, false);
    JSObject::DefineOwnProperty(thread, obj, key, descriptor);
}

JSHandle<JSFunction> ContainersPrivate::NewFunction(JSThread *thread, const JSHandle<JSTaggedValue> &key,
                                                    EcmaEntrypoint func, int length)
{
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    JSHandle<JSFunction> function =
        factory->NewJSFunction(thread->GetEcmaVM()->GetGlobalEnv(), reinterpret_cast<void *>(func));
    JSFunction::SetFunctionLength(thread, function, JSTaggedValue(length));
    JSHandle<JSFunctionBase> baseFunction(function);
    JSFunction::SetFunctionName(thread, baseFunction, key, thread->GlobalConstants()->GetHandledUndefined());
    return function;
}

JSHandle<JSTaggedValue> ContainersPrivate::CreateGetter(JSThread *thread, EcmaEntrypoint func, const char *name,
                                                        int length)
{
    JSHandle<GlobalEnv> env = thread->GetEcmaVM()->GetGlobalEnv();
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    JSHandle<JSFunction> function = factory->NewJSFunction(env, reinterpret_cast<void *>(func));
    JSFunction::SetFunctionLength(thread, function, JSTaggedValue(length));
    JSHandle<JSTaggedValue> funcName(factory->NewFromString(name));
    JSHandle<JSTaggedValue> prefix = thread->GlobalConstants()->GetHandledGetString();
    JSFunction::SetFunctionName(thread, JSHandle<JSFunctionBase>(function), funcName, prefix);
    return JSHandle<JSTaggedValue>(function);
}

void ContainersPrivate::SetGetter(JSThread *thread, const JSHandle<JSObject> &obj, const JSHandle<JSTaggedValue> &key,
                                  const JSHandle<JSTaggedValue> &getter)
{
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    JSHandle<AccessorData> accessor = factory->NewAccessorData();
    accessor->SetGetter(thread, getter);
    PropertyAttributes attr = PropertyAttributes::DefaultAccessor(false, false, false);
    JSObject::AddAccessor(thread, JSHandle<JSTaggedValue>::Cast(obj), key, accessor, attr);
}

void ContainersPrivate::SetFunctionAtSymbol(JSThread *thread, const JSHandle<GlobalEnv> &env,
                                            const JSHandle<JSObject> &obj, const JSHandle<JSTaggedValue> &symbol,
                                            const char *name, EcmaEntrypoint func, int length)
{
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    JSHandle<JSFunction> function = factory->NewJSFunction(env, reinterpret_cast<void *>(func));
    JSFunction::SetFunctionLength(thread, function, JSTaggedValue(length));
    JSHandle<JSTaggedValue> nameString(factory->NewFromString(name));
    JSHandle<JSFunctionBase> baseFunction(function);
    JSFunction::SetFunctionName(thread, baseFunction, nameString, thread->GlobalConstants()->GetHandledUndefined());

    PropertyDescriptor descriptor(thread, JSHandle<JSTaggedValue>::Cast(function), false, false, false);
    JSObject::DefineOwnProperty(thread, obj, symbol, descriptor);
}

void ContainersPrivate::SetStringTagSymbol(JSThread *thread, const JSHandle<GlobalEnv> &env,
                                           const JSHandle<JSObject> &obj, const char *key)
{
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    JSHandle<JSTaggedValue> tag(factory->NewFromString(key));
    JSHandle<JSTaggedValue> symbol = env->GetToStringTagSymbol();
    PropertyDescriptor desc(thread, tag, false, false, false);
    JSObject::DefineOwnProperty(thread, obj, symbol, desc);
}

JSHandle<JSTaggedValue> ContainersPrivate::InitializeArrayList(JSThread *thread)
{
    auto globalConst = const_cast<GlobalEnvConstants *>(thread->GlobalConstants());
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    // ArrayList.prototype
    JSHandle<JSObject> prototype = factory->NewEmptyJSObject();
    JSHandle<JSTaggedValue> arrayListFuncPrototypeValue(prototype);
    // ArrayList.prototype_or_dynclass
    JSHandle<JSHClass> arrayListInstanceDynclass =
        factory->NewEcmaDynClass(JSAPIArrayList::SIZE, JSType::JS_API_ARRAY_LIST, arrayListFuncPrototypeValue);
    // ArrayList() = new Function()
    JSHandle<JSTaggedValue> arrayListFunction(NewContainerConstructor(
        thread, prototype, ContainersArrayList::ArrayListConstructor, "ArrayList", FuncLength::ZERO));
    JSHandle<JSFunction>(arrayListFunction)->SetFunctionPrototype(thread, arrayListInstanceDynclass.GetTaggedValue());

    // "constructor" property on the prototype
    JSHandle<JSTaggedValue> constructorKey = globalConst->GetHandledConstructorString();
    JSObject::SetProperty(thread, JSHandle<JSTaggedValue>(prototype), constructorKey, arrayListFunction);

    // ArrayList.prototype
    SetFrozenFunction(thread, prototype, "add", ContainersArrayList::Add, FuncLength::ONE);
    SetFrozenFunction(thread, prototype, "insert", ContainersArrayList::Insert, FuncLength::TWO);
    SetFrozenFunction(thread, prototype, "clear", ContainersArrayList::Clear, FuncLength::ZERO);
    SetFrozenFunction(thread, prototype, "clone", ContainersArrayList::Clone, FuncLength::ZERO);
    SetFrozenFunction(thread, prototype, "has", ContainersArrayList::Has, FuncLength::ONE);
    SetFrozenFunction(thread, prototype, "getCapacity", ContainersArrayList::GetCapacity, FuncLength::ZERO);
    SetFrozenFunction(thread, prototype, "increaseCapacityTo",
                      ContainersArrayList::IncreaseCapacityTo, FuncLength::ONE);
    SetFrozenFunction(thread, prototype, "trimToCurrentLength",
                      ContainersArrayList::TrimToCurrentLength, FuncLength::ZERO);
    SetFrozenFunction(thread, prototype, "getIndexOf", ContainersArrayList::GetIndexOf, FuncLength::ONE);
    SetFrozenFunction(thread, prototype, "isEmpty", ContainersArrayList::IsEmpty, FuncLength::ZERO);
    SetFrozenFunction(thread, prototype, "getLastIndexOf", ContainersArrayList::GetLastIndexOf, FuncLength::ONE);
    SetFrozenFunction(thread, prototype, "removeByIndex", ContainersArrayList::RemoveByIndex, FuncLength::ONE);
    SetFrozenFunction(thread, prototype, "remove", ContainersArrayList::Remove, FuncLength::ONE);
    SetFrozenFunction(thread, prototype, "removeByRange", ContainersArrayList::RemoveByRange, FuncLength::TWO);
    SetFrozenFunction(thread, prototype, "replaceAllElements",
                      ContainersArrayList::ReplaceAllElements, FuncLength::TWO);
    SetFrozenFunction(thread, prototype, "sort", ContainersArrayList::Sort, FuncLength::ONE);
    SetFrozenFunction(thread, prototype, "subArrayList", ContainersArrayList::SubArrayList, FuncLength::TWO);
    SetFrozenFunction(thread, prototype, "convertToArray", ContainersArrayList::ConvertToArray, FuncLength::ZERO);
    SetFrozenFunction(thread, prototype, "forEach", ContainersArrayList::ForEach, FuncLength::TWO);

    JSHandle<GlobalEnv> env = thread->GetEcmaVM()->GetGlobalEnv();
    SetStringTagSymbol(thread, env, prototype, "ArrayList");

    JSHandle<JSTaggedValue> lengthGetter = CreateGetter(thread, ContainersArrayList::GetSize, "length",
                                                        FuncLength::ZERO);
    JSHandle<JSTaggedValue> lengthKey(factory->NewFromCanBeCompressString("length"));
    SetGetter(thread, prototype, lengthKey, lengthGetter);

    SetFunctionAtSymbol(thread, env, prototype, env->GetIteratorSymbol(), "[Symbol.iterator]",
                        ContainersArrayList::GetIteratorObj, FuncLength::ONE);
    ContainersPrivate::InitializeArrayListIterator(thread, env, globalConst);
    globalConst->SetConstant(ConstantIndex::ARRAYLIST_FUNCTION_INDEX, arrayListFunction.GetTaggedValue());
    return arrayListFunction;
}

void ContainersPrivate::InitializeArrayListIterator(JSThread *thread, const JSHandle<GlobalEnv> &env,
                                                    GlobalEnvConstants *globalConst)
{
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    // Iterator.dynclass
    JSHandle<JSHClass> iteratorFuncDynclass =
        factory->NewEcmaDynClass(JSObject::SIZE, JSType::JS_ITERATOR, env->GetIteratorPrototype());
    // ArrayListIterator.prototype
    JSHandle<JSObject> arrayListIteratorPrototype(factory->NewJSObject(iteratorFuncDynclass));
    // Iterator.prototype.next()
    SetFrozenFunction(thread, arrayListIteratorPrototype, "next", JSAPIArrayListIterator::Next, FuncLength::ONE);
    SetStringTagSymbol(thread, env, arrayListIteratorPrototype, "ArrayList Iterator");
    globalConst->SetConstant(ConstantIndex::ARRAYLIST_ITERATOR_PROTOTYPE_INDEX,
                             arrayListIteratorPrototype.GetTaggedValue());
}

JSHandle<JSTaggedValue> ContainersPrivate::InitializeTreeMap(JSThread *thread)
{
    const GlobalEnvConstants *globalConst = thread->GlobalConstants();
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    // TreeMap.prototype
    JSHandle<JSObject> mapFuncPrototype = factory->NewEmptyJSObject();
    JSHandle<JSTaggedValue> mapFuncPrototypeValue(mapFuncPrototype);
    // TreeMap.prototype_or_dynclass
    JSHandle<JSHClass> mapInstanceDynclass =
        factory->NewEcmaDynClass(JSAPITreeMap::SIZE, JSType::JS_API_TREE_MAP, mapFuncPrototypeValue);
    // TreeMap() = new Function()
    JSHandle<JSTaggedValue> mapFunction(NewContainerConstructor(
        thread, mapFuncPrototype, ContainersTreeMap::TreeMapConstructor, "TreeMap", FuncLength::ZERO));
    JSHandle<JSFunction>(mapFunction)->SetFunctionPrototype(thread, mapInstanceDynclass.GetTaggedValue());

    // "constructor" property on the prototype
    JSHandle<JSTaggedValue> constructorKey = globalConst->GetHandledConstructorString();
    JSObject::SetProperty(thread, JSHandle<JSTaggedValue>(mapFuncPrototype), constructorKey, mapFunction);

    // TreeMap.prototype
    SetFrozenFunction(thread, mapFuncPrototype, "set", ContainersTreeMap::Set, FuncLength::TWO);
    SetFrozenFunction(thread, mapFuncPrototype, "get", ContainersTreeMap::Get, FuncLength::ONE);
    SetFrozenFunction(thread, mapFuncPrototype, "remove", ContainersTreeMap::Remove, FuncLength::ONE);
    SetFrozenFunction(thread, mapFuncPrototype, "hasKey", ContainersTreeMap::HasKey, FuncLength::ONE);
    SetFrozenFunction(thread, mapFuncPrototype, "hasValue", ContainersTreeMap::HasValue, FuncLength::ONE);
    SetFrozenFunction(thread, mapFuncPrototype, "getFirstKey", ContainersTreeMap::GetFirstKey, FuncLength::ZERO);
    SetFrozenFunction(thread, mapFuncPrototype, "getLastKey", ContainersTreeMap::GetLastKey, FuncLength::ZERO);
    SetFrozenFunction(thread, mapFuncPrototype, "setAll", ContainersTreeMap::SetAll, FuncLength::ONE);
    SetFrozenFunction(thread, mapFuncPrototype, "clear", ContainersTreeMap::Clear, FuncLength::ZERO);
    SetFrozenFunction(thread, mapFuncPrototype, "getLowerKey", ContainersTreeMap::GetLowerKey, FuncLength::ONE);
    SetFrozenFunction(thread, mapFuncPrototype, "getHigherKey", ContainersTreeMap::GetHigherKey, FuncLength::ONE);
    SetFrozenFunction(thread, mapFuncPrototype, "keys", ContainersTreeMap::Keys, FuncLength::ZERO);
    SetFrozenFunction(thread, mapFuncPrototype, "values", ContainersTreeMap::Values, FuncLength::ZERO);
    SetFrozenFunction(thread, mapFuncPrototype, "replace", ContainersTreeMap::Replace, FuncLength::TWO);
    SetFrozenFunction(thread, mapFuncPrototype, "forEach", ContainersTreeMap::ForEach, FuncLength::ONE);
    SetFrozenFunction(thread, mapFuncPrototype, "entries", ContainersTreeMap::Entries, FuncLength::ZERO);

    // @@ToStringTag
    JSHandle<GlobalEnv> env = thread->GetEcmaVM()->GetGlobalEnv();
    SetStringTagSymbol(thread, env, mapFuncPrototype, "TreeMap");
    // %TreeMapPrototype% [ @@iterator ]
    JSHandle<JSTaggedValue> iteratorSymbol = env->GetIteratorSymbol();
    JSHandle<JSTaggedValue> entries(factory->NewFromCanBeCompressString("entries"));
    JSHandle<JSTaggedValue> entriesFunc =
        JSObject::GetMethod(thread, JSHandle<JSTaggedValue>::Cast(mapFuncPrototype), entries);
    PropertyDescriptor descriptor(thread, entriesFunc, false, false, false);
    JSObject::DefineOwnProperty(thread, mapFuncPrototype, iteratorSymbol, descriptor);
    // length
    JSHandle<JSTaggedValue> lengthGetter =
        CreateGetter(thread, ContainersTreeMap::GetLength, "length", FuncLength::ZERO);
    JSHandle<JSTaggedValue> lengthKey(thread, globalConst->GetLengthString());
    SetGetter(thread, mapFuncPrototype, lengthKey, lengthGetter);

    InitializeTreeMapIterator(thread);
    return mapFunction;
}

void ContainersPrivate::InitializeTreeMapIterator(JSThread *thread)
{
    JSHandle<GlobalEnv> env = thread->GetEcmaVM()->GetGlobalEnv();
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    // Iterator.dynclass
    JSHandle<JSHClass> iteratorDynclass =
        factory->NewEcmaDynClass(JSObject::SIZE, JSType::JS_ITERATOR, env->GetIteratorPrototype());

    // TreeMapIterator.prototype
    JSHandle<JSObject> mapIteratorPrototype(factory->NewJSObject(iteratorDynclass));
    // TreeIterator.prototype.next()
    SetFrozenFunction(thread, mapIteratorPrototype, "next", JSAPITreeMapIterator::Next, FuncLength::ZERO);
    SetStringTagSymbol(thread, env, mapIteratorPrototype, "TreeMap Iterator");
    auto globalConst = const_cast<GlobalEnvConstants *>(thread->GlobalConstants());
    globalConst->SetConstant(ConstantIndex::TREEMAP_ITERATOR_PROTOTYPE_INDEX, mapIteratorPrototype.GetTaggedValue());
}

JSHandle<JSTaggedValue> ContainersPrivate::InitializeTreeSet(JSThread *thread)
{
    const GlobalEnvConstants *globalConst = thread->GlobalConstants();
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    // TreeSet.prototype
    JSHandle<JSObject> setFuncPrototype = factory->NewEmptyJSObject();
    JSHandle<JSTaggedValue> setFuncPrototypeValue(setFuncPrototype);
    // TreeSet.prototype_or_dynclass
    JSHandle<JSHClass> setInstanceDynclass =
        factory->NewEcmaDynClass(JSAPITreeSet::SIZE, JSType::JS_API_TREE_SET, setFuncPrototypeValue);
    // TreeSet() = new Function()
    JSHandle<JSTaggedValue> setFunction(NewContainerConstructor(
        thread, setFuncPrototype, ContainersTreeSet::TreeSetConstructor, "TreeSet", FuncLength::ZERO));
    JSHandle<JSFunction>(setFunction)->SetFunctionPrototype(thread, setInstanceDynclass.GetTaggedValue());

    // "constructor" property on the prototype
    JSHandle<JSTaggedValue> constructorKey = globalConst->GetHandledConstructorString();
    JSObject::SetProperty(thread, JSHandle<JSTaggedValue>(setFuncPrototype), constructorKey, setFunction);

    // TreeSet.prototype
    SetFrozenFunction(thread, setFuncPrototype, "add", ContainersTreeSet::Add, FuncLength::TWO);
    SetFrozenFunction(thread, setFuncPrototype, "remove", ContainersTreeSet::Remove, FuncLength::ONE);
    SetFrozenFunction(thread, setFuncPrototype, "has", ContainersTreeSet::Has, FuncLength::ONE);
    SetFrozenFunction(thread, setFuncPrototype, "getFirstValue", ContainersTreeSet::GetFirstValue, FuncLength::ZERO);
    SetFrozenFunction(thread, setFuncPrototype, "getLastValue", ContainersTreeSet::GetLastValue, FuncLength::ZERO);
    SetFrozenFunction(thread, setFuncPrototype, "clear", ContainersTreeSet::Clear, FuncLength::ZERO);
    SetFrozenFunction(thread, setFuncPrototype, "getLowerValue", ContainersTreeSet::GetLowerValue, FuncLength::ONE);
    SetFrozenFunction(thread, setFuncPrototype, "getHigherValue", ContainersTreeSet::GetHigherValue, FuncLength::ONE);
    SetFrozenFunction(thread, setFuncPrototype, "popFirst", ContainersTreeSet::PopFirst, FuncLength::ZERO);
    SetFrozenFunction(thread, setFuncPrototype, "popLast", ContainersTreeSet::PopLast, FuncLength::ZERO);
    SetFrozenFunction(thread, setFuncPrototype, "isEmpty", ContainersTreeSet::IsEmpty, FuncLength::TWO);
    SetFrozenFunction(thread, setFuncPrototype, "values", ContainersTreeSet::Values, FuncLength::ZERO);
    SetFrozenFunction(thread, setFuncPrototype, "forEach", ContainersTreeSet::ForEach, FuncLength::ONE);
    SetFrozenFunction(thread, setFuncPrototype, "entries", ContainersTreeSet::Entries, FuncLength::ZERO);

    // @@ToStringTag
    JSHandle<GlobalEnv> env = thread->GetEcmaVM()->GetGlobalEnv();
    SetStringTagSymbol(thread, env, setFuncPrototype, "TreeSet");
    // %TreeSetPrototype% [ @@iterator ]
    JSHandle<JSTaggedValue> iteratorSymbol = env->GetIteratorSymbol();
    JSHandle<JSTaggedValue> values(thread, globalConst->GetValuesString());
    JSHandle<JSTaggedValue> valuesFunc =
        JSObject::GetMethod(thread, JSHandle<JSTaggedValue>::Cast(setFuncPrototype), values);
    PropertyDescriptor descriptor(thread, valuesFunc, false, false, false);
    JSObject::DefineOwnProperty(thread, setFuncPrototype, iteratorSymbol, descriptor);
    // length
    JSHandle<JSTaggedValue> lengthGetter =
        CreateGetter(thread, ContainersTreeSet::GetLength, "length", FuncLength::ZERO);
    JSHandle<JSTaggedValue> lengthKey(thread, globalConst->GetLengthString());
    SetGetter(thread, setFuncPrototype, lengthKey, lengthGetter);

    InitializeTreeSetIterator(thread);
    return setFunction;
}

void ContainersPrivate::InitializeTreeSetIterator(JSThread *thread)
{
    JSHandle<GlobalEnv> env = thread->GetEcmaVM()->GetGlobalEnv();
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    // Iterator.dynclass
    JSHandle<JSHClass> iteratorDynclass =
        factory->NewEcmaDynClass(JSObject::SIZE, JSType::JS_ITERATOR, env->GetIteratorPrototype());

    // TreeSetIterator.prototype
    JSHandle<JSObject> setIteratorPrototype(factory->NewJSObject(iteratorDynclass));
    // TreeSetIterator.prototype.next()
    SetFrozenFunction(thread, setIteratorPrototype, "next", JSAPITreeSetIterator::Next, FuncLength::ZERO);
    SetStringTagSymbol(thread, env, setIteratorPrototype, "TreeSet Iterator");
    auto globalConst = const_cast<GlobalEnvConstants *>(thread->GlobalConstants());
    globalConst->SetConstant(ConstantIndex::TREESET_ITERATOR_PROTOTYPE_INDEX, setIteratorPrototype.GetTaggedValue());
}
}  // namespace panda::ecmascript::containers
