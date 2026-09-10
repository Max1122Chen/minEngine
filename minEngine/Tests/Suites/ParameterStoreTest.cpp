#include "Runtime/Function/Framework/Parameters/ParameterLayout.h"
#include "Runtime/Function/Framework/Parameters/ParameterSchema.h"
#include "Runtime/Function/Framework/Parameters/ParameterStore.h"
#include "Runtime/Function/Framework/Parameters/ParameterValueType.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Core/Serialization/Json.h"

#include "EngineTestFixture.h"
#include "doctest.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace
{
    std::vector<uint8_t> MakeFloatDefault(float value)
    {
        std::vector<uint8_t> bytes(sizeof(float));
        std::memcpy(bytes.data(), &value, sizeof(float));
        return bytes;
    }

    std::vector<uint8_t> MakeInt32Default(int32_t value)
    {
        std::vector<uint8_t> bytes(sizeof(int32_t));
        std::memcpy(bytes.data(), &value, sizeof(int32_t));
        return bytes;
    }

    std::vector<uint8_t> MakeBoolDefault(bool value)
    {
        return std::vector<uint8_t>{static_cast<uint8_t>(value ? 1 : 0)};
    }

    std::shared_ptr<const minEngine::ParameterLayout> CompileSchema(
        minEngine::ParameterSchema& schema)
    {
        using namespace minEngine;
        ParameterLayout layout;
        std::string error;
        REQUIRE(ParameterLayout::Compile(schema, layout, &error));
        return std::make_shared<ParameterLayout>(std::move(layout));
    }
}

TEST_CASE("parameter-store: empty schema compiles [smoke]")
{
    using namespace minEngine;

    ParameterSchema schema;
    ParameterLayout layout;
    std::string error;
    REQUIRE(ParameterLayout::Compile(schema, layout, &error));
    CHECK(layout.GetTotalBytes() == 0);
    CHECK(layout.GetEntryCount() == 0);

    ParameterStore store;
    store.BindLayout(std::make_shared<ParameterLayout>(std::move(layout)));
    CHECK(store.IsBound());
    CHECK_FALSE(store.SetFloat(0, 1.0f));
}

TEST_CASE("parameter-store: three-key round trip [full]")
{
    using namespace minEngine;

    ParameterSchema schema;
    REQUIRE(schema.AddEntry({"Speed", ParameterValueType::Float, MakeFloatDefault(0.0f)}));
    REQUIRE(schema.AddEntry({"Combo", ParameterValueType::Int32, MakeInt32Default(0)}));
    REQUIRE(schema.AddEntry({"IsGrounded", ParameterValueType::Bool, MakeBoolDefault(true)}));

    auto layout = CompileSchema(schema);
    REQUIRE(layout->GetEntryCount() == 3);
    CHECK(layout->GetEntry(0)->Name == "Speed");
    CHECK(layout->GetEntry(1)->Name == "Combo");
    CHECK(layout->GetEntry(2)->Name == "IsGrounded");
    CHECK(layout->GetEntry(2)->Size == 1);

    // Pack: Float/Int32 first (size 4), then Bool — TotalBytes should be 9.
    CHECK(layout->GetTotalBytes() == 9);
    CHECK(layout->GetEntry(0)->Offset == 0);
    CHECK(layout->GetEntry(1)->Offset == 4);
    CHECK(layout->GetEntry(2)->Offset == 8);

    ParameterStore store;
    store.BindLayout(layout);

    bool grounded = false;
    REQUIRE(store.TryGetBool(2, grounded));
    CHECK(grounded == true);

    REQUIRE(store.SetFloat(0, 3.5f));
    REQUIRE(store.SetInt32(1, 7));
    REQUIRE(store.SetBool(2, false));

    float speed = 0.0f;
    int32_t combo = 0;
    REQUIRE(store.TryGetFloat(0, speed));
    REQUIRE(store.TryGetInt32(1, combo));
    REQUIRE(store.TryGetBool(2, grounded));
    CHECK(speed == doctest::Approx(3.5f));
    CHECK(combo == 7);
    CHECK(grounded == false);

    CHECK(store.FindKeyId("Combo") == 1);
    CHECK(store.FindKeyId("Missing") == kInvalidParameterKeyId);
}

TEST_CASE("parameter-store: type mismatch and bad key fail [full]")
{
    using namespace minEngine;

    ParameterSchema schema;
    REQUIRE(schema.AddEntry({"Speed", ParameterValueType::Float, {}}));
    auto layout = CompileSchema(schema);

    ParameterStore store;
    store.BindLayout(layout);

    CHECK_FALSE(store.SetBool(0, true));
    CHECK_FALSE(store.SetInt32(0, 1));
    CHECK_FALSE(store.SetFloat(1, 1.0f));
    CHECK_FALSE(store.SetFloat(kInvalidParameterKeyId, 1.0f));

    float value = 0.0f;
    bool flag = false;
    CHECK_FALSE(store.TryGetBool(0, flag));
    CHECK_FALSE(store.TryGetFloat(99, value));
}

