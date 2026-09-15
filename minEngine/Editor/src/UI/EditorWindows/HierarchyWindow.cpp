#include "HierarchyWindow.h"

#include "ContextMenu/Contexts/HierarchyMenuContext.h"
#include "ContextMenu/EditorContextMenuSystem.h"
#include "ContextMenu/EditorMenuContext.h"
#include "Shell/EditorContextHelpers.h"
#include "UI/Appearance/EditorAppearance.h"
#include "UI/Appearance/EditorThemeScope.h"
#include "UI/Appearance/EditorTypographyScope.h"
#include "UI/Appearance/EditorWindowTheme.h"
#include "UI/Appearance/EditorWindowTypography.h"
#include "UI/Widgets/InlineRenameField.h"

#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"
#include "Runtime/Function/Framework/Prefab/PrefabUtility.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "SubEditor/Scene/SceneEditor.h"

#include "Runtime/Core/Log/LogSystem.h"

#include "IconFontCppHeaders/IconsFontAwesome7.h"

#include <algorithm>
#include <vector>

namespace minEngine
{
    void HierarchyWindow::OnDraw()
    {
        if (!EditorWindowTypography::BeginPanel(m_Context, m_Title.c_str()))
        {
            return;
        }

        {
            EditorAppearance& appearance = m_Context.GetEditorAppearance();
            EditorTypographyScope bodyTypography(appearance, EditorTypographyRole::Body);

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 2.0f));

            m_HitRowThisFrame = false;

            if (m_Context.IsPlaying())
            {
                ImGui::TextDisabled("Inspecting: PIE");
            }

            if (ImGui::Button("Create Empty"))
            {
                GetSceneEditor(&m_Context)->SubmitAddEmptyGOToScene(m_Context);
            }
            ImGui::Separator();

            const std::vector<GameObject*> gameObjects = GetSceneEditor(&m_Context)->GetHierarchyGameObjects();
            if (gameObjects.empty())
            {
                ImGui::TextUnformatted("No GameObject in current scene.");
            }
            else
            {
                TryCaptureF2RenameRequest();
                TryConsumePendingRenameRequest();
                DrawHierarchyTree(gameObjects);
            }

            UpdateDragSessionEndOfFrame();
            DrawStickyTargetHighlight();
            DrawDragGhostOverlay();
            CommitDragSessionIfReleased();

            if (!IsHierarchyDragActive())
            {
                TryDrawRightClickBlankSpaceMenu();
            }

