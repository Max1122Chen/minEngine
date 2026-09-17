#pragma once

#include "Shell/EditorCommandStack.h"

#include <cstdint>
#include <memory>
#include <string>

namespace minEngine
{
    class Prefab;
    class SceneEditor;
    class GameObject;

    class EditorInstantiatePrefabCommand final : public EditorCommand
    {
    public:
        EditorInstantiatePrefabCommand(
            SceneEditor& sceneEditor,
            std::shared_ptr<Prefab> prefab,
            GameObject* attachParent);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        SceneEditor& m_SceneEditor;
        std::shared_ptr<Prefab> m_Prefab;
        uint64_t m_AttachParentId = 0;
        uint64_t m_CreatedRootId = 0;
        mutable std::string m_Description;
    };
}
