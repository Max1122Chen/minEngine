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

    void SkeletalMeshComponent::SyncPlayerClipFromProperty()
    {
        if (m_AnimationPlayer.GetClip() == m_AnimationClip.get())
        {
            return;
        }

        m_AnimationPlayer.SetClip(m_AnimationClip);
        m_bPlayOnAwakeTriggered = false;
    }

    void SkeletalMeshComponent::SyncGraphFromProperty()
    {
        if (m_GraphInstance.GetGraph() == m_AnimationGraph.get())
        {
            return;
        }

        m_GraphInstance.SetGraph(m_AnimationGraph);
        m_bPlayOnAwakeTriggered = false;
    }

    bool SkeletalMeshComponent::TryConsumePlayOnAwake()
    {
        if (!m_bPlayOnAwake || m_bPlayOnAwakeTriggered)
        {
            return false;
        }

        if (UsesGraphPath())
        {
            SyncGraphFromProperty();
            if (!m_GraphInstance.IsBound() || !EnsureGraphSkeletonCompatible())
            {
                return false;
            }
            m_bPlayOnAwakeTriggered = true;
            return true;
        }

        if (m_AnimationClip == nullptr)
        {
            return false;
        }

        SyncPlayerClipFromProperty();
        if (!EnsureClipSkeletonCompatible())
        {
            return false;
        }

        m_bPlayOnAwakeTriggered = true;
        return true;
    }

    void SkeletalMeshComponent::ProcessPlayOnAwake()
    {
        if (!TryConsumePlayOnAwake())
        {
            return;
        }

        if (UsesGraphPath())
        {
            m_GraphInstance.ResetToDefaultState();
            ME_LOG(LogAnimation, Info, 
                "SkeletalMeshComponent: PlayOnAwake -> Graph default state='{}'",
                m_GraphInstance.GetCurrentStateName());
            return;
        }

        m_AnimationPlayer.Play();
        ME_LOG(LogAnimation, Info, 
            "SkeletalMeshComponent: PlayOnAwake -> Playing clip='{}' duration={:.3f}s tracks={}",
            m_AnimationClip ? m_AnimationClip->GetName() : std::string("<null>"),
            m_AnimationClip ? m_AnimationClip->GetDuration() : 0.0f,
            m_AnimationClip ? m_AnimationClip->GetTracks().size() : 0u);
    }

    void SkeletalMeshComponent::OnActivate()
    {
        Component::OnActivate();
        m_bPlayOnAwakeTriggered = false;
        ProcessPlayOnAwake();
    }

    void SkeletalMeshComponent::OnDeactivate()
    {
        m_AnimationPlayer.Stop();
        m_bPlayOnAwakeTriggered = false;
        Component::OnDeactivate();
    }

    void SkeletalMeshComponent::Tick(float deltaTime)
    {
        if (UsesGraphPath())
        {
            SyncGraphFromProperty();
            ProcessPlayOnAwake();

            if (!m_GraphInstance.IsBound() || !EnsureGraphSkeletonCompatible())
            {
                return;
            }

            m_GraphInstance.Update(deltaTime, m_LocalPose);
            m_bPoseDirty = true;
            MarkRenderStateDirty();
            return;
        }

        SyncPlayerClipFromProperty();
        ProcessPlayOnAwake();

        if (m_AnimationPlayer.GetState() != AnimationPlayState::Playing)
        {
            return;
        }

        if (!EnsureClipSkeletonCompatible())
        {
            return;
        }

        m_AnimationPlayer.Update(deltaTime, m_LocalPose);
        m_bPoseDirty = true;
        MarkRenderStateDirty();
    }

    void SkeletalMeshComponent::SetMesh(const std::shared_ptr<SkeletalMesh>& mesh)
    {
        if (m_Mesh == mesh)
        {
            return;
        }
        m_Mesh = mesh;
        ResetToBindPose();
        m_bPlayOnAwakeTriggered = false;
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

    void SkeletalMeshComponent::SetAnimationClip(const std::shared_ptr<AnimationClip>& clip)
    {
        m_AnimationClip = clip;
        m_AnimationPlayer.SetClip(clip);
        m_bPlayOnAwakeTriggered = false;
        if (!UsesGraphPath())
        {
            ProcessPlayOnAwake();
        }
    }

    void SkeletalMeshComponent::SetAnimationGraph(const std::shared_ptr<AnimationGraph>& graph)
    {
        m_AnimationGraph = graph;
        m_GraphInstance.SetGraph(graph);
        m_bPlayOnAwakeTriggered = false;
        ProcessPlayOnAwake();
    }

    bool SkeletalMeshComponent::EnsureClipSkeletonCompatible() const
    {
        AnimationClip* clip = m_AnimationPlayer.GetClip();
        if (clip == nullptr)
        {
            return false;
        }

        Skeleton* meshSkeleton = GetSkeleton();
        if (meshSkeleton == nullptr)
        {
            static thread_local uint32_t s_NullMeshSkeletonLogCounter = 0;
            if ((s_NullMeshSkeletonLogCounter++ % 120u) == 0u)
            {
                ME_LOG(LogAnimation, Warn, 
                    "SkeletalMeshComponent: cannot play AnimationClip — mesh has no Skeleton "
                    "(buddy '.meskmesh' missing or failed; mesh may be using a temporary skeleton).");
            }
            return false;
        }

        Skeleton* clipSkeleton = clip->GetSkeleton();
        if (clipSkeleton == nullptr)
        {
            ME_LOG(LogAnimation, Error, "SkeletalMeshComponent: AnimationClip has no Skeleton.");
            return false;
        }

        if (clipSkeleton != meshSkeleton
            && clipSkeleton->GetGuid() != meshSkeleton->GetGuid())
        {
            static thread_local uint32_t s_GuidMismatchLogCounter = 0;
            if ((s_GuidMismatchLogCounter++ % 120u) == 0u)
            {
                ME_LOG(LogAnimation, Error, 
                    "SkeletalMeshComponent: AnimationClip Skeleton GUID mismatch with mesh Skeleton "
                    "(clip='{}', mesh='{}'). Re-import skeletal mesh so buddy points at the same "
                    "Skeleton asset as the clip.",
                    clipSkeleton->GetGuid().ToString(),
                    meshSkeleton->GetGuid().ToString());
            }
            return false;
        }

        return true;
    }

    bool SkeletalMeshComponent::EnsureGraphSkeletonCompatible() const
    {
        AnimationGraph* graph = m_GraphInstance.GetGraph();
        if (graph == nullptr)
        {
            return false;
        }

        Skeleton* meshSkeleton = GetSkeleton();
        if (meshSkeleton == nullptr)
        {
            static thread_local uint32_t s_NullMeshSkeletonLogCounter = 0;
            if ((s_NullMeshSkeletonLogCounter++ % 120u) == 0u)
            {
                ME_LOG(LogAnimation, Warn, 
                    "SkeletalMeshComponent: cannot play AnimationGraph — mesh has no Skeleton.");
            }
            return false;
        }

        for (const AnimState& state : graph->GetStateMachine().States)
        {
            if (state.Clip == nullptr)
            {
                continue;
            }
            Skeleton* clipSkeleton = state.Clip->GetSkeleton();
            if (clipSkeleton == nullptr)
            {
                ME_LOG(LogAnimation, Error, 
                    "SkeletalMeshComponent: AnimationGraph state '{}' clip has no Skeleton.",
                    state.Name);
                return false;
            }
            if (clipSkeleton != meshSkeleton
                && clipSkeleton->GetGuid() != meshSkeleton->GetGuid())
            {
                static thread_local uint32_t s_GuidMismatchLogCounter = 0;
                if ((s_GuidMismatchLogCounter++ % 120u) == 0u)
                {
                    ME_LOG(LogAnimation, Error, 
                        "SkeletalMeshComponent: AnimationGraph state '{}' Skeleton GUID mismatch "
                        "(clip='{}', mesh='{}').",
                        state.Name,
                        clipSkeleton->GetGuid().ToString(),
                        meshSkeleton->GetGuid().ToString());
                }
                return false;
            }
        }

        return true;
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
                ME_LOG(LogAnimation, Warn, 
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
