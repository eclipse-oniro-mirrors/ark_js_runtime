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

#ifndef ECMASCRIPT_OBJECT_FACTORY_H
#define ECMASCRIPT_OBJECT_FACTORY_H

#include "ecmascript/base/error_type.h"
#include "ecmascript/ecma_string.h"
#include "ecmascript/js_function_kind.h"
#include "ecmascript/js_handle.h"
#include "ecmascript/js_hclass.h"
#include "ecmascript/js_native_object.h"
#include "ecmascript/js_native_pointer.h"
#include "ecmascript/js_tagged_value.h"
#include "ecmascript/mem/machine_code.h"
#include "ecmascript/mem/ecma_heap_manager.h"
#include "ecmascript/mem/region_factory.h"
#include "ecmascript/object_wrapper.h"
#include "ecmascript/tagged_array.h"

namespace panda::ecmascript {
class JSObject;
class JSArray;
class JSSymbol;
class JSFunctionBase;
class JSFunction;
class JSBoundFunction;
class JSProxyRevocFunction;
class JSAsyncAwaitStatusFunction;
class JSPrimitiveRef;
class GlobalEnv;
class GlobalEnvConstants;
class AccessorData;
class JSGlobalObject;
class LexicalEnv;
class JSDate;
class JSProxy;
class JSRealm;
class JSArguments;
class TaggedQueue;
class JSForInIterator;
class JSSet;
class JSMap;
class JSRegExp;
class JSSetIterator;
class JSMapIterator;
class JSArrayIterator;
class JSStringIterator;
class JSGeneratorObject;
class CompletionRecord;
class GeneratorContext;
class JSArrayBuffer;
class JSDataView;
class JSPromise;
class JSPromiseReactionsFunction;
class JSPromiseExecutorFunction;
class JSPromiseAllResolveElementFunction;
class PromiseReaction;
class PromiseCapability;
class PromiseIteratorRecord;
class JSAsyncFuncObject;
class JSAsyncFunction;
class PromiseRecord;
class JSLocale;
class ResolvingFunctionsRecord;
class JSFunctionExtraInfo;
class EcmaVM;
class Heap;
class ConstantPool;
class Program;
class EcmaModule;
class LayoutInfo;
class JSIntlBoundFunction;
class FreeObject;
class JSNativePointer;

namespace job {
class MicroJobQueue;
class PendingJob;
}  // namespace job
class TransitionHandler;
class PrototypeHandler;
class PropertyBox;
class ProtoChangeMarker;
class ProtoChangeDetails;
class ProfileTypeInfo;
class MachineCode;

enum class PrimitiveType : uint8_t;
enum class IterationKind;

using ErrorType = base::ErrorType;
using base::ErrorType;
using DeleteEntryPoint = void (*)(void *, void *);

enum class RemoveSlots { YES, NO };

class ObjectFactory {
public:
    explicit ObjectFactory(JSThread *thread, Heap *heap);

    JSHandle<ProfileTypeInfo> NewProfileTypeInfo(uint32_t length);
    JSHandle<ConstantPool> NewConstantPool(uint32_t capacity);
    JSHandle<Program> NewProgram();
    JSHandle<EcmaModule> NewEmptyEcmaModule();

    JSHandle<JSObject> GetJSError(const ErrorType &errorType, const char *data = nullptr);

    JSHandle<JSObject> NewJSError(const ErrorType &errorType, const JSHandle<EcmaString> &message);

    JSHandle<TransitionHandler> NewTransitionHandler();

    JSHandle<PrototypeHandler> NewPrototypeHandler();

    JSHandle<JSObject> NewEmptyJSObject();

    // use for others create, prototype is Function.prototype
    // use for native function
    JSHandle<JSFunction> NewJSFunction(const JSHandle<GlobalEnv> &env, const void *nativeFunc = nullptr,
                                       FunctionKind kind = FunctionKind::NORMAL_FUNCTION);
    // use for method
    JSHandle<JSFunction> NewJSFunction(const JSHandle<GlobalEnv> &env, JSMethod *method,
                                       FunctionKind kind = FunctionKind::NORMAL_FUNCTION);

