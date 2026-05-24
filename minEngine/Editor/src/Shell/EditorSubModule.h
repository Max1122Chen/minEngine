#pragma once

#include "Core.h"
#include "Shell/IEditorInspectorSource.h"

#include "imgui.h"

#include <string_view>

namespace minEngine
{
    class IEditorContext;
    class AssetMeta;
    class EditorViewportClient;

    class EditorSubModule
    {
    public:
        virtual ~EditorSubModule() = default;

        virtual std::string_view GetModuleId() const = 0;
        virtual std::string_view GetDisplayName() const = 0;

        virtual void Register(IEditorContext& context) = 0;
        virtual void Shutdown() = 0;

        virtual bool CanActivate() const { return true; }
        virtual void OnActivate(IEditorContext& context) = 0;
        virtual void OnDeactivate(IEditorContext& context) = 0;

        virtual void RegisterCommands(IEditorContext& context) {}
        virtual void UnregisterCommands(IEditorContext& context) {}

        virtual void Tick(float deltaTime) = 0;

        virtual void ApplyDefaultLayout(IEditorContext& context, ImGuiID dockspaceId) = 0;

        virtual IEditorInspectorSource* GetInspectorSource() = 0;
        virtual const IEditorInspectorSource* GetInspectorSource() const = 0;

        virtual bool CanOpenAsset(const AssetMeta& meta) const { return false; }
        virtual bool OpenAsset(const AssetMeta& meta) = 0;

        virtual bool RouteViewportInput(EditorViewportClient& client) { return false; }
    };
}
