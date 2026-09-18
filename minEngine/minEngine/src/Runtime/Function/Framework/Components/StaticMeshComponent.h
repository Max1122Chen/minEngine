#pragma once
#include "Core.h"
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"

#include "Runtime/Function/Render/MaterialInterface.h"

namespace minEngine
{
    class PrimitiveComponent;
    class StaticMesh;
    class MaterialInterface;

    ME_CLASS()
    class StaticMeshComponent : public PrimitiveComponent    
    {
        ME_GENERATED_BODY()
    public:
        StaticMeshComponent();
        virtual ~StaticMeshComponent() = default;

        void SetMesh(const std::shared_ptr<StaticMesh>& mesh);
        StaticMesh* GetMesh() const { return m_Mesh.get(); }

        void SetMaterial(const std::shared_ptr<MaterialInterface>& material);
        MaterialInterface* GetMaterial() const { return m_Material.get(); }
        const std::shared_ptr<MaterialInterface>& GetMaterialShared() const { return m_Material; }

        // PrimitiveComponent Contract
        virtual Math::Geometry::AABB GetBoundingBox() const override;
        virtual PrimitiveSceneProxy* CreateSceneProxy() override;

    private:
        ME_PROPERTY()
        std::shared_ptr<StaticMesh> m_Mesh{ nullptr };
        ME_PROPERTY()
        std::shared_ptr<MaterialInterface> m_Material{ nullptr };
    };
}

#include "Generated/Reflection/StaticMeshComponent.gen.h"