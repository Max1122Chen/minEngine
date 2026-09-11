#include "SpriteMaterialFactory.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/Material/MaterialCapability.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompiler.h"
#include "Runtime/Function/Render/Material/MaterialEdGraph.h"
#include "Runtime/Function/Render/Material/MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/Texture.h"

namespace minEngine
{
    namespace
    {
        void PopulateSpriteMaterialGraph(Material& material, bool translucent)
        {
            material.m_Graph = NewObject<MaterialEdGraph>("", &material);
            MaterialEdGraph& graph = *material.m_Graph;
            graph.m_Nodes.clear();

            MaterialEdGraphNode& texCoord = graph.AddNode<MaterialGraphNodeDef_TextureCoordinate>();
            MaterialEdGraphNode& texObject = graph.AddNode<MaterialGraphNodeDef_TextureObject>();
            MaterialEdGraphNode& texSample = graph.AddNode<MaterialGraphNodeDef_TextureSample>();

            MaterialEdGraphNode& tintR = graph.AddNode<MaterialGraphNodeDef_ScalarParameter>();
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(tintR.GetNodeDef())->ParameterName = "TintR";
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(tintR.GetNodeDef())->UniformSlotIndex = 0;
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(tintR.GetNodeDef())->DefaultValue = 1.0f;

            MaterialEdGraphNode& tintG = graph.AddNode<MaterialGraphNodeDef_ScalarParameter>();
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(tintG.GetNodeDef())->ParameterName = "TintG";
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(tintG.GetNodeDef())->UniformSlotIndex = 1;
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(tintG.GetNodeDef())->DefaultValue = 1.0f;

            MaterialEdGraphNode& tintB = graph.AddNode<MaterialGraphNodeDef_ScalarParameter>();
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(tintB.GetNodeDef())->ParameterName = "TintB";
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(tintB.GetNodeDef())->UniformSlotIndex = 2;
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(tintB.GetNodeDef())->DefaultValue = 1.0f;

            MaterialEdGraphNode& makeTint = graph.AddNode<MaterialGraphNodeDef_MakeFloat3>();
            MaterialEdGraphNode& albedoMul = graph.AddNode<MaterialGraphNodeDef_Multiply>();
            MaterialEdGraphNode& output = graph.AddNode<MaterialGraphNodeDef_MaterialOutput>();

            MaterialGraphNodeDef_TextureObject* textureObjectDef =
                static_cast<MaterialGraphNodeDef_TextureObject*>(texObject.GetNodeDef());
            textureObjectDef->ParameterName = "BaseColor";
            textureObjectDef->TextureSlotIndex = 0;

            const MaterialShadingModel shading = MaterialShadingModel::Unlit;
            const MaterialBlendMode blend =
                translucent ? MaterialBlendMode::Translucent : MaterialBlendMode::Opaque;

            graph.ConnectPins(texObject, 0, texSample, 0, shading, blend);
            graph.ConnectPins(texCoord, 0, texSample, 1, shading, blend);
            graph.ConnectPins(tintR, 0, makeTint, 0, shading, blend);
            graph.ConnectPins(tintG, 0, makeTint, 1, shading, blend);
            graph.ConnectPins(tintB, 0, makeTint, 2, shading, blend);
            // TextureSample output 1 = RGB
            graph.ConnectPins(texSample, 1, albedoMul, 0, shading, blend);
            graph.ConnectPins(makeTint, 0, albedoMul, 1, shading, blend);
            graph.ConnectToMaterialProperty(albedoMul, 0, output, MP_Albedo, shading, blend);

            if (translucent)
            {
                MaterialEdGraphNode& opacity = graph.AddNode<MaterialGraphNodeDef_ScalarParameter>();
                static_cast<MaterialGraphNodeDef_ScalarParameter*>(opacity.GetNodeDef())->ParameterName = "Opacity";
                static_cast<MaterialGraphNodeDef_ScalarParameter*>(opacity.GetNodeDef())->UniformSlotIndex = 3;
                static_cast<MaterialGraphNodeDef_ScalarParameter*>(opacity.GetNodeDef())->DefaultValue = 1.0f;

                MaterialEdGraphNode& alphaMask = graph.AddNode<MaterialGraphNodeDef_ComponentMask>();
                static_cast<MaterialGraphNodeDef_ComponentMask*>(alphaMask.GetNodeDef())->ChannelIndex = 3;

                MaterialEdGraphNode& opacityMul = graph.AddNode<MaterialGraphNodeDef_Multiply>();
                // RGBA → mask A
                graph.ConnectPins(texSample, 0, alphaMask, 0, shading, blend);
                graph.ConnectPins(alphaMask, 0, opacityMul, 0, shading, blend);
                graph.ConnectPins(opacity, 0, opacityMul, 1, shading, blend);
                graph.ConnectToMaterialProperty(opacityMul, 0, output, MP_Opacity, shading, blend);
            }

            material.m_ShadingModel = shading;
            material.m_BlendMode = blend;
            MaterialCapabilityUtil::PruneInvalidMaterialOutputLinks(material);
        }
    }

    std::shared_ptr<Material> SpriteMaterialFactory::CreateInstance(RHI& rhi, bool translucent)
    {
        std::shared_ptr<Material> material = NewObject<Material>("SpriteUnlit", nullptr, GenerateGUID());
        PopulateSpriteMaterialGraph(*material, translucent);

        MaterialCompileContext ctx;
        ctx.RHI = &rhi;
        if (!MaterialCompiler::Compile(*material, ctx))
        {
            ME_LOG(LogRender, Error, "SpriteMaterialFactory: failed to compile {} sprite material.", translucent ? "translucent" : "opaque");
            for (const MaterialCompileDiagnostic& diagnostic : material->m_LastCompileDiagnostics)
            {
                ME_LOG(LogRender, Error, "  {}", diagnostic.Message);
            }
            return nullptr;
        }

        return material;
    }

    void SpriteMaterialFactory::ApplyColorAndTexture(
        Material& material,
        const LinearColor& color,
        const std::shared_ptr<Texture2D>& texture)
    {
        // Albedo = texture.rgb * TintRGB; Opacity = texture.a * color.a (translucent graph only).
        material.SetTextureParameter("BaseColor", texture);
        material.SetScalarParameter("TintR", color.R);
        material.SetScalarParameter("TintG", color.G);
        material.SetScalarParameter("TintB", color.B);
        material.SetScalarParameter("Opacity", color.A);
    }
}
