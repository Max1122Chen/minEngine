#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/Reflection/ReflectionDisplayNames.h"
#include "Runtime/Core/Reflection/ReflectionSample.h"

#include "EngineTestFixture.h"

#include "doctest.h"

namespace
{
    const minEngine::Reflection::MEProperty* FindSampleClassProperty(const char* propertyName)
    {
        const minEngine::Reflection::MEClass* sampleClass =
            minEngine::Reflection::ReflectionSystem::Get().FindClass("minEngine::ReflectionSampleClass");
        if (sampleClass == nullptr)
        {
            return nullptr;
        }

        const minEngine::Reflection::MEProperty* foundProperty = nullptr;
        minEngine::Reflection::ReflectionSystem::Get().ForEachPropertyInHierarchy(
            sampleClass->GetName(),
            [&](const minEngine::Reflection::MEProperty& property) -> bool
            {
                if (property.GetName() == propertyName)
                {
                    foundProperty = &property;
                    return false;
                }

                return true;
            });
        return foundProperty;
    }
}

TEST_CASE("reflection-display-names: prefix strip and camelCase [full]")
{
    using namespace minEngine::Reflection;

    CHECK(FormatMemberDisplayName("m_LightColor") == "Light Color");
    CHECK(FormatMemberDisplayName("m_InnerConeAngle") == "Inner Cone Angle");
    CHECK(FormatMemberDisplayName("m_CastShadow") == "Cast Shadow");
    CHECK(FormatMemberDisplayName("x_CustomData") == "Custom Data");
    CHECK(FormatMemberDisplayName("b_Enabled") == "Enabled");
    CHECK(FormatMemberDisplayName("Intensity") == "Intensity");
    CHECK(FormatMemberDisplayName("FloatField") == "Float Field");
    CHECK(FormatMemberDisplayName("m_1st") == "m_1st");
    CHECK(FormatMemberDisplayName("m_") == "m_");
    CHECK(FormatMemberDisplayName("HTTPResponse") == "HTTP Response");
}

TEST_CASE("reflection-display-names: property metadata override [full]")
{
    using namespace minEngine::Reflection;

    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    const MEProperty* intField = FindSampleClassProperty("IntField");
    const MEProperty* floatField = FindSampleClassProperty("FloatField");
    REQUIRE(intField != nullptr);
    REQUIRE(floatField != nullptr);

    CHECK(std::string(GetPropertyDisplayName(*intField)) == "Sample Int");
    CHECK(std::string(GetPropertyDisplayName(*floatField)) == "Float Field");
}
