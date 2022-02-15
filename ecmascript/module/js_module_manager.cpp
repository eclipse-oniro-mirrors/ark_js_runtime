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

#include "js_module_manager.h"

#include "ecmascript/global_env.h"
#include "ecmascript/jspandafile/module_data_extractor.h"
#include "ecmascript/js_array.h"
#include "ecmascript/linked_hash_table.h"
#include "ecmascript/module/js_module_source_text.h"
#include "ecmascript/tagged_dictionary.h"

namespace panda::ecmascript {
EcmaModuleManager::EcmaModuleManager(EcmaVM *vm) : vm_(vm)
{
    ecmaResolvedModules_ = NameDictionary::Create(vm_->GetJSThread(), DEAULT_DICTIONART_CAPACITY).GetTaggedValue();
}

void EcmaModuleManager::Instantiate(JSThread *thread, JSHandle<SourceTextModule> module)
{
    module->Instantiate(thread);
}

void EcmaModuleManager::Evaluate(JSThread *thread, JSHandle<SourceTextModule> module)
{
    module->Evaluate(thread);
}

void EcmaModuleManager::AddSourceModule(JSTaggedValue ecmaModule)
{
    if (!ecmaModule.IsSourceTextModule()) {
        CString UndefinedString("undefined");
        moduleStack_.push(UndefinedString);
        return;
    }
    JSTaggedValue moduleFilename = SourceTextModule::Cast(ecmaModule.GetTaggedObject())->GetEcmaModuleFilename();
    CString currentModuleFilename = ConvertToString(moduleFilename);
    moduleStack_.push(currentModuleFilename);
}

void EcmaModuleManager::PopSourceModule()
{
    if (moduleStack_.empty()) {
        return;
    }
    moduleStack_.pop();
}

JSTaggedValue EcmaModuleManager::GetCurrentSourceModule()
{
    if (moduleStack_.empty()) {
        return JSTaggedValue::Undefined();
    }
    CString currentModuleFilename = moduleStack_.top();
    if (currentModuleFilename == "undefined") {
        return JSTaggedValue::Undefined();
    }
    return HostGetImportedModule(vm_->GetJSThread(), currentModuleFilename).GetTaggedValue();
}

JSTaggedValue EcmaModuleManager::GetModuleValueInner(JSThread *thread, JSTaggedValue key)
{
    JSTaggedValue currentModule = GetCurrentSourceModule();
    if (currentModule.IsUndefined()) {
        LOG_ECMA(FATAL) << "GetModuleValueInner currentModule failed";
    }
    return SourceTextModule::Cast(currentModule.GetHeapObject())->GetModuleValue(thread, key, false);
}

JSTaggedValue EcmaModuleManager::GetModuleValueOutter(JSThread *thread, JSTaggedValue key)
{
    JSTaggedValue currentModule = GetCurrentSourceModule();
    if (currentModule.IsUndefined()) {
        LOG_ECMA(FATAL) << "GetModuleValueOutter currentModule failed";
    }
    JSTaggedValue moduleEnvironment = SourceTextModule::Cast(currentModule.GetHeapObject())->GetEnvironment();
    ASSERT(!moduleEnvironment.IsUndefined());
    JSTaggedValue resolvedBinding = LinkedHashMap::Cast(moduleEnvironment.GetTaggedObject())->Get(key);
    if (resolvedBinding.IsUndefined()) {
        return thread->GlobalConstants()->GetUndefined();
    }
    ASSERT(resolvedBinding.IsResolvedBinding());
    ResolvedBinding *binding = ResolvedBinding::Cast(resolvedBinding.GetTaggedObject());
    JSTaggedValue resolvedModule = binding->GetModule();
    ASSERT(resolvedModule.IsSourceTextModule());
    return SourceTextModule::Cast(resolvedModule.GetHeapObject())->GetModuleValue(thread, binding->GetBindingName(), false);
}

void EcmaModuleManager::StoreModuleValue(JSThread *thread, JSTaggedValue key, JSTaggedValue value)
{
    JSHandle<SourceTextModule> currentModule(thread, GetCurrentSourceModule());
    if (currentModule.GetTaggedValue().IsUndefined()) {
        LOG_ECMA(FATAL) << "StoreModuleValue currentModule failed";
    }
    JSHandle<JSTaggedValue> keyHandle(thread, key);
    JSHandle<JSTaggedValue> valueHandle(thread, value);
    currentModule->StoreModuleValue(thread, keyHandle, valueHandle);
}

JSHandle<SourceTextModule> EcmaModuleManager::HostGetImportedModule(JSThread *thread, const CString &referencingModule)
{
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    JSHandle<JSTaggedValue> referencingHandle = JSHandle<JSTaggedValue>::Cast(factory->NewFromString(referencingModule));
    int entry = NameDictionary::Cast(ecmaResolvedModules_.GetTaggedObject())->FindEntry(referencingHandle.GetTaggedValue());
    LOG_IF(entry == -1, FATAL, ECMASCRIPT) << "cannot get module: " << referencingModule;

    return JSHandle<SourceTextModule>(thread, NameDictionary::Cast(ecmaResolvedModules_.GetTaggedObject())->GetValue(entry));
}

JSHandle<SourceTextModule> EcmaModuleManager::HostResolveImportedModule(JSThread *thread,
                                                                        const std::string &referencingModule)
{
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    JSHandle<JSTaggedValue> referencingHandle =
        JSHandle<JSTaggedValue>::Cast(factory->NewFromStdString(referencingModule));
    int entry =
        NameDictionary::Cast(ecmaResolvedModules_.GetTaggedObject())->FindEntry(referencingHandle.GetTaggedValue());
    if (entry != -1) {
        return JSHandle<SourceTextModule>(thread,
                                          NameDictionary::Cast(ecmaResolvedModules_.GetTaggedObject())->GetValue(entry));
    }
    auto pf = panda_file::OpenPandaFileOrZip(referencingModule, panda_file::File::READ_WRITE);
    if (pf == nullptr) {
        LOG_ECMA(ERROR) << "open pandafile " << referencingModule << " error";
        UNREACHABLE();
    }
    JSHandle<JSTaggedValue> moduleRecord = ModuleDataExtractor::ParseModule(thread, *pf.get(), referencingModule);
    JSHandle<NameDictionary> dict(thread, ecmaResolvedModules_);
    ecmaResolvedModules_ =
        NameDictionary::Put(thread, dict, referencingHandle, moduleRecord, PropertyAttributes::Default())
        .GetTaggedValue();
    return JSHandle<SourceTextModule>::Cast(moduleRecord);
}

void EcmaModuleManager::AddResolveImportedModule(JSThread *thread, const panda_file::File &pf, const std::string &referencingModule)
{
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    JSHandle<JSTaggedValue> moduleRecord = ModuleDataExtractor::ParseModule(thread, pf, referencingModule);
    JSHandle<JSTaggedValue> referencingHandle =
        JSHandle<JSTaggedValue>::Cast(factory->NewFromStdString(referencingModule));
    JSHandle<NameDictionary> dict(thread, ecmaResolvedModules_);
    ecmaResolvedModules_ =
        NameDictionary::Put(thread, dict, referencingHandle, moduleRecord, PropertyAttributes::Default())
        .GetTaggedValue();
}

JSTaggedValue EcmaModuleManager::GetModuleNamespace(JSThread *thread, JSTaggedValue localName)
{
    JSTaggedValue currentModule = GetCurrentSourceModule();
    if (currentModule.IsUndefined()) {
        LOG_ECMA(FATAL) << "GetModuleNamespace currentModule failed";
    }
    JSTaggedValue moduleEnvironment = SourceTextModule::Cast(currentModule.GetHeapObject())->GetEnvironment();
    ASSERT(!moduleEnvironment.IsUndefined());
    JSTaggedValue moduleNamespace = LinkedHashMap::Cast(moduleEnvironment.GetTaggedObject())->Get(localName);
    if (moduleNamespace.IsUndefined()) {
        return thread->GlobalConstants()->GetUndefined();
    }
    ASSERT(moduleNamespace.IsModuleNamespace());
    return moduleNamespace;
}
} // namespace panda::ecmascript