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

#include "ecmascript/js_array.h"
#include "ecmascript/js_tagged_value-inl.h"
#include "ecmascript/object_factory.h"
#include "ecmascript/tests/test_helper.h"
#include "ecmascript/tooling/base/pt_events.h"
#include "ecmascript/tooling/base/pt_types.h"
#include "ecmascript/tooling/dispatcher.h"

using namespace panda::ecmascript;
using namespace panda::ecmascript::tooling;
using ObjectType = RemoteObject::TypeName;
using ObjectSubType = RemoteObject::SubTypeName;
using ObjectClassName = RemoteObject::ClassName;

namespace panda::test {
class DebuggerEventsTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
        GTEST_LOG_(INFO) << "SetUpTestCase";
        Logger::InitializeStdLogging(Logger::Level::FATAL, LoggerComponentMaskAll);
    }

    static void TearDownTestCase()
    {
        Logger::InitializeStdLogging(Logger::Level::ERROR, LoggerComponentMaskAll);
        GTEST_LOG_(INFO) << "TearDownCase";
    }

    void SetUp() override
    {
        TestHelper::CreateEcmaVMWithScope(instance, thread, scope);
        ecmaVm = EcmaVM::Cast(instance);
        // Main logic is JSON parser, so not need trigger GC to decrease execute time
        ecmaVm->SetEnableForceGC(false);
    }

    void TearDown() override
    {
        TestHelper::DestroyEcmaVMWithScope(ecmaVm, scope);
    }

protected:
    EcmaVM *ecmaVm {nullptr};
    PandaVM *instance {nullptr};
    EcmaHandleScope *scope {nullptr};
    JSThread *thread {nullptr};
};

HWTEST_F_L0(DebuggerEventsTest, BreakpointResolvedToJsonTest)
{
    BreakpointResolved breakpointResolved;

    auto location = std::make_unique<Location>();
    location->SetScriptId(2).SetLine(99);
    breakpointResolved.SetBreakpointId("00").SetLocation(std::move(location));

    std::unique_ptr<PtJson> json;
    ASSERT_EQ(breakpointResolved.ToJson()->GetObject("params", &json), Result::SUCCESS);
    std::string breakpointId;
    ASSERT_EQ(json->GetString("breakpointId", &breakpointId), Result::SUCCESS);
    EXPECT_EQ(breakpointId, "00");

    std::unique_ptr<PtJson> locationJson;
    ASSERT_EQ(json->GetObject("location", &locationJson), Result::SUCCESS);
    std::string scriptId;
    ASSERT_EQ(locationJson->GetString("scriptId", &scriptId), Result::SUCCESS);
    EXPECT_EQ(scriptId, "2");
    int32_t lineNumber;
    ASSERT_EQ(locationJson->GetInt("lineNumber", &lineNumber), Result::SUCCESS);
    EXPECT_EQ(lineNumber, 99);
}

#ifdef CHANGE_TOJSON
HWTEST_F_L0(DebuggerEventsTest, PausedToObjectTest)
{
    Paused paused;
    Local<StringRef> tmpStr = StringRef::NewFromUtf8(ecmaVm, "params");

    std::vector<std::unique_ptr<CallFrame>> v;
    paused.SetCallFrames(std::move(v))
        .SetReason(PauseReason::EXCEPTION);
    Local<ObjectRef> object1 = paused.ToObject(ecmaVm);
    Local<JSValueRef> result = object1->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    ASSERT_TRUE(result->IsObject());
    Local<ObjectRef> object = Local<ObjectRef>(result);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "reason");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ("exception", Local<StringRef>(result)->ToString());

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "callFrames");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    ASSERT_TRUE(result->IsArray(ecmaVm));
}

