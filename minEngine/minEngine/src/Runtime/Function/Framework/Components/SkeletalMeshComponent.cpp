#include "SkeletalMeshComponent.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/SkeletalMeshSceneProxy.h"
#include "Runtime/Function/Render/SkeletalMesh.h"
#include "Runtime/Function/Animation/Skeleton.h"

namespace minEngine
{
    SkeletalMeshComponent::SkeletalMeshComponent()
    {
        // Skinned shadow depth shader not yet wired (ShadowPass.vert is rigid-only).
        m_CastShadow = false;
    }

    void SkeletalMeshComponent::SetMesh(const std::shared_ptr<SkeletalMesh>& mesh)
    {
        if (m_Mesh == mesh)
        {
            return;
        }
        m_Mesh = mesh;
        ResetToBindPose();
        MarkRenderStateDirty();
    }

    void SkeletalMeshComponent::SetMaterial(const std::shared_ptr<Material>& material)
    {
        if (m_Material == material)
        {
            return;
        }
        m_Material = material;
        MarkRenderStateDirty();
    }

    void SkeletalMeshComponent::SetLocalPose(const Pose& pose)
    {
        m_LocalPose = pose;
        m_bPoseDirty = true;
        MarkRenderStateDirty();
    }

    void SkeletalMeshComponent::ResetToBindPose()
    {
        Skeleton* skeleton = GetSkeleton();
        if (skeleton == nullptr)
        {
            m_LocalPose = {};
            m_SkinningPalette.clear();
            m_bPoseDirty = true;
            return;
        }
        skeleton->FillBindPose(m_LocalPose);
        m_bPoseDirty = true;
        MarkRenderStateDirty();
    }

    void SkeletalMeshComponent::SetBoneLocalTransform(int32_t boneIndex, const Transform& local)
    {
        if (boneIndex < 0 || boneIndex >= m_LocalPose.GetBoneCount())
        {
            return;
        }
        m_LocalPose.At(boneIndex) = local;
        m_bPoseDirty = true;
        MarkRenderStateDirty();
    }

    Skeleton* SkeletalMeshComponent::GetSkeleton() const
    {
        return m_Mesh ? m_Mesh->GetSkeleton() : nullptr;
    }

    void SkeletalMeshComponent::RebuildPaletteIfNeeded()
    {
        if (!m_bPoseDirty)
        {
            return;
        }
        Skeleton* skeleton = GetSkeleton();
        if (skeleton == nullptr)
        {
            m_SkinningPalette.clear();
            m_bPoseDirty = false;
            return;
        }
        if (m_LocalPose.GetBoneCount() != skeleton->GetBoneCount())
        {
            skeleton->FillBindPose(m_LocalPose);
        }
        skeleton->BuildSkinningPalette(m_LocalPose, m_SkinningPalette);
        m_bPoseDirty = false;
    }

    Math::Geometry::AABB SkeletalMeshComponent::GetBoundingBox() const
    {
        if (m_Mesh)
        {
            return m_Mesh->m_BoundingBox;
        }
        return Math::Geometry::AABB();
    }

    void SkeletalMeshComponent::SyncSceneProxy(SkeletalMeshSceneProxy& sceneProxy)
    {
        RebuildPaletteIfNeeded();

        sceneProxy.m_PrimitiveComponent = this;

        assert(m_Owner);
        sceneProxy.m_Transform = m_Owner->GetTransform();
        sceneProxy.m_CastShadow = CastShadow();
        sceneProxy.m_BonePalette = m_SkinningPalette;

        if (m_Mesh)
        {
            sceneProxy.m_VertexBuffer = m_Mesh->m_VertexBuffer.get();
            sceneProxy.m_VertexInputLayout = m_Mesh->m_VertexInputLayout.get();
            sceneProxy.m_IndexBuffer = m_Mesh->m_IndexBuffer.get();
        }
        else
        {
            sceneProxy.m_VertexBuffer = nullptr;
            sceneProxy.m_VertexInputLayout = nullptr;
            sceneProxy.m_IndexBuffer = nullptr;
        }

        if (m_Material)
        {
            if (!m_Material->EnsureSkinnedCompiled())
            {
                ME_CORE_WARN(
                    "SkeletalMeshComponent: EnsureSkinnedCompiled failed for material '{}'; "
                    "skinned draws will be skipped until compile succeeds.",
                    m_Material->GetName());
            }
            sceneProxy.m_Material = m_Material.get();
        }
        else
        {
            sceneProxy.m_Material = nullptr;
        }
    }

    PrimitiveSceneProxy* SkeletalMeshComponent::CreateSceneProxy()
    {
        SkeletalMeshSceneProxy* sceneProxy = new SkeletalMeshSceneProxy();
        SyncSceneProxy(*sceneProxy);
        m_SceneProxy = sceneProxy;
        return sceneProxy;
    }
}
