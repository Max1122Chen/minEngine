#include "Runtime/Resource/Loaders/StaticMeshLoader.h"

#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/StaticMesh.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/Loaders/AssimpMeshImportUtil.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"

#include <algorithm>
#include <limits>
#include <queue>

namespace minEngine
{
    bool StaticMeshLoader::ImportFromFile(
        const std::string& path,
        StaticMeshImportData& outData,
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
            ME_LOG(LogAsset, Error, "StaticMeshLoader: Assimp failed for {}. {}", path, importer.GetErrorString());
            return false;
        }

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
                if (mesh == nullptr)
                {
                    ME_LOG(LogAsset, Warn, "StaticMeshLoader: null mesh pointer in {}.", path);
                    continue;
                }

                if (!mesh->HasPositions() || mesh->mVertices == nullptr)
                {
                    ME_LOG(LogAsset, Warn, "StaticMeshLoader: skip mesh without positions in {}.", path);
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

                for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
                {
                    boundingBox.Encapsulate(Vector3(
                        mesh->mVertices[vertexIndex].x,
                        mesh->mVertices[vertexIndex].y,
                        mesh->mVertices[vertexIndex].z));
                }

                const float uvExtentX = std::max(maxX - minX, 1e-6f);
                const float uvExtentZ = std::max(maxZ - minZ, 1e-6f);

                StaticMeshImportSection sectionInfo;
                sectionInfo.FirstIndex = static_cast<uint32_t>(outData.Indices.size());

                const uint32_t baseVertex = static_cast<uint32_t>(outData.Vertices.size());

                for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
                {
                    StaticMeshImportVertex vertex;
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
                        vertex.Normal = Vector3(0.0f, 0.0f, 0.0f);
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
                            ME_LOG(LogAsset, Warn, 
                                "StaticMeshLoader: skip invalid index in {} (vertexCount={}, index={}).",
                                path,
                                mesh->mNumVertices,
                                face.mIndices[indexInFace]);
                            continue;
                        }

                        outData.Indices.push_back(face.mIndices[indexInFace] + baseVertex);
                        ++numIndices;
                    }
                }
                sectionInfo.NumIndices = numIndices;

                if (!hasNormals)
                {
                    for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
                    {
                        const aiFace& face = mesh->mFaces[faceIndex];
                        if (face.mNumIndices < 3)
                        {
                            continue;
                        }

                        const uint32_t i0 = baseVertex + face.mIndices[0];
                        const uint32_t i1 = baseVertex + face.mIndices[1];
                        const uint32_t i2 = baseVertex + face.mIndices[2];

                        if (i0 >= outData.Vertices.size() || i1 >= outData.Vertices.size()
                            || i2 >= outData.Vertices.size())
                        {
                            continue;
                        }

                        const Vector3& p0 = outData.Vertices[i0].Position;
                        const Vector3& p1 = outData.Vertices[i1].Position;
                        const Vector3& p2 = outData.Vertices[i2].Position;

                        const Vector3 edge01 = p1 - p0;
                        const Vector3 edge02 = p2 - p0;
                        Vector3 faceNormal = glm::cross(edge01, edge02);
                        const float faceNormalLen2 = glm::dot(faceNormal, faceNormal);
                        if (faceNormalLen2 <= 1e-12f)
                        {
                            continue;
                        }

                        faceNormal = glm::normalize(faceNormal);
                        outData.Vertices[i0].Normal += faceNormal;
                        outData.Vertices[i1].Normal += faceNormal;
                        outData.Vertices[i2].Normal += faceNormal;
                    }

                    for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
                    {
                        Vector3& normal = outData.Vertices[baseVertex + vertexIndex].Normal;
                        const float normalLen2 = glm::dot(normal, normal);
                        if (normalLen2 <= 1e-12f)
                        {
                            normal = Vector3(0.0f, 1.0f, 0.0f);
                        }
                        else
                        {
                            normal = glm::normalize(normal);
                        }
                    }

                    ME_LOG(LogAsset, Warn, "StaticMeshLoader: generated fallback normals for {}.", path);
                }

                for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
                {
                    StaticMeshImportVertex& vertex = outData.Vertices[baseVertex + vertexIndex];
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
                }

                if (!hasTangents)
                {
                    ME_LOG(LogAsset, Warn, "StaticMeshLoader: generated fallback tangents for {}.", path);
                }

                if (!hasTexCoords)
                {
                    ME_LOG(LogAsset, Warn, "StaticMeshLoader: generated fallback UVs for {}.", path);
                }

                if (mesh->mMaterialIndex >= 0)
                {
                    sectionInfo.MaterialIndex = static_cast<int32_t>(mesh->mMaterialIndex);
                }

                outData.Sections.push_back(sectionInfo);
            }

            for (unsigned int childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
            {
                nodeQueue.push(node->mChildren[childIndex]);
            }
        }

        if (outData.Vertices.empty())
        {
            if (outError)
            {
                *outError = "No valid vertices produced.";
            }
            ME_LOG(LogAsset, Error, "StaticMeshLoader: no valid vertices for {}.", path);
            return false;
        }

        outData.BoundingBox = boundingBox;
        return true;
    }

    std::shared_ptr<StaticMesh> StaticMeshLoader::CreateFromImportData(
        const AssetMeta& meta,
        StaticMeshImportData& importData)
    {
        if (!importData.IsValid())
        {
            ME_LOG(LogAsset, Error, "StaticMeshLoader: invalid import data for {}.", meta.AssetPath);
            return nullptr;
        }

        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            ME_LOG(LogAsset, Error, "StaticMeshLoader: RHI is not available.");
            return nullptr;
        }

        std::shared_ptr<StaticMesh> mesh = NewObject<StaticMesh>(meta.AssetName, nullptr, meta.Guid);
        mesh->m_BoundingBox = importData.BoundingBox;
        mesh->m_Sections.clear();
        mesh->m_Sections.reserve(importData.Sections.size());
        for (const StaticMeshImportSection& section : importData.Sections)
        {
            StaticMeshSectionInfo sectionInfo;
            sectionInfo.MaterialIndex = section.MaterialIndex;
            sectionInfo.FirstIndex = section.FirstIndex;
            sectionInfo.NumIndices = section.NumIndices;
            mesh->m_Sections.push_back(sectionInfo);
        }

        RHIBufferCreateDesc vbDesc;
        vbDesc.Usage = RHIBufferUsage::Vertex;
        vbDesc.ByteSize =
            static_cast<uint32_t>(importData.Vertices.size() * sizeof(StaticMeshImportVertex));
        vbDesc.ElementCount = static_cast<uint32_t>(importData.Vertices.size());
        mesh->m_VertexBuffer = rhi->RHICreateBuffer(
            vbDesc,
            reinterpret_cast<const void*>(importData.Vertices.data()));

        mesh->m_VertexInputLayout = rhi->RHICreateVertexInputLayout({
            RHIVertexElement("a_Position", VertexElementType::Float3),
            RHIVertexElement("a_TexCoord", VertexElementType::Float2),
            RHIVertexElement("a_Normal", VertexElementType::Float3),
            RHIVertexElement("a_Tangent", VertexElementType::Float4),
        });

        RHIBufferCreateDesc ibDesc;
        ibDesc.Usage = RHIBufferUsage::Index;
        ibDesc.ByteSize = static_cast<uint32_t>(importData.Indices.size() * sizeof(uint32_t));
        ibDesc.ElementCount = static_cast<uint32_t>(importData.Indices.size());
        mesh->m_IndexBuffer = rhi->RHICreateBuffer(ibDesc, importData.Indices.data());

        return mesh;
    }

    std::shared_ptr<StaticMesh> StaticMeshLoader::LoadFromAssetMeta(const AssetMeta& meta)
    {
        StaticMeshImportData importData;
        std::string error;
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();
        if (!ImportFromFile(absoluteAssetPath, importData, &error))
        {
            return nullptr;
        }

        return CreateFromImportData(meta, importData);
    }

    template<>
    std::shared_ptr<StaticMesh> AssetManager::LoadAsset_Impl<StaticMesh>(const AssetMeta& meta)
    {
        return StaticMeshLoader::LoadFromAssetMeta(meta);
    }
}
