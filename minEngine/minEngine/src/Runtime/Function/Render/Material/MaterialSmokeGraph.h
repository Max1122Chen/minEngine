#pragma once

#include "MaterialCompiler/MaterialCompileTypes.h"
#include "MaterialEdGraph.h"
#include "MaterialGraphNodeDefs/MaterialGraphNodeDef.h"

namespace minEngine
{
    // Shared smoke graph: Albedo = TextureSample * tint, Metallic = scalar parameter.
    class MaterialSmokeGraph
    {
    public:
        MaterialSmokeGraph();

        const MaterialEdGraph& GetGraph() const { return m_Graph; }
        const MaterialGraphNodeDef_MaterialOutput& GetMaterialOutput() const { return m_Output; }
        MaterialCompiledShader CompileUnlit() const;

    private:
        MaterialEdGraph m_Graph;
        MaterialGraphNodeDef_TextureCoordinate m_TexCoord;
        MaterialGraphNodeDef_TextureObject m_TextureObject;
        MaterialGraphNodeDef_TextureSample m_TextureSample;
        MaterialGraphNodeDef_ScalarParameter m_MetallicScalar;
        MaterialGraphNodeDef_Constant m_EmissiveR;
        MaterialGraphNodeDef_Constant m_EmissiveG;
        MaterialGraphNodeDef_Constant m_EmissiveB;
        MaterialGraphNodeDef_MakeFloat3 m_TintColor;
        MaterialGraphNodeDef_Multiply m_AlbedoTintMultiply;
        MaterialGraphNodeDef_MaterialOutput m_Output;
    };
}
