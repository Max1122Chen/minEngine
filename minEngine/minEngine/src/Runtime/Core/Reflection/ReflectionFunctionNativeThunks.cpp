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

        if (MEFunction* isSameObject = sampleComponentClass->FindFunction("IsSameObject"))
        {
            isSameObject->SetNativeThunk(
                &InvokeNativeThunk<ReflectionSampleComponent, &ReflectionSampleComponent::IsSameObject>);
        }

        if (MEFunction* getGreeting = sampleComponentClass->FindFunction("GetGreeting"))
        {
            getGreeting->SetNativeThunk(
                &InvokeNativeThunk<ReflectionSampleComponent, &ReflectionSampleComponent::GetGreeting>);
        }

        if (MEFunction* makeShared = sampleComponentClass->FindFunction("MakeSharedComponent"))
        {
            makeShared->SetNativeThunk(
                &InvokeNativeThunk<ReflectionSampleComponent, &ReflectionSampleComponent::MakeSharedComponent>);
        }

        if (MEFunction* rewriteShared = sampleComponentClass->FindFunction("RewriteSharedComponentRef"))
        {
            rewriteShared->SetNativeThunk(
                &InvokeNativeThunk<ReflectionSampleComponent, &ReflectionSampleComponent::RewriteSharedComponentRef>);
        }

        if (MEFunction* normalizeNested = sampleComponentClass->FindFunction("NormalizeNested"))
        {
            normalizeNested->SetNativeThunk(
                &InvokeNativeThunk<ReflectionSampleComponent, &ReflectionSampleComponent::NormalizeNested>);
        }
    }
}
