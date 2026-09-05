#pragma once
#include "Core.h"
#include "Runtime/Function/Animation/AnimationClip.h"
#include "Runtime/Function/Animation/AnimationPlayer.h"
#include "Runtime/Function/Animation/Pose.h"
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"

#include <memory>
#include <vector>

namespace minEngine
{
    class SkeletalMesh;
    class Material;
    class Skeleton;
    class SkeletalMeshSceneProxy;

    ME_CLASS()
    class SkeletalMeshComponent : public PrimitiveComponent
    {
        ME_GENERATED_BODY(SkeletalMeshComponent)
    public:
        SkeletalMeshComponent();
        ~SkeletalMeshComponent() override = default;

        void Tick(float deltaTime) override;

        void SetMesh(const std::shared_ptr<SkeletalMesh>& mesh);
        SkeletalMesh* GetMesh() const { return m_Mesh.get(); }

        void SetMaterial(const std::shared_ptr<Material>& material);
        Material* GetMaterial() const { return m_Material.get(); }

        void SetLocalPose(const Pose& pose);
        const Pose& GetLocalPose() const { return m_LocalPose; }
        void ResetToBindPose();
        void SetBoneLocalTransform(int32_t boneIndex, const Transform& local);

        Skeleton* GetSkeleton() const;

        void SetAnimationClip(const std::shared_ptr<AnimationClip>& clip);
        AnimationClip* GetAnimationClip() const { return m_AnimationClip.get(); }

        void SetPlayOnAwake(bool playOnAwake) { m_bPlayOnAwake = playOnAwake; }
        bool GetPlayOnAwake() const { return m_bPlayOnAwake; }

        AnimationPlayer& GetAnimationPlayer() { return m_AnimationPlayer; }
        const AnimationPlayer& GetAnimationPlayer() const { return m_AnimationPlayer; }

        Math::Geometry::AABB GetBoundingBox() const override;
        PrimitiveSceneProxy* CreateSceneProxy() override;

        /** Push mesh / material / palette into an existing proxy (RenderScene dirty update). */
        void SyncSceneProxy(SkeletalMeshSceneProxy& sceneProxy);

    protected:
        void OnActivate() override;
        void OnDeactivate() override;

    private:
        void RebuildPaletteIfNeeded();
        bool EnsureClipSkeletonCompatible() const;
        void SyncPlayerClipFromProperty();
        // Audio-like: consume once when clip+skeleton are ready, then Play().
        bool TryConsumePlayOnAwake();
        void ProcessPlayOnAwake();

        ME_PROPERTY()
        std::shared_ptr<SkeletalMesh> m_Mesh{nullptr};
        ME_PROPERTY()
        std::shared_ptr<Material> m_Material{nullptr};

        ME_PROPERTY()
        std::shared_ptr<AnimationClip> m_AnimationClip{nullptr};

        // Same idea as AudioComponent::m_bPlayOnAwake. Default true for F02 authoring/test.
        ME_PROPERTY(EditAnywhere)
        bool m_bPlayOnAwake{true};

        AnimationPlayer m_AnimationPlayer;
        Pose m_LocalPose;
        std::vector<Matrix4> m_SkinningPalette;
        bool m_bPoseDirty = true;
        bool m_bPlayOnAwakeTriggered{false};
    };
}

#include "Generated/Reflection/SkeletalMeshComponent.gen.h"
