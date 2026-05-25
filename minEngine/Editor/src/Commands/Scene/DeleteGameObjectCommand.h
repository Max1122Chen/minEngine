#pragma once

#include "Shell/EditorCommandStack.h"

#include <cstdint>
#include <string>
#include <vector>

namespace minEngine
{
    class SceneEditor;

    class DeleteGameObjectCommand final : public IEditorCommand
    {
    public:
        DeleteGameObjectCommand(SceneEditor& sceneEditor, uint64_t gameObjectId);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        SceneEditor& m_SceneEditor;
        uint64_t m_GameObjectId = 0;
        bool m_Removed = false;
        std::vector<uint8_t> m_SnapshotEnvelope;
        mutable std::string m_Description;
    };
}
