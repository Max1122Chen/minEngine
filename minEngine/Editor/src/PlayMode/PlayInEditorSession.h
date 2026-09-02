#pragma once

#include "Core.h"
#include "IPlayModeService.h"
#include "PlayObjectMapping.h"
#include "Runtime/Function/Framework/Scene/SceneCloneContext.h"
#include "Runtime/Function/Framework/Scene/SceneTypes.h"

#include <memory>
#include <vector>

namespace minEngine
{
    class Scene;

    class PlayInEditorSession final : public IPlayModeService
    {
    public:
        PlayState GetPlayState() const override { return m_State; }
        bool IsPlaying() const override { return m_State == PlayState::Playing; }
        bool EnterPlay() override;
        void Stop() override;
        void TickPIE(float deltaTime) override;

        Scene* GetEditorScene() const override;
        Scene* GetPIEScene() const override;
        PlayObjectMapping& GetObjectMapping() override { return m_ObjectMapping; }

        const SceneContext* GetEditorContext() const;
        const SceneContext* GetPIEContext(int32_t instanceId = 0) const;
        Scene* GetTickTargetScene() const;

    private:
        void SyncSceneContextsToManager();

        PlayState m_State = PlayState::Editing;
        SceneContext m_EditorContext;
        std::vector<SceneContext> m_PIEContexts;
        PlayObjectMapping m_ObjectMapping;
        SceneCloneContext m_CloneContext;
    };
}
