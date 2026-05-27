#include "ReflectionSample.h"

#include "ReflectionFunctionTest.h"

#include "MEFunction.h"
#include "MEFunctionFrame.h"
#include "Reflection.h"
#include "ReflectionFunctionNativeThunks.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Framework/Components/Component.h"

#include <memory>
#include <string_view>
#include <vector>

namespace minEngine
{
    namespace
    {
        using Reflection::MEClass;
        using Reflection::MEFunction;
        using Reflection::MEFunctionFlags;
        using Reflection::MEParamPassKind;
        using Reflection::MEParamRole;
        using Reflection::MEParamDescriptor;
        using Reflection::MEProperty;
        using Reflection::MEFunctionFrame;
        using Reflection::ReflectionSystem;

        bool RegisterAddFunction(ReflectionSystem& reflection, MEClass* sampleComponentClass)
        {
            if (sampleComponentClass->FindFunction("Add") != nullptr)
            {
                return true;
            }

            MEFunction* addFunction = reflection.CreateFunction("Add");
            addFunction->SetFlags(static_cast<MEFunctionFlags>(
                static_cast<uint32_t>(MEFunctionFlags::Native) | static_cast<uint32_t>(MEFunctionFlags::HasReturn)));

            MEProperty* firstOperand = reflection.CreateFunctionParamProperty<int32_t>("FirstOperand");
            MEProperty* secondOperand = reflection.CreateFunctionParamProperty<int32_t>("SecondOperand");
            MEProperty* returnValue = reflection.CreateFunctionParamProperty<int32_t>("ReturnValue");

            if (!addFunction->AddParameter(firstOperand, MEParamRole::In, MEParamPassKind::Value)
                || !addFunction->AddParameter(secondOperand, MEParamRole::In, MEParamPassKind::Value)
                || !addFunction->AddParameter(returnValue, MEParamRole::Return, MEParamPassKind::Value))
            {
                ME_CORE_ERROR("ReflectionFunctionTest: failed to add Add() parameters.");
                return false;
            }

            if (!reflection.RegisterFunction(sampleComponentClass, addFunction))
            {
                ME_CORE_ERROR("ReflectionFunctionTest: failed to register Add.");
                return false;
            }

            return true;
        }

        bool RegisterGetCounterFunction(ReflectionSystem& reflection, MEClass* sampleComponentClass)
        {
            if (sampleComponentClass->FindFunction("GetCounter") != nullptr)
            {
                return true;
            }

            MEFunction* getCounterFunction = reflection.CreateFunction("GetCounter");
            getCounterFunction->SetFlags(static_cast<MEFunctionFlags>(
                static_cast<uint32_t>(MEFunctionFlags::Native) | static_cast<uint32_t>(MEFunctionFlags::HasReturn)));

            MEProperty* returnValue = reflection.CreateFunctionParamProperty<int32_t>("ReturnValue");
            if (!getCounterFunction->AddParameter(returnValue, MEParamRole::Return, MEParamPassKind::Value))
            {
                ME_CORE_ERROR("ReflectionFunctionTest: failed to add GetCounter() return parameter.");
                return false;
            }

            if (!reflection.RegisterFunction(sampleComponentClass, getCounterFunction))
            {
                ME_CORE_ERROR("ReflectionFunctionTest: failed to register GetCounter.");
                return false;
            }

            return true;
        }

        bool RegisterResetCounterFunction(ReflectionSystem& reflection, MEClass* sampleComponentClass)
        {
            if (sampleComponentClass->FindFunction("ResetCounter") != nullptr)
            {
                return true;
            }

            MEFunction* resetCounterFunction = reflection.CreateFunction("ResetCounter");
            resetCounterFunction->SetFlags(MEFunctionFlags::Native);
            if (!reflection.RegisterFunction(sampleComponentClass, resetCounterFunction))
            {
                ME_CORE_ERROR("ReflectionFunctionTest: failed to register ResetCounter.");
                return false;
            }

            return true;
        }

