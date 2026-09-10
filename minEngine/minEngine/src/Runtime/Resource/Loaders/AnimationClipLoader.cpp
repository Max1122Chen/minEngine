#include "Runtime/Resource/Loaders/AnimationClipLoader.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Animation/Skeleton.h"
#include "Runtime/Resource/AssetManager.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"

#include <algorithm>
#include <cmath>

namespace minEngine
{
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

    int AnimationClipLoader::CountAnimationsInFile(const std::string& path, std::string* outError)
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

    bool AnimationClipLoader::ImportFromFile(
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
            ME_CORE_ERROR(
                "AnimationClipLoader: Assimp failed for '{}': {}",
                path,
                importer.GetErrorString());
            return false;
        }

        if (scene->mNumAnimations == 0)
        {
            setError(
                "FBX/glTF opened but contains 0 animations (mesh/rig only). "
                "Import a source file that includes animation takes/clips.");
            ME_CORE_ERROR(
                "AnimationClipLoader: '{}' has meshes={} bones-nodes ok, but mNumAnimations=0.",
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
            ME_CORE_ERROR(
                "AnimationClipLoader: zero matched tracks in '{}' (unmatched={}).",
                path,
                unmatchedChannels);
            return false;
        }

        if (unmatchedChannels > 0)
        {
            ME_CORE_WARN(
                "AnimationClipLoader: {} channel(s) unmatched to skeleton in '{}'.",
                unmatchedChannels,
                path);
        }

        outClip.SetSkeleton(skeleton);
        outClip.SetDuration(std::max(durationSeconds, 0.0f));
        outClip.SetTracks(std::move(tracks));
        outClip.SetNamedFloatTracks({});
        return true;
    }

    bool AnimationClipLoader::Save(const AssetMeta& meta, const AnimationClip& clip, std::string* outError)
    {
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        Serialization::JsonWriterArchive archive;
        const Serialization::SerializeResult serializeResult = Serialization::Serializer::ToFile(
            absoluteAssetPath,
            Reflection::GetClassName<AnimationClip>(),
            &clip,
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = false
                });

        if (!serializeResult.ok)
        {
            if (outError != nullptr)
            {
                *outError = serializeResult.message;
            }
            ME_CORE_ERROR(
                "AnimationClipLoader: Save failed for '{}' — {} (field: {})",
                meta.AssetPath,
                serializeResult.message,
                serializeResult.fieldPath);
            return false;
        }
        return true;
    }

    std::shared_ptr<AnimationClip> AnimationClipLoader::Load(const AssetMeta& meta, std::string* outError)
    {
        std::shared_ptr<AnimationClip> clip = NewObject<AnimationClip>(meta.AssetName, nullptr, meta.Guid);

        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        Serialization::JsonReaderArchive archive;
        const Serialization::SerializeResult deserializeResult = Serialization::Serializer::FromFile(
            absoluteAssetPath,
            Reflection::GetClassName<AnimationClip>(),
            clip.get(),
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = true
                });

        if (!deserializeResult.ok)
        {
            if (outError != nullptr)
            {
                *outError = deserializeResult.message;
            }
            ME_CORE_ERROR(
                "AnimationClipLoader: Load failed for '{}' — {} (field: {})",
                meta.AssetPath,
                deserializeResult.message,
                deserializeResult.fieldPath);
            return nullptr;
        }

        AssetManager::Get().ApplyMetaIdentity(*clip, meta);

        if (clip->GetSkeleton() == nullptr)
        {
            ME_CORE_WARN(
                "AnimationClipLoader: '{}' loaded without resolved Skeleton reference.",
                meta.AssetPath);
        }

        return clip;
    }

    template <>
    std::shared_ptr<AnimationClip> AssetManager::LoadAsset_Impl<AnimationClip>(const AssetMeta& meta)
    {
        return AnimationClipLoader::Load(meta);
    }
}