HWTEST_F_L0(DebuggerEventsTest, ResumedToObjectTest)
{
    Resumed resumed;
    Local<StringRef> tmpStr;

    Local<ObjectRef> object = resumed.ToObject(ecmaVm);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "method");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    Local<JSValueRef> result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(resumed.GetName(), Local<StringRef>(result)->ToString());

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "params");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    ASSERT_TRUE(result->IsObject());
}

HWTEST_F_L0(DebuggerEventsTest, ScriptFailedToParseToObjectTest)
{
    ScriptFailedToParse parsed;
    Local<StringRef> tmpStr = StringRef::NewFromUtf8(ecmaVm, "params");

    parsed.SetScriptId(100)
        .SetUrl("use/test.js")
        .SetStartLine(0)
        .SetStartColumn(4)
        .SetEndLine(10)
        .SetEndColumn(10)
        .SetExecutionContextId(2)
        .SetHash("hash0001")
        .SetSourceMapURL("usr/")
        .SetHasSourceURL(true)
        .SetIsModule(true)
        .SetLength(34)
        .SetCodeOffset(432)
        .SetScriptLanguage("JavaScript")
        .SetEmbedderName("hh");
    Local<ObjectRef> object1 = parsed.ToObject(ecmaVm);
    Local<JSValueRef> result = object1->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    ASSERT_TRUE(result->IsObject());
    Local<ObjectRef> object = Local<ObjectRef>(result);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "scriptId");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ("100", Local<StringRef>(result)->ToString());

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "url");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ("use/test.js", Local<StringRef>(result)->ToString());

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "startLine");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<IntegerRef>(result)->Value(), 0);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "startColumn");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<IntegerRef>(result)->Value(), 4);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "endLine");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<IntegerRef>(result)->Value(), 10);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "endColumn");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<IntegerRef>(result)->Value(), 10);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "executionContextId");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<IntegerRef>(result)->Value(), 2);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "hash");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ("hash0001", Local<StringRef>(result)->ToString());

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "executionContextAuxData");
    ASSERT_FALSE(object->Has(ecmaVm, tmpStr));

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "sourceMapURL");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ("usr/", Local<StringRef>(result)->ToString());

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "hasSourceURL");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    ASSERT_TRUE(result->IsTrue());

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "isModule");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    ASSERT_TRUE(result->IsTrue());

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "length");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<IntegerRef>(result)->Value(), 34);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "codeOffset");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<IntegerRef>(result)->Value(), 432);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "scriptLanguage");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ("JavaScript", Local<StringRef>(result)->ToString());

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "embedderName");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ("hh", Local<StringRef>(result)->ToString());
}

HWTEST_F_L0(DebuggerEventsTest, ScriptParsedToObjectTest)
{
    ScriptParsed parsed;
    Local<StringRef> tmpStr = StringRef::NewFromUtf8(ecmaVm, "params");

    parsed.SetScriptId(10)
        .SetUrl("use/test.js")
        .SetStartLine(0)
        .SetStartColumn(4)
        .SetEndLine(10)
        .SetEndColumn(10)
        .SetExecutionContextId(2)
        .SetHash("hash0001")
        .SetIsLiveEdit(true)
        .SetSourceMapURL("usr/")
        .SetHasSourceURL(true)
        .SetIsModule(true)
        .SetLength(34)
        .SetCodeOffset(432)
        .SetScriptLanguage("JavaScript")
        .SetEmbedderName("hh");
    Local<ObjectRef> object1 = parsed.ToObject(ecmaVm);
    Local<JSValueRef> result = object1->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    ASSERT_TRUE(result->IsObject());
    Local<ObjectRef> object = Local<ObjectRef>(result);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "scriptId");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ("10", Local<StringRef>(result)->ToString());

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "url");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ("use/test.js", Local<StringRef>(result)->ToString());

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "startLine");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<IntegerRef>(result)->Value(), 0);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "startColumn");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<IntegerRef>(result)->Value(), 4);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "endLine");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<IntegerRef>(result)->Value(), 10);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "endColumn");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<IntegerRef>(result)->Value(), 10);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "executionContextId");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<IntegerRef>(result)->Value(), 2);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "hash");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ("hash0001", Local<StringRef>(result)->ToString());

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "executionContextAuxData");
    ASSERT_FALSE(object->Has(ecmaVm, tmpStr));

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "isLiveEdit");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    ASSERT_TRUE(result->IsTrue());

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "sourceMapURL");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ("usr/", Local<StringRef>(result)->ToString());

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "hasSourceURL");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    ASSERT_TRUE(result->IsTrue());

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "isModule");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    ASSERT_TRUE(result->IsTrue());

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "length");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<IntegerRef>(result)->Value(), 34);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "codeOffset");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<IntegerRef>(result)->Value(), 432);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "scriptLanguage");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ("JavaScript", Local<StringRef>(result)->ToString());

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "embedderName");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ("hh", Local<StringRef>(result)->ToString());
}