    JSHandle<JSFunction> NewJSNativeErrorFunction(const JSHandle<GlobalEnv> &env, const void *nativeFunc = nullptr);

    JSHandle<JSFunction> NewSpecificTypedArrayFunction(const JSHandle<GlobalEnv> &env,
                                                       const void *nativeFunc = nullptr);

    JSHandle<JSObject> OrdinaryNewJSObjectCreate(const JSHandle<JSTaggedValue> &proto);

    JSHandle<JSBoundFunction> NewJSBoundFunction(const JSHandle<JSFunctionBase> &target,
                                                 const JSHandle<JSTaggedValue> &boundThis,
                                                 const JSHandle<TaggedArray> &args);

    JSHandle<JSIntlBoundFunction> NewJSIntlBoundFunction(const void *nativeFunc = nullptr, int functionLength = 1);

    JSHandle<JSProxyRevocFunction> NewJSProxyRevocFunction(const JSHandle<JSProxy> &proxy,
                                                           const void *nativeFunc = nullptr);

    JSHandle<JSAsyncAwaitStatusFunction> NewJSAsyncAwaitStatusFunction(const void *nativeFunc = nullptr);
    JSHandle<JSFunction> NewJSGeneratorFunction(JSMethod *method);

    JSHandle<JSAsyncFunction> NewAsyncFunction(JSMethod *method);

    JSHandle<JSGeneratorObject> NewJSGeneratorObject(JSHandle<JSTaggedValue> generatorFunction);

    JSHandle<JSAsyncFuncObject> NewJSAsyncFuncObject();

    JSHandle<JSPrimitiveRef> NewJSPrimitiveRef(const JSHandle<JSFunction> &function,
                                               const JSHandle<JSTaggedValue> &object);
    JSHandle<JSPrimitiveRef> NewJSPrimitiveRef(PrimitiveType type, const JSHandle<JSTaggedValue> &object);

    // get JSHClass for Ecma ClassLinker
    JSHandle<GlobalEnv> NewGlobalEnv(JSHClass *globalEnvClass);

    // get JSHClass for Ecma ClassLinker
    JSHandle<LexicalEnv> NewLexicalEnv(int numSlots);

    inline LexicalEnv *InlineNewLexicalEnv(int numSlots);

    JSHandle<JSSymbol> NewJSSymbol();

    JSHandle<JSSymbol> NewPrivateSymbol();

    JSHandle<JSSymbol> NewPrivateNameSymbol(const JSHandle<JSTaggedValue> &name);

    JSHandle<JSSymbol> NewWellKnownSymbol(const JSHandle<JSTaggedValue> &name);

    JSHandle<JSSymbol> NewPublicSymbol(const JSHandle<JSTaggedValue> &name);

    JSHandle<JSSymbol> NewSymbolWithTable(const JSHandle<JSTaggedValue> &name);

    JSHandle<JSSymbol> NewPrivateNameSymbolWithChar(const char *description);

    JSHandle<JSSymbol> NewWellKnownSymbolWithChar(const char *description);

    JSHandle<JSSymbol> NewPublicSymbolWithChar(const char *description);

    JSHandle<JSSymbol> NewSymbolWithTableWithChar(const char *description);

    JSHandle<AccessorData> NewAccessorData();
    JSHandle<AccessorData> NewInternalAccessor(void *setter, void *getter);

    JSHandle<PromiseCapability> NewPromiseCapability();

    JSHandle<PromiseReaction> NewPromiseReaction();

    JSHandle<PromiseRecord> NewPromiseRecord();

    JSHandle<ResolvingFunctionsRecord> NewResolvingFunctionsRecord();

    JSHandle<PromiseIteratorRecord> NewPromiseIteratorRecord(const JSHandle<JSTaggedValue> &itor,
                                                             const JSHandle<JSTaggedValue> &done);

    JSHandle<job::MicroJobQueue> NewMicroJobQueue();

