#pragma once

#include "Shell/EditorCommandStack.h"

#include <cstdint>
#include <string>

namespace minEngine
{
    struct GUID;
    class SceneEditor;

    class EditorRenameComponentCommand final : public EditorCommand
    {
    public:
        EditorRenameComponentCommand(SceneEditor& sceneEditor,
                               uint64_t ownerGameObjectId,
                               const GUID& componentGuid,
                               std::string oldName,
                               std::string newName);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        SceneEditor& m_SceneEditor;
        uint64_t m_OwnerGameObjectId = 0;
        uint64_t m_ComponentGuidHigh = 0;
        uint64_t m_ComponentGuidLow = 0;
        std::string m_OldName;
        std::string m_NewName;
        mutable std::string m_Description;
    };
}
