#pragma once

#include "Shell/EditorCommandStack.h"

#include <cstdint>
#include <string>

namespace minEngine
{
    class SceneEditor;

    class AddEmptyGameObjectCommand final : public IEditorCommand
    {
    public:
        explicit AddEmptyGameObjectCommand(SceneEditor& sceneEditor);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        SceneEditor& m_SceneEditor;
        uint64_t m_CreatedGameObjectId = 0;
        mutable std::string m_Description;
    };
}

