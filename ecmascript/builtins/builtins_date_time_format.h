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

#ifndef ECMASCRIPT_BUILTINS_BUILTINS_DATE_TIME_FORMAT_H
#define ECMASCRIPT_BUILTINS_BUILTINS_DATE_TIME_FORMAT_H

#include "ecmascript/base/builtins_base.h"
#include "ecmascript/ecma_runtime_call_info.h"

namespace panda::ecmascript::builtins {
class BuiltinsDateTimeFormat : public base::BuiltinsBase {
public:
    // 13.2.1 Intl.DateTimeFormat ( [ locales [ , options ] ] )
    static JSTaggedValue DateTimeFormatConstructor(EcmaRuntimeCallInfo *argv);

    // 13.3.2 Intl.DateTimeFormat.supportedLocalesOf ( locales [ , options ] )
    static JSTaggedValue SupportedLocalesOf(EcmaRuntimeCallInfo *argv);

    // 13.4.3 get Intl.DateTimeFormat.prototype.format
    static JSTaggedValue Format(EcmaRuntimeCallInfo *argv);

    // 13.4.4 Intl.DateTimeFormat.prototype.formatToParts ( date )
    static JSTaggedValue FormatToParts(EcmaRuntimeCallInfo *argv);

    // 13.4.5 Intl.DateTimeFormat.prototype.resolvedOptions ()
    static JSTaggedValue ResolvedOptions(EcmaRuntimeCallInfo *argv);

    // Intl.DateTimeFormat.prototype.formatRange
    static JSTaggedValue FormatRange(EcmaRuntimeCallInfo *argv);

    // Intl.DateTimeFormat.prototype.formatRangeToParts
    static JSTaggedValue FormatRangeToParts(EcmaRuntimeCallInfo *argv);

private:
    // 13.1.5 DateTime Format Functions
    static JSTaggedValue AnonymousDateTimeFormat(EcmaRuntimeCallInfo *argv);
};
}  // namespace panda::ecmascript::builtins
#endif  // ECMASCRIPT_BUILTINS_BUILTINS_DATE_TIME_FORMAT_H_