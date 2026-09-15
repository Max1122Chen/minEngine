#pragma once

#include "Shell/EditorCommandStack.h"

#include <cstdint>
#include <string>

namespace minEngine
{
    class SceneEditor;

    class EditorReparentGameObjectCommand final : public EditorCommand
    {
    public:
        EditorReparentGameObjectCommand(SceneEditor& sceneEditor,
                                  uint64_t gameObjectId,
                                  uint64_t oldParentId,
                                  uint64_t newParentId);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        SceneEditor& m_SceneEditor;
        uint64_t m_GameObjectId = 0;
        uint64_t m_OldParentId = 0;
        uint64_t m_NewParentId = 0;
        mutable std::string m_Description;
    };
}