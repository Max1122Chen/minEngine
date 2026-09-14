#pragma once

#include "Runtime/Function/Framework/Transform/Transform.h"
#include "Shell/EditorCommandStack.h"

#include <string>

namespace minEngine
{
    class SceneEditor;

    class EditorSetGameObjectTransformCommand final : public EditorCommand
    {
    public:
        EditorSetGameObjectTransformCommand(SceneEditor& sceneEditor,
                                      uint64_t gameObjectId,
                                      Transform before,
                                      Transform after);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        SceneEditor& m_SceneEditor;
        uint64_t m_GameObjectId = 0;
        Transform m_Before;
        Transform m_After;
        mutable std::string m_Description;
    };
}
