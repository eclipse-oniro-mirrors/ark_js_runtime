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

#include "ecmascript/cpu_profiler/cpu_profiler.h"
#include <atomic>
#include <chrono>
#include <climits>
#include <csignal>
#include <fstream>
#include "ecmascript/jspandafile/js_pandafile_manager.h"
#include "ecmascript/platform/platform.h"

namespace panda::ecmascript {
CMap<JSMethod *, struct StackInfo> CpuProfiler::staticStackInfo_ = CMap<JSMethod *, struct StackInfo>();
std::atomic<CpuProfiler*> CpuProfiler::singleton_ = nullptr;
sem_t CpuProfiler::sem_ = sem_t {};
CMap<std::string, int> CpuProfiler::scriptIdMap_ = CMap<std::string, int>();
CVector<JSMethod *> CpuProfiler::staticFrameStack_ = CVector<JSMethod *>();
os::memory::Mutex CpuProfiler::synchronizationMutex_;
CpuProfiler::CpuProfiler()
{
    generator_ = new ProfileGenerator();
    if (sem_init(&sem_, 0, 1) != 0) {
        LOG(ERROR, RUNTIME) << "sem_ init failed";
    }
    if (sem_wait(&sem_) != 0) {
        LOG(ERROR, RUNTIME) << "sem_ wait failed";
    }
}

CpuProfiler *CpuProfiler::GetInstance()
{
    CpuProfiler *temp = singleton_;
    if (temp == nullptr) {
        os::memory::LockHolder lock(synchronizationMutex_);
        if ((temp = CpuProfiler::singleton_) == nullptr) {
            CpuProfiler::singleton_ = temp = new CpuProfiler();
        }
    }
    return CpuProfiler::singleton_;
}

void CpuProfiler::StartCpuProfiler(const EcmaVM *vm, const std::string &fileName)
{
    if (isOnly_) {
        return;
    }
    isOnly_ = true;
    std::string absoluteFilePath("");
    if (!CheckFileName(fileName, absoluteFilePath)) {
        LOG(ERROR, RUNTIME) << "The fileName contains illegal characters";
        isOnly_ = false;
        return;
    }
    fileName_ = absoluteFilePath;
    if (fileName_.empty()) {
        fileName_ = GetProfileName();
    }
    generator_->SetFileName(fileName_);
    generator_->fileHandle_.open(fileName_.c_str());
    if (generator_->fileHandle_.fail()) {
        LOG(ERROR, RUNTIME) << "File open failed";
        isOnly_ = false;
        return;
    }
#if ECMASCRIPT_ENABLE_ACTIVE_CPUPROFILER
#else
    struct sigaction sa;
    sa.sa_handler = &GetStackSignalHandler;
    if (sigemptyset(&sa.sa_mask) != 0) {
        LOG(ERROR, RUNTIME) << "Parameter set signal set initialization and emptying failed";
        isOnly_ = false;
        return;
    }
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, nullptr) != 0) {
        LOG(ERROR, RUNTIME) << "sigaction failed to set signal";
        isOnly_ = false;
        return;
    }
#endif
    uint64_t ts = ProfileProcessor::GetMicrosecondsTimeStamp();
    ts = ts % TIME_CHANGE;
    SetProfileStart(ts);
    Platform::GetCurrentPlatform()->PostTask(std::make_unique<ProfileProcessor>(generator_, vm, interval_));
}

void CpuProfiler::StopCpuProfiler()
{
    if (!isOnly_) {
        LOG(ERROR, RUNTIME) << "Do not execute stop cpuprofiler twice in a row or didn't execute the start\
                                or the sampling thread is not started";
        return;
    }
    if (static_cast<long>(tid_) != syscall(SYS_gettid)) {
        LOG(ERROR, RUNTIME) << "Thread attempted to close other sampling threads";
        return;
    }
    isOnly_ = false;
    ProfileProcessor::SetIsStart(false);
    if (sem_wait(&sem_) != 0) {
        LOG(ERROR, RUNTIME) << "sem_ wait failed";
        return;
    }
    generator_->WriteMethodsAndSampleInfo(true);
    generator_->fileHandle_ << generator_->GetSampleData();
    if (singleton_ != nullptr) {
        delete singleton_;
        singleton_ = nullptr;
    }
}