        bool RegisterMixAlignFunction(ReflectionSystem& reflection, MEClass* sampleComponentClass)
        {
            if (sampleComponentClass->FindFunction("MixAlign") != nullptr)
            {
                return true;
            }

            MEFunction* mixAlignFunction = reflection.CreateFunction("MixAlign");
            mixAlignFunction->SetFlags(static_cast<MEFunctionFlags>(
                static_cast<uint32_t>(MEFunctionFlags::Native) | static_cast<uint32_t>(MEFunctionFlags::HasReturn)));

            MEProperty* inputFlag = reflection.CreateFunctionParamProperty<bool>("InputFlag");
            MEProperty* threshold = reflection.CreateFunctionParamProperty<double>("Threshold");
            MEProperty* returnFlag = reflection.CreateFunctionParamProperty<bool>("ReturnValue");

            if (!mixAlignFunction->AddParameter(inputFlag, MEParamRole::In, MEParamPassKind::Value)
                || !mixAlignFunction->AddParameter(threshold, MEParamRole::In, MEParamPassKind::Value)
                || !mixAlignFunction->AddParameter(returnFlag, MEParamRole::Return, MEParamPassKind::Value))
            {
                ME_CORE_ERROR("ReflectionFunctionTest: failed to add MixAlign() parameters.");
                return false;
            }

            if (!reflection.RegisterFunction(sampleComponentClass, mixAlignFunction))
            {
                ME_CORE_ERROR("ReflectionFunctionTest: failed to register MixAlign.");
                return false;
            }

            return true;
        }

        bool EnsureReflectionReadyWithFunctionFixtures()
        {
            ReflectionSystem& reflection = ReflectionSystem::Get();
            if (reflection.IsReady())
            {
                return true;
            }

            MEClass* sampleComponentClass =
                const_cast<MEClass*>(reflection.FindClass("minEngine::ReflectionSampleComponent"));
            if (!RegisterResetCounterFunction(reflection, sampleComponentClass)
                || !RegisterGetCounterFunction(reflection, sampleComponentClass)
                || !RegisterAddFunction(reflection, sampleComponentClass)
                || !RegisterMixAlignFunction(reflection, sampleComponentClass))
            {
                return false;
            }

            BindReflectionSampleComponentNativeThunks(sampleComponentClass);

            if (!reflection.FinalizeReflection())
            {
                for (const std::string& error : reflection.GetLastErrors())
                {
                    ME_CORE_ERROR("{}", error);
                }
                return false;
            }

            reflection.ClearErrors();
            return true;
        }

        bool TestA1_FindFunctionOnSampleComponent()
        {
            const ReflectionSystem& reflection = ReflectionSystem::Get();
            const MEClass* sampleComponentClass =
                reflection.FindClass("minEngine::ReflectionSampleComponent");
            if (sampleComponentClass == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest A1: ReflectionSampleComponent not found.");
                return false;
            }

            MEFunction* addFunction = sampleComponentClass->FindFunction("Add");
            if (addFunction == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest A1: Add function not found.");
                return false;
            }

            if (addFunction->GetOwnerClass() != sampleComponentClass)
            {
                ME_CORE_ERROR("ReflectionFunctionTest A1: Add owner class mismatch.");
                return false;
            }

            return true;
        }

        bool TestA2_FindFunctionOnWrongClass()
        {
            const ReflectionSystem& reflection = ReflectionSystem::Get();
            const MEClass* componentClass = reflection.FindClass("minEngine::Component");
            if (componentClass == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest A2: Component class not found.");
                return false;
            }

            if (componentClass->FindFunction("Add") != nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest A2: expected Add to be absent on Component.");
                return false;
            }

            return true;
        }

