#include "ReflectionFunctionNativeThunks.h"

#include "ReflectionFunctionNativeThunkTemplates.h"
#include "ReflectionSample.h"

namespace minEngine::Reflection
{
    void BindReflectionSampleComponentNativeThunks(MEClass* sampleComponentClass)
    {
        if (sampleComponentClass == nullptr)
        {
            return;
        }

        if (MEFunction* resetCounter = sampleComponentClass->FindFunction("ResetCounter"))
        {
            resetCounter->SetNativeThunk(
                &InvokeNativeThunk<ReflectionSampleComponent, &ReflectionSampleComponent::ResetCounter>);
        }

        if (MEFunction* getCounter = sampleComponentClass->FindFunction("GetCounter"))
        {
            getCounter->SetNativeThunk(
                &InvokeNativeThunk<ReflectionSampleComponent, &ReflectionSampleComponent::GetCounter>);
        }

        if (MEFunction* addFunction = sampleComponentClass->FindFunction("Add"))
        {
            addFunction->SetNativeThunk(&InvokeNativeThunk<ReflectionSampleComponent, &ReflectionSampleComponent::Add>);
        }

        if (MEFunction* addInPlace = sampleComponentClass->FindFunction("AddInPlace"))
        {
            addInPlace->SetNativeThunk(
                &InvokeNativeThunk<ReflectionSampleComponent, &ReflectionSampleComponent::AddInPlace>);
        }

        if (MEFunction* peekString = sampleComponentClass->FindFunction("PeekString"))
        {
            peekString->SetNativeThunk(
                &InvokeNativeThunk<ReflectionSampleComponent, &ReflectionSampleComponent::PeekString>);
        }

        if (MEFunction* fillOut = sampleComponentClass->FindFunction("FillOut"))
        {
            fillOut->SetNativeThunk(
                &InvokeNativeThunk<ReflectionSampleComponent, &ReflectionSampleComponent::FillOut>);
        }

        if (MEFunction* echoEnum = sampleComponentClass->FindFunction("EchoEnum"))
        {
            echoEnum->SetNativeThunk(
                &InvokeNativeThunk<ReflectionSampleComponent, &ReflectionSampleComponent::EchoEnum>);
        }

        if (MEFunction* sumIntArray = sampleComponentClass->FindFunction("SumIntArray"))
        {
            sumIntArray->SetNativeThunk(
                &InvokeNativeThunk<ReflectionSampleComponent, &ReflectionSampleComponent::SumIntArray>);
        }

        if (MEFunction* isSameObject = sampleComponentClass->FindFunction("IsSameObject"))
        {
            isSameObject->SetNativeThunk(
                &InvokeNativeThunk<ReflectionSampleComponent, &ReflectionSampleComponent::IsSameObject>);
        }
    }
}
