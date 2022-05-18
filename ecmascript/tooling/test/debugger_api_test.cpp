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

#include "ecmascript/ecma_vm.h"
#include "ecmascript/napi/include/jsnapi.h"
#include "ecmascript/tests/test_helper.h"
#include "ecmascript/tooling/test/utils/testcases/test_list.h"

namespace panda::ecmascript::tooling::test {
using panda::test::TestHelper;

class DebuggerApiTest : public testing::TestWithParam<const char *> {
public:
    static void SetUpTestCase()
    {
        GTEST_LOG_(INFO) << "SetUpTestCase";
    }

    static void TearDownTestCase()
    {
        GTEST_LOG_(INFO) << "TearDownCase";
    }

    void SetUp() override
    {
        SetCurrentTestName(GetParam());
        TestHelper::CreateEcmaVMWithScope(instance, thread, scope, DEBUGGER_TEST_LIBRARY);
    }

    void TearDown() override
    {
        TestHelper::DestroyEcmaVMWithScope(instance, scope);
    }

    PandaVM *instance {nullptr};
    EcmaHandleScope *scope {nullptr};
    JSThread *thread {nullptr};
};

HWTEST_P_L0(DebuggerApiTest, EcmaScriptSuite)
{
    const char *testName = GetCurrentTestName();
    std::cout << "Running " << testName << std::endl;
    EcmaVM *vm = EcmaVM::Cast(instance);
    ASSERT_NE(vm, nullptr);
    auto [pandaFile, entryPoint] = GetTestEntryPoint(testName);

    std::string fileNameStr(pandaFile);
    std::string entryStr(entryPoint);
    auto res = JSNApi::Execute(vm, fileNameStr, entryStr);
    ASSERT_TRUE(res);
}

INSTANTIATE_TEST_CASE_P(EcmaDebugApiTest, DebuggerApiTest,
                        testing::ValuesIn(GetTestList(panda::panda_file::SourceLang::ECMASCRIPT)));
}  // namespace panda::ecmascript::tooling::test
