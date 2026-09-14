#pragma once

#include "Shell/EditorCommandStack.h"

#include <cstdint>
#include <string>

namespace minEngine
{
    struct GUID;
    class SceneEditor;

    class EditorMoveComponentCommand final : public EditorCommand
    {
    public:
        EditorMoveComponentCommand(SceneEditor& sceneEditor,
                             uint64_t ownerGameObjectId,
                             const GUID& componentGuid,
                             size_t fromIndex,
                             size_t toIndex);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        SceneEditor& m_SceneEditor;
        uint64_t m_OwnerGameObjectId = 0;
        uint64_t m_ComponentGuidHigh = 0;
        uint64_t m_ComponentGuidLow = 0;
        size_t m_FromIndex = 0;
        size_t m_ToIndex = 0;
        mutable std::string m_Description;
    };
}
