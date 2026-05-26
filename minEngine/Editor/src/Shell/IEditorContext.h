#pragma once

#include "Core.h"

#include <string_view>

namespace minEngine
{
    class Engine;
    class EditorGUIManager;
    class EditorSubModule;
    class EditorServiceModule;
    class ViewportClientRegistry;
    class AssetWorkflowModule;
    class ContentBrowserModule;
    class ConsoleModule;
    class EditorCommandStack;
    class EditorInputHub;
    class EditorAppearance;
    class IFileDialogService;

    class IEditorContext
    {
    public:
        virtual ~IEditorContext() = default;

        virtual Engine& GetEngine() = 0;
        virtual const Engine& GetEngine() const = 0;

        virtual EditorGUIManager& GetGUIManager() = 0;
        virtual const EditorGUIManager& GetGUIManager() const = 0;

        virtual ViewportClientRegistry& GetViewportRegistry() = 0;
        virtual const ViewportClientRegistry& GetViewportRegistry() const = 0;

        virtual EditorSubModule* GetActiveSubModule() = 0;
        virtual const EditorSubModule* GetActiveSubModule() const = 0;
        virtual EditorSubModule* FindSubModule(std::string_view moduleId) = 0;
        virtual const EditorSubModule* FindSubModule(std::string_view moduleId) const = 0;

        virtual AssetWorkflowModule& GetAssetWorkflow() = 0;
        virtual const AssetWorkflowModule& GetAssetWorkflow() const = 0;
        virtual ContentBrowserModule& GetContentBrowser() = 0;
        virtual const ContentBrowserModule& GetContentBrowser() const = 0;
        virtual ConsoleModule& GetConsole() = 0;
        virtual EditorCommandStack& GetCommandStack() = 0;
        virtual EditorInputHub& GetInputHub() = 0;
        virtual EditorAppearance& GetEditorAppearance() = 0;
        virtual const EditorAppearance& GetEditorAppearance() const = 0;

        virtual IFileDialogService& GetFileDialogService() = 0;
        virtual const IFileDialogService& GetFileDialogService() const = 0;

        virtual void SetLastDeltaTime(float deltaTime) = 0;
        virtual float GetLastDeltaTime() const = 0;
        virtual bool IsPlaying() const = 0;

        virtual bool ActivateSubModule(std::string_view moduleId) = 0;

        virtual void RequestExit() = 0;

        virtual bool& DockLayoutInitialized() = 0;
        virtual bool& RequestResetLayout() = 0;
    };
}
