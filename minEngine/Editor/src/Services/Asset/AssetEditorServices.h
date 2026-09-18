#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    class AssetMeta;
    class IEditorContext;

    struct AssetEditorResult
    {
        bool Ok = false;
        std::string Message;
    };

    /** Asset library primitives. Create/delete are not Scene undo units. */
    class AssetEditorServices
    {
    public:
        AssetEditorResult List(
            std::string_view directory,
            std::string_view assetType,
            std::vector<const AssetMeta*>& outMetas) const;

        AssetEditorResult Create(
            std::string_view assetType,
            std::string_view directory,
            std::string_view name,
            std::string& outPath,
            std::string& outGuid);

        AssetEditorResult Delete(IEditorContext& context, std::string_view assetPath);

    private:
        static AssetEditorResult OkResult();
        static AssetEditorResult Fail(std::string message);
    };
}
