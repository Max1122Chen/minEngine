#include "Services/AddComponentPicker.h"

#include "Services/ComponentTypeUiCatalog.h"
#include "SubEditor/Scene/SceneEditor.h"
#include "UI/Appearance/EditorAppearance.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cfloat>

namespace minEngine
{
    namespace
    {
        constexpr float kInspectorPopupListHeight = 220.0f;
        constexpr float kContextMenuListHeight = 240.0f;
        constexpr float kContextMenuListWidth = 260.0f;
    }

    bool AddComponentPicker::MatchesFilter(std::string_view typeName, std::string_view filterText)
    {
        if (filterText.empty())
        {
            return true;
        }

        const std::string displayName = ComponentTypeUiCatalog::MakeTypeDisplayName(typeName);
        const std::string shortName = ComponentTypeUiCatalog::GetShortTypeName(typeName);

        auto containsInsensitive = [](std::string_view haystack, std::string_view needle) -> bool
        {
            if (needle.empty())
            {
                return true;
            }
            if (haystack.size() < needle.size())
            {
                return false;
            }

            for (size_t start = 0; start + needle.size() <= haystack.size(); ++start)
            {
                bool match = true;
                for (size_t i = 0; i < needle.size(); ++i)
                {
                    const unsigned char left = static_cast<unsigned char>(haystack[start + i]);
                    const unsigned char right = static_cast<unsigned char>(needle[i]);
                    if (std::tolower(left) != std::tolower(right))
                    {
                        match = false;
                        break;
                    }
                }
                if (match)
                {
                    return true;
                }
            }
            return false;
        };

        return containsInsensitive(displayName, filterText) || containsInsensitive(shortName, filterText)
            || containsInsensitive(typeName, filterText);
    }

    void AddComponentPicker::DrawTypeRowIcon(IEditorContext& editor, std::string_view typeName)
    {
        ComponentTypeUiCatalog::DrawIcon(editor.GetEditorAppearance(), typeName);
        ImGui::SameLine();
    }

    bool AddComponentPicker::DrawFilteredTypeList(IEditorContext& editor,
                                                  SceneEditor& sceneEditor,
                                                  const std::vector<std::string>& componentTypeNames,
                                                  std::string_view filterText,
                                                  uint64_t targetGameObjectId,
                                                  bool selectTargetBeforeAdd,
                                                  bool closePopupOnAdd)
    {
        bool added = false;
        for (const std::string& typeName : componentTypeNames)
        {
            if (!MatchesFilter(typeName, filterText))
            {
                continue;
            }

            ImGui::PushID(typeName.c_str());
            DrawTypeRowIcon(editor, typeName);
            const std::string displayName = ComponentTypeUiCatalog::MakeTypeDisplayName(typeName);
            if (ImGui::Selectable(displayName.c_str()))
            {
                if (selectTargetBeforeAdd)
                {
                    sceneEditor.SelectGameObject(targetGameObjectId);
                }
                sceneEditor.SubmitAddComponentToSelectedGameObject(editor, typeName);
                added = true;
                if (closePopupOnAdd)
                {
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::PopID();

            if (added)
            {
                break;
            }
        }
        return added;
    }

    bool AddComponentPicker::DrawInspectorAddSection(IEditorContext& editor,
                                                     SceneEditor& sceneEditor,
                                                     std::string& selectedTypeNameInOut,
                                                     char* filterBuffer,
                                                     size_t filterBufferSize)
    {
        (void)selectedTypeNameInOut;

        const std::vector<std::string>& componentTypeNames = sceneEditor.GetAllComponentTypeNames();
        if (componentTypeNames.empty())
        {
            ImGui::TextUnformatted("No reflected Component derived types found.");
            return false;
        }

        const ImVec2 triggerSize(ImGui::GetContentRegionAvail().x, 0.0f);
        if (ImGui::Button("Add Component##InspectorDropdown", triggerSize))
        {
            ImGui::OpenPopup("##InspectorAddComponentPopup");
            if (filterBuffer != nullptr && filterBufferSize > 0)
            {
                filterBuffer[0] = '\0';
            }
        }

        bool added = false;
        if (ImGui::BeginPopup("##InspectorAddComponentPopup"))
        {
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (filterBuffer != nullptr && filterBufferSize > 0)
            {
                if (ImGui::IsWindowAppearing())
                {
                    ImGui::SetKeyboardFocusHere();
                }
                ImGui::InputTextWithHint(
                    "##AddComponentFilter", "Search components...", filterBuffer, filterBufferSize);
            }

            const std::string_view filterText = filterBuffer != nullptr ? filterBuffer : "";
            ImGui::BeginChild(
                "##AddComponentList",
                ImVec2(0.0f, kInspectorPopupListHeight),
                ImGuiChildFlags_Borders,
                ImGuiWindowFlags_AlwaysVerticalScrollbar);
            added = DrawFilteredTypeList(
                editor, sceneEditor, componentTypeNames, filterText, 0, false, true);
            ImGui::EndChild();
            ImGui::EndPopup();
        }

        return added;
    }

    void AddComponentPicker::DrawContextSubMenu(IEditorContext& editor,
                                                SceneEditor& sceneEditor,
                                                uint64_t targetGameObjectId,
                                                char* filterBuffer,
                                                size_t filterBufferSize)
    {
        const std::vector<std::string>& componentTypeNames = sceneEditor.GetAllComponentTypeNames();
        if (componentTypeNames.empty())
        {
            ImGui::TextDisabled("No component types");
            return;
        }

        ImGui::SetNextItemWidth(kContextMenuListWidth);
        ImGui::InputTextWithHint("##ContextAddComponentFilter", "Search...", filterBuffer, filterBufferSize);
        const std::string_view filterText = filterBuffer != nullptr ? filterBuffer : "";

        ImGui::BeginChild(
            "##ContextAddComponentList",
            ImVec2(kContextMenuListWidth, kContextMenuListHeight),
            ImGuiChildFlags_Borders,
            ImGuiWindowFlags_AlwaysVerticalScrollbar);
        DrawFilteredTypeList(
            editor, sceneEditor, componentTypeNames, filterText, targetGameObjectId, true, false);
        ImGui::EndChild();
    }
}
