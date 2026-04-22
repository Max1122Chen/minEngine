#pragma once

#include "Core.h"

#include "imgui.h"

#include "Editor.h"
#include "EditorWindow.h"

#include "Runtime/Function/Framework/GameObject/GameObject.h"

#include <cstring>
#include <limits>

namespace minEngine
{
    class HierarchyWindow final : public EditorWindow
    {
    public:
        explicit HierarchyWindow(Editor& editor)
            : EditorWindow(editor)
        {
        }

        const std::string& GetId() const override
        {
            return m_Id;
        }

        const std::string& GetTitle() const override
        {
            return m_Title;
        }

        void OnDraw() override;

    private:
        static constexpr uint64_t kInvalidGameObjectId = std::numeric_limits<uint64_t>::max();

        void BeginRename(const GameObject& gameObject)
        {
            m_RenamingGameObjectId = gameObject.GetID();
            std::memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
            std::strncpy(m_RenameBuffer, gameObject.GetName().c_str(), sizeof(m_RenameBuffer) - 1);
            m_RequestRenameFocus = true;
        }

        const std::string m_Id = "hierarchy";
        const std::string m_Title = "Hierarchy";
        uint64_t m_RenamingGameObjectId = kInvalidGameObjectId;
        bool m_RequestRenameFocus = false;
        char m_RenameBuffer[256] = {};
    };
}