    JSHandle<job::PendingJob> NewPendingJob(const JSHandle<JSFunction> &func, const JSHandle<TaggedArray> &argv);

    JSHandle<JSFunctionExtraInfo> NewFunctionExtraInfo(const JSHandle<JSNativePointer> &callBack,
                                                       const JSHandle<JSNativePointer> &data);

    JSHandle<JSArray> NewJSArray();

    JSHandle<JSProxy> NewJSProxy(const JSHandle<JSTaggedValue> &target, const JSHandle<JSTaggedValue> &handler);
    JSHandle<JSRealm> NewJSRealm();

    JSHandle<JSArguments> NewJSArguments();

    JSHandle<JSPrimitiveRef> NewJSString(const JSHandle<JSTaggedValue> &str);

    JSHandle<TaggedArray> NewTaggedArray(array_size_t length, JSTaggedValue initVal = JSTaggedValue::Hole());
    JSHandle<TaggedArray> NewTaggedArray(array_size_t length, JSTaggedValue initVal, bool nonMovable);
    JSHandle<TaggedArray> NewTaggedArray(array_size_t length, JSTaggedValue initVal, MemSpaceType spaceType);
    JSHandle<TaggedArray> NewDictionaryArray(array_size_t length);
    JSHandle<JSForInIterator> NewJSForinIterator(const JSHandle<JSTaggedValue> &obj);

    JSHandle<PropertyBox> NewPropertyBox(const JSHandle<JSTaggedValue> &name);

    JSHandle<ProtoChangeMarker> NewProtoChangeMarker();

    JSHandle<ProtoChangeDetails> NewProtoChangeDetails();

    // use for copy properties keys's array to another array
    JSHandle<TaggedArray> ExtendArray(const JSHandle<TaggedArray> &old, array_size_t length,
                                      JSTaggedValue initVal = JSTaggedValue::Hole());
    JSHandle<TaggedArray> CopyPartArray(const JSHandle<TaggedArray> &old, array_size_t start, array_size_t end);
    JSHandle<TaggedArray> CopyArray(const JSHandle<TaggedArray> &old, array_size_t oldLength, array_size_t newLength,
                                    JSTaggedValue initVal = JSTaggedValue::Hole());
    JSHandle<TaggedArray> CloneProperties(const JSHandle<TaggedArray> &old);
    JSHandle<TaggedArray> CloneProperties(const JSHandle<TaggedArray> &old, const JSHandle<JSTaggedValue> &env,
                                          const JSHandle<JSObject> &obj, const JSHandle<JSTaggedValue> &constpool);

    JSHandle<LayoutInfo> CreateLayoutInfo(int properties, JSTaggedValue initVal = JSTaggedValue::Hole());

    JSHandle<LayoutInfo> ExtendLayoutInfo(const JSHandle<LayoutInfo> &old, int properties,
                                          JSTaggedValue initVal = JSTaggedValue::Hole());

    JSHandle<LayoutInfo> CopyLayoutInfo(const JSHandle<LayoutInfo> &old);

    JSHandle<LayoutInfo> CopyAndReSort(const JSHandle<LayoutInfo> &old, int end, int capacity);

    JSHandle<EcmaString> GetEmptyString() const;

    JSHandle<TaggedArray> EmptyArray() const;

    JSHandle<ObjectWrapper> NewObjectWrapper(const JSHandle<JSTaggedValue> &value);

    // start gc test
    void SetTriggerGc(bool flag, bool triggerSemiGC = false);

    FreeObject *FillFreeObject(uintptr_t address, size_t size, RemoveSlots removeSlots = RemoveSlots::NO);

    TaggedObject *NewDynObject(const JSHandle<JSHClass> &dynclass, int inobjPropCount = 0);

    TaggedObject *NewNonMovableDynObject(const JSHandle<JSHClass> &dynclass, int inobjPropCount = 0);

    void InitializeExtraProperties(const JSHandle<JSHClass> &dynclass, TaggedObject *obj, int inobjPropCount);

    JSHandle<TaggedQueue> NewTaggedQueue(array_size_t length);

