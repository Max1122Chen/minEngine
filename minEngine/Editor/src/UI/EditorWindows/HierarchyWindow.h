#pragma once

#include "Core.h"

#include "imgui.h"

#include "Scene/SceneEditor.h"
#include "UI/EditorWindows/EditorWindow.h"

#include "Runtime/Function/Framework/GameObject/GameObject.h"

#include <cstring>
#include <limits>

namespace minEngine
{
    class HierarchyWindow final : public EditorWindow
    {
    public:
        explicit HierarchyWindow(IEditorContext& context)
            : EditorWindow(context)
        {
        }

        const std::string& GetId() const override { return m_Id; }
        const std::string& GetTitle() const override { return m_Title; }
        std::string_view GetOwnerModuleId() const override { return SceneEditor::kModuleId; }

        void OnDraw() override;

    private:
        static constexpr uint64_t kInvalidGameObjectId = std::numeric_limits<uint64_t>::max();

        void TryCaptureF2RenameRequest();
        void BeginRename(const GameObject& gameObject);
        bool TryDrawRightClickBlankSpaceMenu();
        bool TryDrawRightClickGOMenu(GameObject& gameObject);

        const std::string m_Id = "hierarchy";
        const std::string m_Title = "Hierarchy";
        uint64_t m_RenamingGameObjectId = kInvalidGameObjectId;
        bool m_RequestRenameFocus = false;
        char m_RenameBuffer[256] = {};
    };
}
