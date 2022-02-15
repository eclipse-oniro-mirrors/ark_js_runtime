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

#ifndef ECMASCRIPT_JS_MODULE_H
#define ECMASCRIPT_JS_MODULE_H

#include "ecmascript/base/string_helper.h"
#include "ecmascript/mem/c_containers.h"
#include "ecmascript/module/js_module_record.h"
#include "ecmascript/tagged_array.h"

namespace panda::ecmascript {
enum class ModuleStatus : uint8_t { UNINSTANTIATED = 0x01, INSTANTIATING, INSTANTIATED, EVALUATING, EVALUATED };

class ImportEntry final : public Record {
public:
    CAST_CHECK(ImportEntry, IsImportEntry);

    static constexpr size_t IMPORT_ENTRY_OFFSET = Record::SIZE;
    ACCESSORS(ModuleRequest, IMPORT_ENTRY_OFFSET, MODULE_REQUEST_OFFSET);
    ACCESSORS(ImportName, MODULE_REQUEST_OFFSET, IMPORT_NAME_OFFSET);
    ACCESSORS(LocalName, IMPORT_NAME_OFFSET, SIZE);

    DECL_DUMP()
    DECL_VISIT_OBJECT(IMPORT_ENTRY_OFFSET, SIZE)
};

class ExportEntry final : public Record {
public:
    CAST_CHECK(ExportEntry, IsExportEntry);

    static constexpr size_t EXPORT_ENTRY_OFFSET = Record::SIZE;
    ACCESSORS(ExportName, EXPORT_ENTRY_OFFSET, EXPORT_NAME_OFFSET);
    ACCESSORS(ModuleRequest, EXPORT_NAME_OFFSET, MODULE_REQUEST_OFFSET);
    ACCESSORS(ImportName, MODULE_REQUEST_OFFSET, IMPORT_NAME_OFFSET);
    ACCESSORS(LocalName, IMPORT_NAME_OFFSET, SIZE);

    DECL_DUMP()
    DECL_VISIT_OBJECT(EXPORT_ENTRY_OFFSET, SIZE)
};

class SourceTextModule final : public ModuleRecord {
public:
    static constexpr int32_t UNDEFINED_INDEX = -1;

    CAST_CHECK(SourceTextModule, IsSourceTextModule);

    // 15.2.1.17 Runtime Semantics: HostResolveImportedModule ( referencingModule, specifier )
    static JSHandle<SourceTextModule> HostResolveImportedModule(JSThread *thread,
                                                                const JSHandle<SourceTextModule> &module,
                                                                const JSHandle<JSTaggedValue> &moduleRequest);

    JSTaggedValue ParseModule(JSThread *thread, JSTaggedValue sourceText);

    // 15.2.1.16.2 GetExportedNames(exportStarSet)
    CVector<std::string> GetExportedNames(JSThread *thread, JSHandle<TaggedArray> &exportStarSet);

    // 15.2.1.16.3 ResolveExport(exportName, resolveSet)
    JSHandle<JSTaggedValue> ResolveExport(JSThread *thread, const JSHandle<JSTaggedValue> &exportName,
        CVector<std::pair<JSHandle<SourceTextModule>, JSHandle<JSTaggedValue>>> &resolveSet);

    // 15.2.1.16.4.1 InnerModuleInstantiation ( module, stack, index )
    static int32_t InnerModuleInstantiation(JSThread *thread, JSHandle<ModuleRecord> moduleRecord,
                                                  CVector<JSHandle<SourceTextModule>> &stack, int index);

    // 15.2.1.16.4.2 ModuleDeclarationEnvironmentSetup ( module )
    static void ModuleDeclarationEnvironmentSetup(JSThread *thread, JSHandle<SourceTextModule> module);

    // 15.2.1.16.5.1 InnerModuleEvaluation ( module, stack, index )
    static int32_t InnerModuleEvaluation(JSThread *thread, JSHandle<ModuleRecord> moduleRecord,
                                               CVector<JSHandle<SourceTextModule>> &stack, int index);

    // 15.2.1.16.5.2 ModuleExecution ( module )
    static void ModuleExecution(JSThread *thread, JSHandle<SourceTextModule> module);

    // 15.2.1.18 Runtime Semantics: GetModuleNamespace ( module )
    static JSHandle<JSTaggedValue> GetModuleNamespace(JSThread *thread, JSHandle<SourceTextModule> module);

