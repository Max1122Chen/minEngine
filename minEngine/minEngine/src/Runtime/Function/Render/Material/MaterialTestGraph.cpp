#include "MaterialTestGraph.h"

#include "../Material.h"
#include "../RHI/RHI.h"
#include "../Texture.h"
#include "MaterialCompiler/MaterialCompiler.h"
namespace minEngine
{
    void PopulateSmokeMaterialGraph(MaterialEdGraph& graph)
    {
        graph.m_Nodes.clear();
        graph.m_Nodes.reserve(16);

        graph.AddNode<MaterialGraphNodeDef_TextureCoordinate>();
        graph.AddNode<MaterialGraphNodeDef_TextureObject>("BaseColor", 0);
        graph.AddNode<MaterialGraphNodeDef_TextureSample>();
        graph.AddNode<MaterialGraphNodeDef_ScalarParameter>("Metallic", 0, 0.3f);
        graph.AddNode<MaterialGraphNodeDef_Constant>(0.2f);
        graph.AddNode<MaterialGraphNodeDef_Constant>(0.8f);
        graph.AddNode<MaterialGraphNodeDef_Constant>(0.2f);
        graph.AddNode<MaterialGraphNodeDef_MakeFloat3>();
        graph.AddNode<MaterialGraphNodeDef_Multiply>();
        graph.AddNode<MaterialGraphNodeDef_MaterialOutput>();

        MaterialEdGraphNode& texCoord = graph.m_Nodes[0];
        MaterialEdGraphNode& texObject = graph.m_Nodes[1];
        MaterialEdGraphNode& texSample = graph.m_Nodes[2];
        MaterialEdGraphNode& metallicScalar = graph.m_Nodes[3];
        MaterialEdGraphNode& emissiveR = graph.m_Nodes[4];
        MaterialEdGraphNode& emissiveG = graph.m_Nodes[5];
        MaterialEdGraphNode& emissiveB = graph.m_Nodes[6];
        MaterialEdGraphNode& tintColor = graph.m_Nodes[7];
        MaterialEdGraphNode& albedoTintMultiply = graph.m_Nodes[8];
        MaterialEdGraphNode& output = graph.m_Nodes[9];

        graph.ConnectPins(texObject, 0, texSample, 0);
        graph.ConnectPins(texCoord, 0, texSample, 1);
        graph.ConnectPins(emissiveR, 0, tintColor, 0);
        graph.ConnectPins(emissiveG, 0, tintColor, 1);
        graph.ConnectPins(emissiveB, 0, tintColor, 2);
        graph.ConnectPins(texSample, 1, albedoTintMultiply, 0);
        graph.ConnectPins(tintColor, 0, albedoTintMultiply, 1);
        graph.ConnectToMaterialProperty(albedoTintMultiply, 0, output, MP_Albedo);
        graph.ConnectToMaterialProperty(metallicScalar, 0, output, MP_Metallic);
    }

    const MaterialGraphNodeDef_MaterialOutput* FindMaterialOutputNode(const MaterialEdGraph& graph)
    {
        for (const MaterialEdGraphNode& node : graph.m_Nodes)
        {
            if (const MaterialGraphNodeDef_MaterialOutput* outputNode =
                    dynamic_cast<const MaterialGraphNodeDef_MaterialOutput*>(node.GetDefinition()))
            {
                return outputNode;
            }
        }

        return nullptr;
    }

    bool SetupSmokeMaterial(Material& material, RHI& rhi, std::string* outError)
    {
        material.m_ShadingModel = MaterialShadingModel::Unlit;
        PopulateSmokeMaterialGraph(material.m_Graph);

        MaterialCompileContext ctx;
        ctx.RHI = &rhi;
        if (!MaterialCompiler::Compile(material, ctx))
        {
            if (outError != nullptr)
            {
                *outError = "Material smoke compile failed.";
                for (const MaterialCompileDiagnostic& diagnostic : material.m_LastCompileDiagnostics)
                {
                    *outError += "\n";
                    *outError += diagnostic.Message;
                }
            }
            return false;
        }

        material.SetTextureParameter("BaseColor", Texture2D::CreateSolidRGBA(rhi, 255, 255, 255, 255));
        return true;
    }
}
