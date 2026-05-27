#include "MainMenuWindow.h"

#include "EditorGUIManager.h"
#include "Shell/EditorCommandStack.h"
#include "SubEditor/Material/MaterialEditor.h"
#include "SubEditor/Scene/SceneEditor.h"
#include "Shell/EditorContextHelpers.h"
#include "Services/AssetWorkflowModule.h"
#include "Services/ContentBrowser/ContentBrowserModule.h"
#include "UI/Appearance/EditorAppearance.h"
#include "UI/Appearance/EditorThemePresets.h"
#include "UI/Appearance/EditorTypographyScope.h"
#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Platform/FileDialog/IFileDialogService.h"
#include "Runtime/Resource/AssetTypeRegistry.h"

#include <filesystem>

namespace minEngine
{
    void MainMenuWindow::OnDraw()
    {
        const ImVec2 framePadding = ImGui::GetStyle().FramePadding;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(framePadding.x, framePadding.y + 3.0f));

        if (!ImGui::BeginMainMenuBar())
        {
            ImGui::PopStyleVar();
            return;
        }

        {
            EditorTypographyScope menuTypography(
                m_Context.GetEditorAppearance(),
                EditorTypographyRole::MenuBar);

            DrawFileMenu();
            DrawEditMenu();
            DrawViewMenu();
            DrawWindowModeMenu();
            DrawToolsMenu();
            DrawHelpMenu();
        }
        ImGui::EndMainMenuBar();
        ImGui::PopStyleVar();
    }

    void MainMenuWindow::DrawFileMenu()
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Scene", "Ctrl+N"))
            {
            }

            if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
            {
            }

            SceneEditor* sceneEditor = GetSceneEditor(&m_Context);
            const bool hasScene = sceneEditor && sceneEditor->GetActiveScene();
            const bool canSave = hasScene && sceneEditor->IsSceneDirty();
            if (ImGui::MenuItem("Save", "Ctrl+S", false, canSave))
            {
                sceneEditor->SaveCurrentScene();
            }

            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, false))
            {
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Import Asset...", nullptr, false, true))
            {
                m_Context.GetAssetWorkflow().ImportAssetDialog(
                    m_Context.GetContentBrowser().GetModel().GetCurrentDirectory());
            }
            if (ImGui::MenuItem("Exit"))
            {
                m_Context.RequestExit();
            }
            ImGui::EndMenu();
        }
    }

    void MainMenuWindow::DrawEditMenu()
    {
        if (ImGui::BeginMenu("Edit"))
        {
            const bool canUndo = m_Context.GetCommandStack().CanUndo();
            const bool canRedo = m_Context.GetCommandStack().CanRedo();
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo) && canUndo)
            {
                m_Context.GetCommandStack().Undo();
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo) && canRedo)
            {
                m_Context.GetCommandStack().Redo();
            }
            ImGui::Separator();
            ImGui::MenuItem("Cut", "Ctrl+X", false, false);
            ImGui::MenuItem("Copy", "Ctrl+C", false, false);
            ImGui::MenuItem("Paste", "Ctrl+V", false, false);
            ImGui::EndMenu();
        }
    }

    void MainMenuWindow::DrawViewMenu()
    {
        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::BeginMenu("Panels"))
            {
                for (const auto& window : m_Context.GetGUIManager().GetWindows())
                {
                    if (window->GetId() == m_Id)
                    {
                        continue;
                    }

                    if (!window->IsVisibleForActiveModule())
                    {
                        continue;
                    }

                    const bool isOpen = window->IsOpen();
                    if (ImGui::MenuItem(window->GetTitle().c_str(), nullptr, isOpen, true))
                    {
                        window->SetOpen(!isOpen);
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Layout"))
            {
                if (ImGui::MenuItem("Reset To Default", nullptr, false, true))
                {
                    m_Context.RequestResetLayout() = true;
                    m_Context.DockLayoutInitialized() = false;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Theme"))
            {
                const std::string& activePreset = m_Context.GetEditorAppearance().GetAppearanceSettings().ThemePresetId;
                const bool darkSelected = activePreset == std::string(EditorThemePresetIds::DarkEngine);
                const bool lightSelected = activePreset == std::string(EditorThemePresetIds::LightEngine);

                if (ImGui::MenuItem("Dark", nullptr, darkSelected))
                {
                    m_Context.GetEditorAppearance().SetThemePreset(EditorThemePresetIds::DarkEngine, true);
                }

                if (ImGui::MenuItem("Light", nullptr, lightSelected))
                {
                    m_Context.GetEditorAppearance().SetThemePreset(EditorThemePresetIds::LightEngine, true);
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Typography"))
            {
                EditorAppearance& appearance = m_Context.GetEditorAppearance();
                const bool cjkEnabled = appearance.GetAppearanceSettings().Typography.bEnableCjkGlyphs;
                if (ImGui::MenuItem("Enable CJK Glyphs", nullptr, cjkEnabled))
                {
                    appearance.SetCjkGlyphsEnabled(!cjkEnabled, true);
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }
    }

    void MainMenuWindow::DrawWindowModeMenu()
    {
        if (ImGui::BeginMenu("Window"))
        {
            const EditorSubModule* active = m_Context.GetActiveSubModule();
            const bool sceneMode = active && active->GetModuleId() == SceneEditor::kModuleId;
            const bool materialMode = active && active->GetModuleId() == MaterialEditor::kModuleId;

            if (ImGui::MenuItem("Scene Editor", nullptr, sceneMode))
            {
                m_Context.ActivateSubModule(SceneEditor::kModuleId);
            }

            if (ImGui::MenuItem("Material Editor", nullptr, materialMode))
            {
                m_Context.ActivateSubModule(MaterialEditor::kModuleId);
            }

            ImGui::EndMenu();
        }
    }

    void MainMenuWindow::DrawToolsMenu()
    {
        if (ImGui::BeginMenu("Tools"))
        {
            DrawFileDialogTestMenu();
            ImGui::Separator();
            ImGui::MenuItem("Build Settings", nullptr, false, false);
            ImGui::MenuItem("Project Settings", nullptr, false, false);
            ImGui::MenuItem("Profiler", nullptr, false, false);
            ImGui::EndMenu();
        }
    }

    void MainMenuWindow::DrawFileDialogTestMenu()
    {
        if (!ImGui::BeginMenu("File Dialog (P3)"))
        {
            return;
        }

        IFileDialogService& fileDialogService = m_Context.GetFileDialogService();
        const PathRegistry& paths = PathRegistry::Get();

        auto makeAssetRequest = [&](const char* title, bool allowMultiple) -> FileDialogRequest
        {
            FileDialogRequest request;
            request.Title = title;
            request.Filters = AssetTypeRegistry::Get().BuildFileDialogFilters();
            request.bAllowMultiple = allowMultiple;
            if (!paths.GetProjectContentRoot().empty())
            {
                request.InitialDirectory = paths.GetProjectContentRoot();
            }

            return request;
        };

        if (ImGui::MenuItem("Open Asset Files..."))
        {
            const FileDialogResult dialogResult =
                fileDialogService.OpenFiles(makeAssetRequest("Open Asset Files", true));
            if (dialogResult.bCancelled)
            {
                ME_CORE_INFO("FileDialog P3: Open cancelled.");
            }
            else
            {
                for (const std::filesystem::path& selectedPath : dialogResult.Paths)
                {
                    ME_CORE_INFO("FileDialog P3: Open selected '{}'", selectedPath.string());
                }
            }
        }

        if (ImGui::MenuItem("Save Asset File..."))
        {
            FileDialogRequest request = makeAssetRequest("Save Asset File", false);
            const FileDialogResult dialogResult =
                fileDialogService.SaveFile(request, "Untitled.memtl");
            if (dialogResult.bCancelled)
            {
                ME_CORE_INFO("FileDialog P3: Save cancelled.");
            }
            else if (!dialogResult.Paths.empty())
            {
                ME_CORE_INFO("FileDialog P3: Save path '{}'", dialogResult.Paths.front().string());
            }
        }

        if (ImGui::MenuItem("Select Folder..."))
        {
            FileDialogRequest request;
            request.Title = "Select Folder";
            if (!paths.GetProjectContentRoot().empty())
            {
                request.InitialDirectory = paths.GetProjectContentRoot();
            }

            const FileDialogResult dialogResult = fileDialogService.SelectFolder(request);
            if (dialogResult.bCancelled)
            {
                ME_CORE_INFO("FileDialog P3: Folder cancelled.");
            }
            else if (!dialogResult.Paths.empty())
            {
                ME_CORE_INFO("FileDialog P3: Folder '{}'", dialogResult.Paths.front().string());
            }
        }

        ImGui::EndMenu();
    }

    void MainMenuWindow::DrawHelpMenu()
    {
        if (ImGui::BeginMenu("Help"))
        {
            ImGui::MenuItem("Documentation", nullptr, false, false);
            ImGui::MenuItem("Shortcuts", nullptr, false, false);
            ImGui::MenuItem("About", nullptr, false, false);
            ImGui::EndMenu();
        }
    }
}
