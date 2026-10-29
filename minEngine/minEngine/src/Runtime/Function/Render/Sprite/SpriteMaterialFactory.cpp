#include "SpriteMaterialFactory.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/Material/MaterialCapability.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompiler.h"
#include "Runtime/Function/Render/Material/MaterialEdGraph.h"
#include "Runtime/Function/Render/Material/MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/Texture.h"

#include <mutex>

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
            MaterialEdGraphNode& maskU = graph.AddNode<MaterialGraphNodeDef_ComponentMask>();
            static_cast<MaterialGraphNodeDef_ComponentMask*>(maskU.GetNodeDef())->ChannelIndex = 0;
            MaterialEdGraphNode& maskV = graph.AddNode<MaterialGraphNodeDef_ComponentMask>();
            static_cast<MaterialGraphNodeDef_ComponentMask*>(maskV.GetNodeDef())->ChannelIndex = 1;

            // UV remap: uv = lerp(uvMin, uvMax, attrib). Slots after tint/opacity.
            const int uvSlotBase = translucent ? 4 : 3;

            MaterialEdGraphNode& uvU0 = graph.AddNode<MaterialGraphNodeDef_ScalarParameter>();
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(uvU0.GetNodeDef())->ParameterName = "UVRectU0";
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(uvU0.GetNodeDef())->UniformSlotIndex = uvSlotBase + 0;
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(uvU0.GetNodeDef())->DefaultValue = 0.0f;

            MaterialEdGraphNode& uvV0 = graph.AddNode<MaterialGraphNodeDef_ScalarParameter>();
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(uvV0.GetNodeDef())->ParameterName = "UVRectV0";
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(uvV0.GetNodeDef())->UniformSlotIndex = uvSlotBase + 1;
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(uvV0.GetNodeDef())->DefaultValue = 0.0f;

            MaterialEdGraphNode& uvU1 = graph.AddNode<MaterialGraphNodeDef_ScalarParameter>();
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(uvU1.GetNodeDef())->ParameterName = "UVRectU1";
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(uvU1.GetNodeDef())->UniformSlotIndex = uvSlotBase + 2;
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(uvU1.GetNodeDef())->DefaultValue = 1.0f;

            MaterialEdGraphNode& uvV1 = graph.AddNode<MaterialGraphNodeDef_ScalarParameter>();
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(uvV1.GetNodeDef())->ParameterName = "UVRectV1";
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(uvV1.GetNodeDef())->UniformSlotIndex = uvSlotBase + 3;
            static_cast<MaterialGraphNodeDef_ScalarParameter*>(uvV1.GetNodeDef())->DefaultValue = 1.0f;

            MaterialEdGraphNode& lerpU = graph.AddNode<MaterialGraphNodeDef_Lerp>();
            MaterialEdGraphNode& lerpV = graph.AddNode<MaterialGraphNodeDef_Lerp>();
            MaterialEdGraphNode& makeUv = graph.AddNode<MaterialGraphNodeDef_MakeFloat2>();

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

            graph.ConnectPins(texCoord, 0, maskU, 0, shading, blend);
            graph.ConnectPins(texCoord, 0, maskV, 0, shading, blend);
            graph.ConnectPins(uvU0, 0, lerpU, 0, shading, blend);
            graph.ConnectPins(uvU1, 0, lerpU, 1, shading, blend);
            graph.ConnectPins(maskU, 0, lerpU, 2, shading, blend);
            graph.ConnectPins(uvV0, 0, lerpV, 0, shading, blend);
            graph.ConnectPins(uvV1, 0, lerpV, 1, shading, blend);
            graph.ConnectPins(maskV, 0, lerpV, 2, shading, blend);
            graph.ConnectPins(lerpU, 0, makeUv, 0, shading, blend);
            graph.ConnectPins(lerpV, 0, makeUv, 1, shading, blend);

            graph.ConnectPins(texObject, 0, texSample, 0, shading, blend);
            graph.ConnectPins(makeUv, 0, texSample, 1, shading, blend);
            graph.ConnectPins(tintR, 0, makeTint, 0, shading, blend);
            graph.ConnectPins(tintG, 0, makeTint, 1, shading, blend);
            graph.ConnectPins(tintB, 0, makeTint, 2, shading, blend);
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

    namespace
    {
        /**
         * One compiled template per blend mode per RHI instance. Templates own the GPU program;
         * per-widget Materials derive from them (Material::CreateRuntimeInstance) and never compile.
         */
        struct SpriteTemplateCache
        {
            std::mutex Mutex;
            uint64_t RhiSerial = 0;
            std::shared_ptr<Material> Opaque;
            std::shared_ptr<Material> Translucent;
            SpriteMaterialCacheStats Stats;
            /**
             * A program the driver refused is refused for the life of the template, so it is
             * reported once per cached template instead of once per widget that asks.
             */
            bool bOpaqueFailureReported = false;
            bool bTranslucentFailureReported = false;
        };

        SpriteTemplateCache& GetTemplateCache()
        {
            static SpriteTemplateCache s_Cache;
            return s_Cache;
        }

        std::shared_ptr<Material> CompileTemplate(RHI& rhi, bool translucent)
        {
            std::shared_ptr<Material> material =
                NewObject<Material>("SpriteUnlit", nullptr, GenerateGUID());
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

            if (material->IsProgramPendingMaterialization())
            {
                // Not a failure: the compile finished and the program is created by the graphics
                // role at the start of the next render frame, which is before anything records.
                ME_LOG(
                    LogRender,
                    Info,
                    "SpriteMaterialFactory: {} sprite material compiled; its program is created by "
                    "the graphics role at the next frame start.",
                    translucent ? "translucent" : "opaque");
            }

            return material;
        }
    }

    std::shared_ptr<Material> SpriteMaterialFactory::GetOrCreateTemplate(RHI& rhi, bool translucent)
    {
        SpriteTemplateCache& cache = GetTemplateCache();
        std::lock_guard<std::mutex> lock(cache.Mutex);

        if (cache.RhiSerial != rhi.GetInstanceSerial())
        {
            // GPU programs belong to the RHI instance that linked them.
            cache.RhiSerial = rhi.GetInstanceSerial();
            cache.Opaque.reset();
            cache.Translucent.reset();
            cache.bOpaqueFailureReported = false;
            cache.bTranslucentFailureReported = false;
        }

        std::shared_ptr<Material>& slot = translucent ? cache.Translucent : cache.Opaque;
        if (slot)
        {
            bool& bFailureReported =
                translucent ? cache.bTranslucentFailureReported : cache.bOpaqueFailureReported;
            if (slot->HasProgramCreateFailed())
            {
                if (!bFailureReported)
                {
                    // The graphics role already reported the driver's own message. This adds the
                    // one thing it cannot know: what the failure costs, so a scene with no sprites
                    // is explained rather than silently empty.
                    bFailureReported = true;
                    ME_LOG(
                        LogRender,
                        Error,
                        "SpriteMaterialFactory: the {} sprite material's program failed to create; "
                        "sprite and UI draws with it are skipped. {}",
                        translucent ? "translucent" : "opaque",
                        slot->GetShaderCompileLog());
                }
                return nullptr;
            }

            return slot;
        }

        slot = CompileTemplate(rhi, translucent);
        if (slot)
        {
            ++cache.Stats.TemplateCompiles;
        }
        return slot;
    }

    std::shared_ptr<Material> SpriteMaterialFactory::CreateInstance(RHI& rhi, bool translucent)
    {
        std::shared_ptr<Material> templateMaterial = GetOrCreateTemplate(rhi, translucent);
        if (!templateMaterial)
        {
            return nullptr;
        }

        std::shared_ptr<Material> instance = templateMaterial->CreateRuntimeInstance();
        if (!instance)
        {
            return nullptr;
        }

        SpriteTemplateCache& cache = GetTemplateCache();
        std::lock_guard<std::mutex> lock(cache.Mutex);
        ++cache.Stats.InstanceDerivations;
        return instance;
    }

    SpriteMaterialCacheStats SpriteMaterialFactory::GetCacheStats()
    {
        SpriteTemplateCache& cache = GetTemplateCache();
        std::lock_guard<std::mutex> lock(cache.Mutex);
        return cache.Stats;
    }

    void SpriteMaterialFactory::ResetCacheStats()
    {
        SpriteTemplateCache& cache = GetTemplateCache();
        std::lock_guard<std::mutex> lock(cache.Mutex);
        cache.Stats = SpriteMaterialCacheStats{};
    }

    size_t SpriteMaterialFactory::GetTemplateCacheEntryCount()
    {
        SpriteTemplateCache& cache = GetTemplateCache();
        std::lock_guard<std::mutex> lock(cache.Mutex);
        size_t count = 0;
        count += cache.Opaque ? 1u : 0u;
        count += cache.Translucent ? 1u : 0u;
        return count;
    }

    void SpriteMaterialFactory::ClearTemplateCache()
    {
        SpriteTemplateCache& cache = GetTemplateCache();
        std::lock_guard<std::mutex> lock(cache.Mutex);
        cache.Opaque.reset();
        cache.Translucent.reset();
        cache.RhiSerial = 0;
        cache.Stats = SpriteMaterialCacheStats{};
    }

    void SpriteMaterialFactory::ApplyColorAndTexture(
        Material& material,
        const LinearColor& color,
        const std::shared_ptr<Texture2D>& texture)
    {
        ApplyColorTextureAndUVRect(material, color, texture, Vector4(0.0f, 0.0f, 1.0f, 1.0f));
    }

    void SpriteMaterialFactory::ApplyColorTextureAndUVRect(
        Material& material,
        const LinearColor& color,
        const std::shared_ptr<Texture2D>& texture,
        const Vector4& uvRect)
    {
        material.SetTextureParameter("BaseColor", texture);
        material.SetScalarParameter("TintR", color.R);
        material.SetScalarParameter("TintG", color.G);
        material.SetScalarParameter("TintB", color.B);
        material.SetScalarParameter("Opacity", color.A);
        material.SetScalarParameter("UVRectU0", uvRect.x);
        material.SetScalarParameter("UVRectV0", uvRect.y);
        material.SetScalarParameter("UVRectU1", uvRect.z);
        material.SetScalarParameter("UVRectV1", uvRect.w);
    }
}
