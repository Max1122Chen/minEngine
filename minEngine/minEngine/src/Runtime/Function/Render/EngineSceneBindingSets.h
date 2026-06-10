#pragma once

#include "Core.h"
#include "Render/RHI/RHIBinding.h"
#include "Runtime/Function/Render/Environment/EngineIBLEnvironment.h"
#include "Runtime/Function/Render/SceneRenderContext.h"

namespace minEngine
{
    class RHICommandList;
    class RHIBuffer;

    class EngineSceneBindingSets
    {
    public:
        void Initialize(RHICommandList& cmdList);
        void Shutdown();

        void BuildSceneSet0(RHICommandList& cmdList, RHIBuffer* perFrame, RHIBuffer* lights, RHIBuffer* perObject);
        void BuildSceneSet1(
            RHICommandList& cmdList,
            const SceneRenderContext& ctx,
            RHIBuffer* dirLightViewProjs,
            RHIBuffer* cascadeFarPlanes,
            RHIBuffer* spotLightViewProjs,
            const EngineIBLEnvironment* iblEnvironment);

        void UpdatePerObjectModel(RHIBuffer* perObjectBuffer, const Matrix4& model) const;

        RHIBindingSet* GetSceneSet0() const { return m_SceneSet0.get(); }
        RHIBindingSet* GetSceneSet1() const { return m_SceneSet1.get(); }

    private:
        RHIBindingLayoutRef m_SceneSet0Layout;
        RHIBindingLayoutRef m_SceneSet1Layout;
        RHIBindingSetRef m_SceneSet0;
        RHIBindingSetRef m_SceneSet1;

        std::shared_ptr<RHIShaderResourceView> m_DirShadowSRV;
        std::array<std::shared_ptr<RHIShaderResourceView>, MAX_SPOT_SHADOW_MAPS> m_SpotShadowSRVs{};
        std::array<std::shared_ptr<RHIShaderResourceView>, MAX_POINT_SHADOW_MAPS> m_PointShadowSRVs{};
        std::array<std::shared_ptr<RHIShaderResourceView>, 3> m_IblSRVs{};
    };
}
