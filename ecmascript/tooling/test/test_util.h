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

#ifndef ECMASCRIPT_TOOLING_TEST_TEST_UTIL_H
#define ECMASCRIPT_TOOLING_TEST_TEST_UTIL_H

#include <climits>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "api_test.h"
#include "ecmascript/tooling/interface/js_debugger.h"
#include "os/mutex.h"
#include "test_extractor.h"

namespace panda::tooling::ecmascript::test {
using TestMap = std::unordered_map<panda_file::SourceLang, std::unordered_map<const char *, std::unique_ptr<ApiTest>>>;

class TestUtil {
public:
    static void RegisterTest(panda_file::SourceLang language, const char *testName, std::unique_ptr<ApiTest> test)
    {
        auto it = testMap_.find(language);
        if (it == testMap_.end()) {
            std::unordered_map<const char *, std::unique_ptr<ApiTest>> entry;
            auto res = testMap_.emplace(language, std::move(entry));
            it = res.first;
        }
        it->second.insert({testName, std::move(test)});
    }

    static ApiTest *GetTest(const char *name)
    {
        for (auto it = testMap_.begin(); it != testMap_.end(); ++it) {
            auto &internalMap = it->second;
            auto internalIt = std::find_if(internalMap.begin(), internalMap.end(),
                                           [name](auto &it) { return !::strcmp(it.first, name); });
            if (internalIt != internalMap.end()) {
                return internalIt->second.get();
            }
        }
        LOG(FATAL, DEBUGGER) << "Test " << name << " not found";
        return nullptr;
    }

    static PtThread WaitForBreakpoint(PtLocation location)
    {
        PtThread stoppedThread(PtThread::NONE);
        auto predicate = [&location]() REQUIRES(eventMutex_) { return lastEventLocation_ == location; };
        auto onSuccess = [&stoppedThread]() REQUIRES(eventMutex_) {
            stoppedThread = lastEventThread_;

            // Need to reset location, because we might want to stop at the same point
            lastEventLocation_ = PtLocation("", EntityId(0), 0);
        };

        WaitForEvent(DebugEvent::BREAKPOINT, predicate, onSuccess);
        return stoppedThread;
    }

    static bool WaitForExit()
    {
        return WaitForEvent(DebugEvent::VM_DEATH,
            []() REQUIRES(eventMutex_) { return lastEvent_ == DebugEvent::VM_DEATH; }, [] {});
    }

    static bool WaitForInit()
    {
        return WaitForEvent(DebugEvent::VM_INITIALIZATION,
            []() REQUIRES(eventMutex_) { return initialized_; }, [] {});
    }

    static void Event(DebugEvent event, PtThread thread = PtThread::NONE,
                      PtLocation location = PtLocation("", EntityId(0), 0))
    {
        LOG(DEBUG, DEBUGGER) << "Occured event " << event << " in thread with id " << thread.GetId();
        os::memory::LockHolder holder(eventMutex_);
        lastEvent_ = event;
        lastEventThread_ = thread;
        lastEventLocation_ = location;
        if (event == DebugEvent::VM_INITIALIZATION) {
            initialized_ = true;
        }
        eventCv_.Signal();
    }

    static void Reset()
    {
        os::memory::LockHolder lock(eventMutex_);
        initialized_ = false;
        lastEvent_ = DebugEvent::VM_START;
    }

    static TestMap &GetTests()
    {
        return testMap_;
    }

    static bool IsTestFinished()
    {
        os::memory::LockHolder lock(eventMutex_);
        return lastEvent_ == DebugEvent::VM_DEATH;
    }

    static PtLocation GetLocation(const char *sourceFile, uint32_t line, const char *pandaFile)
    {
        std::unique_ptr<const panda_file::File> uFile = panda_file::File::Open(pandaFile);
        const panda_file::File *pf = uFile.get();
        if (pf == nullptr) {
            return PtLocation("", EntityId(0), 0);
        }

        TestExtractor extractor(pf);
        auto [id, offset] = extractor.GetBreakpointAddress({sourceFile, line});
#if PANDA_LIBCORE_SUPPORT
        static std::unordered_set<std::string> absolute_paths;
        static char buf[PATH_MAX];
        auto res = absolute_paths.insert(::realpath(pandaFile, buf));
        return PtLocation(res.first->c_str(), id, offset);
#else   // PANDA_LIBCORE_SUPPORT
        return PtLocation(pandaFile, id, offset);
#endif  // PANDA_LIBCORE_SUPPORT
    }

    static std::vector<panda_file::LocalVariableInfo> GetVariables(JSMethod *method, uint32_t offset);

    static std::vector<panda_file::LocalVariableInfo> GetVariables(PtLocation location);

    static int32_t GetValueRegister(JSMethod *method, const char *varName);

    static bool SuspendUntilContinue(DebugEvent reason, PtThread thread, PtLocation location)
    {
        {
            os::memory::LockHolder lock(suspendMutex_);
            suspended_ = true;
        }

        // Notify the debugger thread about the suspend event
        Event(reason, thread, location);

        // Wait for continue
        {
            os::memory::LockHolder lock(suspendMutex_);
            while (suspended_) {
                suspendCv_.Wait(&suspendMutex_);
            }
        }

        return true;
    }

