#include "ReflectionFunctionNativeThunks.h"

#include "ReflectionSample.h"

namespace minEngine::Reflection
{
    namespace
    {
        void Invoke_ReflectionSampleComponent_ResetCounter(MEObject* context, void* /*parms*/)
        {
            auto* sampleComponent = static_cast<ReflectionSampleComponent*>(context);
            sampleComponent->ResetCounter();
        }

        void Invoke_ReflectionSampleComponent_GetCounter(MEObject* context, void* parms)
        {
            auto* sampleComponent = static_cast<ReflectionSampleComponent*>(context);
            const MEClass* ownerClass = sampleComponent->GetClass();
            if (ownerClass == nullptr)
            {
                return;
            }

            MEFunction* function = ownerClass->FindFunction("GetCounter");
            if (function == nullptr)
            {
                return;
            }

            const int32_t counter = sampleComponent->GetCounter();
            function->CopyParamToBuffer(parms, "ReturnValue", &counter, sizeof(counter));
        }

        void Invoke_ReflectionSampleComponent_Add(MEObject* context, void* parms)
        {
            auto* sampleComponent = static_cast<ReflectionSampleComponent*>(context);
            const MEClass* ownerClass = sampleComponent->GetClass();
            if (ownerClass == nullptr)
            {
                return;
            }

            MEFunction* function = ownerClass->FindFunction("Add");
            if (function == nullptr)
            {
                return;
            }

            int32_t firstOperand = 0;
            int32_t secondOperand = 0;
            function->CopyParamFromBuffer(parms, "FirstOperand", &firstOperand, sizeof(firstOperand));
            function->CopyParamFromBuffer(parms, "SecondOperand", &secondOperand, sizeof(secondOperand));

            const int32_t result = sampleComponent->Add(firstOperand, secondOperand);
            function->CopyParamToBuffer(parms, "ReturnValue", &result, sizeof(result));
        }
    }

    void BindReflectionSampleComponentNativeThunks(MEClass* sampleComponentClass)
    {
        if (sampleComponentClass == nullptr)
        {
            return;
        }

        if (MEFunction* resetCounter = sampleComponentClass->FindFunction("ResetCounter"))
        {
            resetCounter->SetNativeThunk(&Invoke_ReflectionSampleComponent_ResetCounter);
        }

        if (MEFunction* getCounter = sampleComponentClass->FindFunction("GetCounter"))
        {
            getCounter->SetNativeThunk(&Invoke_ReflectionSampleComponent_GetCounter);
        }

        if (MEFunction* addFunction = sampleComponentClass->FindFunction("Add"))
        {
            addFunction->SetNativeThunk(&Invoke_ReflectionSampleComponent_Add);
        }
    }
}
