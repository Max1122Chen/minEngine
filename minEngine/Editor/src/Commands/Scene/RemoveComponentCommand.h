#pragma once

#include "Shell/EditorCommandStack.h"

#include <cstdint>
#include <string>

namespace minEngine
{
    class Component;
    class GameObject;
    class SceneEditor;

    class RemoveComponentCommand final : public IEditorCommand
    {
    public:
        RemoveComponentCommand(SceneEditor& sceneEditor,
                               uint64_t ownerGameObjectId,
                               std::string componentTypeName);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        bool TryFindFirstComponentByType(GameObject& owner, Component*& outComponent) const;

        SceneEditor& m_SceneEditor;
        uint64_t m_OwnerGameObjectId = 0;
        std::string m_ComponentTypeName;
        bool m_Removed = false;
        mutable std::string m_Description;
    };
}

