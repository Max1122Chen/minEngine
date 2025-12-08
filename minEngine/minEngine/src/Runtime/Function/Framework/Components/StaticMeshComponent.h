#pragma once
#include "Core.h"
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"
#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Function/Render/Material.h"

namespace minEngine
{
    class PrimitiveComponent;
    class StaticMesh;
    class Material;

    class StaticMeshComponent : public PrimitiveComponent    
    {
    public:
        StaticMeshComponent() = default;
        StaticMeshComponent(std::shared_ptr<GameObject> owner) : PrimitiveComponent(owner) {}
        virtual ~StaticMeshComponent() = default;

        void SetMesh(const std::shared_ptr<StaticMesh>& mesh);
        std::shared_ptr<StaticMesh> GetMesh() const { return m_Mesh; }

        void SetMaterial(const std::shared_ptr<Material>& material);
        std::shared_ptr<Material> GetMaterial() const { return m_Material; }

        // PrimitiveComponent Contract
        virtual PrimitiveSceneProxy* CreateSceneProxy() override;

    private:
        std::shared_ptr<StaticMesh> m_Mesh{ nullptr };
        std::shared_ptr<Material> m_Material{ nullptr };
    };
}