    void AddImportEntry(JSThread *thread, JSHandle<ImportEntry> importEntry);
    void AddLocalExportEntry(JSThread *thread, JSHandle<ExportEntry> exportEntry);
    void AddIndirectExportEntry(JSThread *thread, JSHandle<ExportEntry> exportEntry);
    void AddStarExportEntry(JSThread *thread, JSHandle<ExportEntry> exportEntry);

    static constexpr size_t SOURCE_TEXT_MODULE_OFFSET = ModuleRecord::SIZE;
    ACCESSORS(Environment, SOURCE_TEXT_MODULE_OFFSET, NAMESPACE_OFFSET);
    ACCESSORS(Namespace, NAMESPACE_OFFSET, ECMA_MODULE_FILENAME);
    ACCESSORS(EcmaModuleFilename, ECMA_MODULE_FILENAME, REQUESTED_MODULES_OFFSET);
    ACCESSORS(RequestedModules, REQUESTED_MODULES_OFFSET, IMPORT_ENTRIES_OFFSET);
    ACCESSORS(ImportEntries, IMPORT_ENTRIES_OFFSET, LOCAL_EXPORT_ENTTRIES_OFFSET);
    ACCESSORS(LocalExportEntries, LOCAL_EXPORT_ENTTRIES_OFFSET, INDIRECT_EXPORT_ENTTRIES_OFFSET);
    ACCESSORS(IndirectExportEntries, INDIRECT_EXPORT_ENTTRIES_OFFSET, START_EXPORT_ENTTRIES_OFFSET);
    ACCESSORS(StarExportEntries, START_EXPORT_ENTTRIES_OFFSET, NAME_DICTIONARY_OFFSET);
    ACCESSORS(NameDictionary, NAME_DICTIONARY_OFFSET, EVALUATION_ERROR_OFFSET);
    ACCESSORS_PRIMITIVE_FIELD(EvaluationError, int32_t, EVALUATION_ERROR_OFFSET, DFS_ANCESTOR_INDEX_OFFSET);
    ACCESSORS_PRIMITIVE_FIELD(DFSAncestorIndex, int32_t, DFS_ANCESTOR_INDEX_OFFSET, DFS_INDEX_OFFSET);
    ACCESSORS_PRIMITIVE_FIELD(DFSIndex, int32_t, DFS_INDEX_OFFSET, BIT_FIELD_OFFSET);
    ACCESSORS_BIT_FIELD(BitField, BIT_FIELD_OFFSET, LAST_OFFSET)

    DEFINE_ALIGN_SIZE(LAST_OFFSET);

    // define BitField
    static constexpr size_t STATUS_BITS = 3;
    FIRST_BIT_FIELD(BitField, Status, ModuleStatus, STATUS_BITS)

    DECL_DUMP()
    DECL_VISIT_OBJECT(SOURCE_TEXT_MODULE_OFFSET, EVALUATION_ERROR_OFFSET)

    // 15.2.1.16.5 Evaluate()
    int32_t Evaluate(JSThread *thread);

    // 15.2.1.16.4 Instantiate()
    int32_t Instantiate(JSThread *thread);

    JSTaggedValue GetModuleValue(JSThread *thread, JSTaggedValue key, bool isThrow);
    JSTaggedValue FindExportName(JSThread *thread, JSHandle<JSTaggedValue> localName);
    void StoreModuleValue(JSThread *thread, JSHandle<JSTaggedValue> key, JSHandle<JSTaggedValue> value);

    static constexpr uint32_t DEAULT_DICTIONART_CAPACITY = 4;
};

class ResolvedBinding final : public Record {
public:
    CAST_CHECK(ResolvedBinding, IsResolvedBinding);

    static constexpr size_t RESOLVED_BINDING_OFFSET = Record::SIZE;
    ACCESSORS(Module, RESOLVED_BINDING_OFFSET, MODULE_OFFSET);
    ACCESSORS(BindingName, MODULE_OFFSET, SIZE);

    DECL_DUMP()
    DECL_VISIT_OBJECT(RESOLVED_BINDING_OFFSET, SIZE)
};
}  // namespace panda::ecmascript
#endif  // ECMASCRIPT_JS_MODULE_H