    JSHandle<TaggedQueue> GetEmptyTaggedQueue() const;

    JSHandle<JSSetIterator> NewJSSetIterator(const JSHandle<JSSet> &set, IterationKind kind);

    JSHandle<JSMapIterator> NewJSMapIterator(const JSHandle<JSMap> &map, IterationKind kind);

    JSHandle<JSArrayIterator> NewJSArrayIterator(const JSHandle<JSObject> &array, IterationKind kind);

    JSHandle<CompletionRecord> NewCompletionRecord(uint8_t type, JSHandle<JSTaggedValue> value);

    JSHandle<GeneratorContext> NewGeneratorContext();

    JSHandle<JSPromiseReactionsFunction> CreateJSPromiseReactionsFunction(const void *nativeFunc);

    JSHandle<JSPromiseExecutorFunction> CreateJSPromiseExecutorFunction(const void *nativeFunc);

    JSHandle<JSPromiseAllResolveElementFunction> NewJSPromiseAllResolveElementFunction(const void *nativeFunc);

    JSHandle<JSObject> CloneObjectLiteral(JSHandle<JSObject> object, const JSHandle<JSTaggedValue> &env,
                                          const JSHandle<JSTaggedValue> &constpool);
    JSHandle<JSObject> CloneObjectLiteral(JSHandle<JSObject> object);
    JSHandle<JSArray> CloneArrayLiteral(JSHandle<JSArray> object);
    JSHandle<JSFunction> CloneJSFuction(JSHandle<JSFunction> obj, FunctionKind kind);

    void NewJSArrayBufferData(const JSHandle<JSArrayBuffer> &array, int32_t length);

    JSHandle<JSArrayBuffer> NewJSArrayBuffer(int32_t length);

    JSHandle<JSArrayBuffer> NewJSArrayBuffer(void *buffer, int32_t length, const DeleteEntryPoint &deleter, void *data,
                                             bool share = false);

    JSHandle<JSDataView> NewJSDataView(JSHandle<JSArrayBuffer> buffer, int32_t offset, int32_t length);

    void NewJSRegExpByteCodeData(const JSHandle<JSRegExp> &regexp, void *buffer, size_t size);

    template<typename T, typename S>
    inline void NewJSIntlIcuData(const JSHandle<T> &obj, const S &icu, const DeleteEntryPoint &callback);

    EcmaString *InternString(const JSHandle<JSTaggedValue> &key);

    inline JSHandle<JSNativePointer> NewJSNativePointer(void *externalPointer, bool nonMovable = false);
    inline JSHandle<JSNativePointer> NewJSNativePointer(void *externalPointer, const DeleteEntryPoint &callBack,
                                                        void *data, bool nonMovable = false);

    JSHandle<JSObject> NewJSObjectByClass(const JSHandle<TaggedArray> &keys, const JSHandle<TaggedArray> &values);
    JSHandle<JSObject> NewJSObjectByClass(const JSHandle<TaggedArray> &properties, size_t length);

    // only use for creating Function.prototype and Function
    JSHandle<JSFunction> NewJSFunctionByDynClass(JSMethod *method, const JSHandle<JSHClass> &clazz,
                                                 FunctionKind kind = FunctionKind::NORMAL_FUNCTION);
    EcmaString *ResolveString(uint32_t stringId);

    void ObtainRootClass(const JSHandle<GlobalEnv> &globalEnv);

    const EcmaHeapManager &GetHeapManager() const
    {
        return heapHelper_;
    }

    // used for creating jsobject by constructor
    JSHandle<JSObject> NewJSObjectByConstructor(const JSHandle<JSFunction> &constructor,
                                                const JSHandle<JSTaggedValue> &newTarget);

    uintptr_t NewSpaceBySnapShotAllocator(size_t size);
    JSHandle<MachineCode> NewMachineCodeObject(size_t length, const uint8_t *data);
    JSHandle<JSNativeObject> NewJSNativeObject(void *externalPointer);
    JSHandle<JSNativeObject> NewJSNativeObject(void *externalPointer, DeleteEntryPoint callback, void *data);

