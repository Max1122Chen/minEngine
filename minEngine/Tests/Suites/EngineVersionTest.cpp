#include "EngineVersionTest.h"

#include "Runtime/Core/EngineVersion.h"
#include "Runtime/Core/Serialization/DiskVersion.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/SerializationTypes.h"
#include "Runtime/Core/Serialization/Serializer.h"

#include "doctest.h"

#include <string>

TEST_CASE("engine-version: ToString matches 0.0.9 [smoke]")
{
    using namespace minEngine;
    CHECK(kEngineVersion.Major == 0);
    CHECK(kEngineVersion.Minor == 0);
    CHECK(kEngineVersion.Patch == 9);
    CHECK(GetEngineVersion().ToString() == "0.0.9");
    CHECK(kDiskSchemaVersion == 1u);
}

TEST_CASE("engine-version: ValidateDiskSchema rejects future schema [smoke]")
{
    using namespace minEngine::Serialization;

    DiskVersionInfo future;
    future.schemaVersion = 999u;
    future.engineVersion = "9.9.9";
    const SerializeResult fail = ValidateDiskSchema(future);
    CHECK_FALSE(fail.ok);
    CHECK(fail.message.find("999") != std::string::npos);

    DiskVersionInfo current;
    current.schemaVersion = minEngine::kDiskSchemaVersion;
    current.engineVersion = "0.0.9";
    CHECK(ValidateDiskSchema(current).ok);

    DiskVersionInfo legacy;
    legacy.schemaVersion = 0;
    CHECK(ValidateDiskSchema(legacy).ok);
}

TEST_CASE("engine-version: Json root meta round-trip stamps [smoke]")
{
    using namespace minEngine;
    using namespace minEngine::Serialization;

    JsonWriterArchive writer;
    REQUIRE(writer.BeginObject(""));
    REQUIRE(writer.BeginField("m_Dummy"));
    REQUIRE(writer.WriteInt64(1));
    REQUIRE(writer.EndField());
    REQUIRE(writer.EndObject());

    writer.ApplyRootSchemaVersion(kDiskSchemaVersion);
    writer.ApplyRootEngineVersion(GetEngineVersion().ToString());

    const Json root = writer.GetRoot();
    REQUIRE(root.is_object());
    REQUIRE(root.contains("$schemaVersion"));
    REQUIRE(root.contains("$engineVersion"));
    CHECK(root["$schemaVersion"].get<uint32_t>() == 1u);
    CHECK(root["$engineVersion"].get<std::string>() == "0.0.9");
    CHECK(root.contains("m_Dummy"));

    JsonReaderArchive reader(root);
    CHECK(reader.GetReadSchemaVersion() == 1u);
    CHECK(reader.GetReadEngineVersion() == "0.0.9");

    DiskVersionInfo info;
    info.schemaVersion = reader.GetReadSchemaVersion();
    info.engineVersion = reader.GetReadEngineVersion();
    CHECK(ValidateDiskSchema(info).ok);
}
