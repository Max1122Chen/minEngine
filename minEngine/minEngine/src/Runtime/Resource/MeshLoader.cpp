#include "MeshLoader.h"

#include "Runtime/Core/Log/LogSystem.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include <algorithm>
#include <limits>
#include <queue>

namespace minEngine
{
    namespace
    {
        Vector4 ComputeFallbackTangent(const Vector3& normal)
        {
            const Vector3 up = (std::abs(normal.y) < 0.999f) ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(1.0f, 0.0f, 0.0f);
            Vector3 tangent = glm::normalize(glm::cross(up, normal));
            return Vector4(tangent, 1.0f);
        }

        Vector4 ComputeTangentWithHandedness(
            const Vector3& normal,
            const aiVector3D* assimpTangent,
            const aiVector3D* assimpBitangent)
        {
            if (assimpTangent == nullptr)
            {
                return ComputeFallbackTangent(normal);
            }

            Vector3 tangent(assimpTangent->x, assimpTangent->y, assimpTangent->z);
            if (glm::dot(tangent, tangent) <= 1e-12f)
            {
                return ComputeFallbackTangent(normal);
            }

            tangent = glm::normalize(tangent - normal * glm::dot(normal, tangent));
            float handedness = 1.0f;
            if (assimpBitangent != nullptr)
            {
                const Vector3 bitangent(assimpBitangent->x, assimpBitangent->y, assimpBitangent->z);
                handedness = (glm::dot(glm::cross(normal, tangent), bitangent) < 0.0f) ? -1.0f : 1.0f;
            }

            return Vector4(tangent, handedness);
        }
    }

    bool MeshLoader::ImportFromFile(const std::string& path, MeshImportData& outData, std::string* outError)
    {
        outData = {};

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            path.c_str(),
            aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals);

        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
        {
            if (outError)
            {
                *outError = std::string("Assimp failed: ") + importer.GetErrorString();
            }
            ME_CORE_ERROR("MeshLoader: Assimp failed for {}. {}", path, importer.GetErrorString());
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
                    ME_CORE_WARN("MeshLoader: null mesh pointer in {}.", path);
                    continue;
                }

                if (!mesh->HasPositions() || mesh->mVertices == nullptr)
                {
                    ME_CORE_WARN("MeshLoader: skip mesh without positions in {}.", path);
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

                MeshImportSection sectionInfo;
                sectionInfo.FirstIndex = static_cast<uint32_t>(outData.Indices.size());

                const uint32_t baseVertex = static_cast<uint32_t>(outData.Vertices.size());

                for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
                {
                    MeshImportVertex vertex;
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
                            ME_CORE_WARN(
                                "MeshLoader: skip invalid index in {} (vertexCount={}, index={}).",
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

                    ME_CORE_WARN("MeshLoader: generated fallback normals for {}.", path);
                }

                for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
                {
                    MeshImportVertex& vertex = outData.Vertices[baseVertex + vertexIndex];
                    if (hasTangents)
                    {
                        const aiVector3D* assimpTangent = &mesh->mTangents[vertexIndex];
                        const aiVector3D* assimpBitangent =
                            mesh->mBitangents != nullptr ? &mesh->mBitangents[vertexIndex] : nullptr;
                        vertex.Tangent =
                            ComputeTangentWithHandedness(vertex.Normal, assimpTangent, assimpBitangent);
                    }
                    else
                    {
                        vertex.Tangent = ComputeFallbackTangent(vertex.Normal);
                    }
                }

                if (!hasTangents)
                {
                    ME_CORE_WARN("MeshLoader: generated fallback tangents for {}.", path);
                }

                if (!hasTexCoords)
                {
                    ME_CORE_WARN("MeshLoader: generated fallback UVs for {}.", path);
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
            ME_CORE_ERROR("MeshLoader: no valid vertices for {}.", path);
            return false;
        }

        outData.BoundingBox = boundingBox;
        return true;
    }
}
