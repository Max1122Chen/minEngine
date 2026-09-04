#pragma once

#include "Core.h"

#include "imgui.h"

#include "SubEditor/Scene/SceneEditor.h"
#include "UI/EditorWindows/EditorWindow.h"

#include "Runtime/Function/Framework/GameObject/GameObject.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <unordered_set>

namespace minEngine
{
    class EditorAppearance;

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
        enum class ReparentDropKind : uint8_t
        {
            None = 0,
            AsChild,
            AsRoot,
        };

        static constexpr uint64_t kInvalidGameObjectId = std::numeric_limits<uint64_t>::max();
        static constexpr int kTargetGraceFrames = 12;
        static constexpr float kRowHitPaddingX = 4.0f;
        static constexpr float kRowHitPaddingY = 3.0f;

        void TryCaptureF2RenameRequest();
        void TryConsumePendingRenameRequest();
        void BeginRename(const GameObject& gameObject);
        bool TryDrawRightClickBlankSpaceMenu();
        void DrawHierarchyGameObjectContextMenu(GameObject& gameObject);

        void DrawHierarchyTree(const std::vector<GameObject*>& allGameObjects);
        void DrawGameObjectNode(GameObject& gameObject);
        void DrawDragGhostOverlay();
        void DrawStickyTargetHighlight();

        void BeginDragSession(uint64_t draggedGoId);
        void ClearDragSession();
        void ClearStickyTarget();
        void SetStickyTargetAsChild(uint64_t targetParentId, ImVec2 rectMin, ImVec2 rectMax);
        void SetStickyTargetAsRoot();
        void NoteRowHitThisFrame();
        void UpdateDragSessionEndOfFrame();
        void CommitDragSessionIfReleased();

        bool IsHierarchyDragActive() const { return m_DragSessionActive; }
        uint64_t GetDraggingGameObjectId() const;
        bool IsValidReparentTarget(uint64_t draggedId, uint64_t targetParentId) const;
        bool IsMouseInHierarchyPanel() const;
        static bool IsMouseInRect(ImVec2 rectMin, ImVec2 rectMax, float padX, float padY);
        void DrawDropHighlight(ImVec2 rectMin, ImVec2 rectMax, EditorAppearance& appearance);

        const std::string m_Id = "hierarchy";
        const std::string m_Title = "Hierarchy";
        uint64_t m_RenamingGameObjectId = kInvalidGameObjectId;
        bool m_RequestRenameFocus = false;
        char m_RenameBuffer[256] = {};

        /** Default expanded; ids in this set are collapsed. */
        std::unordered_set<uint64_t> m_CollapsedGameObjectIds;

        bool m_DragSessionActive = false;
        uint64_t m_DraggedGoId = kInvalidGameObjectId;
        ReparentDropKind m_DropKind = ReparentDropKind::None;
        uint64_t m_TargetParentId = 0;
        ImVec2 m_TargetHighlightMin{};
        ImVec2 m_TargetHighlightMax{};
        int m_TargetGraceFramesRemaining = 0;
        bool m_HitRowThisFrame = false;
        bool m_DragMouseWasDown = false;
    };
}