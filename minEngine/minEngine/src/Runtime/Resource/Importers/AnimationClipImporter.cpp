#include "Runtime/Resource/Importers/AnimationClipImporter.h"

#include "Runtime/Resource/Importers/AssetImportHelpers.h"
#include "Runtime/Resource/AssetTypeRegistry.h"
#include "Runtime/Resource/Loaders/AnimationClipLoader.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Function/Animation/Skeleton.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"

#include <algorithm>
#include <cmath>
#include <system_error>

namespace minEngine
{
    bool AnimationClipImporter::AcceptsSourceExtension(std::string_view extension)
    {
        return AssetTypeRegistry::IsExternalMeshSourceExtension(extension);
    }

    ImportResult AnimationClipImporter::Import(AssetManager& manager, const ImportRequest& request)
    {
        std::string skeletonAssetPath = request.SkeletonAssetPath;
        if (skeletonAssetPath.empty())
        {
            const PathRegistry& paths = PathRegistry::Get();
            const std::filesystem::path& contentRoot = paths.GetProjectContentRoot();
            std::filesystem::path absoluteDestDirectory = request.DestDirectory.is_absolute()
                ? std::filesystem::weakly_canonical(request.DestDirectory)
                : std::filesystem::weakly_canonical(contentRoot / request.DestDirectory);

            const std::filesystem::path fallbackAbsolute =
                absoluteDestDirectory
                / (request.SourcePath.stem().string() + "_Skeleton.meskeleton");
            if (std::filesystem::exists(fallbackAbsolute))
            {
                std::error_code relativeError;
                const std::filesystem::path relativePath =
                    std::filesystem::relative(fallbackAbsolute, contentRoot, relativeError);
                if (!relativeError && !relativePath.empty())
                {
                    skeletonAssetPath = relativePath.generic_string();
                }
            }
        }

        if (skeletonAssetPath.empty())
        {
            ImportResult result;
            result.ErrorMessage =
                "AnimationClip import requires SkeletonAssetPath "
                "(or an existing {stem}_Skeleton.meskeleton next to the destination)";
            return result;
        }

        return AssetImportHelpers::MakeResultFromLegacy(
            Cook(
                manager,
                request.SourcePath,
                request.DestDirectory,
                skeletonAssetPath,
                request.AnimationIndex,
                request.bOverwriteExisting));
    }

    namespace
    {
        Vector3 ToVector3(const aiVector3D& value)
        {
            return Vector3(value.x, value.y, value.z);
        }

        Quaternion ToQuaternion(const aiQuaternion& value)
        {
            return Quaternion::FromGlm(glm::normalize(glm::quat(value.w, value.x, value.y, value.z)));
        }

        float TicksToSeconds(double timeInTicks, double ticksPerSecond)
        {
            const double rate = (ticksPerSecond > 1e-8) ? ticksPerSecond : 25.0;
            return static_cast<float>(timeInTicks / rate);
        }
    }

