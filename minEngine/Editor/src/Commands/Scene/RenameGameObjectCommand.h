#pragma once

#include "Shell/EditorCommandStack.h"

#include <string>

namespace minEngine
{
    class SceneEditor;

    class RenameGameObjectCommand final : public IEditorCommand
    {
    public:
        RenameGameObjectCommand(SceneEditor& sceneEditor,
                                uint64_t gameObjectId,
                                std::string oldName,
                                std::string newName);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        SceneEditor& m_SceneEditor;
        uint64_t m_GameObjectId = 0;
        std::string m_OldName;
        std::string m_NewName;
        mutable std::string m_Description;
    };
}
