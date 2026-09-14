#include "MainMenuWindow.h"

#include "EditorGUIManager.h"
#include "Shell/EditorCommandStack.h"
#include "Shell/EditorUndoRedoActions.h"
#include "SubEditor/AnimationGraph/AnimationGraphEditor.h"
#include "SubEditor/Material/MaterialEditor.h"
#include "SubEditor/Scene/SceneEditor.h"
#include "Shell/EditorContextHelpers.h"
#include "Services/AssetWorkflowModule.h"
#include "Services/ContentBrowser/ContentBrowserModule.h"
#include "Shell/Document/EditorDocumentHost.h"
#include "Shell/Document/EditorDocumentSession.h"
#include "UI/Appearance/EditorAppearance.h"
#include "UI/Appearance/EditorThemePresets.h"
#include "UI/Appearance/EditorTypographyScope.h"
#include "Runtime/Core/EngineVersion.h"
#include "Runtime/Core/ProductBranding.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"


namespace minEngine
{
    void MainMenuWindow::OnDraw()
    {
    }

    void MainMenuWindow::DrawChrome()
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
            DrawWindowMenu();
            DrawToolsMenu();
            DrawHelpMenu();
        }
        ImGui::EndMainMenuBar();
        ImGui::PopStyleVar();

        DrawAboutPopup();
    }

    void MainMenuWindow::DrawDocumentTabBar()
    {
        m_Context.GetDocumentHost().DrawTabBar();
        m_Context.GetDocumentHost().DrawPendingDialogs();
    }

    void MainMenuWindow::DrawFileMenu()
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Scene", "Ctrl+N"))
            {
                m_Context.GetAssetWorkflow().TryNewScene();
            }

            if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
            {
                m_Context.GetAssetWorkflow().OpenSceneDialog();
            }

            SceneEditor* sceneEditor = GetSceneEditor(&m_Context);
            const bool hasScene = sceneEditor && sceneEditor->GetActiveScene();
            if (ImGui::MenuItem("Save", "Ctrl+S", false, hasScene))
            {
                if (sceneEditor)
                {
                    sceneEditor->SaveCurrentScene(m_Context);
                }
            }

            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, hasScene))
            {
                if (sceneEditor)
                {
                    sceneEditor->SaveCurrentSceneAs(m_Context);
                }
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
                TryUndo(m_Context);
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo) && canRedo)
            {
                TryRedo(m_Context);
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

    void MainMenuWindow::DrawWindowMenu()
    {
        if (ImGui::BeginMenu("Window"))
        {
            EditorDocumentHost& host = m_Context.GetDocumentHost();
            const EditorDocumentSession* activeSession = host.GetActiveSession();
            const std::string activeType = activeSession != nullptr ? activeSession->GetTypeId() : std::string{};

            auto focusDocumentType = [this, &host](const char* typeId)
            {
                if (EditorDocumentSession* session = host.FindFirstSessionOfType(typeId))
                {
                    host.Activate(session->GetId());
                    return;
                }

                ME_LOG(
                    LogEditor,
                    Info,
                    "No open '{}' document. Open one from the Content Browser or File menu.",
                    typeId);
            };

            if (ImGui::BeginMenu("Focus Document"))
            {
                if (ImGui::MenuItem("Scene", nullptr, activeType == "Scene"))
                {
                    focusDocumentType("Scene");
                }
                if (ImGui::MenuItem("Material", nullptr, activeType == "Material"))
                {
                    focusDocumentType("Material");
                }
                if (ImGui::MenuItem("Animation Graph", nullptr, activeType == "AnimationGraph"))
                {
                    focusDocumentType("AnimationGraph");
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }
    }

    void MainMenuWindow::DrawToolsMenu()
    {
        if (ImGui::BeginMenu("Tools"))
        {
            ImGui::MenuItem("Build Settings", nullptr, false, false);
            ImGui::MenuItem("Project Settings", nullptr, false, false);
            ImGui::MenuItem("Profiler", nullptr, false, false);
            ImGui::EndMenu();
        }
    }

    void MainMenuWindow::DrawHelpMenu()
    {
        if (ImGui::BeginMenu("Help"))
        {
            ImGui::MenuItem("Documentation", nullptr, false, false);
            ImGui::MenuItem("Shortcuts", nullptr, false, false);
            if (ImGui::MenuItem("About"))
            {
                m_OpenAboutPopup = true;
            }
            ImGui::EndMenu();
        }
    }

    void MainMenuWindow::DrawAboutPopup()
    {
        if (m_OpenAboutPopup)
        {
            ImGui::OpenPopup("About Maximum");
            m_OpenAboutPopup = false;
        }

        if (!ImGui::BeginPopupModal(
                "About Maximum",
                nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
        {
            return;
        }

        const std::string versionLine = FormatProductVersionLine(GetEngineVersion().ToString());
        ImGui::TextUnformatted(versionLine.c_str());
        ImGui::Spacing();
        ImGui::TextUnformatted("Engine identity: minEngine");
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(120.0f, 0.0f)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
