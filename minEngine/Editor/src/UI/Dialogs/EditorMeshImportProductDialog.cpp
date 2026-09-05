#include "UI/Dialogs/EditorMeshImportProductDialog.h"

#include <imgui.h>

#include <cstdio>

namespace minEngine
{
    void EditorMeshImportProductDialog::Open(int sourceFileCount)
    {
        m_Open = true;
        m_SourceFileCount = sourceFileCount;
        ImGui::OpenPopup("Import Mesh Product");
    }

    void EditorMeshImportProductDialog::Close()
    {
        m_Open = false;
        m_SourceFileCount = 0;
    }

    MeshImportProductChoice EditorMeshImportProductDialog::Draw()
    {
        if (!m_Open)
        {
            return MeshImportProductChoice::None;
        }

        ImGui::OpenPopup("Import Mesh Product");

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (!ImGui::BeginPopupModal("Import Mesh Product", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return MeshImportProductChoice::None;
        }

        char message[256];
        std::snprintf(
            message,
            sizeof(message),
            "Import %d source file(s) as which engine asset type?",
            m_SourceFileCount);
        ImGui::TextUnformatted(message);
        ImGui::Separator();
        ImGui::TextWrapped(
            "FBX/glTF are Import Sources. Mesh → .glb/.obj; AnimationClip → .meaclip "
            "(requires {stem}_Skeleton.meskeleton in the destination folder). "
            "Originals are kept under Assets/Sources/.");

        MeshImportProductChoice choice = MeshImportProductChoice::None;

        if (ImGui::Button("SkeletalMesh", ImVec2(140.0f, 0.0f)))
        {
            choice = MeshImportProductChoice::SkeletalMesh;
        }
        ImGui::SameLine();
        if (ImGui::Button("StaticMesh", ImVec2(140.0f, 0.0f)))
        {
            choice = MeshImportProductChoice::StaticMesh;
        }
        ImGui::SameLine();
        if (ImGui::Button("AnimationClip", ImVec2(140.0f, 0.0f)))
        {
            choice = MeshImportProductChoice::AnimationClip;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
        {
            choice = MeshImportProductChoice::Cancel;
        }

        if (choice != MeshImportProductChoice::None)
        {
            ImGui::CloseCurrentPopup();
            m_Open = false;
        }

        ImGui::EndPopup();
        return choice;
    }
}
