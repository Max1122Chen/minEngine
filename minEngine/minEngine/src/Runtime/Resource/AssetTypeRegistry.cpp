#include "AssetTypeRegistry.h"

#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Shader.h"
#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Core/Reflection/Reflection.h"

#include <algorithm>
#include <cctype>

namespace minEngine
{
    AssetTypeRegistry& AssetTypeRegistry::Get()
    {
        static AssetTypeRegistry instance;
        return instance;
    }

    std::string AssetTypeRegistry::NormalizeExtension(std::string_view extension)
    {
        std::string normalized(extension);
        if (normalized.empty())
        {
            return normalized;
        }

        if (normalized.front() != '.')
        {
            normalized.insert(normalized.begin(), '.');
        }

        for (char& character : normalized)
        {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }

        return normalized;
    }

    void AssetTypeRegistry::RegisterBuiltinTypes()
    {
        if (!m_Descriptors.empty())
        {
            return;
        }

        using namespace Reflection;

        RegisterType(AssetTypeDescriptor{
            .AssetTypeId = "Texture2D",
            .RuntimeClassName = GetClassName<Texture2D>(),
            .Extensions = {".png", ".jpg", ".jpeg"},
            .FileDialogFilterLabel = "Texture2D (*.png;*.jpg;*.jpeg)"});

        RegisterType(AssetTypeDescriptor{
            .AssetTypeId = "StaticMesh",
            .RuntimeClassName = GetClassName<StaticMesh>(),
            .Extensions = {".obj", ".fbx", ".gltf"},
            .FileDialogFilterLabel = "Static Mesh (*.obj;*.fbx;*.gltf)"});

        RegisterType(AssetTypeDescriptor{
            .AssetTypeId = "Material",
            .RuntimeClassName = GetClassName<Material>(),
            .Extensions = {".memtl"},
            .FileDialogFilterLabel = "Material (*.memtl)"});

        RegisterType(AssetTypeDescriptor{
            .AssetTypeId = "Shader",
            .RuntimeClassName = GetClassName<Shader>(),
            .Extensions = {".meshader"},
            .FileDialogFilterLabel = "Shader (*.meshader)"});

        RegisterType(AssetTypeDescriptor{
            .AssetTypeId = "Scene",
            .RuntimeClassName = GetClassName<Scene>(),
            .Extensions = {".mescene"},
            .FileDialogFilterLabel = "Scene (*.mescene)"});
    }

    void AssetTypeRegistry::RegisterType(const AssetTypeDescriptor& descriptor)
    {
        if (descriptor.AssetTypeId.empty())
        {
            return;
        }

        for (const AssetTypeDescriptor& existing : m_Descriptors)
        {
            if (existing.AssetTypeId == descriptor.AssetTypeId)
            {
                return;
            }
        }

        m_Descriptors.push_back(descriptor);
    }

    const AssetTypeDescriptor* AssetTypeRegistry::FindByExtension(std::string_view extension) const
    {
        const std::string normalizedExtension = NormalizeExtension(extension);
        for (const AssetTypeDescriptor& descriptor : m_Descriptors)
        {
            for (const std::string& candidate : descriptor.Extensions)
            {
                if (NormalizeExtension(candidate) == normalizedExtension)
                {
                    return &descriptor;
                }
            }
        }

        return nullptr;
    }

    const AssetTypeDescriptor* AssetTypeRegistry::FindByAssetTypeId(std::string_view assetTypeId) const
    {
        for (const AssetTypeDescriptor& descriptor : m_Descriptors)
        {
            if (descriptor.AssetTypeId == assetTypeId)
            {
                return &descriptor;
            }
        }

        return nullptr;
    }

    std::string AssetTypeRegistry::InferAssetTypeFromExtension(const std::filesystem::path& path) const
    {
        const AssetTypeDescriptor* descriptor = FindByExtension(path.extension().string());
        return descriptor != nullptr ? descriptor->AssetTypeId : std::string();
    }

    std::string AssetTypeRegistry::InferAssetTypeFromRuntimeClassName(std::string_view runtimeClassName) const
    {
        for (const AssetTypeDescriptor& descriptor : m_Descriptors)
        {
            if (descriptor.RuntimeClassName == runtimeClassName)
            {
                return descriptor.AssetTypeId;
            }
        }

        return std::string();
    }

    std::vector<std::string> AssetTypeRegistry::BuildFileDialogFilterSpec() const
    {
        std::vector<std::string> filters;
        filters.reserve(m_Descriptors.size());
        for (const AssetTypeDescriptor& descriptor : m_Descriptors)
        {
            filters.push_back(descriptor.FileDialogFilterLabel);
        }

        return filters;
    }
}
