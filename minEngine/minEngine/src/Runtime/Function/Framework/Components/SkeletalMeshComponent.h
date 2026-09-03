#pragma once
#include "Core.h"
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

        void SetMesh(const std::shared_ptr<SkeletalMesh>& mesh);
        SkeletalMesh* GetMesh() const { return m_Mesh.get(); }

        void SetMaterial(const std::shared_ptr<Material>& material);
        Material* GetMaterial() const { return m_Material.get(); }

        void SetLocalPose(const Pose& pose);
        const Pose& GetLocalPose() const { return m_LocalPose; }
        void ResetToBindPose();
        void SetBoneLocalTransform(int32_t boneIndex, const Transform& local);

        Skeleton* GetSkeleton() const;

        Math::Geometry::AABB GetBoundingBox() const override;
        PrimitiveSceneProxy* CreateSceneProxy() override;

        /** Push mesh / material / palette into an existing proxy (RenderScene dirty update). */
        void SyncSceneProxy(SkeletalMeshSceneProxy& sceneProxy);

    private:
        void RebuildPaletteIfNeeded();

        ME_PROPERTY()
        std::shared_ptr<SkeletalMesh> m_Mesh{nullptr};
        ME_PROPERTY()
        std::shared_ptr<Material> m_Material{nullptr};

        Pose m_LocalPose;
        std::vector<Matrix4> m_SkinningPalette;
        bool m_bPoseDirty = true;
    };
}

#include "Generated/Reflection/SkeletalMeshComponent.gen.h"
