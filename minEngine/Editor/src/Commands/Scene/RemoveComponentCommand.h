#pragma once

#include "Shell/EditorCommandStack.h"

#include <cstdint>
#include <string>
#include <vector>

namespace minEngine
{
    class Component;
    class SceneEditor;

    class RemoveComponentCommand final : public IEditorCommand
    {
    public:
        RemoveComponentCommand(SceneEditor& sceneEditor,
                               uint64_t ownerGameObjectId,
                               const Component& targetComponent);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        SceneEditor& m_SceneEditor;
        uint64_t m_OwnerGameObjectId = 0;
        uint64_t m_ComponentGuidHigh = 0;
        uint64_t m_ComponentGuidLow = 0;
        int32_t m_ComponentIndex = -1;
        std::string m_ComponentTypeName;
        bool m_Removed = false;
        std::vector<uint8_t> m_SnapshotEnvelope;
        mutable std::string m_Description;
    };
}
