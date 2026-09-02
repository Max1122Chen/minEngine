#include "UI/Dialogs/EditorUnsavedChangesDialog.h"

#include "imgui.h"

#include <cstring>

namespace minEngine
{
    void EditorUnsavedChangesDialog::Open(const char* message)
    {
        m_Open = true;
        if (message != nullptr)
        {
            std::strncpy(m_Message, message, sizeof(m_Message) - 1);
            m_Message[sizeof(m_Message) - 1] = '\0';
        }
        else
        {
            m_Message[0] = '\0';
        }

        ImGui::OpenPopup("Unsaved Changes");
    }

    void EditorUnsavedChangesDialog::Close()
    {
        m_Open = false;
        m_Message[0] = '\0';
    }

    UnsavedChangesChoice EditorUnsavedChangesDialog::Draw()
    {
        if (!m_Open)
        {
            return UnsavedChangesChoice::None;
        }

        ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal(
                "Unsaved Changes",
                &m_Open,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
        {
            return UnsavedChangesChoice::None;
        }

        if (m_Message[0] != '\0')
        {
            ImGui::TextWrapped("%s", m_Message);
        }
        else
        {
            ImGui::TextWrapped("Save changes before continuing?");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        UnsavedChangesChoice choice = UnsavedChangesChoice::None;

        if (ImGui::Button("Save", ImVec2(120.0f, 0.0f)))
        {
            choice = UnsavedChangesChoice::Save;
            ImGui::CloseCurrentPopup();
            Close();
        }

        ImGui::SameLine();
        if (ImGui::Button("Don't Save", ImVec2(120.0f, 0.0f)))
        {
            choice = UnsavedChangesChoice::Discard;
            ImGui::CloseCurrentPopup();
            Close();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
        {
            choice = UnsavedChangesChoice::Cancel;
            ImGui::CloseCurrentPopup();
            Close();
        }

        ImGui::EndPopup();
        return choice;
    }
}