        bool TestA3_ParmsLayout()
        {
            const MEClass* sampleComponentClass =
                ReflectionSystem::Get().FindClass("minEngine::ReflectionSampleComponent");
            MEFunction* addFunction =
                sampleComponentClass != nullptr ? sampleComponentClass->FindFunction("Add") : nullptr;
            MEFunction* resetFunction =
                sampleComponentClass != nullptr ? sampleComponentClass->FindFunction("ResetCounter") : nullptr;

            if (addFunction == nullptr || resetFunction == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest A3: fixture functions missing.");
                return false;
            }

            if (addFunction->GetNumParms() != 3 || addFunction->GetParmsSize() != 12
                || addFunction->GetReturnValueOffset() != 8)
            {
                ME_CORE_ERROR(
                    "ReflectionFunctionTest A3: Add layout mismatch (numParms={}, parmsSize={}, returnOffset={}).",
                    addFunction->GetNumParms(),
                    addFunction->GetParmsSize(),
                    addFunction->GetReturnValueOffset());
                return false;
            }

            if (resetFunction->GetNumParms() != 0 || resetFunction->GetParmsSize() != 0
                || resetFunction->GetReturnValueOffset() != -1)
            {
                ME_CORE_ERROR("ReflectionFunctionTest A3: ResetCounter layout mismatch.");
                return false;
            }

            MEFunction* mixAlignFunction =
                sampleComponentClass != nullptr ? sampleComponentClass->FindFunction("MixAlign") : nullptr;
            if (mixAlignFunction == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest A3: MixAlign fixture function missing.");
                return false;
            }

            if (mixAlignFunction->GetNumParms() != 3 || mixAlignFunction->GetParmsSize() != 24
                || mixAlignFunction->GetReturnValueOffset() != 16)
            {
                ME_CORE_ERROR(
                    "ReflectionFunctionTest A3: MixAlign layout mismatch (numParms={}, parmsSize={}, returnOffset={}).",
                    mixAlignFunction->GetNumParms(),
                    mixAlignFunction->GetParmsSize(),
                    mixAlignFunction->GetReturnValueOffset());
                return false;
            }

            return true;
        }

        bool TestA4_ParamDescriptors()
        {
            const MEClass* sampleComponentClass =
                ReflectionSystem::Get().FindClass("minEngine::ReflectionSampleComponent");
            const MEFunction* addFunction =
                sampleComponentClass != nullptr ? sampleComponentClass->FindFunction("Add") : nullptr;
            if (addFunction == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest A4: Add function not found.");
                return false;
            }

            uint32_t expectedOffset = 0;
            uint32_t returnCount = 0;
            for (const MEParamDescriptor& param : addFunction->GetParams())
            {
                if (param.Property == nullptr)
                {
                    ME_CORE_ERROR("ReflectionFunctionTest A4: null parameter property.");
                    return false;
                }

                if (param.PassKind != MEParamPassKind::Value)
                {
                    ME_CORE_ERROR("ReflectionFunctionTest A4: expected Value pass kind.");
                    return false;
                }

                if (param.Offset != expectedOffset)
                {
                    ME_CORE_ERROR("ReflectionFunctionTest A4: non-monotonic offset at parameter '{}'.",
                                  param.Property->GetName());
                    return false;
                }

                if (param.Role == MEParamRole::Return)
                {
                    ++returnCount;
                    expectedOffset += static_cast<uint32_t>(param.Property->GetStorageSize());
                }
                else if (param.Role == MEParamRole::In)
                {
                    expectedOffset += static_cast<uint32_t>(param.Property->GetStorageSize());
                }
                else
                {
                    ME_CORE_ERROR("ReflectionFunctionTest A4: unexpected param role.");
                    return false;
                }
            }

            if (returnCount != 1)
            {
                ME_CORE_ERROR("ReflectionFunctionTest A4: expected exactly one return parameter.");
                return false;
            }

            return true;
        }

