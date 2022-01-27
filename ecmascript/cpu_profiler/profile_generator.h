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

#ifndef ECMASCRIPT_PROFILE_GENERSTOR_H
#define ECMASCRIPT_PROFILE_GENERSTOR_H

#include <ctime>
#include <fstream>
#include <string>
#include <unistd.h>
#include <sys/syscall.h>
#include "ecmascript/js_method.h"
#include "ecmascript/mem/c_containers.h"
namespace panda::ecmascript {
const long long TIME_CHANGE = 10000000000000; // 10000000000000:Discard the first 3 bits of the current nanosecond time
struct StackInfo {
    std::string codeType = "";
    std::string functionName = "";
    int columnNumber = 0;
    int lineNumber = 0;
    int scriptId = 0;
    std::string url = "";
};
struct MethodNode {
    int id = 0;
    int parentId = 0;
    struct StackInfo codeEntry;
};
struct SampleInfo {
    int id = 0;
    int line = 0;
    time_t timeStamp = 0;
};
struct MethodKey {
    JSMethod *method = nullptr;
    int parentId = 0;
    bool operator< (const MethodKey &methodKey) const
    {
        return parentId < methodKey.parentId || (parentId == methodKey.parentId && method < methodKey.method);
    }
};

class ProfileGenerator {
public:
    explicit ProfileGenerator();
    virtual ~ProfileGenerator();

    void AddSample(CVector<JSMethod *> sample, time_t sampleTimeStamp);
    void WriteMethodsAndSampleInfo(bool timeEnd);
    CVector<struct MethodNode> GetMethodNodes() const;
    CDeque<struct SampleInfo> GetSamples() const;
    std::string GetSampleData() const;
    void SetThreadStartTime(time_t threadStartTime);
    void SetStartsampleData(std::string sampleData);
    void SetFileName(std::string &fileName);
    const std::string GetFileName() const;
    void ClearSampleData();

    static int staticGcState_;
    std::ofstream fileHandle_;
private:
    void WriteAddNodes();
    void WriteAddSamples();
    struct StackInfo GetMethodInfo(JSMethod *method);
    struct StackInfo GetGcInfo();

    CVector<struct MethodNode> methodNodes_;
    CVector<int> stackTopLines_;
    CMap<struct MethodKey, int> methodMap_;
    CDeque<struct SampleInfo> samples_;
    std::string sampleData_;
    time_t threadStartTime_;
    std::string fileName_;
};
} // namespace panda::ecmascript
#endif // ECMASCRIPT_PROFILE_GENERSTOR_H