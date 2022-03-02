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

#include "ecmascript/ecma_module.h"
#include "ecmascript/ecma_vm.h"
#include "ecmascript/global_env.h"
#include "ecmascript/js_thread.h"
#include "ecmascript/object_factory.h"
#include "ecmascript/tagged_array.h"
#include "ecmascript/tagged_dictionary.h"

namespace panda::ecmascript {
JSHandle<JSTaggedValue> EcmaModule::GetItem(const JSThread *thread, JSHandle<JSTaggedValue> itemName)
{
    JSHandle<NameDictionary> moduleItems(thread, NameDictionary::Cast(GetNameDictionary().GetTaggedObject()));
    int entry = moduleItems->FindEntry(itemName.GetTaggedValue());
    if (entry != -1) {
        return JSHandle<JSTaggedValue>(thread, moduleItems->GetValue(entry));
    }

    return JSHandle<JSTaggedValue>(thread, JSTaggedValue::Undefined());
}

void EcmaModule::AddItem(const JSThread *thread, JSHandle<EcmaModule> module, JSHandle<JSTaggedValue> itemName,
                         JSHandle<JSTaggedValue> itemValue)
{
    JSMutableHandle<JSTaggedValue> data(thread, module->GetNameDictionary());
    if (data->IsUndefined()) {
        data.Update(NameDictionary::Create(thread, DEAULT_DICTIONART_CAPACITY));
    }
    JSHandle<NameDictionary> dataDict = JSHandle<NameDictionary>::Cast(data);
    JSHandle<NameDictionary> newDict =
        NameDictionary::Put(thread, dataDict, itemName, itemValue, PropertyAttributes::Default());
    module->SetNameDictionary(thread, newDict);
}

void EcmaModule::RemoveItem(const JSThread *thread, JSHandle<EcmaModule> module, JSHandle<JSTaggedValue> itemName)
{
    JSHandle<JSTaggedValue> data(thread, module->GetNameDictionary());
    if (data->IsUndefined()) {
        return;
    }
    JSHandle<NameDictionary> moduleItems(data);
    int entry = moduleItems->FindEntry(itemName.GetTaggedValue());
    if (entry != -1) {
        JSHandle<NameDictionary> newDict = NameDictionary::Remove(thread, moduleItems, entry);
        module->SetNameDictionary(thread, newDict);
    }
}

void EcmaModule::CopyModuleInternal(const JSThread *thread, JSHandle<EcmaModule> dstModule,
                                    JSHandle<EcmaModule> srcModule)
{
    JSHandle<NameDictionary> moduleItems(thread,
                                         NameDictionary::Cast(srcModule->GetNameDictionary().GetTaggedObject()));
    uint32_t numAllKeys = moduleItems->EntriesCount();
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    JSHandle<TaggedArray> allKeys = factory->NewTaggedArray(numAllKeys);
    moduleItems->GetAllKeys(thread, 0, allKeys.GetObject<TaggedArray>());

    JSMutableHandle<JSTaggedValue> itemName(thread, JSTaggedValue::Undefined());
    JSMutableHandle<JSTaggedValue> itemValue(thread, JSTaggedValue::Undefined());
    unsigned int capcity = allKeys->GetLength();
    for (unsigned int i = 0; i < capcity; i++) {
        int entry = moduleItems->FindEntry(allKeys->Get(i));
        if (entry != -1) {
            itemName.Update(allKeys->Get(i));
            itemValue.Update(moduleItems->GetValue(entry));
            EcmaModule::AddItem(thread, dstModule, itemName, itemValue);
        }
    }
}

void EcmaModule::DebugPrint(const JSThread *thread, const CString &caller)
{
    JSHandle<NameDictionary> moduleItems(thread, NameDictionary::Cast(GetNameDictionary().GetTaggedObject()));
    uint32_t numAllKeys = moduleItems->EntriesCount();
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    JSHandle<TaggedArray> allKeys = factory->NewTaggedArray(numAllKeys);
    moduleItems->GetAllKeys(thread, 0, allKeys.GetObject<TaggedArray>());

    unsigned int capcity = allKeys->GetLength();
    for (unsigned int i = 0; i < capcity; i++) {
        int entry = moduleItems->FindEntry(allKeys->Get(i));
        if (entry != -1) {
            std::cout << "[" << caller << "]" << std::endl
                      << "--itemName: " << ConvertToString(EcmaString::Cast(allKeys->Get(i).GetTaggedObject()))
                      << std::endl
                      << "--itemValue(ObjRef): 0x" << std::hex << moduleItems->GetValue(entry).GetRawData()
                      << std::endl;
        }
    }
}

ModuleManager::ModuleManager(EcmaVM *vm) : vm_(vm)
{
    ecmaModules_ = NameDictionary::Create(vm_->GetJSThread(), DEAULT_DICTIONART_CAPACITY).GetTaggedValue();
}

// class ModuleManager
void ModuleManager::AddModule(JSHandle<JSTaggedValue> moduleName, JSHandle<JSTaggedValue> module)
{
    JSThread *thread = vm_->GetJSThread();
    [[maybe_unused]] EcmaHandleScope scope(thread);
    JSHandle<NameDictionary> dict(thread, ecmaModules_);
    ecmaModules_ =
        NameDictionary::Put(thread, dict, moduleName, module, PropertyAttributes::Default()).GetTaggedValue();
}

void ModuleManager::RemoveModule(JSHandle<JSTaggedValue> moduleName)
{
    JSThread *thread = vm_->GetJSThread();
    [[maybe_unused]] EcmaHandleScope scope(thread);
    JSHandle<NameDictionary> moduleItems(thread, ecmaModules_);
    int entry = moduleItems->FindEntry(moduleName.GetTaggedValue());
    if (entry != -1) {
        ecmaModules_ = NameDictionary::Remove(thread, moduleItems, entry).GetTaggedValue();
    }
}

JSHandle<JSTaggedValue> ModuleManager::GetModule(const JSThread *thread,
                                                 [[maybe_unused]] JSHandle<JSTaggedValue> moduleName)
{
    int entry = NameDictionary::Cast(ecmaModules_.GetTaggedObject())->FindEntry(moduleName.GetTaggedValue());
    if (entry != -1) {
        return JSHandle<JSTaggedValue>(thread, NameDictionary::Cast(ecmaModules_.GetTaggedObject())->GetValue(entry));
    }
    return thread->GlobalConstants()->GetHandledUndefined();
}

CString ModuleManager::GenerateAmiPath(const CString &currentPathFile, const CString &relativeFile)
{
    if (relativeFile.find("./") != 0 && relativeFile.find("../") != 0) {  // not start with "./" or "../"
        return relativeFile;                                              // not relative
    }

    auto slashPos = currentPathFile.find_last_of('/');
    if (slashPos == CString::npos) {
        return relativeFile;  // no need to process
    }

    auto dotPos = relativeFile.find_last_of(".");
    if (dotPos == CString::npos) {
        dotPos = 0;
    }

    CString fullPath;
    fullPath.append(currentPathFile.substr(0, slashPos + 1));  // 1: with "/"
    fullPath.append(relativeFile.substr(0, dotPos));
    fullPath.append(".abc");  // ".js" -> ".abc"
    return fullPath;
}

const CString &ModuleManager::GetCurrentExportModuleName()
{
    return moduleNames_.back();
}

void ModuleManager::SetCurrentExportModuleName(const std::string_view &moduleFile)
{
    moduleNames_.emplace_back(CString(moduleFile));  // xx/xx/x.abc
}

void ModuleManager::RestoreCurrentExportModuleName()
{
    auto s = moduleNames_.size();
    if (s > 0) {
        moduleNames_.resize(s - 1);
        return;
    }
    UNREACHABLE();
}

const CString &ModuleManager::GetPrevExportModuleName()
{
    static const int MINIMUM_COUNT = 2;
    auto count = moduleNames_.size();
    ASSERT(count >= MINIMUM_COUNT);
    return moduleNames_[count - MINIMUM_COUNT];
}

void ModuleManager::AddModuleItem(const JSThread *thread, JSHandle<JSTaggedValue> itemName,
                                  JSHandle<JSTaggedValue> value)
{
    CString name_str = GetCurrentExportModuleName();
    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();

    JSHandle<EcmaString> moduleName = factory->NewFromString(name_str);

    JSHandle<JSTaggedValue> module = GetModule(thread, JSHandle<JSTaggedValue>::Cast(moduleName));
    if (module->IsUndefined()) {
        JSHandle<EcmaModule> emptyModule = factory->NewEmptyEcmaModule();
        EcmaModule::AddItem(thread, emptyModule, itemName, value);

        AddModule(JSHandle<JSTaggedValue>::Cast(moduleName), JSHandle<JSTaggedValue>::Cast(emptyModule));
    } else {
        EcmaModule::AddItem(thread, JSHandle<EcmaModule>(module), itemName, value);
    }
}

JSHandle<JSTaggedValue> ModuleManager::GetModuleItem(const JSThread *thread, JSHandle<JSTaggedValue> module,
                                                     JSHandle<JSTaggedValue> itemName)
{
    return EcmaModule::Cast(module->GetTaggedObject())->GetItem(thread, itemName);  // Assume the module is exist
}

void ModuleManager::CopyModule(const JSThread *thread, JSHandle<JSTaggedValue> src)
{
    ASSERT(src->IsHeapObject());
    JSHandle<EcmaModule> srcModule = JSHandle<EcmaModule>::Cast(src);
    CString name_str = GetCurrentExportModuleName();  // Assume the srcModule exist when dstModule Execute

    ObjectFactory *factory = thread->GetEcmaVM()->GetFactory();
    JSHandle<EcmaString> moduleName = factory->NewFromString(name_str);

    JSHandle<JSTaggedValue> dstModule = GetModule(thread, JSHandle<JSTaggedValue>::Cast(moduleName));

    if (dstModule->IsUndefined()) {
        JSHandle<EcmaModule> emptyModule = factory->NewEmptyEcmaModule();
        EcmaModule::CopyModuleInternal(thread, emptyModule, srcModule);

        AddModule(JSHandle<JSTaggedValue>::Cast(moduleName), JSHandle<JSTaggedValue>::Cast(emptyModule));
    } else {
        EcmaModule::CopyModuleInternal(thread, JSHandle<EcmaModule>(dstModule), srcModule);
    }
}

void ModuleManager::DebugPrint([[maybe_unused]] const JSThread *thread, [[maybe_unused]] const CString &caller)
{
    std::cout << "ModuleStack:";
    for_each(moduleNames_.cbegin(),
             moduleNames_.cend(),
             [](const CString& s)->void {
                 std::cout << s << " ";
             });
    std::cout << "\n";
}
}  // namespace panda::ecmascript
