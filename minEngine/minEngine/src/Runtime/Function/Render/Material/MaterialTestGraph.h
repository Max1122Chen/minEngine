#pragma once

#include "MaterialEdGraph.h"
#include "MaterialGraphNodeDefs/MaterialGraphNodeDef.h"

#include <string>

namespace minEngine
{
    class Material;
    class RHI;

    // Builds the shared MIR smoke graph into an existing MaterialEdGraph (nodes owned by the graph).
    void PopulateSmokeMaterialGraph(MaterialEdGraph& graph);

    const MaterialGraphNodeDef_MaterialOutput* FindMaterialOutputNode(const MaterialEdGraph& graph);

    // Fills material.m_Graph, compiles via MaterialCompiler::Compile, and sets smoke draw parameters.
    bool SetupSmokeMaterial(Material& material, RHI& rhi, std::string* outError = nullptr);
}
