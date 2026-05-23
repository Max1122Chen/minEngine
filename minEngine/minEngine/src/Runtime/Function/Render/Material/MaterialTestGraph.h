#pragma once

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

    void PopulateSmokeMaterialGraph(Material& material);

    const MaterialGraphNodeDef_MaterialOutput* FindMaterialOutputNode(const MaterialEdGraph& graph);

    /** Returns empty path if the golden asset file cannot be resolved. */
    std::filesystem::path ResolveGoldenMaterialIRSmokeMemtlPath();

    /** Checks on-disk golden `.memtl` root fields (no full deserialize). */
    bool VerifyGoldenMaterialIRSmokeMemtlOnDisk(std::string* outError = nullptr);

    bool SetupSmokeMaterial(Material& material, RHI& rhi, std::string* outError = nullptr);
}