HWTEST_F_L0(DebuggerEventsTest, ConsoleProfileFinishedToObjectTest)
{
    ConsoleProfileFinished consoleProfileFinished;
    Local<StringRef> tmpStr = StringRef::NewFromUtf8(ecmaVm, "params");

    auto location = std::make_unique<Location>();
    location->SetScriptId(13).SetLine(20);
    std::vector<std::unique_ptr<ProfileNode>> v;
    auto profile = std::make_unique<Profile>();
    profile->SetNodes(std::move(v))
        .SetStartTime(0)
        .SetEndTime(15)
        .SetSamples(std::vector<int32_t>{})
        .SetTimeDeltas(std::vector<int32_t>{});
    consoleProfileFinished.SetId("11").SetLocation(std::move(location)).SetProfile(std::move(profile)).SetTitle("001");
    Local<ObjectRef> object1 = consoleProfileFinished.ToObject(ecmaVm);
    Local<JSValueRef> result = object1->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    ASSERT_TRUE(result->IsObject());
    Local<ObjectRef> object = Local<ObjectRef>(result);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "id");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<StringRef>(result)->ToString(), "11");

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "location");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    ASSERT_TRUE(result->IsObject());

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "profile");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    ASSERT_TRUE(result->IsObject());

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "title");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<StringRef>(result)->ToString(), "001");
}

HWTEST_F_L0(DebuggerEventsTest, ConsoleProfileStartedToObjectTest)
{
    ConsoleProfileStarted consoleProfileStarted;
    Local<StringRef> tmpStr = StringRef::NewFromUtf8(ecmaVm, "params");

    auto location = std::make_unique<Location>();
    location->SetScriptId(17).SetLine(30);
    consoleProfileStarted.SetId("12").SetLocation(std::move(location)).SetTitle("002");
    Local<ObjectRef> object1 = consoleProfileStarted.ToObject(ecmaVm);
    Local<JSValueRef> result = object1->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    ASSERT_TRUE(result->IsObject());
    Local<ObjectRef> object = Local<ObjectRef>(result);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "id");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<StringRef>(result)->ToString(), "12");

    Local<ObjectRef> tmpObject = consoleProfileStarted.GetLocation()->ToObject(ecmaVm);
    tmpStr = StringRef::NewFromUtf8(ecmaVm, "scriptId");
    ASSERT_TRUE(tmpObject->Has(ecmaVm, tmpStr));
    Local<JSValueRef> tmpResult = tmpObject->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!tmpResult.IsEmpty() && !tmpResult->IsUndefined());
    EXPECT_EQ(Local<StringRef>(tmpResult)->ToString(), "17");
    tmpStr = StringRef::NewFromUtf8(ecmaVm, "lineNumber");
    ASSERT_TRUE(tmpObject->Has(ecmaVm, tmpStr));
    tmpResult = tmpObject->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!tmpResult.IsEmpty() && !tmpResult->IsUndefined());
    EXPECT_EQ(Local<IntegerRef>(tmpResult)->Value(), 30);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "title");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<StringRef>(result)->ToString(), "002");
}