            ImGui::PopStyleVar(2);
        }

        ImGui::End();
    }

    void HierarchyWindow::DrawHierarchyTree(const std::vector<GameObject*>& allGameObjects)
    {
        std::vector<GameObject*> roots;
        roots.reserve(allGameObjects.size());
        for (GameObject* gameObject : allGameObjects)
        {
            if (gameObject != nullptr && gameObject->GetParent() == nullptr)
            {
                roots.push_back(gameObject);
            }
        }

        for (GameObject* root : roots)
        {
            DrawGameObjectNode(*root);
        }
    }

    void HierarchyWindow::DrawGameObjectNode(GameObject& gameObject)
    {
        EditorAppearance& appearance = m_Context.GetEditorAppearance();
        SceneEditor* sceneEditor = GetSceneEditor(&m_Context);
        if (!sceneEditor)
        {
            return;
        }

        const uint64_t gameObjectId = gameObject.GetID();
        ImGui::PushID(static_cast<int>(gameObjectId ^ (gameObjectId >> 32)));

        if (m_RenamingGameObjectId == gameObjectId)
        {
            if (m_RequestRenameFocus)
            {
                ImGui::SetKeyboardFocusHere();
                m_RequestRenameFocus = false;
            }

            {
                EditorThemeScope renameFieldTheme = EditorWindowTheme::Field(appearance);
                const bool committed = ImGui::InputText("##Rename",
                                                        m_RenameBuffer,
                                                        sizeof(m_RenameBuffer),
                                                        ImGuiInputTextFlags_AutoSelectAll |
                                                            ImGuiInputTextFlags_EnterReturnsTrue);

                if (committed || ImGui::IsItemDeactivatedAfterEdit())
                {
                    sceneEditor->SubmitRenameGameObject(m_Context, gameObjectId, m_RenameBuffer);
                    m_RenamingGameObjectId = kInvalidGameObjectId;
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                {
                    m_RenamingGameObjectId = kInvalidGameObjectId;
                }
            }

            std::vector<GameObject*> renameChildren = gameObject.GetChildren();
            std::sort(renameChildren.begin(),
                      renameChildren.end(),
                      [](const GameObject* lhs, const GameObject* rhs)
                      {
                          return lhs->GetID() < rhs->GetID();
                      });
            if (m_CollapsedGameObjectIds.find(gameObjectId) == m_CollapsedGameObjectIds.end())
            {
                for (GameObject* child : renameChildren)
                {
                    if (child != nullptr)
                    {
                        DrawGameObjectNode(*child);
                    }
                }
            }

            ImGui::PopID();
            return;
        }

        std::vector<GameObject*> children = gameObject.GetChildren();
        std::sort(children.begin(), children.end(), [](const GameObject* lhs, const GameObject* rhs)
        {
            return lhs->GetID() < rhs->GetID();
        });

        const bool hasChildren = !children.empty();
        const bool selected = sceneEditor->IsGameObjectSelected(gameObjectId);
        const bool isDraggingThis = GetDraggingGameObjectId() == gameObjectId;

        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (!hasChildren)
        {
            nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
        if (selected)
        {
            nodeFlags |= ImGuiTreeNodeFlags_Selected;
        }

        const bool wantOpen = m_CollapsedGameObjectIds.find(gameObjectId) == m_CollapsedGameObjectIds.end();
        ImGui::SetNextItemOpen(wantOpen);

        if (isDraggingThis)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.55f);
        }

        const std::string displayName = sceneEditor->GetGameObjectDisplayName(gameObject);

        bool pushedPrefabTextColor = false;
        bool isPrefabInstanceRoot = false;
        if (!sceneEditor->IsEditingPrefabStage())
        {
            if (Scene* scene = sceneEditor->GetActiveScene())
            {
                if (const PrefabInstanceRecord* record =
                        PrefabUtility::FindInstanceRecord(*scene, gameObject.GetGuid()))
                {
                    isPrefabInstanceRoot = record->RootInstanceGuid == gameObject.GetGuid();
                    const ImU32 prefabColor =
                        appearance.GetDisplayColorU32(appearance.GetSemanticColors().HierarchyPrefabInstance);
                    ImGui::PushStyleColor(ImGuiCol_Text, prefabColor);
                    pushedPrefabTextColor = true;
                }
            }
        }

        std::string label = displayName;
        if (isPrefabInstanceRoot)
        {
            label = std::string(ICON_FA_CUBES) + " " + displayName;
        }

        const bool nodeOpen = ImGui::TreeNodeEx("##node", nodeFlags, "%s", label.c_str());
        if (pushedPrefabTextColor)
        {
            ImGui::PopStyleColor();
        }

        const ImVec2 itemRectMin = ImGui::GetItemRectMin();
        const ImVec2 itemRectMax = ImGui::GetItemRectMax();

        if (isDraggingThis)
        {
            ImGui::PopStyleVar();
        }

        if (ImGui::IsItemToggledOpen())
        {
            if (nodeOpen)
            {
                m_CollapsedGameObjectIds.erase(gameObjectId);
            }
            else
            {
                m_CollapsedGameObjectIds.insert(gameObjectId);
            }
        }

        if (selected)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImU32 barColor =
                appearance.GetDisplayColorU32(appearance.GetSemanticColors().HierarchySelectionBar);
            drawList->AddRectFilled(ImVec2(itemRectMin.x + 3.0f, itemRectMin.y + 2.0f),
                                    ImVec2(itemRectMin.x + 6.0f, itemRectMax.y - 2.0f),
                                    barColor,
                                    2.0f);
        }

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
        {
            sceneEditor->SelectGameObject(gameObjectId);
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
            !IsHierarchyDragActive())
        {
            sceneEditor->SelectGameObject(gameObjectId);
            BeginRename(gameObject);
        }

        // Start drag from our own threshold — do not rely on ImGui DnD delivery/ownership.
        if (!m_DragSessionActive && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            sceneEditor->SelectGameObject(gameObjectId);
            BeginDragSession(gameObjectId);
        }

        if (m_DragSessionActive &&
            IsMouseInRect(itemRectMin, itemRectMax, kRowHitPaddingX, kRowHitPaddingY))
        {
            NoteRowHitThisFrame();
            if (IsValidReparentTarget(m_DraggedGoId, gameObjectId))
            {
                SetStickyTargetAsChild(gameObjectId, itemRectMin, itemRectMax);
            }
        }

        if (ImGui::BeginPopupContextItem())
        {
            DrawHierarchyGameObjectContextMenu(gameObject);
            ImGui::EndPopup();
        }

        if (nodeOpen && hasChildren)
        {
            for (GameObject* child : children)
            {
                if (child != nullptr)
                {
                    DrawGameObjectNode(*child);
                }
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    void HierarchyWindow::BeginDragSession(uint64_t draggedGoId)
    {
        m_DragSessionActive = true;
        m_DraggedGoId = draggedGoId;
        m_DragMouseWasDown = true;
        ClearStickyTarget();
        m_HitRowThisFrame = false;
        ME_LOG(LogEditor, Info, "Hierarchy DnD: begin drag GO id={}.", draggedGoId);
    }

    void HierarchyWindow::ClearDragSession()
    {
        m_DragSessionActive = false;
        m_DraggedGoId = kInvalidGameObjectId;
        m_DragMouseWasDown = false;
        ClearStickyTarget();
        m_HitRowThisFrame = false;
    }

    void HierarchyWindow::ClearStickyTarget()
    {
        m_DropKind = ReparentDropKind::None;
        m_TargetParentId = 0;
        m_TargetHighlightMin = ImVec2();
        m_TargetHighlightMax = ImVec2();
        m_TargetGraceFramesRemaining = 0;
    }

    void HierarchyWindow::SetStickyTargetAsChild(uint64_t targetParentId, ImVec2 rectMin, ImVec2 rectMax)
    {
        m_DropKind = ReparentDropKind::AsChild;
        m_TargetParentId = targetParentId;
        m_TargetHighlightMin = rectMin;
        m_TargetHighlightMax = rectMax;
        m_TargetGraceFramesRemaining = kTargetGraceFrames;
    }

    void HierarchyWindow::SetStickyTargetAsRoot()
    {
        m_DropKind = ReparentDropKind::AsRoot;
        m_TargetParentId = 0;
        m_TargetGraceFramesRemaining = kTargetGraceFrames;

        const ImVec2 absMin = ImGui::GetWindowPos();
        const ImVec2 minPos = ImGui::GetWindowContentRegionMin();
        const ImVec2 maxPos = ImGui::GetWindowContentRegionMax();
        m_TargetHighlightMin = ImVec2(absMin.x + minPos.x, absMin.y + maxPos.y - 22.0f);
        m_TargetHighlightMax = ImVec2(absMin.x + maxPos.x, absMin.y + maxPos.y);
    }

    void HierarchyWindow::NoteRowHitThisFrame()
    {
        m_HitRowThisFrame = true;
    }

    void HierarchyWindow::UpdateDragSessionEndOfFrame()
    {
        if (!m_DragSessionActive)
        {
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        {
            ClearDragSession();
            return;
        }

        if (!m_HitRowThisFrame && IsMouseInHierarchyPanel())
        {
            SceneEditor* sceneEditor = GetSceneEditor(&m_Context);
            Scene* scene = sceneEditor != nullptr ? sceneEditor->GetActiveScene() : nullptr;
            GameObject* dragged = scene != nullptr ? scene->FindGameObjectById(m_DraggedGoId) : nullptr;
            if (dragged != nullptr && dragged->GetParent() != nullptr)
            {
                SetStickyTargetAsRoot();
            }
        }
        else if (!m_HitRowThisFrame)
        {
            if (m_DropKind != ReparentDropKind::None)
            {
                --m_TargetGraceFramesRemaining;
                if (m_TargetGraceFramesRemaining <= 0)
                {
                    ClearStickyTarget();
                }
            }
        }
    }

    void HierarchyWindow::CommitDragSessionIfReleased()
    {
        if (!m_DragSessionActive)
        {
            return;
        }

        // Use raw IO + edge detect. ImGui::IsMouseReleased() can fail under ActiveId/DnD ownership,
        // which previously cleared the session on the next frame without committing.
        const bool mouseDown = ImGui::GetIO().MouseDown[ImGuiMouseButton_Left];
        const bool releasedEdge = m_DragMouseWasDown && !mouseDown;
        m_DragMouseWasDown = mouseDown;

        if (!releasedEdge)
        {
            return;
        }

        const ReparentDropKind dropKind = m_DropKind;
        const uint64_t draggedId = m_DraggedGoId;
        const uint64_t targetParentId = m_TargetParentId;
        ClearDragSession();

        if (dropKind == ReparentDropKind::None)
        {
            ME_LOG(LogEditor, Info, "Hierarchy DnD: release with no sticky target (cancelled).");
            return;
        }

        SceneEditor* sceneEditor = GetSceneEditor(&m_Context);
        if (!sceneEditor)
        {
            return;
        }

        if (dropKind == ReparentDropKind::AsRoot)
        {
            ME_LOG(LogEditor, Info, "Hierarchy DnD: commit detach GO id={}.", draggedId);
            sceneEditor->SubmitReparentGameObject(m_Context, draggedId, SceneEditor::kSceneRootParentId);
            return;
        }

        if (dropKind == ReparentDropKind::AsChild)
        {
            if (!IsValidReparentTarget(draggedId, targetParentId))
            {
                ME_LOG(LogEditor, Warn, 
                    "Hierarchy DnD: sticky target id={} invalid for dragged id={}; skip.",
                    targetParentId,
                    draggedId);
                return;
            }

            ME_LOG(LogEditor, Info, 
                "Hierarchy DnD: commit reparent GO id={} under parent id={}.",
                draggedId,
                targetParentId);
            sceneEditor->SubmitReparentGameObject(m_Context, draggedId, targetParentId);
        }
    }

    void HierarchyWindow::DrawStickyTargetHighlight()
    {
        if (!m_DragSessionActive || m_DropKind == ReparentDropKind::None)
        {
            return;
        }

        DrawDropHighlight(m_TargetHighlightMin, m_TargetHighlightMax, m_Context.GetEditorAppearance());
    }

    void HierarchyWindow::DrawDropHighlight(ImVec2 rectMin, ImVec2 rectMax, EditorAppearance& appearance)
    {
        const ImU32 border = appearance.GetDisplayColorU32(appearance.GetSemanticColors().HierarchySelectionBar);
        const ImU32 fill =
            appearance.GetDisplayColorU32(appearance.GetSemanticColors().HierarchySelectionHeader);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(rectMin, rectMax, fill, 2.0f);
        drawList->AddRect(rectMin, rectMax, border, 2.0f, 0, 1.5f);
    }

    void HierarchyWindow::DrawDragGhostOverlay()
    {
        if (!m_DragSessionActive)
        {
            return;
        }

        SceneEditor* sceneEditor = GetSceneEditor(&m_Context);
        Scene* scene = sceneEditor != nullptr ? sceneEditor->GetActiveScene() : nullptr;
        GameObject* dragged = scene != nullptr ? scene->FindGameObjectById(m_DraggedGoId) : nullptr;
        if (!dragged)
        {
            return;
        }

        EditorAppearance& appearance = m_Context.GetEditorAppearance();
        const std::string label = sceneEditor->GetGameObjectDisplayName(*dragged);
        const ImVec2 mousePos = ImGui::GetMousePos();
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
        const ImVec2 pad(8.0f, 4.0f);
        const ImVec2 rectMin(mousePos.x + 12.0f, mousePos.y + 16.0f);
        const ImVec2 rectMax(rectMin.x + textSize.x + pad.x * 2.0f, rectMin.y + textSize.y + pad.y * 2.0f);

        drawList->AddRectFilled(rectMin, rectMax, IM_COL32(40, 40, 40, 180), 4.0f);
        drawList->AddRect(rectMin,
                          rectMax,
                          appearance.GetDisplayColorU32(appearance.GetSemanticColors().HierarchySelectionBar),
                          4.0f,
                          0,
                          1.5f);
        drawList->AddText(ImVec2(rectMin.x + pad.x, rectMin.y + pad.y),
                          IM_COL32(255, 255, 255, 200),
                          label.c_str());
    }

    uint64_t HierarchyWindow::GetDraggingGameObjectId() const
    {
        return m_DragSessionActive ? m_DraggedGoId : kInvalidGameObjectId;
    }

    bool HierarchyWindow::IsValidReparentTarget(uint64_t draggedId, uint64_t targetParentId) const
    {
        if (draggedId == targetParentId)
        {
            return false;
        }

        SceneEditor* sceneEditor = GetSceneEditor(&m_Context);
        Scene* scene = sceneEditor != nullptr ? sceneEditor->GetActiveScene() : nullptr;
        if (!scene)
        {
            return false;
        }

        GameObject* target = scene->FindGameObjectById(targetParentId);
        if (!target)
        {
            return false;
        }

        for (GameObject* walk = target; walk != nullptr; walk = walk->GetParent())
        {
            if (walk->GetID() == draggedId)
            {
                return false;
            }
        }

        GameObject* dragged = scene->FindGameObjectById(draggedId);
        if (dragged != nullptr && dragged->GetParent() == target)
        {
            return false;
        }

        return true;
    }

    bool HierarchyWindow::IsMouseInHierarchyPanel() const
    {
        return ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    }

    bool HierarchyWindow::IsMouseInRect(ImVec2 rectMin, ImVec2 rectMax, float padX, float padY)
    {
        const ImVec2 mouse = ImGui::GetMousePos();
        return mouse.x >= rectMin.x - padX && mouse.x <= rectMax.x + padX && mouse.y >= rectMin.y - padY &&
               mouse.y <= rectMax.y + padY;
    }

    void HierarchyWindow::TryCaptureF2RenameRequest()
    {
        if (IsHierarchyDragActive())
        {
            return;
        }

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_F2, false))
        {
            if (GameObject* selected = GetSceneEditor(&m_Context)->GetSelectedGameObject())
            {
                BeginRename(*selected);
            }
        }
    }

    void HierarchyWindow::TryConsumePendingRenameRequest()
    {
        if (IsHierarchyDragActive())
        {
            return;
        }

        SceneEditor* sceneEditor = GetSceneEditor(&m_Context);
        if (!sceneEditor)
        {
            return;
        }

        const uint64_t pendingId = sceneEditor->ConsumePendingRenameGameObjectId();
        if (pendingId == kInvalidGameObjectId)
        {
            return;
        }

        Scene* scene = sceneEditor->GetActiveScene();
        if (!scene)
        {
            return;
        }

        GameObject* gameObject = scene->FindGameObjectById(pendingId);
        if (!gameObject)
        {
            return;
        }

        BeginRename(*gameObject);
    }

    void HierarchyWindow::BeginRename(const GameObject& gameObject)
    {
        m_RenamingGameObjectId = gameObject.GetID();
        std::memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
        std::strncpy(m_RenameBuffer, gameObject.GetName().c_str(), sizeof(m_RenameBuffer) - 1);
        m_RequestRenameFocus = true;
    }

    bool HierarchyWindow::TryDrawRightClickBlankSpaceMenu()
    {
        if (ImGui::BeginPopupContextWindow("##HierarchyBlankMenu",
                                           ImGuiPopupFlags_MouseButtonRight |
                                               ImGuiPopupFlags_NoOpenOverItems))
        {
            SceneEditor* sceneEditor = GetSceneEditor(&m_Context);
            auto hierarchyContext = std::make_shared<HierarchyMenuContext>();
            hierarchyContext->HitKind = HierarchyHitKind::Blank;
            hierarchyContext->bClickedEmpty = true;
            hierarchyContext->bAllowPrefabLevelWorkflow =
                sceneEditor != nullptr
                && !sceneEditor->IsEditingPrefabStage()
                && !m_Context.IsPlaying();

            EditorMenuContext menuContext;
            menuContext.Add(hierarchyContext);
            m_Context.GetContextMenu().BuildAndDraw(m_Context, menuContext);
            ImGui::EndPopup();
            return true;
        }
        return false;
    }

    void HierarchyWindow::DrawHierarchyGameObjectContextMenu(GameObject& gameObject)
    {
        SceneEditor* sceneEditor = GetSceneEditor(&m_Context);
        sceneEditor->SelectGameObject(gameObject.GetID());

        auto hierarchyContext = std::make_shared<HierarchyMenuContext>();
        hierarchyContext->HitKind = HierarchyHitKind::GameObjectItem;
        hierarchyContext->bClickedEmpty = false;
        hierarchyContext->bAllowPrefabLevelWorkflow =
            sceneEditor != nullptr
            && !sceneEditor->IsEditingPrefabStage()
            && !m_Context.IsPlaying();
        hierarchyContext->SelectedGameObjectIds.push_back(gameObject.GetID());

        EditorMenuContext menuContext;
        menuContext.Add(hierarchyContext);
        m_Context.GetContextMenu().BuildAndDraw(m_Context, menuContext);
    }
}