    static bool Continue()
    {
        os::memory::LockHolder lock(suspendMutex_);
        suspended_ = false;
        suspendCv_.Signal();
        return true;
    }

private:
    template<class Predicate, class OnSuccessAction>
    static bool WaitForEvent(DebugEvent event, Predicate predicate, OnSuccessAction action)
    {
        os::memory::LockHolder holder(eventMutex_);
        while (!predicate()) {
            if (lastEvent_ == DebugEvent::VM_DEATH) {
                return false;
            }
            constexpr uint64_t TIMEOUT_MSEC = 100000U;
            bool timeExceeded = eventCv_.TimedWait(&eventMutex_, TIMEOUT_MSEC);
            if (timeExceeded) {
                LOG(FATAL, DEBUGGER) << "Time limit exceeded while waiting " << event;
                return false;
            }
        }
        action();
        return true;
    }

    static TestMap testMap_;
    static os::memory::Mutex eventMutex_;
    static os::memory::ConditionVariable eventCv_ GUARDED_BY(eventMutex_);
    static DebugEvent lastEvent_ GUARDED_BY(eventMutex_);
    static PtThread lastEventThread_ GUARDED_BY(eventMutex_);
    static PtLocation lastEventLocation_ GUARDED_BY(eventMutex_);
    static os::memory::Mutex suspendMutex_;
    static os::memory::ConditionVariable suspendCv_ GUARDED_BY(suspendMutex_);
    static bool suspended_ GUARDED_BY(suspendMutex_);
    static bool initialized_ GUARDED_BY(eventMutex_);
};

std::ostream &operator<<(std::ostream &out, std::nullptr_t);

#define _ASSERT_FAIL(val1, val2, strval1, strval2, msg)                              \
    std::cerr << "Assertion failed at " << __FILE__ << ':' << __LINE__ << std::endl; \
    std::cerr << "Expected that " strval1 " is " << msg << " " strval2 << std::endl; \
    std::cerr << "\t" strval1 ": " << (val1) << std::endl;                           \
    std::cerr << "\t" strval2 ": " << (val2) << std::endl;                           \
    std::abort();

#define ASSERT_TRUE(cond)                                      \
    do {                                                       \
        auto res = (cond);                                     \
        if (!res) {                                            \
            _ASSERT_FAIL(res, true, #cond, "true", "equal to") \
        }                                                      \
    } while (0)

#define ASSERT_FALSE(cond)                                       \
    do {                                                         \
        auto res = (cond);                                       \
        if (res) {                                               \
            _ASSERT_FAIL(res, false, #cond, "false", "equal to") \
        }                                                        \
    } while (0)

#define ASSERT_EQ(lhs, rhs)                                  \
    do {                                                     \
        auto res1 = (lhs);                                   \
        decltype(res1) res2 = (rhs);                         \
        if (res1 != res2) {                                  \
            _ASSERT_FAIL(res1, res2, #lhs, #rhs, "equal to") \
        }                                                    \
    } while (0)

#define ASSERT_NE(lhs, rhs)                                      \
    do {                                                         \
        auto res1 = (lhs);                                       \
        decltype(res1) res2 = (rhs);                             \
        if (res1 == res2) {                                      \
            _ASSERT_FAIL(res1, res2, #lhs, #rhs, "not equal to") \
        }                                                        \
    } while (0)

#define ASSERT_STREQ(lhs, rhs)                               \
    do {                                                     \
        auto res1 = (lhs);                                   \
        decltype(res1) res2 = (rhs);                         \
        if (::strcmp(res1, res2) != 0) {                     \
            _ASSERT_FAIL(res1, res2, #lhs, #rhs, "equal to") \
        }                                                    \
    } while (0)

#define ASSERT_SUCCESS(api_call)                                                                   \
    do {                                                                                           \
        auto error = api_call;                                                                     \
        if (error) {                                                                               \
            _ASSERT_FAIL(error.value().GetMessage(), "Success", "API call result", "Expected", "") \
        }                                                                                          \
    } while (0)

#define ASSERT_EXITED()                                                                              \
    do {                                                                                             \
        bool res = TestUtil::WaitForExit();                                                          \
        if (!res) {                                                                                  \
            _ASSERT_FAIL(TestUtil::IsTestFinished(), true, "TestUtil::IsTestFinished()", "true", "") \
        }                                                                                            \
    } while (0)

#define ASSERT_LOCATION_EQ(lhs, rhs)                                             \
    do {                                                                         \
        ASSERT_STREQ(lhs.GetPandaFile(), rhs.GetPandaFile());                    \
        ASSERT_EQ(lhs.GetMethodId().GetOffset(), rhs.GetMethodId().GetOffset()); \
        ASSERT_EQ(lhs.GetBytecodeOffset(), rhs.GetBytecodeOffset());             \
    } while (0);

#define ASSERT_THREAD_VALID(ecmaVm)                        \
    do {                                                   \
        ASSERT_NE(ecmaVm.GetId(), PtThread::NONE.GetId()); \
    } while (0);

#define ASSERT_BREAKPOINT_SUCCESS(location)                         \
    do {                                                            \
        PtThread suspended = TestUtil::WaitForBreakpoint(location); \
        ASSERT_THREAD_VALID(suspended);                             \
    } while (0);
}  // namespace panda::tooling::ecmascript::test
#endif  // ECMASCRIPT_TOOLING_TEST_TEST_UTIL_H