HWTEST_F_L0(DebuggerEventsTest, PreciseCoverageDeltaUpdateToObjectTest)
{
    PreciseCoverageDeltaUpdate preciseCoverageDeltaUpdate;
    Local<StringRef> tmpStr = StringRef::NewFromUtf8(ecmaVm, "params");

    std::vector<std::unique_ptr<ScriptCoverage>> v;
    preciseCoverageDeltaUpdate.SetOccasion("percise")
        .SetResult(std::move(v))
        .SetTimestamp(77);
    Local<ObjectRef> object1 = preciseCoverageDeltaUpdate.ToObject(ecmaVm);
    Local<JSValueRef> result = object1->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    ASSERT_TRUE(result->IsObject());
    Local<ObjectRef> object = Local<ObjectRef>(result);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "timestamp");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<IntegerRef>(result)->Value(), 77);
    tmpStr = StringRef::NewFromUtf8(ecmaVm, "occasion");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<StringRef>(result)->ToString(), "percise");
    tmpStr = StringRef::NewFromUtf8(ecmaVm, "result");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    ASSERT_TRUE(result->IsArray(ecmaVm));
}

HWTEST_F_L0(DebuggerEventsTest, HeapStatsUpdateToObjectTest)
{
    HeapStatsUpdate heapStatsUpdate;
    Local<StringRef> tmpStr = StringRef::NewFromUtf8(ecmaVm, "params");

    heapStatsUpdate.SetStatsUpdate(std::vector<int32_t> {});
    Local<ObjectRef> object1 = heapStatsUpdate.ToObject(ecmaVm);
    Local<JSValueRef> result = object1->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    ASSERT_TRUE(result->IsObject());
    Local<ObjectRef> object = Local<ObjectRef>(result);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "statsUpdate");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    ASSERT_TRUE(result->IsArray(ecmaVm));
}

HWTEST_F_L0(DebuggerEventsTest, LastSeenObjectIdToObjectTest)
{
    LastSeenObjectId lastSeenObjectId;
    Local<StringRef> tmpStr = StringRef::NewFromUtf8(ecmaVm, "params");

    lastSeenObjectId.SetLastSeenObjectId(10).SetTimestamp(77);
    Local<ObjectRef> object1 = lastSeenObjectId.ToObject(ecmaVm);
    Local<JSValueRef> result = object1->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    ASSERT_TRUE(result->IsObject());
    Local<ObjectRef> object = Local<ObjectRef>(result);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "lastSeenObjectId");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<IntegerRef>(result)->Value(), 10);
    tmpStr = StringRef::NewFromUtf8(ecmaVm, "timestamp");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<IntegerRef>(result)->Value(), 77);
}

HWTEST_F_L0(DebuggerEventsTest, ReportHeapSnapshotProgressToObjectTest)
{
    ReportHeapSnapshotProgress reportHeapSnapshotProgress;
    Local<StringRef> tmpStr = StringRef::NewFromUtf8(ecmaVm, "params");

    reportHeapSnapshotProgress.SetDone(10).SetTotal(100);
    Local<ObjectRef> object1 = reportHeapSnapshotProgress.ToObject(ecmaVm);
    Local<JSValueRef> result = object1->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    ASSERT_TRUE(result->IsObject());
    Local<ObjectRef> object = Local<ObjectRef>(result);

    tmpStr = StringRef::NewFromUtf8(ecmaVm, "done");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<IntegerRef>(result)->Value(), 10);
    tmpStr = StringRef::NewFromUtf8(ecmaVm, "total");
    ASSERT_TRUE(object->Has(ecmaVm, tmpStr));
    result = object->Get(ecmaVm, tmpStr);
    ASSERT_TRUE(!result.IsEmpty() && !result->IsUndefined());
    EXPECT_EQ(Local<IntegerRef>(result)->Value(), 100);
}
#endif
}  // namespace panda::test