    int AnimationClipImporter::CountAnimationsInSourceFile(const std::string& path, std::string* outError)
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path.c_str(), 0);
        if (scene == nullptr)
        {
            if (outError != nullptr)
            {
                *outError = importer.GetErrorString();
            }
            return -1;
        }
        return static_cast<int>(scene->mNumAnimations);
    }

    bool AnimationClipImporter::ParseClipFromSourceFile(
        const std::string& path,
        const std::shared_ptr<Skeleton>& skeleton,
        int animationIndex,
        AnimationClip& outClip,
        std::string* outError)
    {
        auto setError = [outError](const std::string& message)
        {
            if (outError != nullptr)
            {
                *outError = message;
            }
        };

        if (skeleton == nullptr || skeleton->GetBoneCount() <= 0)
        {
            setError("skeleton is null or empty");
            return false;
        }

        Assimp::Importer importer;
        // Prefer minimal flags for animation reads; geometry post-process is unnecessary here.
        const aiScene* scene = importer.ReadFile(path.c_str(), 0);
        if (scene == nullptr)
        {
            setError(std::string("Assimp ReadFile failed: ") + importer.GetErrorString());
            ME_LOG(LogAsset, Error, 
                "AnimationClipImporter: Assimp failed for '{}': {}",
                path,
                importer.GetErrorString());
            return false;
        }

        if (scene->mNumAnimations == 0)
        {
            setError(
                "FBX/glTF opened but contains 0 animations (mesh/rig only). "
                "Import a source file that includes animation takes/clips.");
            ME_LOG(LogAsset, Error, 
                "AnimationClipImporter: '{}' has meshes={} bones-nodes ok, but mNumAnimations=0.",
                path,
                scene->mNumMeshes);
            return false;
        }

        if (animationIndex < 0 || animationIndex >= static_cast<int>(scene->mNumAnimations))
        {
            setError(
                "animationIndex out of range (file has "
                + std::to_string(scene->mNumAnimations)
                + " animation(s))");
            return false;
        }

        const aiAnimation* animation = scene->mAnimations[animationIndex];
        if (animation == nullptr)
        {
            setError("animation pointer is null");
            return false;
        }

        const double ticksPerSecond =
            (animation->mTicksPerSecond > 1e-8) ? animation->mTicksPerSecond : 25.0;
        const float durationSeconds = TicksToSeconds(animation->mDuration, ticksPerSecond);

        std::vector<AnimationTrack> tracks;
        tracks.reserve(animation->mNumChannels);
        int unmatchedChannels = 0;

        for (unsigned int channelIndex = 0; channelIndex < animation->mNumChannels; ++channelIndex)
        {
            const aiNodeAnim* nodeAnim = animation->mChannels[channelIndex];
            if (nodeAnim == nullptr)
            {
                continue;
            }

            const int32_t boneIndex = skeleton->FindBoneIndex(nodeAnim->mNodeName.C_Str());
            if (boneIndex < 0)
            {
                ++unmatchedChannels;
                continue;
            }

            AnimationTrack track;
            track.BoneIndex = boneIndex;

            track.PositionKeys.reserve(nodeAnim->mNumPositionKeys);
            for (unsigned int keyIndex = 0; keyIndex < nodeAnim->mNumPositionKeys; ++keyIndex)
            {
                const aiVectorKey& key = nodeAnim->mPositionKeys[keyIndex];
                AnimationVec3Key outKey;
                outKey.Time = TicksToSeconds(key.mTime, ticksPerSecond);
                outKey.Value = ToVector3(key.mValue);
                track.PositionKeys.push_back(outKey);
            }

            track.RotationKeys.reserve(nodeAnim->mNumRotationKeys);
            for (unsigned int keyIndex = 0; keyIndex < nodeAnim->mNumRotationKeys; ++keyIndex)
            {
                const aiQuatKey& key = nodeAnim->mRotationKeys[keyIndex];
                AnimationQuatKey outKey;
                outKey.Time = TicksToSeconds(key.mTime, ticksPerSecond);
                outKey.Value = ToQuaternion(key.mValue);
                track.RotationKeys.push_back(outKey);
            }

            track.ScaleKeys.reserve(nodeAnim->mNumScalingKeys);
            for (unsigned int keyIndex = 0; keyIndex < nodeAnim->mNumScalingKeys; ++keyIndex)
            {
                const aiVectorKey& key = nodeAnim->mScalingKeys[keyIndex];
                AnimationVec3Key outKey;
                outKey.Time = TicksToSeconds(key.mTime, ticksPerSecond);
                outKey.Value = ToVector3(key.mValue);
                track.ScaleKeys.push_back(outKey);
            }

            tracks.push_back(std::move(track));
        }

        if (tracks.empty())
        {
            setError("no animation channels matched skeleton bone names");
            ME_LOG(LogAsset, Error, 
                "AnimationClipImporter: zero matched tracks in '{}' (unmatched={}).",
                path,
                unmatchedChannels);
            return false;
        }

        if (unmatchedChannels > 0)
        {
            ME_LOG(LogAsset, Warn, 
                "AnimationClipImporter: {} channel(s) unmatched to skeleton in '{}'.",
                unmatchedChannels,
                path);
        }

        outClip.SetSkeleton(skeleton);
        outClip.SetDuration(std::max(durationSeconds, 0.0f));
        outClip.SetTracks(std::move(tracks));
        outClip.SetNamedFloatTracks({});
        return true;
    }

    ImportAssetResult AnimationClipImporter::Cook(
        AssetManager& manager,
        const std::filesystem::path& sourcePath,
        const std::filesystem::path& destDirectory,
        std::string_view skeletonAssetPath,
        int animationIndex,
        bool bOverwriteExisting)
{
        AssetManager::AssetRegistryBroadcastBatchScope batchScope;
        ImportAssetResult result;

        if (!std::filesystem::exists(sourcePath) || !std::filesystem::is_regular_file(sourcePath))
        {
            result.ErrorMessage = "source file does not exist: " + sourcePath.string();
            return result;
        }

        if (!AssetTypeRegistry::IsExternalMeshSourceExtension(sourcePath.extension().string()))
        {
            result.ErrorMessage =
                "ImportAnimationClip expects .fbx/.gltf/.glb, got: " + sourcePath.extension().string();
            return result;
        }

        if (skeletonAssetPath.empty())
        {
            result.ErrorMessage = "skeletonAssetPath is empty";
            return result;
        }

        const PathRegistry& paths = PathRegistry::Get();
        const std::filesystem::path& contentRoot = paths.GetProjectContentRoot();
        if (contentRoot.empty())
        {
            result.ErrorMessage = "project content root is not set";
            return result;
        }

        std::shared_ptr<Skeleton> skeleton = manager.LoadAsset<Skeleton>(std::string(skeletonAssetPath));
        if (skeleton == nullptr)
        {
            result.ErrorMessage = "failed to load Skeleton: " + std::string(skeletonAssetPath);
            return result;
        }

        std::filesystem::path absoluteDestDirectory = destDirectory.is_absolute()
            ? std::filesystem::weakly_canonical(destDirectory)
            : std::filesystem::weakly_canonical(contentRoot / destDirectory);

        if (!manager.IsUnderProjectContentRoot(absoluteDestDirectory))
        {
            result.ErrorMessage =
                "destination directory is outside project Assets: " + absoluteDestDirectory.string();
            return result;
        }

        const std::filesystem::path sourcesDirectory = contentRoot / "Sources";
        std::error_code sourcesCreateError;
        std::filesystem::create_directories(sourcesDirectory, sourcesCreateError);
        if (sourcesCreateError)
        {
            result.ErrorMessage = "failed to create Assets/Sources: " + sourcesCreateError.message();
            return result;
        }

        const std::filesystem::path sourceCopyPath = sourcesDirectory / sourcePath.filename();
        manager.NoteEditorFilesystemMutation(sourcesDirectory);
        manager.NoteEditorFilesystemMutation(sourceCopyPath);

        if (!std::filesystem::exists(sourceCopyPath))
        {
            std::error_code copySourceError;
            std::filesystem::copy_file(
                sourcePath,
                sourceCopyPath,
                std::filesystem::copy_options::none,
                copySourceError);
            if (copySourceError)
            {
                result.ErrorMessage =
                    "failed to copy source into Assets/Sources: " + copySourceError.message();
                return result;
            }
        }
        else if (!std::filesystem::equivalent(sourceCopyPath, sourcePath))
        {
            if (bOverwriteExisting)
            {
                std::error_code copySourceError;
                std::filesystem::copy_file(
                    sourcePath,
                    sourceCopyPath,
                    std::filesystem::copy_options::overwrite_existing,
                    copySourceError);
                if (copySourceError)
                {
                    result.ErrorMessage =
                        "failed to refresh Assets/Sources copy: " + copySourceError.message();
                    return result;
                }
            }
            else
            {
                ME_LOG(LogAsset, Info, 
                    "ImportAnimationClip: reusing existing Sources copy '{}'",
                    sourceCopyPath.string());
            }
        }

        AnimationClip importedClip;
        std::string importError;
        if (!AnimationClipImporter::ParseClipFromSourceFile(
                sourceCopyPath.string(),
                skeleton,
                animationIndex,
                importedClip,
                &importError))
        {
            result.ErrorMessage = importError;
            return result;
        }

        std::filesystem::path productFileName = sourcePath.stem();
        if (animationIndex > 0)
        {
            productFileName += "_";
            productFileName += std::to_string(animationIndex);
        }
        productFileName += ".meaclip";
        const std::filesystem::path productPath = absoluteDestDirectory / productFileName;
        if (std::filesystem::exists(productPath) && !bOverwriteExisting)
        {
            result.ErrorMessage = "destination clip already exists: " + productPath.string();
            return result;
        }

        manager.NoteEditorFilesystemMutation(absoluteDestDirectory);
        manager.NoteEditorFilesystemMutation(productPath);

        // Write clip + meta with a stable Guid before RegisterAsset (same order as Skeleton cook).
        AssetMeta pendingMeta;
        pendingMeta.AssetName = productFileName.stem().string();
        pendingMeta.AssetPath =
            manager.NormalizeProjectRelativeAssetPath(productPath.string());
        pendingMeta.AssetType = "AnimationClip";
        pendingMeta.Guid = GenerateGUID();
        if (pendingMeta.AssetPath.empty())
        {
            result.ErrorMessage = "failed to resolve AnimationClip destination path";
            return result;
        }

        if (bOverwriteExisting)
        {
            if (const AssetMeta* existingMeta = manager.FindAssetMetaByPath(pendingMeta.AssetPath))
            {
                pendingMeta.Guid = existingMeta->Guid;
                if (!existingMeta->SourcePath.empty())
                {
                    pendingMeta.SourcePath = existingMeta->SourcePath;
                }
            }
        }

        manager.ApplyMetaIdentity(importedClip, pendingMeta);

        std::string saveError;
        if (!AnimationClipLoader::Save(pendingMeta, importedClip, &saveError))
        {
            result.ErrorMessage = saveError;
            return result;
        }

        if (!manager.WriteOrUpdateMetaFile(pendingMeta))
        {
            result.ErrorMessage = "failed to write AnimationClip meta";
            return result;
        }

        result.Meta = manager.RegisterAsset(productPath.string(), "AnimationClip");
        if (result.Meta.AssetPath.empty())
        {
            result.ErrorMessage = "failed to register AnimationClip asset";
            return result;
        }

        result.Meta.SourcePath = manager.NormalizeProjectRelativeAssetPath(sourceCopyPath.string());
        if (!manager.WriteMetaFile(result.Meta))
        {
            result.ErrorMessage = "failed to write SourcePath into meta";
            return result;
        }
        manager.CacheMeta(result.Meta, true);
        manager.NoteEditorFilesystemMutation(manager.BuildMetaAbsolutePath(result.Meta.AssetPath));

        if (bOverwriteExisting)
        {
            manager.EvictLoadedAssetCache(result.Meta.AssetPath);
        }

        result.bSuccess = true;
        return result;
    }

}
