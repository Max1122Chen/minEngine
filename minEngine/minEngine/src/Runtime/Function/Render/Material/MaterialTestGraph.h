#pragma once

#include "Runtime/Core/EngineAPI.h"

#include "MaterialEdGraph.h"
#include "MaterialGraphNodeDefs/MaterialGraphNodeDef.h"

#include <filesystem>
#include <string>

namespace minEngine
{
    class Material;
    class RHI;

    /** Matches golden `MyMEProject/Assets/Materials/MaterialIRSmoke.memtl` root fields. */
    void ApplySmokeMaterialAssetDefaults(Material& material);

    MINENGINE_API void PopulateSmokeMaterialGraph(Material& material);

    MINENGINE_API const MaterialGraphNodeDef_MaterialOutput* FindMaterialOutputNode(const MaterialEdGraph& graph);

    /** Returns empty path if the golden asset file cannot be resolved. */
    std::filesystem::path ResolveGoldenMaterialIRSmokeMemtlPath();

    /** Checks on-disk golden `.memtl` root fields (no full deserialize). */
    MINENGINE_API bool VerifyGoldenMaterialIRSmokeMemtlOnDisk(std::string* outError = nullptr);

    bool SetupSmokeMaterial(Material& material, RHI& rhi, std::string* outError = nullptr);
}