    ~ObjectFactory() = default;

    // ----------------------------------- new string ----------------------------------------
    JSHandle<EcmaString> NewFromString(const CString &data);
    JSHandle<EcmaString> NewFromCanBeCompressString(const CString &data);

    JSHandle<EcmaString> NewFromStdString(const std::string &data);
    JSHandle<EcmaString> NewFromStdStringUnCheck(const std::string &data, bool canBeCompress);

    JSHandle<EcmaString> NewFromUtf8(const uint8_t *utf8Data, uint32_t utf8Len);
    JSHandle<EcmaString> NewFromUtf8UnCheck(const uint8_t *utf8Data, uint32_t utf8Len, bool canBeCompress);

    JSHandle<EcmaString> NewFromUtf16(const uint16_t *utf16Data, uint32_t utf16Len);
    JSHandle<EcmaString> NewFromUtf16UnCheck(const uint16_t *utf16Data, uint32_t utf16Len, bool canBeCompress);

    JSHandle<EcmaString> NewFromUtf8Literal(const uint8_t *utf8Data, uint32_t utf8Len);
    JSHandle<EcmaString> NewFromUtf8LiteralUnCheck(const uint8_t *utf8Data, uint32_t utf8Len, bool canBeCompress);

    JSHandle<EcmaString> NewFromUtf16Literal(const uint16_t *utf16Data, uint32_t utf16Len);
    JSHandle<EcmaString> NewFromUtf16LiteralUnCheck(const uint16_t *utf16Data, uint32_t utf16Len, bool canBeCompress);

    JSHandle<EcmaString> NewFromString(EcmaString *str);
    JSHandle<EcmaString> ConcatFromString(const JSHandle<EcmaString> &prefix, const JSHandle<EcmaString> &suffix);

private:
    friend class GlobalEnv;
    friend class GlobalEnvConstants;
    friend class EcmaString;
    JSHandle<JSFunction> NewJSFunctionImpl(JSMethod *method);

    void InitObjectFields(const TaggedObject *object);

    JSThread *thread_ {nullptr};
    bool isTriggerGc_ {false};
    bool triggerSemiGC_ {false};
    EcmaHeapManager heapHelper_;

    JSHClass *hclassClass_ {nullptr};
    JSHClass *stringClass_ {nullptr};
    JSHClass *arrayClass_ {nullptr};
    JSHClass *dictionaryClass_ {nullptr};
    JSHClass *freeObjectWithNoneFieldClass_ {nullptr};
    JSHClass *freeObjectWithOneFieldClass_ {nullptr};
    JSHClass *freeObjectWithTwoFieldClass_ {nullptr};

    JSHClass *completionRecordClass_ {nullptr};
    JSHClass *generatorContextClass_ {nullptr};
    JSHClass *envClass_ {nullptr};
    JSHClass *symbolClass_ {nullptr};
    JSHClass *accessorDataClass_ {nullptr};
    JSHClass *internalAccessorClass_ {nullptr};
    JSHClass *capabilityRecordClass_ {nullptr};
    JSHClass *reactionsRecordClass_ {nullptr};
    JSHClass *promiseIteratorRecordClass_ {nullptr};
    JSHClass *microJobQueueClass_ {nullptr};
    JSHClass *pendingJobClass_ {nullptr};
    JSHClass *jsProxyOrdinaryClass_ {nullptr};
    JSHClass *jsProxyCallableClass_ {nullptr};
    JSHClass *jsProxyConstructClass_ {nullptr};
    JSHClass *objectWrapperClass_ {nullptr};
    JSHClass *PropertyBoxClass_ {nullptr};
    JSHClass *protoChangeDetailsClass_ {nullptr};
    JSHClass *protoChangeMarkerClass_ {nullptr};
    JSHClass *promiseRecordClass_ {nullptr};
    JSHClass *promiseResolvingFunctionsRecord_ {nullptr};
    JSHClass *jsNativePointerClass_ {nullptr};
    JSHClass *transitionHandlerClass_ {nullptr};
    JSHClass *prototypeHandlerClass_ {nullptr};
    JSHClass *functionExtraInfo_ {nullptr};
    JSHClass *jsRealmClass_ {nullptr};
    JSHClass *programClass_ {nullptr};
    JSHClass *machineCodeClass_ {nullptr};
    JSHClass *ecmaModuleClass_ {nullptr};

