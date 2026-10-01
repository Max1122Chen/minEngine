#pragma once

#include "Runtime/Function/Animation/AnimationClip.h"
#include "Runtime/Resource/AssetManager.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace minEngine
{
    class Skeleton;

    /**
     * ASSET-F06: Assimp source → .meaclip cook + Register.
     * Clip Assimp parse lives here (Import), not on AnimationClipLoader (Load .meaclip).
     */
    class AnimationClipImporter
    {
    public:
        static bool AcceptsSourceExtension(std::string_view extension);
        static ImportResult Import(AssetManager& manager, const ImportRequest& request);

        static int CountAnimationsInSourceFile(const std::string& path, std::string* outError = nullptr);

        static bool ParseClipFromSourceFile(
            const std::string& path,
            const std::shared_ptr<Skeleton>& skeleton,
            int animationIndex,
            AnimationClip& outClip,
            std::string* outError = nullptr);

    private:
        static ImportAssetResult Cook(
            AssetManager& manager,
            const std::filesystem::path& sourcePath,
            const std::filesystem::path& destDirectory,
            std::string_view skeletonAssetPath,
            int animationIndex,
            bool bOverwriteExisting);
    };
}
