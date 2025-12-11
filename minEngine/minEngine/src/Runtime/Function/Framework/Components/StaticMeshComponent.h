#pragma once
#include "Core.h"
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"

namespace minEngine
{
    class PrimitiveComponent;
    class StaticMesh;
    class Material;

    class StaticMeshComponent : public PrimitiveComponent    
    {
    public:
        StaticMeshComponent();
        virtual ~StaticMeshComponent() = default;

        void SetMesh(const std::shared_ptr<StaticMesh>& mesh);
        StaticMesh* GetMesh() const { return m_Mesh.get(); }

        void SetMaterial(const std::shared_ptr<Material>& material);
        Material* GetMaterial() const { return m_Material.get(); }

        // PrimitiveComponent Contract
        virtual PrimitiveSceneProxy* CreateSceneProxy() override;

    private:
        std::shared_ptr<StaticMesh> m_Mesh{ nullptr };
        std::shared_ptr<Material> m_Material{ nullptr };
    };
}