TEST_CASE("parameter-store: two stores isolated + CopyFrom [full]")
{
    using namespace minEngine;

    ParameterSchema schema;
    REQUIRE(schema.AddEntry({"A", ParameterValueType::Float, MakeFloatDefault(1.0f)}));
    auto layout = CompileSchema(schema);

    ParameterStore storeA;
    ParameterStore storeB;
    storeA.BindLayout(layout);
    storeB.BindLayout(layout);

    REQUIRE(storeA.SetFloat(0, 10.0f));
    float a = 0.0f;
    float b = 0.0f;
    REQUIRE(storeA.TryGetFloat(0, a));
    REQUIRE(storeB.TryGetFloat(0, b));
    CHECK(a == doctest::Approx(10.0f));
    CHECK(b == doctest::Approx(1.0f));

    REQUIRE(storeB.CopyFrom(storeA));
    REQUIRE(storeB.TryGetFloat(0, b));
    CHECK(b == doctest::Approx(10.0f));

    REQUIRE(storeA.SetFloat(0, 99.0f));
    REQUIRE(storeB.TryGetFloat(0, b));
    CHECK(b == doctest::Approx(10.0f));
}

TEST_CASE("parameter-store: ResetToDefaults and DefaultBytes validate [full]")
{
    using namespace minEngine;

    ParameterSchema schema;
    REQUIRE(schema.AddEntry({"Speed", ParameterValueType::Float, MakeFloatDefault(2.5f)}));
    REQUIRE(schema.AddEntry({"Flag", ParameterValueType::Bool, MakeBoolDefault(true)}));

    ParameterSchemaEntry bad;
    bad.Name = "Broken";
    bad.Type = ParameterValueType::Int32;
    bad.DefaultBytes = {1, 2}; // wrong size
    std::string error;
    CHECK_FALSE(schema.AddEntry(std::move(bad), &error));
    CHECK(error.find("DefaultBytes") != std::string::npos);

    auto layout = CompileSchema(schema);
    ParameterStore store;
    store.BindLayout(layout);

    REQUIRE(store.SetFloat(0, 9.0f));
    REQUIRE(store.SetBool(1, false));
    store.ResetToDefaults();

    float speed = 0.0f;
    bool flag = false;
    REQUIRE(store.TryGetFloat(0, speed));
    REQUIRE(store.TryGetBool(1, flag));
    CHECK(speed == doctest::Approx(2.5f));
    CHECK(flag == true);
}

TEST_CASE("parameter-store: duplicate and empty name rejected [full]")
{
    using namespace minEngine;

    ParameterSchema schema;
    REQUIRE(schema.AddEntry({"Speed", ParameterValueType::Float, {}}));

    std::string error;
    CHECK_FALSE(schema.AddEntry({"Speed", ParameterValueType::Int32, {}}, &error));
    CHECK(error.find("Duplicate") != std::string::npos);

    CHECK_FALSE(schema.AddEntry({"", ParameterValueType::Bool, {}}, &error));
    CHECK(error.find("non-empty") != std::string::npos);

    ParameterLayout layout;
    REQUIRE(ParameterLayout::Compile(schema, layout, &error));
    CHECK(layout.GetEntryCount() == 1);
}

TEST_CASE("parameter-store: schema JSON round-trip [full]")
{
    using namespace minEngine;

    EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    ParameterSchema source;
    REQUIRE(source.AddEntry({"Speed", ParameterValueType::Float, MakeFloatDefault(2.5f)}));
    REQUIRE(source.AddEntry({"Combo", ParameterValueType::Int32, MakeInt32Default(3)}));
    REQUIRE(source.AddEntry({"Flag", ParameterValueType::Bool, MakeBoolDefault(true)}));

    Json json;
    const Serialization::SerializeResult writeResult =
        Serialization::Serializer::SerializeObjectToJson("minEngine::ParameterSchema", &source, json);
    REQUIRE(writeResult.ok);

    ParameterSchema restored;
    std::vector<Serialization::PendingObjectRef> unresolvedRefs;
    const Serialization::SerializeResult readResult = Serialization::Serializer::DeserializeObjectFromJson(
        "minEngine::ParameterSchema",
        &restored,
        json,
        unresolvedRefs);
    REQUIRE(readResult.ok);
    CHECK(unresolvedRefs.empty());

    REQUIRE(restored.GetEntryCount() == 3);
    CHECK(restored.GetEntries()[0].Name == "Speed");
    CHECK(restored.GetEntries()[0].Type == ParameterValueType::Float);
    CHECK(restored.GetEntries()[0].DefaultBytes == MakeFloatDefault(2.5f));
    CHECK(restored.GetEntries()[1].Name == "Combo");
    CHECK(restored.GetEntries()[1].Type == ParameterValueType::Int32);
    CHECK(restored.GetEntries()[1].DefaultBytes == MakeInt32Default(3));
    CHECK(restored.GetEntries()[2].Name == "Flag");
    CHECK(restored.GetEntries()[2].Type == ParameterValueType::Bool);
    CHECK(restored.GetEntries()[2].DefaultBytes == MakeBoolDefault(true));

    REQUIRE(restored.Validate());
    auto layout = CompileSchema(restored);
    ParameterStore store;
    store.BindLayout(layout);

    float speed = 0.0f;
    int32_t combo = 0;
    bool flag = false;
    REQUIRE(store.TryGetFloat(0, speed));
    REQUIRE(store.TryGetInt32(1, combo));
    REQUIRE(store.TryGetBool(2, flag));
    CHECK(speed == doctest::Approx(2.5f));
    CHECK(combo == 3);
    CHECK(flag == true);
}
