#pragma once
#include "Core.h"
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"

namespace minEngine
{
    class PrimitiveComponent;
    class StaticMesh;
    class Material;

    ME_CLASS()
    class StaticMeshComponent : public PrimitiveComponent    
    {
        ME_REFLECTION_FRIEND(StaticMeshComponent)
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
        ME_PROPERTY()
        std::shared_ptr<StaticMesh> m_Mesh{ nullptr };
        ME_PROPERTY()
        std::shared_ptr<Material> m_Material{ nullptr };
    };
}

#include "Generated/Reflection/StaticMeshComponent.gen.h"