CpuProfiler::~CpuProfiler()
{
    if (sem_destroy(&sem_) != 0) {
        LOG(ERROR, RUNTIME) << "sem_ destroy failed";
    }
    if (generator_ != nullptr) {
        delete generator_;
        generator_ = nullptr;
    }
}

void CpuProfiler::SetProfileStart(uint64_t nowTimeStamp)
{
    uint64_t ts = ProfileProcessor::GetMicrosecondsTimeStamp();
    ts = ts % TIME_CHANGE;
    struct CurrentProcessInfo currentProcessInfo = {0};
    GetCurrentProcessInfo(currentProcessInfo);
    std::string data = "";
    data = "[{\"args\":{\"data\":{\"frames\":[{\"processId\":" + std::to_string(currentProcessInfo.pid) + "}]"
            + ",\"persistentIds\":true}},\"cat\":\"disabled-by-default-devtools.timeline\","
            + "\"name\":\"TracingStartedInBrowser\",\"ph\":\"I\",\"pid\":"
            + std::to_string(currentProcessInfo.pid) + ",\"s\":\"t\",\"tid\":"
            + std::to_string(currentProcessInfo.tid) + ",\"ts\":"
            + std::to_string(ts) + ",\"tts\":178460227},\n";
    ts = ProfileProcessor::GetMicrosecondsTimeStamp();
    ts = ts % TIME_CHANGE;
    data += "{\"args\":{\"data\":{\"startTime\":" + std::to_string(nowTimeStamp) + "}},"
            + "\"cat\":\"disabled-by-default-ark.cpu_profiler\",\"id\":\"0x2\","
            + "\"name\":\"Profile\",\"ph\":\"P\",\"pid\":"
            + std::to_string(currentProcessInfo.pid) + ",\"tid\":"
            + std::to_string(currentProcessInfo.tid) + ",\"ts\":"
            + std::to_string(ts) + ",\"tts\":" + std::to_string(currentProcessInfo.tts)
            + "},\n";
    generator_->SetStartsampleData(data);
}

void CpuProfiler::GetCurrentProcessInfo(struct CurrentProcessInfo &currentProcessInfo)
{
    currentProcessInfo.nowTimeStamp = ProfileProcessor::GetMicrosecondsTimeStamp() % TIME_CHANGE;
    currentProcessInfo.pid = getpid();
    tid_ = currentProcessInfo.tid = syscall(SYS_gettid);
    struct timespec time = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &time);
    currentProcessInfo.tts = time.tv_nsec / 1000; // 1000:Nanoseconds to milliseconds.
}

void CpuProfiler::GetFrameStack(JSThread *thread)
{
    staticFrameStack_.clear();
    ProfileGenerator::staticGcState_ = thread->GetGcState();
    if (!ProfileGenerator::staticGcState_) {
        JSTaggedType *sp_ = const_cast<JSTaggedType *>(thread->GetCurrentSPFrame());
        InterpretedFrameHandler frameHandler(sp_);
        for (; frameHandler.HasFrame(); frameHandler.PrevInterpretedFrame()) {
            if (frameHandler.IsBreakFrame()) {
                continue;
            }
            auto *method = frameHandler.GetMethod();
            if (method != nullptr && staticStackInfo_.count(method) == 0) {
                ParseMethodInfo(method, thread, frameHandler);
            }
            staticFrameStack_.push_back(method);
        }
    }
}