        bool TestA5_SingleReturnParam()
        {
            const MEClass* sampleComponentClass =
                ReflectionSystem::Get().FindClass("minEngine::ReflectionSampleComponent");
            const MEFunction* addFunction =
                sampleComponentClass != nullptr ? sampleComponentClass->FindFunction("Add") : nullptr;
            if (addFunction == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest A5: Add function not found.");
                return false;
            }

            const MEParamDescriptor* returnParam = addFunction->GetReturnParam();
            if (returnParam == nullptr || !returnParam->IsReturn())
            {
                ME_CORE_ERROR("ReflectionFunctionTest A5: return parameter missing.");
                return false;
            }

            uint32_t returnCount = 0;
            for (const MEParamDescriptor& param : addFunction->GetParams())
            {
                if (param.IsReturn())
                {
                    ++returnCount;
                }
            }

            if (returnCount != 1 || !addFunction->HasReturn())
            {
                ME_CORE_ERROR("ReflectionFunctionTest A5: invalid return metadata.");
                return false;
            }

            return true;
        }

        bool TestA6_DuplicateFunctionRegistrationRejected()
        {
            ReflectionSystem& reflection = ReflectionSystem::Get();
            if (reflection.IsReady())
            {
                ME_CORE_ERROR("ReflectionFunctionTest A6: expected Collecting state before finalize.");
                return false;
            }

            MEClass* sampleComponentClass =
                const_cast<MEClass*>(reflection.FindClass("minEngine::ReflectionSampleComponent"));
            if (sampleComponentClass == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest A6: ReflectionSampleComponent not found.");
                return false;
            }

            if (!RegisterAddFunction(reflection, sampleComponentClass))
            {
                ME_CORE_ERROR("ReflectionFunctionTest A6: failed to register baseline Add function.");
                return false;
            }

            MEFunction* duplicateAddFunction = reflection.CreateFunction("Add");
            MEProperty* operandA = reflection.CreateFunctionParamProperty<int32_t>("OperandA");
            MEProperty* operandB = reflection.CreateFunctionParamProperty<int32_t>("OperandB");
            MEProperty* duplicateReturn = reflection.CreateFunctionParamProperty<int32_t>("ReturnValue");

            duplicateAddFunction->AddParameter(operandA, MEParamRole::In, MEParamPassKind::Value);
            duplicateAddFunction->AddParameter(operandB, MEParamRole::In, MEParamPassKind::Value);
            duplicateAddFunction->AddParameter(duplicateReturn, MEParamRole::Return, MEParamPassKind::Value);

            reflection.ClearErrors();
            if (reflection.RegisterFunction(sampleComponentClass, duplicateAddFunction))
            {
                ME_CORE_ERROR("ReflectionFunctionTest A6: duplicate RegisterFunction should fail.");
                return false;
            }

            if (reflection.GetLastErrors().empty())
            {
                ME_CORE_ERROR("ReflectionFunctionTest A6: expected reflection error for duplicate function.");
                return false;
            }

            return true;
        }

        bool RunMetaPhaseTests()
        {
            if (!TestA6_DuplicateFunctionRegistrationRejected())
            {
                return false;
            }

            if (!EnsureReflectionReadyWithFunctionFixtures())
            {
                ME_CORE_ERROR("ReflectionFunctionTest: reflection init failed.");
                return false;
            }

            if (!TestA1_FindFunctionOnSampleComponent())
            {
                return false;
            }

            if (!TestA2_FindFunctionOnWrongClass())
            {
                return false;
            }

            if (!TestA3_ParmsLayout())
            {
                return false;
            }

            if (!TestA4_ParamDescriptors())
            {
                return false;
            }

            if (!TestA5_SingleReturnParam())
            {
                return false;
            }

            return true;
        }

        ReflectionSampleComponent* CreateInvokeTestComponent()
        {
            const ReflectionSystem& reflection = ReflectionSystem::Get();
            const MEClass* sampleComponentClass =
                reflection.FindClass("minEngine::ReflectionSampleComponent");
            if (sampleComponentClass == nullptr)
            {
                return nullptr;
            }

            static std::shared_ptr<ReflectionSampleComponent> s_TestComponent;
            std::shared_ptr<void> instance = sampleComponentClass->CreateDefaultInstance();
            s_TestComponent = std::static_pointer_cast<ReflectionSampleComponent>(instance);
            return s_TestComponent.get();
        }

