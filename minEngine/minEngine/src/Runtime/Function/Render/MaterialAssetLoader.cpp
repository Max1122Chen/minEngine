#include "MaterialAssetLoader.h"

#include "Material/MaterialCompiler/MaterialCompiler.h"
#include "Material.h"
#include "RenderSystem.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/MaterialLoader.h"

namespace minEngine
{
    std::shared_ptr<Material> MaterialAssetLoader::LoadFromAssetMeta(const AssetMeta& meta)
    {
        std::shared_ptr<Material> material = MaterialLoader::LoadDeserialized(meta);
        if (!material)
        {
            return nullptr;
        }

        RHI* rhi = RenderSystem::Get().GetRHI();
        if (rhi == nullptr)
        {
            ME_CORE_ERROR("MaterialAssetLoader: RHI unavailable while loading {}.", meta.AssetPath);
            return nullptr;
        }

        MaterialCompileContext ctx;
        ctx.RHI = rhi;
        if (!MaterialCompiler::Compile(*material, ctx))
        {
            ME_CORE_ERROR("MaterialAssetLoader: compile failed for {}.", meta.AssetPath);
            for (const MaterialCompileDiagnostic& diagnostic : material->m_LastCompileDiagnostics)
            {
                ME_CORE_ERROR("  {}", diagnostic.Message);
            }
            return nullptr;
        }

        return material;
    }

    template<>
    std::shared_ptr<Material> AssetManager::LoadAsset_Impl<Material>(const AssetMeta& meta)
    {
        return MaterialAssetLoader::LoadFromAssetMeta(meta);
    }
}
