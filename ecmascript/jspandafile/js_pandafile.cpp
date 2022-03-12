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

#include "ecmascript/jspandafile/js_pandafile.h"
#include "ecmascript/jspandafile/js_pandafile_manager.h"
#include "ecmascript/class_linker/program_object-inl.h"

namespace panda::ecmascript {
JSPandaFile::JSPandaFile(const panda_file::File *pf, const CString &descriptor) : pf_(pf), desc_(descriptor)
{
    InitMethods();
}

JSPandaFile::~JSPandaFile()
{
    if (pf_ != nullptr) {
        delete pf_;
        pf_ = nullptr;
    }
    methodMap_.clear();
    if (methods_ != nullptr) {
        JSPandaFileManager::FreeBuffer(methods_);
        methods_ = nullptr;
    }
}

tooling::ecmascript::PtJSExtractor *JSPandaFile::GetOrCreatePtJSExtractor()
{
    if (ptJSExtractor_) {
        return ptJSExtractor_.get();
    }
    ptJSExtractor_ = std::make_unique<tooling::ecmascript::PtJSExtractor>(pf_);
    return ptJSExtractor_.get();
}

uint32_t JSPandaFile::GetOrInsertConstantPool(ConstPoolType type, uint32_t offset)
{
    auto it = constpoolMap_.find(offset);
    if (it != constpoolMap_.cend()) {
        ConstPoolValue value(it->second);
        return value.GetConstpoolIndex();
    }
    ASSERT(constpoolIndex_ != UINT32_MAX);
    uint32_t index = constpoolIndex_++;
    ConstPoolValue value(type, index);
    constpoolMap_.insert({offset, value.GetValue()});
    return index;
}

void JSPandaFile::InitMethods()
{
    Span<const uint32_t> classIndexes = pf_->GetClasses();
    for (const uint32_t index : classIndexes) {
        panda_file::File::EntityId classId(index);
        if (pf_->IsExternal(classId)) {
            continue;
        }
        panda_file::ClassDataAccessor cda(*pf_, classId);
        numMethods_ += cda.GetMethodsNumber();
    }
    methods_ = static_cast<JSMethod *>(JSPandaFileManager::AllocateBuffer(sizeof(JSMethod) * numMethods_));
}

const JSMethod *JSPandaFile::FindMethods(uint32_t offset) const
{
    return methodMap_.at(offset);
}
}  // namespace panda::ecmascript
