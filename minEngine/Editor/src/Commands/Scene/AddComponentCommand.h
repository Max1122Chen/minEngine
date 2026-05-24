#pragma once

#include "Shell/EditorCommandStack.h"

#include <cstdint>
#include <string>

namespace minEngine
{
    class Component;
    class SceneEditor;

    class AddComponentCommand final : public IEditorCommand
    {
    public:
        AddComponentCommand(SceneEditor& sceneEditor,
                            uint64_t ownerGameObjectId,
                            std::string componentTypeName);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        SceneEditor& m_SceneEditor;
        uint64_t m_OwnerGameObjectId = 0;
        std::string m_ComponentTypeName;
        Component* m_CreatedComponent = nullptr;
        mutable std::string m_Description;
    };
}

