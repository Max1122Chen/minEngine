#pragma once

#include "Core.h"
#include "minEngine.h"

#include "EditorGUIManager.h"
#include "Material/MaterialEditor.h"
#include "Scene/SceneEditor.h"
#include "Services/AssetWorkflowModule.h"
#include "Services/ConsoleModule.h"
#include "Services/InspectorModule.h"
#include "Services/MainMenuModule.h"
#include "Shell/EditorInputHub.h"
#include "Shell/EditorCommandHistory.h"
#include "Shell/EditorSubModule.h"
#include "Shell/IEditorContext.h"
#include "Shell/ViewportClientRegistry.h"

#include <memory>
#include <vector>

namespace minEngine
{
    class Editor : public Application, public IEditorContext
    {
    public:
        Editor();
        ~Editor() override;

        void Initialize(int argc, char** argv) override;
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
        ConsoleModule& GetConsole() override { return m_ConsoleModule; }
        EditorCommandHistory& GetCommandHistory() override { return m_CommandHistory; }
        EditorInputHub& GetInputHub() override { return m_InputHub; }

        void SetLastDeltaTime(float deltaTime) override { m_LastDeltaTime = deltaTime; }
        float GetLastDeltaTime() const override { return m_LastDeltaTime; }
        bool IsPlaying() const override { return m_IsPlaying; }

        bool ActivateSubModule(std::string_view moduleId) override;

        void RequestExit() override { m_ExitRequested = true; }

        bool& DockLayoutInitialized() override { return m_DockLayoutInitialized; }
        bool& RequestResetLayout() override { return m_RequestResetLayout; }

        bool OpenProject(const std::string& projectPath);
        void CloseProject();

    private:
        void PostInitialize();
        void RegisterModules();
        void UpdateWindowTitle();

        Engine* m_Engine = nullptr;
        EditorGUIManager m_EditorGUIManager;
        SceneEditor m_SceneEditor;
        std::unique_ptr<MaterialEditor> m_MaterialEditor;
        MainMenuModule m_MainMenuModule;
        InspectorModule m_InspectorModule;
        ConsoleModule m_ConsoleModule;
        AssetWorkflowModule m_AssetWorkflow;
        EditorInputHub m_InputHub;
        ViewportClientRegistry m_ViewportRegistry;
        EditorCommandHistory m_CommandHistory;
        std::vector<EditorSubModule*> m_SubModules;
        EditorSubModule* m_ActiveSubModule = nullptr;
        bool m_ExitRequested = false;
        bool m_IsPlaying = false;
        float m_LastDeltaTime = 0.0f;
        bool m_DockLayoutInitialized = false;
        bool m_RequestResetLayout = false;
    };

    Application* CreateApplication();
}