void CpuProfiler::ParseMethodInfo(JSMethod *method, JSThread *thread, InterpretedFrameHandler frameHandler)
{
    struct StackInfo codeEntry;
    if (method != nullptr && method->IsNative()) {
        codeEntry.codeType = "other";
        codeEntry.functionName = "native";
        staticStackInfo_.insert(std::make_pair(method, codeEntry));
    } else {
        codeEntry.codeType = "JS";
        const std::string &functionName = method->ParseFunctionName();
        if (functionName.empty()) {
            codeEntry.functionName = "anonymous";
        } else {
            codeEntry.functionName = functionName.c_str();
        }
        // source file
        tooling::JSPtExtractor *debugExtractor =
            JSPandaFileManager::GetInstance()->GetJSPtExtractor(method->GetJSPandaFile());
        if (method == nullptr) {
            return;
        }
        const std::string &sourceFile = debugExtractor->GetSourceFile(method->GetFileId());
        if (sourceFile.empty()) {
            codeEntry.url = "";
        } else {
            codeEntry.url = sourceFile.c_str();
            auto iter = scriptIdMap_.find(codeEntry.url);
            if (iter == scriptIdMap_.end()) {
                scriptIdMap_.insert(std::make_pair(codeEntry.url, scriptIdMap_.size() + 1));
                codeEntry.scriptId = scriptIdMap_.size();
            } else {
                codeEntry.scriptId = iter->second;
            }
        }
        // line number
        int32_t lineNumber = 0;
        int32_t columnNumber = 0;
        auto callbackLineFunc = [&](int32_t line) -> bool {
            lineNumber = line + 1;
            return true;
        };
        auto callbackColumnFunc = [&](int32_t column) -> bool {
            columnNumber = column + 1;
            return true;
        };
        panda_file::File::EntityId methodId = method->GetFileId();
        uint32_t offset = frameHandler.GetBytecodeOffset();
        if (!debugExtractor->MatchLineWithOffset(callbackLineFunc, methodId, offset) ||
            !debugExtractor->MatchColumnWithOffset(callbackColumnFunc, methodId, offset)) {
            codeEntry.lineNumber = 0;
            codeEntry.columnNumber = 0;
        } else {
            codeEntry.lineNumber = lineNumber;
            codeEntry.columnNumber = columnNumber;
        }
        staticStackInfo_.insert(std::make_pair(method, codeEntry));
    }
}

void CpuProfiler::IsNeedAndGetStack(JSThread *thread)
{
    if (thread->GetStackSignal()) {
        GetFrameStack(thread);
        if (sem_post(&CpuProfiler::sem_) != 0) {
            LOG(ERROR, RUNTIME) << "sem_ post failed";
            return;
        }
        thread->SetGetStackSignal(false);
    }
}

void CpuProfiler::GetStackSignalHandler(int signal)
{
    JSThread *thread = ProfileProcessor::GetJSThread();
    GetFrameStack(thread);
    if (sem_post(&CpuProfiler::sem_) != 0) {
        LOG(ERROR, RUNTIME) << "sem_ post failed";
        return;
    }
}

std::string CpuProfiler::GetProfileName() const
{
    char time1[16] = {0}; // 16:Time format length
    char time2[16] = {0}; // 16:Time format length
    time_t timep = std::time(NULL);
    struct tm nowTime1;
    localtime_r(&timep, &nowTime1);
    size_t result = 0;
    result = strftime(time1, sizeof(time1), "%Y%m%d", &nowTime1);
    if (result == 0) {
        LOG(ERROR, RUNTIME) << "get time failed";
        return "";
    }
    result = strftime(time2, sizeof(time2), "%H%M%S", &nowTime1);
    if (result == 0) {
        LOG(ERROR, RUNTIME) << "get time failed";
        return "";
    }
    std::string profileName = "cpuprofile-";
    profileName += time1;
    profileName += "TO";
    profileName += time2;
    profileName += ".json";
    return profileName;
}

bool CpuProfiler::CheckFileName(const std::string &fileName, std::string &absoluteFilePath) const
{
    if (fileName.empty()) {
        return true;
    }

    if (fileName.size() > PATH_MAX) {
        return false;
    }

    CVector<char> resolvedPath(PATH_MAX);
    auto result = realpath(fileName.c_str(), resolvedPath.data());
    if (result == nullptr) {
        LOG(INFO, RUNTIME) << "The file path does not exist";
    }
    std::ofstream file(resolvedPath.data());
    if (!file.good()) {
        return false;
    }
    file.close();
    absoluteFilePath = resolvedPath.data();
    return true;
}
} // namespace panda::ecmascript
