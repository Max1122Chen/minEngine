#include "ReflectionSample.h"

#include "ReflectionFunctionTest.h"

#include "MEFunction.h"
#include "MEFunctionFrame.h"
#include "Reflection.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Framework/Components/Component.h"

#include <memory>
#include <cmath>
#include <string_view>
#include <vector>

namespace minEngine
{
    namespace
    {
        using Reflection::MEClass;
        using Reflection::MEFunction;
        using Reflection::MEParamPassKind;
        using Reflection::MEParamRole;
        using Reflection::MEParamDescriptor;
        using Reflection::MEProperty;
        using Reflection::MEFunctionFrame;
        using Reflection::ReflectionSystem;

        bool EnsureReflectionReadyWithFunctionFixtures()
        {
            ReflectionSystem& reflection = ReflectionSystem::Get();
            if (reflection.IsReady())
            {
                return true;
            }

            MEClass* sampleComponentClass =
                const_cast<MEClass*>(reflection.FindClass("minEngine::ReflectionSampleComponent"));
            if (sampleComponentClass == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest: ReflectionSampleComponent class not found.");
                return false;
            }

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

            if (sampleComponentClass->FindFunction("Add") == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest A6: baseline Add function is missing.");
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

        bool TestC1_AddInPlace()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                return false;
            }

            MEFunction* function = component->GetClass()->FindFunction("AddInPlace");
            if (function == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest C1: AddInPlace not found.");
                return false;
            }

            int32_t value = 10;
            MEFunctionFrame frame(*function);
            frame.SetParamRef("Value", value);
            frame.SetParam("Delta", static_cast<int32_t>(5));

            if (!component->InvokeFunction(function, frame.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest C1: InvokeFunction AddInPlace failed.");
                return false;
            }

            if (value != 15)
            {
                ME_CORE_ERROR("ReflectionFunctionTest C1: expected value 15, got {}.", value);
                return false;
            }

            return true;
        }

        bool TestC2_PeekString()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                return false;
            }

            MEFunction* function = component->GetClass()->FindFunction("PeekString");
            if (function == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest C2: PeekString not found.");
                return false;
            }

            const std::string input = "hello";
            int32_t outLength = -1;
            MEFunctionFrame frame(*function);
            frame.SetParamConstRef("Input", input);
            frame.SetOutParam("OutLength", outLength);

            if (!component->InvokeFunction(function, frame.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest C2: InvokeFunction PeekString failed.");
                return false;
            }

            if (outLength != 5)
            {
                ME_CORE_ERROR("ReflectionFunctionTest C2: expected outLength 5, got {}.", outLength);
                return false;
            }

            return true;
        }

        bool TestC3_FillOut()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                return false;
            }

            MEFunction* function = component->GetClass()->FindFunction("FillOut");
            if (function == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest C3: FillOut not found.");
                return false;
            }

            int32_t outValue = -1;
            MEFunctionFrame frame(*function);
            frame.SetParam("Value", static_cast<int32_t>(123));
            frame.SetOutParam("OutValue", outValue);

            if (!component->InvokeFunction(function, frame.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest C3: InvokeFunction FillOut failed.");
                return false;
            }

            if (outValue != 123)
            {
                ME_CORE_ERROR("ReflectionFunctionTest C3: expected outValue 123, got {}.", outValue);
                return false;
            }

            return true;
        }

        bool RunRefPhaseTests()
        {
            if (!EnsureReflectionReadyWithFunctionFixtures())
            {
                ME_CORE_ERROR("ReflectionFunctionTest: reflection init failed for ref.");
                return false;
            }

            if (!TestC1_AddInPlace())
            {
                return false;
            }

            if (!TestC2_PeekString())
            {
                return false;
            }

            if (!TestC3_FillOut())
            {
                return false;
            }

            return true;
        }