    EcmaVM *vm_ {nullptr};
    Heap *heap_ {nullptr};

    NO_COPY_SEMANTIC(ObjectFactory);
    NO_MOVE_SEMANTIC(ObjectFactory);

    void NewObjectHook();

    // used for creating jshclass in Builtins, Function, Class_Linker
    JSHandle<JSHClass> NewEcmaDynClass(uint32_t size, JSType type);
    // used for creating jshclass in GlobalEnv, EcmaVM
    JSHandle<JSHClass> NewEcmaDynClass(JSHClass *hclass, uint32_t size, JSType type);
    // used for creating jshclass in Builtins, Function, Class_Linker
    JSHandle<JSHClass> NewEcmaDynClass(uint32_t size, JSType type, const JSHandle<JSTaggedValue> &prototype);

    // used for creating Function
    JSHandle<JSObject> NewJSObject(const JSHandle<JSHClass> &jshclass);

    // used to create nonmovable js_object
    JSHandle<JSObject> NewNonMovableJSObject(const JSHandle<JSHClass> &jshclass);

    // used for creating Function
    JSHandle<JSFunction> NewJSFunction(const JSHandle<GlobalEnv> &env, const JSHandle<JSHClass> &dynKlass);
    JSHandle<JSHClass> CreateObjectClass(const JSHandle<TaggedArray> &keys, const JSHandle<TaggedArray> &values);
    JSHandle<JSHClass> CreateObjectClass(const JSHandle<TaggedArray> &properties, size_t length);
    JSHandle<JSHClass> CreateFunctionClass(FunctionKind kind, uint32_t size, JSType type,
                                           const JSHandle<JSTaggedValue> &prototype);

    // used for creating ref.prototype in buildins, such as Number.prototype
    JSHandle<JSPrimitiveRef> NewJSPrimitiveRef(const JSHandle<JSHClass> &dynKlass,
                                               const JSHandle<JSTaggedValue> &object);

    JSHandle<EcmaString> GetStringFromStringTable(const uint8_t *utf8Data, uint32_t utf8Len, bool canBeCompress) const;
    // For MUtf-8 string data
    EcmaString *GetRawStringFromStringTable(const uint8_t *mutf8Data, uint32_t utf16Len, bool canBeCompressed) const;

    JSHandle<EcmaString> GetStringFromStringTable(const uint16_t *utf16Data, uint32_t utf16Len,
                                                  bool canBeCompress) const;

    JSHandle<EcmaString> GetStringFromStringTable(EcmaString *string) const;

    inline EcmaString *AllocStringObject(size_t size);
    inline EcmaString *AllocNonMovableStringObject(size_t size);
    JSHandle<TaggedArray> NewEmptyArray();  // only used for EcmaVM.

    JSHandle<JSHClass> CreateJSArguments();
    JSHandle<JSHClass> CreateJSArrayInstanceClass(JSHandle<JSTaggedValue> proto);
    JSHandle<JSHClass> CreateJSRegExpInstanceClass(JSHandle<JSTaggedValue> proto);

    friend class Builtins;    // create builtins object need dynclass
    friend class JSFunction;  // create prototype_or_dynclass need dynclass
    friend class JSHClass;    // HC transition need dynclass
    friend class EcmaVM;      // hold the factory instance
    friend class JsVerificationTest;
    friend class PandaFileTranslator;
    friend class LiteralDataExtractor;
};

class ClassLinkerFactory {
private:
    friend class GlobalEnv;  // root class in class_linker need dynclass
    friend class EcmaVM;     // root class in class_linker need dynclass
};
}  // namespace panda::ecmascript
#endif  // ECMASCRIPT_OBJECT_FACTORY_H
