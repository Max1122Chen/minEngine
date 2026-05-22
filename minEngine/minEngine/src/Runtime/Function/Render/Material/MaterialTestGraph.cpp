#include "MaterialTestGraph.h"

#include "../Material.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "../RHI/RHI.h"
#include "../Texture.h"
#include "MaterialCompiler/MaterialCompiler.h"

namespace minEngine
{
    void PopulateSmokeMaterialGraph(Material& material)
    {
        material.m_Graph = NewObject<MaterialEdGraph>("", &material);
        MaterialEdGraph& graph = *material.m_Graph;
        graph.m_Nodes.clear();
        graph.m_Nodes.reserve(16);

        graph.AddNode<MaterialGraphNodeDef_TextureCoordinate>();
        MaterialEdGraphNode& texObject = graph.AddNode<MaterialGraphNodeDef_TextureObject>();
        graph.AddNode<MaterialGraphNodeDef_TextureSample>();
        MaterialEdGraphNode& metallicScalar = graph.AddNode<MaterialGraphNodeDef_ScalarParameter>();
        static_cast<MaterialGraphNodeDef_ScalarParameter*>(metallicScalar.GetNodeDef())->ParameterName = "Metallic";
        static_cast<MaterialGraphNodeDef_ScalarParameter*>(metallicScalar.GetNodeDef())->UniformSlotIndex = 0;
        static_cast<MaterialGraphNodeDef_ScalarParameter*>(metallicScalar.GetNodeDef())->DefaultValue = 0.3f;
        MaterialEdGraphNode& emissiveR = graph.AddNode<MaterialGraphNodeDef_Constant>();
        static_cast<MaterialGraphNodeDef_Constant*>(emissiveR.GetNodeDef())->Value = 0.2f;
        MaterialEdGraphNode& emissiveG = graph.AddNode<MaterialGraphNodeDef_Constant>();
        static_cast<MaterialGraphNodeDef_Constant*>(emissiveG.GetNodeDef())->Value = 0.8f;
        MaterialEdGraphNode& emissiveB = graph.AddNode<MaterialGraphNodeDef_Constant>();
        static_cast<MaterialGraphNodeDef_Constant*>(emissiveB.GetNodeDef())->Value = 0.2f;
        graph.AddNode<MaterialGraphNodeDef_MakeFloat3>();
        MaterialEdGraphNode& albedoTintMultiply = graph.AddNode<MaterialGraphNodeDef_Multiply>();
        MaterialEdGraphNode& output = graph.AddNode<MaterialGraphNodeDef_MaterialOutput>();

        MaterialEdGraphNode& texCoord = *graph.m_Nodes[0];
        MaterialEdGraphNode& texSample = *graph.m_Nodes[2];

        graph.ConnectPins(texObject, 0, texSample, 0);
        graph.ConnectPins(texCoord, 0, texSample, 1);
        graph.ConnectPins(emissiveR, 0, *graph.m_Nodes[7], 0);
        graph.ConnectPins(emissiveG, 0, *graph.m_Nodes[7], 1);
        graph.ConnectPins(emissiveB, 0, *graph.m_Nodes[7], 2);
        graph.ConnectPins(texSample, 1, albedoTintMultiply, 0);
        graph.ConnectPins(*graph.m_Nodes[7], 0, albedoTintMultiply, 1);
        graph.ConnectToMaterialProperty(albedoTintMultiply, 0, output, MP_Albedo);
        graph.ConnectToMaterialProperty(metallicScalar, 0, output, MP_Metallic);

        MaterialGraphNodeDef_TextureObject* baseColorTextureObject =
            static_cast<MaterialGraphNodeDef_TextureObject*>(texObject.GetNodeDef());
        if (baseColorTextureObject != nullptr)
        {
            baseColorTextureObject->ParameterName = "BaseColor";
            baseColorTextureObject->TextureSlotIndex = 0;
        }
    }

    const MaterialGraphNodeDef_MaterialOutput* FindMaterialOutputNode(const MaterialEdGraph& graph)
    {
        for (const std::shared_ptr<MaterialEdGraphNode>& node : graph.m_Nodes)
        {
            if (!node)
            {
                continue;
            }

            if (const MaterialGraphNodeDef_MaterialOutput* outputNode =
                    dynamic_cast<const MaterialGraphNodeDef_MaterialOutput*>(node->GetDefinition()))
            {
                return outputNode;
            }
        }

        return nullptr;
    }

    bool SetupSmokeMaterial(Material& material, RHI& rhi, std::string* outError)
    {
        material.m_ShadingModel = MaterialShadingModel::Unlit;
        PopulateSmokeMaterialGraph(material);

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

        MaterialGraphNodeDef_TextureObject* baseColorTextureObject = nullptr;
        for (const std::shared_ptr<MaterialEdGraphNode>& graphNode : material.m_Graph->m_Nodes)
        {
            if (!graphNode)
            {
                continue;
            }

            if (MaterialGraphNodeDef_TextureObject* textureObject =
                    dynamic_cast<MaterialGraphNodeDef_TextureObject*>(graphNode->GetNodeDef()))
            {
                if (textureObject->ParameterName == "BaseColor" && textureObject->DefaultTexture == nullptr)
                {
                    baseColorTextureObject = textureObject;
                    break;
                }
            }
        }

        if (baseColorTextureObject != nullptr)
        {
            baseColorTextureObject->DefaultTexture = Texture2D::CreateSolidRGBA(rhi, 255, 255, 255, 255);
        }

        return true;
    }
}

