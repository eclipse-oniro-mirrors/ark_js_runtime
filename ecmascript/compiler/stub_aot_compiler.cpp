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

#include "stub_aot_compiler.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <unistd.h>

#include "fast_stub.h"
#include "interpreter_stub-inl.h"
#include "generated/stub_aot_options_gen.h"
#include "libpandabase/utils/pandargs.h"
#include "libpandabase/utils/span.h"
#include "llvm_codegen.h"
#include "scheduler.h"
#include "stub-inl.h"
#include "verifier.h"

namespace panda::ecmascript::kungfu {
class PassPayLoad {
public:
    explicit PassPayLoad(Stub *stub, LLVMStubModule *module) : module_(module), stub_(stub) {}
    ~PassPayLoad() = default;
    const ControlFlowGraph &GetScheduleResult() const
    {
        return cfg_;
    }

    void SetScheduleResult(const ControlFlowGraph &result)
    {
        cfg_ = result;
    }

    const CompilationConfig *GetCompilationConfig() const
    {
        return module_->GetCompilationConfig();
    }

    Circuit *GetCircuit() const
    {
        return stub_->GetEnvironment()->GetCircuit();
    }

    LLVMStubModule *GetStubModule() const
    {
        return module_;
    }

    Stub *GetStub() const
    {
        return stub_;
    }

private:
    LLVMStubModule *module_;
    Stub *stub_;
    ControlFlowGraph cfg_;
};

class PassRunner {
public:
    explicit PassRunner(PassPayLoad *data) : data_(data) {}
    ~PassRunner() = default;
    template<typename T, typename... Args>
    bool RunPass(Args... args)
    {
        T pass;
        return pass.Run(data_, std::forward<Args>(args)...);
    }

private:
    PassPayLoad *data_;
};

class BuildCircuitPass {
public:
    bool Run(PassPayLoad *data)
    {
        auto stub = data->GetStub();
        std::cout << "Stub Name: " << stub->GetMethodName() << std::endl;
        stub->GenerateCircuit(data->GetCompilationConfig());
        return true;
    }
};

class VerifierPass {
public:
    bool Run(PassPayLoad *data)
    {
        Verifier::Run(data->GetCircuit());
        return true;
    }
};

class SchedulerPass {
public:
    bool Run(PassPayLoad *data)
    {
        data->SetScheduleResult(Scheduler::Run(data->GetCircuit()));
        return true;
    }
};

class LLVMCodegenPass {
public:
    void CreateCodeGen(LLVMStubModule *module)
    {
        llvmImpl_ = std::make_unique<LLVMCodeGeneratorImpl>(module);
    }
    bool Run(PassPayLoad *data, int index)
    {
        auto stubModule = data->GetStubModule();
        CreateCodeGen(stubModule);
        CodeGenerator codegen(llvmImpl_);
        codegen.Run(data->GetCircuit(), data->GetScheduleResult(), index, data->GetCompilationConfig());
        return true;
    }
private:
    std::unique_ptr<CodeGeneratorImpl> llvmImpl_{nullptr};
};

void StubAotCompiler::BuildStubModuleAndSave(const std::string &triple, panda::ecmascript::StubModule *module,
                                             const std::string &filename)
{
    LLVMStubModule stubModule("fast_stubs", triple);
    std::vector<int> stubSet = GetStubIndices();
    stubModule.Initialize(stubSet);
    for (int i = 0; i < ALL_STUB_MAXCOUNT; i++) {
        auto stub = stubs_[i];
        if (stub != nullptr) {
            PassPayLoad data(stub, &stubModule);
            PassRunner pipeline(&data);
            pipeline.RunPass<BuildCircuitPass>();
            pipeline.RunPass<VerifierPass>();
            pipeline.RunPass<SchedulerPass>();
            pipeline.RunPass<LLVMCodegenPass>(i);
        }
    }

    LLVMModuleAssembler assembler(&stubModule);
    assembler.AssembleModule();
    assembler.AssembleStubModule(module);

    auto codeSize = assembler.GetCodeSize();
    panda::ecmascript::MachineCode *code = reinterpret_cast<panda::ecmascript::MachineCode *>(
        new uint64_t[(panda::ecmascript::MachineCode::SIZE + codeSize) / sizeof(uint64_t) + 1]);
    code->SetInstructionSizeInBytes(nullptr, panda::ecmascript::JSTaggedValue(codeSize),
                                    panda::ecmascript::SKIP_BARRIER);

    assembler.CopyAssemblerToCode(code);

    module->SetCode(code);
    module->Save(filename);

    delete[] code;
}
}  // namespace panda::ecmascript::kungfu


int main(const int argc, const char **argv)
{
    panda::Span<const char *> sp(argv, argc);
    panda::Stub_Aot_Options stubOptions(sp[0]);
    panda::PandArg<bool> help("help", false, "Print this message and exit");
    panda::PandArg<bool> options("options", false, "Print compiler options");
    panda::PandArgParser paParser;

    stubOptions.AddOptions(&paParser);
    paParser.Add(&help);
    paParser.Add(&options);

    if (!paParser.Parse(argc, argv) || help.GetValue()) {
        std::cerr << paParser.GetErrorString() << std::endl;
        std::cerr << "Usage: " << "ark_stub_opt" << " [OPTIONS]" << std::endl;
        std::cerr << std::endl;
        std::cerr << "optional arguments:" << std::endl;

        std::cerr << paParser.GetHelpString() << std::endl;
        return 1;
    }

    std::string tripleString = stubOptions.GetTargetTriple();
    std::string moduleFilename = stubOptions.GetStubOutputFile();

    panda::ecmascript::kungfu::StubAotCompiler mouldeBuilder;
#define SET_STUB_TO_MODULE(name, counter) \
    panda::ecmascript::kungfu::Circuit name##Circuit; \
    panda::ecmascript::kungfu::name##Stub name##Stub(& name##Circuit); \
    mouldeBuilder.SetStub(STUB_ID(name), &name##Stub);
    FAST_RUNTIME_STUB_LIST(SET_STUB_TO_MODULE)
    INTERPRETER_STUB_LIST(SET_STUB_TO_MODULE)
#ifndef NDEBUG
    TEST_FUNC_LIST(SET_STUB_TO_MODULE)
#endif
#undef SET_STUB_TO_MODULE
    panda::ecmascript::StubModule stubModule;
    mouldeBuilder.BuildStubModuleAndSave(tripleString, &stubModule, moduleFilename);
    std::cout << "BuildStubModuleAndSave success" << std::endl;
    return 0;
}