        bool TestB1_ResetCounter()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest B1: failed to create sample component.");
                return false;
            }

            component->SetFunctionTestCounter(7);
            MEFunction* resetFunction = component->GetClass()->FindFunction("ResetCounter");
            if (!component->InvokeFunction(resetFunction, nullptr))
            {
                ME_CORE_ERROR("ReflectionFunctionTest B1: InvokeFunction ResetCounter failed.");
                return false;
            }

            if (component->GetCounter() != 0)
            {
                ME_CORE_ERROR("ReflectionFunctionTest B1: counter expected 0, got {}.", component->GetCounter());
                return false;
            }

            return true;
        }

        bool TestB2_GetCounter()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                return false;
            }

            component->SetFunctionTestCounter(42);
            MEFunction* getCounterFunction = component->GetClass()->FindFunction("GetCounter");
            MEFunctionFrame frame(*getCounterFunction);
            if (!component->InvokeFunction(getCounterFunction, frame.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest B2: InvokeFunction GetCounter failed.");
                return false;
            }

            int32_t returnValue = 0;
            if (!frame.GetParam("ReturnValue", returnValue) || returnValue != 42)
            {
                ME_CORE_ERROR("ReflectionFunctionTest B2: return value mismatch (expected 42, got {}).", returnValue);
                return false;
            }

            return true;
        }

        bool TestB3_Add()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                return false;
            }

            MEFunction* addFunction = component->GetClass()->FindFunction("Add");
            MEFunctionFrame frame(*addFunction);
            frame.SetParam("FirstOperand", static_cast<int32_t>(2));
            frame.SetParam("SecondOperand", static_cast<int32_t>(3));

            if (!component->InvokeFunction(addFunction, frame.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest B3: InvokeFunction Add failed.");
                return false;
            }

            int32_t returnValue = 0;
            if (!frame.GetParam("ReturnValue", returnValue) || returnValue != 5)
            {
                ME_CORE_ERROR("ReflectionFunctionTest B3: Add return mismatch (expected 5, got {}).", returnValue);
                return false;
            }

            return true;
        }

        bool TestB4_FrameMatchesRawBufferInvoke()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                return false;
            }

            MEFunction* addFunction = component->GetClass()->FindFunction("Add");
            MEFunctionFrame frame(*addFunction);
            frame.SetParam("FirstOperand", static_cast<int32_t>(10));
            frame.SetParam("SecondOperand", static_cast<int32_t>(20));

            std::vector<uint8_t> rawBuffer(addFunction->GetParmsSize(), 0);
            const int32_t ten = 10;
            const int32_t twenty = 20;
            addFunction->CopyParamToBuffer(rawBuffer.data(), "FirstOperand", &ten, sizeof(ten));
            addFunction->CopyParamToBuffer(rawBuffer.data(), "SecondOperand", &twenty, sizeof(twenty));

            if (!component->InvokeFunction(addFunction, frame.GetBuffer()))
            {
                return false;
            }

            int32_t frameResult = 0;
            frame.GetParam("ReturnValue", frameResult);

            if (!component->InvokeFunction(addFunction, rawBuffer.data()))
            {
                return false;
            }

            int32_t rawResult = 0;
            addFunction->CopyParamFromBuffer(rawBuffer.data(), "ReturnValue", &rawResult, sizeof(rawResult));

            if (frameResult != 30 || rawResult != 30)
            {
                ME_CORE_ERROR("ReflectionFunctionTest B4: frame/raw mismatch ({} vs {}).", frameResult, rawResult);
                return false;
            }

            return true;
        }

        bool TestB5_InvokeFailurePaths()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                return false;
            }

            MEFunction* addFunction = component->GetClass()->FindFunction("Add");
            if (component->InvokeFunction(nullptr, nullptr))
            {
                ME_CORE_ERROR("ReflectionFunctionTest B5: null function should fail.");
                return false;
            }

            if (component->InvokeFunction(addFunction, nullptr))
            {
                ME_CORE_ERROR("ReflectionFunctionTest B5: null buffer should fail for Add.");
                return false;
            }

            const MEClass* componentClass = ReflectionSystem::Get().FindClass("minEngine::Component");
            static std::shared_ptr<Component> s_BaseComponent;
            s_BaseComponent = std::static_pointer_cast<Component>(componentClass->CreateDefaultInstance());
            MEFunctionFrame frame(*addFunction);
            if (s_BaseComponent->InvokeFunction(addFunction, frame.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest B5: IsA mismatch should fail.");
                return false;
            }

            return true;
        }

        bool RunInvokePhaseTests()
        {
            if (!EnsureReflectionReadyWithFunctionFixtures())
            {
                ME_CORE_ERROR("ReflectionFunctionTest: reflection init failed for invoke.");
                return false;
            }

            if (!TestB1_ResetCounter())
            {
                return false;
            }

            if (!TestB2_GetCounter())
            {
                return false;
            }

            if (!TestB3_Add())
            {
                return false;
            }

            if (!TestB4_FrameMatchesRawBufferInvoke())
            {
                return false;
            }

            if (!TestB5_InvokeFailurePaths())
            {
                return false;
            }

            return true;
        }
    }

    bool ShouldRunReflectionFunctionTestsOnly(int argc, char** argv)
    {
        for (int argIndex = 1; argIndex < argc; ++argIndex)
        {
            if (argv[argIndex] == nullptr)
            {
                continue;
            }

            const std::string_view argument(argv[argIndex]);
            if (argument == "--reflection-function-test"
                || argument.rfind("--reflection-function-test=", 0) == 0)
            {
                return true;
            }
        }
        return false;
    }

    namespace
    {
        bool ParseTestSuiteArgument(std::string_view argument, bool& runMeta, bool& runInvoke)
        {
            if (argument == "--reflection-function-test")
            {
                runMeta = true;
                runInvoke = true;
                return true;
            }

            constexpr std::string_view prefix = "--reflection-function-test=";
            if (argument.rfind(prefix, 0) != 0)
            {
                return false;
            }

            const std::string_view suiteList = argument.substr(prefix.size());
            size_t start = 0;
            while (start < suiteList.size())
            {
                const size_t comma = suiteList.find(',', start);
                const size_t end = comma == std::string_view::npos ? suiteList.size() : comma;
                const std::string_view token = suiteList.substr(start, end - start);
                if (token == "meta")
                {
                    runMeta = true;
                }
                else if (token == "invoke")
                {
                    runInvoke = true;
                }
                start = (comma == std::string_view::npos) ? suiteList.size() : comma + 1;
            }
            return runMeta || runInvoke;
        }
    }

    bool RunReflectionFunctionTests(int argc, char** argv)
    {
        bool runMeta = false;
        bool runInvoke = false;
        for (int argIndex = 1; argIndex < argc; ++argIndex)
        {
            if (argv[argIndex] == nullptr)
            {
                continue;
            }

            ParseTestSuiteArgument(std::string_view(argv[argIndex]), runMeta, runInvoke);
        }

        if (!runMeta && !runInvoke)
        {
            runMeta = true;
            runInvoke = true;
        }

        bool passed = true;
        if (runMeta)
        {
            passed = RunMetaPhaseTests();
        }

        if (passed && runInvoke)
        {
            passed = RunInvokePhaseTests();
        }

        if (passed)
        {
            if (runMeta && runInvoke)
            {
                ME_CORE_INFO("ReflectionFunctionTest: PASSED (meta, invoke)");
            }
            else if (runMeta)
            {
                ME_CORE_INFO("ReflectionFunctionTest: PASSED (meta)");
            }
            else
            {
                ME_CORE_INFO("ReflectionFunctionTest: PASSED (invoke)");
            }
        }
        else
        {
            ME_CORE_ERROR("ReflectionFunctionTest: FAILED");
        }

        return passed;
    }
}
