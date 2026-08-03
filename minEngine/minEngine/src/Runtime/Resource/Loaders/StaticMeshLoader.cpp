#include "Runtime/Resource/Loaders/StaticMeshLoader.h"

#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/StaticMesh.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/Loaders/MeshLoader.h"

namespace minEngine
{
    std::shared_ptr<StaticMesh> StaticMeshLoader::CreateFromImportData(
        const AssetMeta& meta,
        MeshImportData& importData)
    {
        if (!importData.IsValid())
        {
            ME_CORE_ERROR("StaticMeshLoader: invalid import data for {}.", meta.AssetPath);
            return nullptr;
        }

        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            ME_CORE_ERROR("StaticMeshLoader: RHI is not available.");
            return nullptr;
        }

        std::shared_ptr<StaticMesh> mesh = NewObject<StaticMesh>(meta.AssetName, nullptr, meta.Guid);
        mesh->m_BoundingBox = importData.BoundingBox;
        mesh->m_Sections.clear();
        mesh->m_Sections.reserve(importData.Sections.size());
        for (const MeshImportSection& section : importData.Sections)
        {
            StaticMeshSectionInfo sectionInfo;
            sectionInfo.MaterialIndex = section.MaterialIndex;
            sectionInfo.FirstIndex = section.FirstIndex;
            sectionInfo.NumIndices = section.NumIndices;
            mesh->m_Sections.push_back(sectionInfo);
        }

        RHIBufferCreateDesc vbDesc;
        vbDesc.Usage = RHIBufferUsage::Vertex;
        vbDesc.ByteSize = static_cast<uint32_t>(importData.Vertices.size() * sizeof(MeshImportVertex));
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
        MeshImportData importData;
        std::string error;
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();
        if (!MeshLoader::ImportFromFile(absoluteAssetPath, importData, &error))
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
