/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ECMASCRIPT_MODULE_DATA_ACCESSOR_INL_H_
#define ECMASCRIPT_MODULE_DATA_ACCESSOR_INL_H_

#include <string>
#include "file_items.h"
#include "helpers.h"
#include "module_data_accessor.h"
#include "utils/utf.h"

namespace panda::ecmascript::jspandafile {

template <class Callback>
inline void ModuleDataAccessor::EnumerateModuleRecord(const Callback &cb)
{
    auto sp = entryDataSp_;

    auto regularImportNum = panda_file::helpers::Read<panda_file::ID_SIZE>(&sp);
    for (size_t idx = 0; idx < regularImportNum; idx++) {
        auto localNameOffset = static_cast<uint32_t>(panda_file::helpers::Read<sizeof(uint32_t)>(&sp));
        auto importNameOffset = static_cast<uint32_t>(panda_file::helpers::Read<sizeof(uint32_t)>(&sp));
        auto moduleRequestIdx = static_cast<uint32_t>(panda_file::helpers::Read<sizeof(uint16_t)>(&sp));
        cb(ModuleTag::REGULARIMPORT, 0, moduleRequestIdx, importNameOffset, localNameOffset);
    }

    auto namespaceImportNum = panda_file::helpers::Read<panda_file::ID_SIZE>(&sp);
    for (size_t idx = 0; idx < namespaceImportNum; idx++) {
        auto localNameOffset = static_cast<uint32_t>(panda_file::helpers::Read<sizeof(uint32_t)>(&sp));
        auto moduleRequestIdx = static_cast<uint32_t>(panda_file::helpers::Read<sizeof(uint16_t)>(&sp));
        cb(ModuleTag::NAMESPACEIMPORT, 0, moduleRequestIdx, 0, localNameOffset);
    }

    auto localExportNum = panda_file::helpers::Read<panda_file::ID_SIZE>(&sp);
    for (size_t idx = 0; idx < localExportNum; idx++) {
        auto localNameOffset = static_cast<uint32_t>(panda_file::helpers::Read<sizeof(uint32_t)>(&sp));
        auto exportNameOffset = static_cast<uint32_t>(panda_file::helpers::Read<sizeof(uint32_t)>(&sp));
        cb(ModuleTag::LOCALEXPORT, exportNameOffset, 0, 0, localNameOffset);
    }

    auto indirectExportNum = panda_file::helpers::Read<panda_file::ID_SIZE>(&sp);
    for (size_t idx = 0; idx < indirectExportNum; idx++) {
        auto exportNameOffset = static_cast<uint32_t>(panda_file::helpers::Read<sizeof(uint32_t)>(&sp));
        auto importNameOffset = static_cast<uint32_t>(panda_file::helpers::Read<sizeof(uint32_t)>(&sp));
        auto moduleRequestIdx = static_cast<uint32_t>(panda_file::helpers::Read<sizeof(uint16_t)>(&sp));
        cb(ModuleTag::INDIRECTEXPORT, exportNameOffset, moduleRequestIdx, importNameOffset, 0);
    }

    auto starExportNum = panda_file::helpers::Read<panda_file::ID_SIZE>(&sp);
    for (size_t idx = 0; idx < starExportNum; idx++) {
        auto moduleRequestIdx = static_cast<uint32_t>(panda_file::helpers::Read<sizeof(uint16_t)>(&sp));
        cb(ModuleTag::STAREXPORT, 0, moduleRequestIdx, 0, 0);
    }
}
}  // namespace panda::ecmascript::jspandafile

#endif  // ECMASCRIPT_MODULE_DATA_ACCESSOR_INL_H_
