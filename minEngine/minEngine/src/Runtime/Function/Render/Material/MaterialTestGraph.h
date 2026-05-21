#pragma once

#include "MaterialEdGraph.h"
#include "MaterialGraphNodeDefs/MaterialGraphNodeDef.h"

#include <string>

namespace minEngine
{
    class Material;
    class RHI;

    void PopulateSmokeMaterialGraph(Material& material);

    const MaterialGraphNodeDef_MaterialOutput* FindMaterialOutputNode(const MaterialEdGraph& graph);

    bool SetupSmokeMaterial(Material& material, RHI& rhi, std::string* outError = nullptr);
}