        bool TestD1_EchoEnum()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                return false;
            }

            MEFunction* function = component->GetClass()->FindFunction("EchoEnum");
            if (function == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest D1: EchoEnum not found.");
                return false;
            }

            MEFunctionFrame frame(*function);
            const ReflectionSampleEnum input = ReflectionSampleEnum::ValueC;
            frame.SetParam("Value", input);

            if (!component->InvokeFunction(function, frame.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest D1: InvokeFunction EchoEnum failed.");
                return false;
            }

            ReflectionSampleEnum outValue = ReflectionSampleEnum::ValueA;
            if (!frame.GetParam("ReturnValue", outValue) || outValue != ReflectionSampleEnum::ValueC)
            {
                ME_CORE_ERROR("ReflectionFunctionTest D1: enum return mismatch.");
                return false;
            }

            return true;
        }

        bool TestD2_SumIntArray()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                return false;
            }

            MEFunction* function = component->GetClass()->FindFunction("SumIntArray");
            if (function == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest D2: SumIntArray not found.");
                return false;
            }

            const std::vector<int> values{ 1, 2, 3, 4 };
            MEFunctionFrame frame(*function);
            frame.SetParamConstRef("Values", values);

            if (!component->InvokeFunction(function, frame.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest D2: InvokeFunction SumIntArray failed.");
                return false;
            }

            int32_t returnValue = 0;
            if (!frame.GetParam("ReturnValue", returnValue) || returnValue != 4)
            {
                ME_CORE_ERROR("ReflectionFunctionTest D2: expected return 4, got {}.", returnValue);
                return false;
            }

            return true;
        }

        bool TestD3_IsSameObject()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                return false;
            }

            MEFunction* function = component->GetClass()->FindFunction("IsSameObject");
            if (function == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest D3: IsSameObject not found.");
                return false;
            }

            MEFunctionFrame frame(*function);
            MEObject* a = component;
            MEObject* b = component;
            frame.SetParam("A", a);
            frame.SetParam("B", b);

            if (!component->InvokeFunction(function, frame.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest D3: InvokeFunction IsSameObject failed.");
                return false;
            }

            bool returnValue = false;
            if (!frame.GetParam("ReturnValue", returnValue) || returnValue != true)
            {
                ME_CORE_ERROR("ReflectionFunctionTest D3: expected true, got {}.", returnValue);
                return false;
            }

            return true;
        }

        bool IsNearFloat(float lhs, float rhs, float epsilon = 0.0001f)
        {
            return std::abs(lhs - rhs) <= epsilon;
        }

        bool IsNearVector2(const Vector2& lhs, const Vector2& rhs, float epsilon = 0.0001f)
        {
            return IsNearFloat(lhs.x, rhs.x, epsilon) && IsNearFloat(lhs.y, rhs.y, epsilon);
        }

        bool IsNearVector3(const Vector3& lhs, const Vector3& rhs, float epsilon = 0.0001f)
        {
            return IsNearFloat(lhs.x, rhs.x, epsilon) && IsNearFloat(lhs.y, rhs.y, epsilon)
                   && IsNearFloat(lhs.z, rhs.z, epsilon);
        }

        bool IsNearVector4(const Vector4& lhs, const Vector4& rhs, float epsilon = 0.0001f)
        {
            return IsNearFloat(lhs.x, rhs.x, epsilon) && IsNearFloat(lhs.y, rhs.y, epsilon)
                   && IsNearFloat(lhs.z, rhs.z, epsilon) && IsNearFloat(lhs.w, rhs.w, epsilon);
        }

        bool TestD4_AddVector2()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                return false;
            }

            MEFunction* function = component->GetClass()->FindFunction("AddVector2");
            if (function == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest D4: AddVector2 not found.");
                return false;
            }

            MEFunctionFrame frame(*function);
            frame.SetParam("Lhs", Vector2(1.0f, -2.0f));
            frame.SetParam("Rhs", Vector2(0.25f, 3.0f));

            if (!component->InvokeFunction(function, frame.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest D4: InvokeFunction AddVector2 failed.");
                return false;
            }

            Vector2 returnValue{};
            const Vector2 expected(1.25f, 1.0f);
            if (!frame.GetParam("ReturnValue", returnValue) || !IsNearVector2(returnValue, expected))
            {
                ME_CORE_ERROR("ReflectionFunctionTest D4: vector2 return mismatch.");
                return false;
            }

            return true;
        }

        bool TestD5_ScaleVector2InPlace()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                return false;
            }

            MEFunction* function = component->GetClass()->FindFunction("ScaleVector2InPlace");
            if (function == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest D5: ScaleVector2InPlace not found.");
                return false;
            }

            Vector2 value(2.0f, -4.0f);
            MEFunctionFrame frame(*function);
            frame.SetParamRef("Value", value);
            frame.SetParam("Scale", 0.5f);

            if (!component->InvokeFunction(function, frame.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest D5: InvokeFunction ScaleVector2InPlace failed.");
                return false;
            }

            const Vector2 expected(1.0f, -2.0f);
            if (!IsNearVector2(value, expected))
            {
                ME_CORE_ERROR("ReflectionFunctionTest D5: vector2 ref mismatch.");
                return false;
            }

            return true;
        }

        bool TestD6_AddVector3()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                return false;
            }

            MEFunction* function = component->GetClass()->FindFunction("AddVector3");
            if (function == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest D6: AddVector3 not found.");
                return false;
            }

            MEFunctionFrame frame(*function);
            frame.SetParam("Lhs", Vector3(1.0f, 2.0f, 3.0f));
            frame.SetParam("Rhs", Vector3(0.5f, -1.0f, 4.0f));

            if (!component->InvokeFunction(function, frame.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest D6: InvokeFunction AddVector3 failed.");
                return false;
            }

            Vector3 returnValue{};
            const Vector3 expected(1.5f, 1.0f, 7.0f);
            if (!frame.GetParam("ReturnValue", returnValue) || !IsNearVector3(returnValue, expected))
            {
                ME_CORE_ERROR("ReflectionFunctionTest D6: vector3 return mismatch.");
                return false;
            }

            return true;
        }

        bool TestD7_ScaleVector3InPlace()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                return false;
            }

            MEFunction* function = component->GetClass()->FindFunction("ScaleVector3InPlace");
            if (function == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest D7: ScaleVector3InPlace not found.");
                return false;
            }

            Vector3 value(2.0f, -3.0f, 0.5f);
            MEFunctionFrame frame(*function);
            frame.SetParamRef("Value", value);
            frame.SetParam("Scale", 2.5f);

            if (!component->InvokeFunction(function, frame.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest D7: InvokeFunction ScaleVector3InPlace failed.");
                return false;
            }

            const Vector3 expected(5.0f, -7.5f, 1.25f);
            if (!IsNearVector3(value, expected))
            {
                ME_CORE_ERROR("ReflectionFunctionTest D7: vector3 ref mismatch.");
                return false;
            }

            return true;
        }

        bool TestD8_AddVector4()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                return false;
            }

            MEFunction* function = component->GetClass()->FindFunction("AddVector4");
            if (function == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest D8: AddVector4 not found.");
                return false;
            }

            MEFunctionFrame frame(*function);
            frame.SetParam("Lhs", Vector4(1.0f, 2.0f, 3.0f, 4.0f));
            frame.SetParam("Rhs", Vector4(0.5f, -1.0f, 2.0f, -3.0f));

            if (!component->InvokeFunction(function, frame.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest D8: InvokeFunction AddVector4 failed.");
                return false;
            }

            Vector4 returnValue{};
            const Vector4 expected(1.5f, 1.0f, 5.0f, 1.0f);
            if (!frame.GetParam("ReturnValue", returnValue) || !IsNearVector4(returnValue, expected))
            {
                ME_CORE_ERROR("ReflectionFunctionTest D8: vector4 return mismatch.");
                return false;
            }

            return true;
        }

        bool TestD9_ScaleVector4InPlace()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                return false;
            }

            MEFunction* function = component->GetClass()->FindFunction("ScaleVector4InPlace");
            if (function == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest D9: ScaleVector4InPlace not found.");
                return false;
            }

            Vector4 value(2.0f, -1.0f, 0.5f, 4.0f);
            MEFunctionFrame frame(*function);
            frame.SetParamRef("Value", value);
            frame.SetParam("Scale", 2.0f);

            if (!component->InvokeFunction(function, frame.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest D9: InvokeFunction ScaleVector4InPlace failed.");
                return false;
            }

            const Vector4 expected(4.0f, -2.0f, 1.0f, 8.0f);
            if (!IsNearVector4(value, expected))
            {
                ME_CORE_ERROR("ReflectionFunctionTest D9: vector4 ref mismatch.");
                return false;
            }

            return true;
        }

        bool TestD10_PrefixString()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                return false;
            }

            MEFunction* function = component->GetClass()->FindFunction("PrefixString");
            if (function == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest D10: PrefixString not found.");
                return false;
            }

            const std::string prefix = "me:";
            std::string value = "hello";
            MEFunctionFrame frame(*function);
            frame.SetParamConstRef("Prefix", prefix);
            frame.SetParamRef("InOutValue", value);

            if (!component->InvokeFunction(function, frame.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest D10: InvokeFunction PrefixString failed.");
                return false;
            }

            if (value != "me:hello")
            {
                ME_CORE_ERROR("ReflectionFunctionTest D10: expected 'me:hello', got '{}'.", value);
                return false;
            }

            return true;
        }

        bool TestD11_IsValidComponentPtr()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                return false;
            }

            MEFunction* function = component->GetClass()->FindFunction("IsValidComponentPtr");
            if (function == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest D11: IsValidComponentPtr not found.");
                return false;
            }

            MEFunctionFrame frameValid(*function);
            Component* validPtr = component;
            frameValid.SetParam("Candidate", validPtr);
            if (!component->InvokeFunction(function, frameValid.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest D11: InvokeFunction IsValidComponentPtr(valid) failed.");
                return false;
            }

            bool validResult = false;
            if (!frameValid.GetParam("ReturnValue", validResult) || !validResult)
            {
                ME_CORE_ERROR("ReflectionFunctionTest D11: expected true for valid pointer.");
                return false;
            }

            MEFunctionFrame frameNull(*function);
            Component* nullPtr = nullptr;
            frameNull.SetParam("Candidate", nullPtr);
            if (!component->InvokeFunction(function, frameNull.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest D11: InvokeFunction IsValidComponentPtr(null) failed.");
                return false;
            }

            bool nullResult = true;
            if (!frameNull.GetParam("ReturnValue", nullResult) || nullResult)
            {
                ME_CORE_ERROR("ReflectionFunctionTest D11: expected false for null pointer.");
                return false;
            }

            return true;
        }

        bool TestE1_StaticResetCounter()
        {
            const MEClass* sampleComponentClass =
                ReflectionSystem::Get().FindClass("minEngine::ReflectionSampleComponent");
            if (sampleComponentClass == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest E1: ReflectionSampleComponent not found.");
                return false;
            }

            ReflectionSampleComponent::SetStaticTestCounter(9);
            MEFunction* resetFunction = sampleComponentClass->FindFunction("StaticResetCounter");
            if (resetFunction == nullptr || !resetFunction->IsStatic())
            {
                ME_CORE_ERROR("ReflectionFunctionTest E1: StaticResetCounter missing or not static.");
                return false;
            }

            if (!sampleComponentClass->InvokeStaticFunction(resetFunction, nullptr))
            {
                ME_CORE_ERROR("ReflectionFunctionTest E1: InvokeStaticFunction StaticResetCounter failed.");
                return false;
            }

            if (ReflectionSampleComponent::StaticGetCounter() != 0)
            {
                ME_CORE_ERROR("ReflectionFunctionTest E1: static counter expected 0, got {}.",
                              ReflectionSampleComponent::StaticGetCounter());
                return false;
            }

            return true;
        }

        bool TestE2_StaticAdd()
        {
            const MEClass* sampleComponentClass =
                ReflectionSystem::Get().FindClass("minEngine::ReflectionSampleComponent");
            if (sampleComponentClass == nullptr)
            {
                return false;
            }

            MEFunction* staticAddFunction = sampleComponentClass->FindFunction("StaticAdd");
            if (staticAddFunction == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest E2: StaticAdd not found.");
                return false;
            }

            MEFunctionFrame frame(*staticAddFunction);
            frame.SetParam("FirstOperand", static_cast<int32_t>(4));
            frame.SetParam("SecondOperand", static_cast<int32_t>(6));

            if (!sampleComponentClass->InvokeStaticFunction(staticAddFunction, frame.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest E2: InvokeStaticFunction StaticAdd failed.");
                return false;
            }

            int32_t returnValue = 0;
            if (!frame.GetParam("ReturnValue", returnValue) || returnValue != 10)
            {
                ME_CORE_ERROR("ReflectionFunctionTest E2: return value mismatch (expected 10, got {}).", returnValue);
                return false;
            }

            return true;
        }

        bool TestE3_StaticAddInPlace()
        {
            const MEClass* sampleComponentClass =
                ReflectionSystem::Get().FindClass("minEngine::ReflectionSampleComponent");
            if (sampleComponentClass == nullptr)
            {
                return false;
            }

            MEFunction* function = sampleComponentClass->FindFunction("StaticAddInPlace");
            if (function == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest E3: StaticAddInPlace not found.");
                return false;
            }

            int32_t value = 5;
            MEFunctionFrame frame(*function);
            frame.SetParamRef("Value", value);
            frame.SetParam("Delta", static_cast<int32_t>(7));

            if (!sampleComponentClass->InvokeStaticFunction(function, frame.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest E3: InvokeStaticFunction StaticAddInPlace failed.");
                return false;
            }

            if (value != 12)
            {
                ME_CORE_ERROR("ReflectionFunctionTest E3: ref value expected 12, got {}.", value);
                return false;
            }

            return true;
        }

        bool TestE4_StaticInvokeViaObject()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                ME_CORE_ERROR("ReflectionFunctionTest E4: failed to create sample component.");
                return false;
            }

            ReflectionSampleComponent::SetStaticTestCounter(3);
            MEFunction* getCounterFunction = component->GetClass()->FindFunction("StaticGetCounter");
            MEFunctionFrame frame(*getCounterFunction);
            if (!component->InvokeFunction(getCounterFunction, frame.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest E4: InvokeFunction StaticGetCounter failed.");
                return false;
            }

            int32_t returnValue = 0;
            if (!frame.GetParam("ReturnValue", returnValue) || returnValue != 3)
            {
                ME_CORE_ERROR("ReflectionFunctionTest E4: return value mismatch (expected 3, got {}).", returnValue);
                return false;
            }

            return true;
        }

        bool TestE5_StaticInvokeByName()
        {
            ReflectionSampleComponent* component = CreateInvokeTestComponent();
            if (component == nullptr)
            {
                return false;
            }

            ReflectionSampleComponent::SetStaticTestCounter(11);
            MEFunctionFrame frame(*component->GetClass()->FindFunction("StaticGetCounter"));
            if (!component->InvokeFunctionByName("StaticGetCounter", frame.GetBuffer()))
            {
                ME_CORE_ERROR("ReflectionFunctionTest E5: InvokeFunctionByName StaticGetCounter failed.");
                return false;
            }

            int32_t returnValue = 0;
            if (!frame.GetParam("ReturnValue", returnValue) || returnValue != 11)
            {
                ME_CORE_ERROR("ReflectionFunctionTest E5: return value mismatch (expected 11, got {}).", returnValue);
                return false;
            }

            return true;
        }

        bool RunStaticPhaseTests()
        {
            if (!EnsureReflectionReadyWithFunctionFixtures())
            {
                ME_CORE_ERROR("ReflectionFunctionTest: reflection init failed for static.");
                return false;
            }

            if (!TestE1_StaticResetCounter())
            {
                return false;
            }

            if (!TestE2_StaticAdd())
            {
                return false;
            }

            if (!TestE3_StaticAddInPlace())
            {
                return false;
            }

            if (!TestE4_StaticInvokeViaObject())
            {
                return false;
            }

            if (!TestE5_StaticInvokeByName())
            {
                return false;
            }

            return true;
        }

        bool RunTypesPhaseTests()
        {
            if (!EnsureReflectionReadyWithFunctionFixtures())
            {
                ME_CORE_ERROR("ReflectionFunctionTest: reflection init failed for types.");
                return false;
            }

            // D-base: enum, container, object pointer smoke.
            if (!TestD1_EchoEnum())
            {
                return false;
            }

            if (!TestD2_SumIntArray())
            {
                return false;
            }

            if (!TestD3_IsSameObject())
            {
                return false;
            }

            // D-trivial: Math vectors (trivially copyable value/ref).
            if (!TestD4_AddVector2())
            {
                return false;
            }

            if (!TestD5_ScaleVector2InPlace())
            {
                return false;
            }

            if (!TestD6_AddVector3())
            {
                return false;
            }

            if (!TestD7_ScaleVector3InPlace())
            {
                return false;
            }

            if (!TestD8_AddVector4())
            {
                return false;
            }

            if (!TestD9_ScaleVector4InPlace())
            {
                return false;
            }

            // D-nontrivial: std::string via const-ref + ref (no value semantics).
            if (!TestD10_PrefixString())
            {
                return false;
            }

            // D-objectptr: non-owning pointer null/valid paths.
            if (!TestD11_IsValidComponentPtr())
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
        std::string_view TrimToken(std::string_view token)
        {
            while (!token.empty() && (token.front() == ' ' || token.front() == '\t' || token.front() == '\r'
                                      || token.front() == '\n'))
            {
                token.remove_prefix(1);
            }
            while (!token.empty() && (token.back() == ' ' || token.back() == '\t' || token.back() == '\r'
                                      || token.back() == '\n'))
            {
                token.remove_suffix(1);
            }
            return token;
        }

        bool ParseTestSuiteArgument(std::string_view argument, bool& runMeta, bool& runInvoke, bool& runRef,
                                    bool& runTypes, bool& runStatic)
        {
            if (argument == "--reflection-function-test")
            {
                runMeta = true;
                runInvoke = true;
                runRef = true;
                runTypes = true;
                runStatic = true;
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
                const std::string_view token = TrimToken(suiteList.substr(start, end - start));
                if (token == "meta")
                {
                    runMeta = true;
                }
                else if (token == "invoke")
                {
                    runInvoke = true;
                }
                else if (token == "ref")
                {
                    runRef = true;
                }
                else if (token == "types")
                {
                    runTypes = true;
                }
                else if (token == "static")
                {
                    runStatic = true;
                }
                start = (comma == std::string_view::npos) ? suiteList.size() : comma + 1;
            }
            return runMeta || runInvoke || runRef || runTypes || runStatic;
        }
    }

    bool RunReflectionFunctionTests(int argc, char** argv)
    {
        bool runMeta = false;
        bool runInvoke = false;
        bool runRef = false;
        bool runTypes = false;
        bool runStatic = false;
        for (int argIndex = 1; argIndex < argc; ++argIndex)
        {
            if (argv[argIndex] == nullptr)
            {
                continue;
            }

            ParseTestSuiteArgument(std::string_view(argv[argIndex]), runMeta, runInvoke, runRef, runTypes, runStatic);
        }

        if (!runMeta && !runInvoke && !runRef && !runTypes && !runStatic)
        {
            runMeta = true;
            runInvoke = true;
            runRef = true;
            runTypes = true;
            runStatic = true;
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

        if (passed && runRef)
        {
            passed = RunRefPhaseTests();
        }

        if (passed && runTypes)
        {
            passed = RunTypesPhaseTests();
        }

        if (passed && runStatic)
        {
            passed = RunStaticPhaseTests();
        }

        if (passed)
        {
            if (runMeta && runInvoke && runRef && runTypes && runStatic)
            {
                ME_CORE_INFO("ReflectionFunctionTest: PASSED (meta, invoke, ref, types, static)");
            }
            else if (runMeta && runInvoke && runRef && runTypes)
            {
                ME_CORE_INFO("ReflectionFunctionTest: PASSED (meta, invoke, ref, types)");
            }
            else if (runMeta && runInvoke && runRef)
            {
                ME_CORE_INFO("ReflectionFunctionTest: PASSED (meta, invoke, ref)");
            }
            else if (runMeta && runInvoke)
            {
                ME_CORE_INFO("ReflectionFunctionTest: PASSED (meta, invoke)");
            }
            else if (runMeta)
            {
                ME_CORE_INFO("ReflectionFunctionTest: PASSED (meta)");
            }
            else if (runInvoke)
            {
                ME_CORE_INFO("ReflectionFunctionTest: PASSED (invoke)");
            }
            else if (runRef)
            {
                ME_CORE_INFO("ReflectionFunctionTest: PASSED (ref)");
            }
            else if (runTypes)
            {
                ME_CORE_INFO("ReflectionFunctionTest: PASSED (types)");
            }
            else
            {
                ME_CORE_INFO("ReflectionFunctionTest: PASSED (static)");
            }
        }
        else
        {
            ME_CORE_ERROR("ReflectionFunctionTest: FAILED");
        }

        return passed;
    }
}
