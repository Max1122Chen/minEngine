#pragma once

#include "Core.h"
#include "minEngine.h"

#include "EditorGUIManager.h"
#include "Platform/EditorImGuiBackend.h"
#include "SubEditor/Material/MaterialEditor.h"
#include "SubEditor/Scene/SceneEditor.h"
#include "Services/AssetWatch/ProjectAssetWatcher.h"
#include "Services/AssetWorkflowModule.h"
#include "Services/ContentBrowser/ContentBrowserModule.h"
#include "Services/ConsoleModule.h"
#include "Services/Inspector/InspectorModule.h"
#include "Services/MainMenuModule.h"
#include "Services/ToolbarModule.h"
#include "Shell/EditorInputHub.h"
#include "ContextMenu/EditorContextMenuSystem.h"
#include "Shell/EditorCommandStack.h"
#include "Shell/EditorSubModule.h"
#include "PlayMode/PlayInEditorSession.h"
#include "PlayMode/IPlayModeService.h"
#include "Shell/ViewportClientRegistry.h"
#include "UI/Appearance/EditorAppearance.h"

#include "Runtime/Core/CLI/CommandLineResult.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace minEngine
{
    class Editor : public Application, public IEditorContext
    {
    public:
        Editor();
        ~Editor() override;

        void Initialize(int argc, char** argv) override;
        void Initialize(int argc, char** argv, const CommandLineResult& commandLine) override;
        void Shutdown() override;
        void Run() override;

        EditorGUIManager& GetGUIManager() override { return m_EditorGUIManager; }
        const EditorGUIManager& GetGUIManager() const override { return m_EditorGUIManager; }

        Engine& GetEngine() override { return *m_Engine; }
        const Engine& GetEngine() const override { return *m_Engine; }

        ViewportClientRegistry& GetViewportRegistry() override { return m_ViewportRegistry; }
        const ViewportClientRegistry& GetViewportRegistry() const override { return m_ViewportRegistry; }

        EditorSubModule* GetActiveSubModule() override { return m_ActiveSubModule; }
        const EditorSubModule* GetActiveSubModule() const override { return m_ActiveSubModule; }
        EditorSubModule* FindSubModule(std::string_view moduleId) override;
        const EditorSubModule* FindSubModule(std::string_view moduleId) const override;

        AssetWorkflowModule& GetAssetWorkflow() override { return m_AssetWorkflow; }
        const AssetWorkflowModule& GetAssetWorkflow() const override { return m_AssetWorkflow; }
        ContentBrowserModule& GetContentBrowser() override { return m_ContentBrowser; }
        const ContentBrowserModule& GetContentBrowser() const override { return m_ContentBrowser; }
        ProjectAssetWatcher& GetProjectAssetWatcher() { return m_ProjectAssetWatcher; }
        const ProjectAssetWatcher& GetProjectAssetWatcher() const { return m_ProjectAssetWatcher; }
        ConsoleModule& GetConsole() override { return m_ConsoleModule; }
        InspectorModule& GetInspectorModule() override { return m_InspectorModule; }
        const InspectorModule& GetInspectorModule() const override { return m_InspectorModule; }
        EditorCommandStack& GetCommandStack() override { return m_CommandStack; }
        EditorContextMenuSystem& GetContextMenu() override { return m_ContextMenu; }
        const EditorContextMenuSystem& GetContextMenu() const override { return m_ContextMenu; }
        EditorInputHub& GetInputHub() override { return m_InputHub; }
        EditorAppearance& GetEditorAppearance() override { return m_Appearance; }
        const EditorAppearance& GetEditorAppearance() const override { return m_Appearance; }

        void SetLastDeltaTime(float deltaTime) override { m_LastDeltaTime = deltaTime; }
        float GetLastDeltaTime() const override { return m_LastDeltaTime; }
        bool IsPlaying() const override { return m_PlayInEditorSession.IsPlaying(); }
        IPlayModeService& GetPlayModeService() override { return m_PlayInEditorSession; }
        const IPlayModeService& GetPlayModeService() const override { return m_PlayInEditorSession; }

        bool ActivateSubModule(std::string_view moduleId) override;

        void RequestExit() override { m_ExitRequested = true; }

        bool& DockLayoutInitialized() override { return m_DockLayoutInitialized; }
        bool& RequestResetLayout() override { return m_RequestResetLayout; }

        IFileDialogService& GetFileDialogService() override;
        const IFileDialogService& GetFileDialogService() const override;

        bool OpenProject(const std::string& projectPath);
        void CloseProject();

    private:
        void RegisterModules();
        void UpdateWindowTitle();
        void ApplyCommandStackSettingsFromProject();
        void ApplyAppearanceSettingsFromProject();
        void ResetCommandStackForNewDocument();
        bool InitializeImGuiBackend();

        Engine* m_Engine = nullptr;
        EditorGUIManager m_EditorGUIManager;
        SceneEditor m_SceneEditor;
        std::unique_ptr<MaterialEditor> m_MaterialEditor;
        MainMenuModule m_MainMenuModule;
        ToolbarModule m_ToolbarModule;
        InspectorModule m_InspectorModule;
        ConsoleModule m_ConsoleModule;
        AssetWorkflowModule m_AssetWorkflow;
        ContentBrowserModule m_ContentBrowser;
        ProjectAssetWatcher m_ProjectAssetWatcher;
        EditorInputHub m_InputHub;
        ViewportClientRegistry m_ViewportRegistry;
        EditorCommandStack m_CommandStack;
        EditorContextMenuSystem m_ContextMenu;
        EditorAppearance m_Appearance;
        EditorImGuiBackend m_ImGuiBackend;
        std::vector<EditorSubModule*> m_SubModules;
        EditorSubModule* m_ActiveSubModule = nullptr;
        bool m_ExitRequested = false;
        PlayInEditorSession m_PlayInEditorSession;
        float m_LastDeltaTime = 0.0f;
        bool m_DockLayoutInitialized = false;
        bool m_RequestResetLayout = false;

        static std::optional<std::filesystem::path> ResolveProjectDescriptorPath(
            const CommandLineResult& commandLine);
    };

    Application* CreateApplication();
}
