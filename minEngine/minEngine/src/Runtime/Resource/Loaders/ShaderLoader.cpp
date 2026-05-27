#include "Runtime/Resource/Loaders/ShaderLoader.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/Shader.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetResources/ShaderResource.h"

namespace minEngine
{
    std::shared_ptr<Shader> ShaderLoader::LoadFromAssetMeta(const AssetMeta& meta)
    {
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        ShaderResource resource;
        Serialization::JsonReaderArchive archive;
        const Serialization::SerializeResult result = Serialization::Serializer::FromFile(
            absoluteAssetPath,
            "minEngine::ShaderResource",
            &resource,
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = false,
                .allowObjectPtrSerialization = true});
        if (!result.ok)
        {
            ME_CORE_ERROR(
                "ShaderLoader: failed to deserialize '{}' — {} (field: {})",
                absoluteAssetPath,
                result.message,
                result.fieldPath);
            return nullptr;
        }

        std::shared_ptr<Shader> shader = NewObject<Shader>(meta.AssetName, nullptr, meta.Guid);
        std::string compileError;
        if (!shader->CompileFromFiles(
                *RenderSystem::Get().GetRHI(),
                resource.m_VertexPath,
                resource.m_FragmentPath,
                &compileError))
        {
            ME_CORE_ERROR(
                "ShaderLoader: failed to compile '{}' (vertex: '{}', fragment: '{}') — {}",
                absoluteAssetPath,
                resource.m_VertexPath,
                resource.m_FragmentPath,
                compileError);
            return nullptr;
        }
        return shader;
    }

    template<>
    std::shared_ptr<Shader> AssetManager::LoadAsset_Impl<Shader>(const AssetMeta& meta)
    {
        return ShaderLoader::LoadFromAssetMeta(meta);
    }
}
