#include "Runtime/Resource/Loaders/SkeletalMeshLoader.h"

#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/SkeletalMesh.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/Loaders/AssimpMeshImportUtil.h"
#include "Runtime/Resource/Loaders/SkeletonLoader.h"

#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <limits>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace minEngine
{
    namespace
    {
        struct InfluenceAccum
        {
            int32_t BoneIndex = -1;
            float Weight = 0.0f;
        };

        Matrix4 ComputeNodeGlobalTransform(const aiNode* node)
        {
            if (node == nullptr)
            {
                return Matrix4(1.0f);
            }
            return ComputeNodeGlobalTransform(node->mParent)
                * AssimpMeshImportUtil::ConvertMatrix(node->mTransformation);
        }

        const aiNode* FindNodeByName(const aiNode* node, const std::string& name)
        {
            if (node == nullptr)
            {
                return nullptr;
            }
            if (node->mName.C_Str() == name)
            {
                return node;
            }
            for (unsigned int childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
            {
                if (const aiNode* found = FindNodeByName(node->mChildren[childIndex], name))
                {
                    return found;
                }
            }
            return nullptr;
        }

        void NormalizeInfluences(SkeletalMeshImportVertex& vertex)
        {
            float weightSum = 0.0f;
            for (int influence = 0; influence < kMaxBoneInfluences; ++influence)
            {
                weightSum += (&vertex.BoneWeights.x)[influence];
            }
            if (weightSum <= 1e-8f)
            {
                vertex.BoneIndices[0] = 0;
                vertex.BoneWeights = Vector4(1.0f, 0.0f, 0.0f, 0.0f);
                return;
            }
            vertex.BoneWeights /= weightSum;
        }

        void AssignInfluences(
            SkeletalMeshImportVertex& vertex,
            std::vector<InfluenceAccum>& influences)
        {
            std::sort(
                influences.begin(),
                influences.end(),
                [](const InfluenceAccum& a, const InfluenceAccum& b) { return a.Weight > b.Weight; });

            if (static_cast<int>(influences.size()) > kMaxBoneInfluences)
            {
                influences.resize(static_cast<size_t>(kMaxBoneInfluences));
            }

            vertex.BoneWeights = Vector4(0.0f);
            for (int influence = 0; influence < kMaxBoneInfluences; ++influence)
            {
                vertex.BoneIndices[influence] = 0;
            }

            for (size_t influence = 0; influence < influences.size(); ++influence)
            {
                vertex.BoneIndices[influence] =
                    static_cast<uint16_t>(std::max(influences[influence].BoneIndex, 0));
                (&vertex.BoneWeights.x)[influence] = influences[influence].Weight;
            }
            NormalizeInfluences(vertex);
        }
    }

    bool SkeletalMeshLoader::ImportFromFile(
        const std::string& path,
        SkeletalMeshImportData& outData,
        std::string* outError)
    {
        outData = {};

        Assimp::Importer importer;
        const aiScene* scene =
            importer.ReadFile(path.c_str(), AssimpMeshImportUtil::GetDefaultPostProcessFlags());

        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
        {
            if (outError)
            {
                *outError = std::string("Assimp failed: ") + importer.GetErrorString();
            }
            ME_CORE_ERROR("SkeletalMeshLoader: Assimp failed for {}. {}", path, importer.GetErrorString());
            return false;
        }

        std::unordered_map<std::string, Matrix4> inverseBindByName;
        std::unordered_set<std::string> boneNames;
        for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
        {
            const aiMesh* mesh = scene->mMeshes[meshIndex];
            if (mesh == nullptr)
            {
                continue;
            }
            for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
            {
                const aiBone* bone = mesh->mBones[boneIndex];
                if (bone == nullptr)
                {
                    continue;
                }
                const std::string name = bone->mName.C_Str();
                boneNames.insert(name);
                if (inverseBindByName.find(name) == inverseBindByName.end())
                {
                    inverseBindByName.emplace(name, AssimpMeshImportUtil::ConvertMatrix(bone->mOffsetMatrix));
                }
            }
        }

        if (boneNames.empty())
        {
            if (outError)
            {
                *outError = "No skinned bones found in file.";
            }
            ME_CORE_ERROR("SkeletalMeshLoader: no bones in {}.", path);
            return false;
        }

        if (static_cast<int32_t>(boneNames.size()) > kMaxBonesPerSkeleton)
        {
            if (outError)
            {
                *outError = "Bone count exceeds kMaxBonesPerSkeleton.";
            }
            ME_CORE_ERROR("SkeletalMeshLoader: too many bones in {}.", path);
            return false;
        }

        std::unordered_map<std::string, int32_t> boneNameToIndex;
        std::vector<SkeletonBone> bones;
        bones.reserve(boneNames.size());

        std::function<void(const aiNode*, int32_t)> visitNode =
            [&](const aiNode* node, int32_t parentBoneIndex)
            {
                if (node == nullptr)
                {
                    return;
                }

                const std::string nodeName = node->mName.C_Str();
                int32_t currentBoneIndex = parentBoneIndex;
                if (boneNames.find(nodeName) != boneNames.end()
                    && boneNameToIndex.find(nodeName) == boneNameToIndex.end())
                {
                    SkeletonBone bone;
                    bone.Name = nodeName;
                    bone.ParentIndex = parentBoneIndex;

                    const Matrix4 nodeGlobal = ComputeNodeGlobalTransform(node);
                    Matrix4 parentGlobal(1.0f);
                    if (parentBoneIndex >= 0)
                    {
                        const aiNode* parentNode =
                            FindNodeByName(scene->mRootNode, bones[static_cast<size_t>(parentBoneIndex)].Name);
                        parentGlobal = ComputeNodeGlobalTransform(parentNode);
                    }
                    const Matrix4 localMatrix = glm::inverse(parentGlobal) * nodeGlobal;
                    AssimpMeshImportUtil::DecomposeMatrix(localMatrix, bone.LocalBind);

                    const auto invBindIt = inverseBindByName.find(nodeName);
                    bone.InverseBindPose =
                        invBindIt != inverseBindByName.end() ? invBindIt->second : Matrix4(1.0f);

                    currentBoneIndex = static_cast<int32_t>(bones.size());
                    boneNameToIndex.emplace(nodeName, currentBoneIndex);
                    bones.push_back(std::move(bone));
                }

                for (unsigned int childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
                {
                    visitNode(node->mChildren[childIndex], currentBoneIndex);
                }
            };
        visitNode(scene->mRootNode, -1);

        if (bones.size() != boneNames.size())
        {
            // Orphan bones (named in mesh but missing from node tree): append as roots.
            for (const std::string& name : boneNames)
            {
                if (boneNameToIndex.find(name) != boneNameToIndex.end())
                {
                    continue;
                }
                SkeletonBone bone;
                bone.Name = name;
                bone.ParentIndex = -1;
                bone.LocalBind = Transform{};
                bone.InverseBindPose = inverseBindByName[name];
                boneNameToIndex.emplace(name, static_cast<int32_t>(bones.size()));
                bones.push_back(std::move(bone));
                ME_CORE_WARN("SkeletalMeshLoader: bone '{}' missing from node tree; attached as root.", name);
            }
        }

        outData.Bones = std::move(bones);

        Math::Geometry::AABB boundingBox;
        boundingBox.Min = Vector3(std::numeric_limits<float>::max());
        boundingBox.Max = Vector3(std::numeric_limits<float>::lowest());

        std::queue<aiNode*> nodeQueue;
        nodeQueue.push(scene->mRootNode);
        while (!nodeQueue.empty())
        {
            aiNode* node = nodeQueue.front();
            nodeQueue.pop();

            for (unsigned int meshIndex = 0; meshIndex < node->mNumMeshes; ++meshIndex)
            {
                aiMesh* mesh = scene->mMeshes[node->mMeshes[meshIndex]];
                if (mesh == nullptr || !mesh->HasPositions() || mesh->mVertices == nullptr)
                {
                    continue;
                }

                const bool hasNormals = mesh->HasNormals() && mesh->mNormals != nullptr;
                const bool hasTexCoords = mesh->HasTextureCoords(0) && mesh->mTextureCoords[0] != nullptr;
                const bool hasTangents = mesh->HasTangentsAndBitangents() && mesh->mTangents != nullptr;

                float minX = 0.0f;
                float maxX = 1.0f;
                float minZ = 0.0f;
                float maxZ = 1.0f;
                if (!hasTexCoords && mesh->mNumVertices > 0)
                {
                    minX = maxX = mesh->mVertices[0].x;
                    minZ = maxZ = mesh->mVertices[0].z;
                    for (unsigned int vertexIndex = 1; vertexIndex < mesh->mNumVertices; ++vertexIndex)
                    {
                        minX = std::min(minX, mesh->mVertices[vertexIndex].x);
                        maxX = std::max(maxX, mesh->mVertices[vertexIndex].x);
                        minZ = std::min(minZ, mesh->mVertices[vertexIndex].z);
                        maxZ = std::max(maxZ, mesh->mVertices[vertexIndex].z);
                    }
                }

                const float uvExtentX = std::max(maxX - minX, 1e-6f);
                const float uvExtentZ = std::max(maxZ - minZ, 1e-6f);
                const uint32_t baseVertex = static_cast<uint32_t>(outData.Vertices.size());

                SkeletalMeshImportSection sectionInfo;
                sectionInfo.FirstIndex = static_cast<uint32_t>(outData.Indices.size());
                if (mesh->mMaterialIndex >= 0)
                {
                    sectionInfo.MaterialIndex = static_cast<int32_t>(mesh->mMaterialIndex);
                }

                std::vector<std::vector<InfluenceAccum>> vertexInfluences(mesh->mNumVertices);

                for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
                {
                    const aiBone* bone = mesh->mBones[boneIndex];
                    if (bone == nullptr)
                    {
                        continue;
                    }
                    const auto boneIt = boneNameToIndex.find(bone->mName.C_Str());
                    if (boneIt == boneNameToIndex.end())
                    {
                        continue;
                    }
                    for (unsigned int weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex)
                    {
                        const aiVertexWeight& weight = bone->mWeights[weightIndex];
                        if (weight.mVertexId >= mesh->mNumVertices)
                        {
                            continue;
                        }
                        vertexInfluences[weight.mVertexId].push_back(
                            InfluenceAccum{boneIt->second, weight.mWeight});
                    }
                }

                for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
                {
                    boundingBox.Encapsulate(Vector3(
                        mesh->mVertices[vertexIndex].x,
                        mesh->mVertices[vertexIndex].y,
                        mesh->mVertices[vertexIndex].z));

                    SkeletalMeshImportVertex vertex;
                    vertex.Position = Vector3(
                        mesh->mVertices[vertexIndex].x,
                        mesh->mVertices[vertexIndex].y,
                        mesh->mVertices[vertexIndex].z);
                    if (hasNormals)
                    {
                        vertex.Normal = Vector3(
                            mesh->mNormals[vertexIndex].x,
                            mesh->mNormals[vertexIndex].y,
                            mesh->mNormals[vertexIndex].z);
                    }
                    else
                    {
                        vertex.Normal = Vector3(0.0f, 1.0f, 0.0f);
                    }

                    if (hasTexCoords)
                    {
                        vertex.TexCoord = Vector2(
                            mesh->mTextureCoords[0][vertexIndex].x,
                            mesh->mTextureCoords[0][vertexIndex].y);
                    }
                    else
                    {
                        vertex.TexCoord = Vector2(
                            (vertex.Position.x - minX) / uvExtentX,
                            (vertex.Position.z - minZ) / uvExtentZ);
                    }

                    if (hasTangents)
                    {
                        const Vector3 tangent(
                            mesh->mTangents[vertexIndex].x,
                            mesh->mTangents[vertexIndex].y,
                            mesh->mTangents[vertexIndex].z);
                        Vector3 bitangentValue;
                        const Vector3* bitangentPtr = nullptr;
                        if (mesh->mBitangents != nullptr)
                        {
                            bitangentValue = Vector3(
                                mesh->mBitangents[vertexIndex].x,
                                mesh->mBitangents[vertexIndex].y,
                                mesh->mBitangents[vertexIndex].z);
                            bitangentPtr = &bitangentValue;
                        }
                        vertex.Tangent = AssimpMeshImportUtil::ComputeTangentWithHandedness(
                            vertex.Normal,
                            &tangent,
                            bitangentPtr);
                    }
                    else
                    {
                        vertex.Tangent = AssimpMeshImportUtil::ComputeFallbackTangent(vertex.Normal);
                    }

                    AssignInfluences(vertex, vertexInfluences[vertexIndex]);
                    outData.Vertices.push_back(vertex);
                }

                uint32_t numIndices = 0;
                for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
                {
                    const aiFace& face = mesh->mFaces[faceIndex];
                    for (unsigned int indexInFace = 0; indexInFace < face.mNumIndices; ++indexInFace)
                    {
                        if (face.mIndices[indexInFace] >= mesh->mNumVertices)
                        {
                            continue;
                        }
                        outData.Indices.push_back(face.mIndices[indexInFace] + baseVertex);
                        ++numIndices;
                    }
                }
                sectionInfo.NumIndices = numIndices;
                outData.Sections.push_back(sectionInfo);
            }

            for (unsigned int childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
            {
                nodeQueue.push(node->mChildren[childIndex]);
            }
        }

        if (!outData.IsValid())
        {
            if (outError)
            {
                *outError = "No valid skinned vertices produced.";
            }
            ME_CORE_ERROR("SkeletalMeshLoader: invalid import data for {}.", path);
            return false;
        }

        outData.BoundingBox = boundingBox;
        ME_CORE_INFO(
            "SkeletalMeshLoader: imported {} (bones={}, vertices={}, indices={}).",
            path,
            outData.Bones.size(),
            outData.Vertices.size(),
            outData.Indices.size());
        return true;
    }

    std::shared_ptr<Skeleton> SkeletalMeshLoader::CreateSkeletonFromImport(
        const AssetMeta& meta,
        const SkeletalMeshImportData& data)
    {
        std::shared_ptr<Skeleton> skeleton = NewObject<Skeleton>(meta.AssetName, nullptr, meta.Guid);
        std::string error;
        if (!skeleton->SetBones(data.Bones, &error))
        {
            ME_CORE_ERROR("SkeletalMeshLoader: SetBones failed for {}: {}", meta.AssetPath, error);
            return nullptr;
        }
        return skeleton;
    }

    std::shared_ptr<SkeletalMesh> SkeletalMeshLoader::CreateFromImportData(
        const AssetMeta& meta,
        SkeletalMeshImportData& data,
        const std::shared_ptr<Skeleton>& skeleton)
    {
        if (!data.IsValid() || !skeleton)
        {
            ME_CORE_ERROR("SkeletalMeshLoader: invalid import/skeleton for {}.", meta.AssetPath);
            return nullptr;
        }

        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            ME_CORE_ERROR("SkeletalMeshLoader: RHI is not available.");
            return nullptr;
        }

        std::shared_ptr<SkeletalMesh> mesh = NewObject<SkeletalMesh>(meta.AssetName, nullptr, meta.Guid);
        mesh->SetSkeleton(skeleton);
        mesh->m_BoundingBox = data.BoundingBox;
        mesh->m_Sections.clear();
        mesh->m_Sections.reserve(data.Sections.size());
        for (const SkeletalMeshImportSection& section : data.Sections)
        {
            SkeletalMeshSectionInfo sectionInfo;
            sectionInfo.MaterialIndex = section.MaterialIndex;
            sectionInfo.FirstIndex = section.FirstIndex;
            sectionInfo.NumIndices = section.NumIndices;
            mesh->m_Sections.push_back(sectionInfo);
        }

        struct GPUSkinnedVertex
        {
            Vector3 Position;
            Vector2 TexCoord;
            Vector3 Normal;
            Vector4 Tangent;
            int32_t BoneIndices[4];
            Vector4 BoneWeights;
        };

        std::vector<GPUSkinnedVertex> gpuVertices;
        gpuVertices.reserve(data.Vertices.size());
        for (const SkeletalMeshImportVertex& source : data.Vertices)
        {
            GPUSkinnedVertex vertex{};
            vertex.Position = source.Position;
            vertex.TexCoord = source.TexCoord;
            vertex.Normal = source.Normal;
            vertex.Tangent = source.Tangent;
            for (int influence = 0; influence < kMaxBoneInfluences; ++influence)
            {
                vertex.BoneIndices[influence] = source.BoneIndices[influence];
            }
            vertex.BoneWeights = source.BoneWeights;
            gpuVertices.push_back(vertex);
        }

        RHIBufferCreateDesc vbDesc;
        vbDesc.Usage = RHIBufferUsage::Vertex;
        vbDesc.ByteSize = static_cast<uint32_t>(gpuVertices.size() * sizeof(GPUSkinnedVertex));
        vbDesc.ElementCount = static_cast<uint32_t>(gpuVertices.size());
        mesh->m_VertexBuffer = rhi->RHICreateBuffer(vbDesc, gpuVertices.data());

        mesh->m_VertexInputLayout = rhi->RHICreateVertexInputLayout({
            RHIVertexElement("a_Position", VertexElementType::Float3),
            RHIVertexElement("a_TexCoord", VertexElementType::Float2),
            RHIVertexElement("a_Normal", VertexElementType::Float3),
            RHIVertexElement("a_Tangent", VertexElementType::Float4),
            RHIVertexElement("a_BoneIndices", VertexElementType::Int4),
            RHIVertexElement("a_BoneWeights", VertexElementType::Float4),
        });

        RHIBufferCreateDesc ibDesc;
        ibDesc.Usage = RHIBufferUsage::Index;
        ibDesc.ByteSize = static_cast<uint32_t>(data.Indices.size() * sizeof(uint32_t));
        ibDesc.ElementCount = static_cast<uint32_t>(data.Indices.size());
        mesh->m_IndexBuffer = rhi->RHICreateBuffer(ibDesc, data.Indices.data());

        return mesh;
    }

    std::string SkeletalMeshLoader::BuildBuddyRelativePath(std::string_view meshAssetPath)
    {
        const std::filesystem::path meshPath(meshAssetPath);
        std::filesystem::path buddyPath = meshPath;
        buddyPath.replace_extension(".meskmesh");
        return buddyPath.generic_string();
    }

    bool SkeletalMeshLoader::SaveBuddy(const AssetMeta& meshMeta, const std::shared_ptr<Skeleton>& skeleton)
    {
        if (!skeleton)
        {
            return false;
        }

        SkeletalMesh buddyPayload;
        buddyPayload.SetSkeleton(skeleton);

        const std::string buddyRelativePath = BuildBuddyRelativePath(meshMeta.AssetPath);
        const std::string absoluteBuddyPath =
            AssetManager::Get().ResolveAssetAbsolutePath(buddyRelativePath).string();

        Serialization::JsonWriterArchive archive;
        const Serialization::SerializeResult serializeResult = Serialization::Serializer::ToFile(
            absoluteBuddyPath,
            minEngine::Reflection::GetClassName<SkeletalMesh>(),
            &buddyPayload,
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = false,
                .allowObjectPtrSerialization = true});

        if (!serializeResult.ok)
        {
            ME_CORE_ERROR(
                "SkeletalMeshLoader: SaveBuddy failed for '{}' — {} (field: {})",
                buddyRelativePath,
                serializeResult.message,
                serializeResult.fieldPath);
            return false;
        }

        return true;
    }

    bool SkeletalMeshLoader::TryLoadBuddySkeleton(
        const AssetMeta& meshMeta,
        std::shared_ptr<Skeleton>& outSkeleton)
    {
        outSkeleton.reset();

        const std::string buddyRelativePath = BuildBuddyRelativePath(meshMeta.AssetPath);
        const std::string absoluteBuddyPath =
            AssetManager::Get().ResolveAssetAbsolutePath(buddyRelativePath).string();

        if (!std::filesystem::exists(absoluteBuddyPath))
        {
            return false;
        }

        SkeletalMesh buddyPayload;
        Serialization::JsonReaderArchive archive;
        const Serialization::SerializeResult deserializeResult = Serialization::Serializer::FromFile(
            absoluteBuddyPath,
            minEngine::Reflection::GetClassName<SkeletalMesh>(),
            &buddyPayload,
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = true,
                .allowObjectPtrSerialization = true});

        if (!deserializeResult.ok)
        {
            ME_CORE_WARN(
                "SkeletalMeshLoader: TryLoadBuddySkeleton failed for '{}' — {}",
                buddyRelativePath,
                deserializeResult.message);
            return false;
        }

        outSkeleton = buddyPayload.m_Skeleton;
        return outSkeleton != nullptr;
    }

    bool SkeletalMeshLoader::FinishSkeletalImportCook(
        const AssetMeta& meshMeta,
        const std::filesystem::path& sourceAbsolutePath,
        std::string* outError)
    {
        SkeletalMeshImportData importData;
        if (!ImportFromFile(sourceAbsolutePath.string(), importData, outError))
        {
            return false;
        }

        if (!importData.IsValid())
        {
            if (outError != nullptr)
            {
                *outError = "import data has no skinned bones";
            }
            return false;
        }

        const std::filesystem::path meshPath(meshMeta.AssetPath);
        const std::string skeletonFileName = meshPath.stem().string() + "_Skeleton.meskeleton";
        const std::filesystem::path skeletonAbsolutePath =
            AssetManager::Get().ResolveAssetAbsolutePath(meshMeta.AssetPath).parent_path()
            / skeletonFileName;

        const std::string skeletonRelativePath =
            (meshPath.parent_path() / skeletonFileName).generic_string();

        AssetMeta skeletonMeta;
        skeletonMeta.AssetName = meshPath.stem().string() + "_Skeleton";
        skeletonMeta.AssetPath = skeletonRelativePath;
        skeletonMeta.AssetType = "Skeleton";
        skeletonMeta.Guid = GenerateGUID();

        std::shared_ptr<Skeleton> skeleton = CreateSkeletonFromImport(skeletonMeta, importData);
        if (!skeleton)
        {
            if (outError != nullptr)
            {
                *outError = "failed to build skeleton from import data";
            }
            return false;
        }

        std::error_code createError;
        std::filesystem::create_directories(skeletonAbsolutePath.parent_path(), createError);

        if (!SkeletonLoader::Save(skeletonMeta, *skeleton, outError))
        {
            return false;
        }

        // Persist meta with the same Guid used by NewObject/Save, before RegisterAsset
        // (RegisterAsset would otherwise mint a new Guid when .meta is missing).
        AssetManager& assetManager = AssetManager::Get();
        if (!assetManager.WriteOrUpdateMetaFile(skeletonMeta))
        {
            if (outError != nullptr)
            {
                *outError = "failed to write skeleton meta";
            }
            return false;
        }

        skeletonMeta = assetManager.RegisterAsset(skeletonAbsolutePath.string(), "Skeleton");
        if (skeletonMeta.AssetPath.empty())
        {
            if (outError != nullptr)
            {
                *outError = "failed to register skeleton asset";
            }
            return false;
        }

        std::shared_ptr<Skeleton> registeredSkeleton = assetManager.LoadAsset<Skeleton>(skeletonMeta.AssetPath);
        if (!registeredSkeleton)
        {
            if (outError != nullptr)
            {
                *outError = "failed to load registered skeleton";
            }
            return false;
        }

        if (!SaveBuddy(meshMeta, registeredSkeleton))
        {
            if (outError != nullptr)
            {
                *outError = "failed to write skeletal mesh buddy file";
            }
            return false;
        }

        return true;
    }

    std::shared_ptr<SkeletalMesh> SkeletalMeshLoader::LoadFromAssetMeta(const AssetMeta& meta)
    {
        SkeletalMeshImportData importData;
        std::string error;
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

        std::shared_ptr<Skeleton> skeleton;
        if (TryLoadBuddySkeleton(meta, skeleton))
        {
            if (!ImportFromFile(absoluteAssetPath, importData, &error))
            {
                return nullptr;
            }
            return CreateFromImportData(meta, importData, skeleton);
        }

        ME_CORE_WARN(
            "SkeletalMeshLoader: no '.meskmesh' buddy for '{}'; using legacy import (temporary skeleton).",
            meta.AssetPath);

        if (!ImportFromFile(absoluteAssetPath, importData, &error))
        {
            return nullptr;
        }

        AssetMeta legacySkeletonMeta = meta;
        legacySkeletonMeta.AssetName = meta.AssetName + "_Skeleton";
        legacySkeletonMeta.Guid = GenerateGUID();
        skeleton = CreateSkeletonFromImport(legacySkeletonMeta, importData);
        if (!skeleton)
        {
            return nullptr;
        }

        return CreateFromImportData(meta, importData, skeleton);
    }

    template<>
    std::shared_ptr<SkeletalMesh> AssetManager::LoadAsset_Impl<SkeletalMesh>(const AssetMeta& meta)
    {
        return SkeletalMeshLoader::LoadFromAssetMeta(meta);
    }

    template<>
    std::shared_ptr<Skeleton> AssetManager::LoadAsset_Impl<Skeleton>(const AssetMeta& meta)
    {
        return SkeletonLoader::Load(meta);
    }
}
