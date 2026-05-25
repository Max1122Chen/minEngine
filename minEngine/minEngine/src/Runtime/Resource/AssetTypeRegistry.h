#pragma once

#include "Core.h"
#include "Runtime/Platform/FileDialog/FileDialogTypes.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    struct AssetTypeDescriptor
    {
        std::string AssetTypeId;
        std::string RuntimeClassName;
        std::vector<std::string> Extensions;
        std::string FileDialogFilterLabel;
    };

    class AssetTypeRegistry
    {
    public:
        static AssetTypeRegistry& Get();

        void RegisterBuiltinTypes();
        void RegisterType(const AssetTypeDescriptor& descriptor);

        const AssetTypeDescriptor* FindByExtension(std::string_view extension) const;
        const AssetTypeDescriptor* FindByAssetTypeId(std::string_view assetTypeId) const;

        std::string InferAssetTypeFromExtension(const std::filesystem::path& path) const;
        std::string InferAssetTypeFromRuntimeClassName(std::string_view runtimeClassName) const;

        std::vector<std::string> BuildFileDialogFilterSpec() const;
        std::vector<FileDialogFilter> BuildFileDialogFilters() const;
        const std::vector<AssetTypeDescriptor>& GetDescriptors() const { return m_Descriptors; }

    private:
        AssetTypeRegistry() = default;

        static std::string NormalizeExtension(std::string_view extension);
        std::string BuildExtensionSpec(const std::vector<std::string>& extensions) const;

        std::vector<AssetTypeDescriptor> m_Descriptors;
    };
}
