#include "ContextMenu/Actions/SceneBuiltInActions.h"

#include "ContextMenu/Contexts/HierarchyMenuContext.h"
#include "ContextMenu/Contexts/SceneInspectorMenuContext.h"
#include "ContextMenu/EditorActionIds.h"
#include "ContextMenu/EditorMenuContext.h"
#include "ContextMenu/IEditorAction.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Shell/EditorContextHelpers.h"
#include "Shell/IEditorContext.h"
#include "SubEditor/Scene/SceneEditor.h"

#include <limits>
#include <memory>

namespace minEngine
{
    namespace
    {
        constexpr uint64_t kInvalidGameObjectId = std::numeric_limits<uint64_t>::max();

        uint64_t GetPrimarySceneGameObjectId(const EditorMenuContext& ctx)
        {
            if (const HierarchyMenuContext* hierarchyCtx = ctx.Find<HierarchyMenuContext>())
            {
                if (hierarchyCtx->HitKind == HierarchyHitKind::GameObjectItem
                    && !hierarchyCtx->SelectedGameObjectIds.empty())
                {
                    return hierarchyCtx->SelectedGameObjectIds.front();
                }
            }

            if (const SceneInspectorMenuContext* inspectorCtx = ctx.Find<SceneInspectorMenuContext>())
            {
                if (inspectorCtx->SelectionKind == SceneInspectorSelectionKind::GameObjectHeader
                    && inspectorCtx->GameObjectId != 0)
                {
                    return inspectorCtx->GameObjectId;
                }
            }

            return kInvalidGameObjectId;
        }

        bool HasSceneGameObjectTarget(const EditorMenuContext& ctx)
        {
            return GetPrimarySceneGameObjectId(ctx) != kInvalidGameObjectId;
        }

        class SceneDeferredAction : public IEditorAction
        {
        protected:
            static bool HasTarget(const EditorMenuContext& ctx) { return HasSceneGameObjectTarget(ctx); }
        };

        class DuplicateEditorAction final : public SceneDeferredAction
        {
        public:
            EditorActionId GetId() const override { return EditorActionId::Duplicate; }
            const char* GetLabel(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Duplicate";
            }
            EditorMenuSectionId GetSection() const override { return EditorMenuSectionId::Edit; }
            int GetSortOrder() const override { return 20; }

            bool IsVisibleInMenu(const EditorMenuContext& ctx) const override { return HasTarget(ctx); }

            bool CanExecute(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return false;
            }

            const char* GetDisabledReason(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Duplicate is not implemented yet.";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                (void)editor;
                (void)ctx;
            }
        };

        class CreateEmptyGameObjectAction final : public IEditorAction
        {
        public:
            EditorActionId GetId() const override { return EditorActionId::CreateEmptyGameObject; }
            const char* GetLabel(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Create Empty";
            }
            EditorMenuSectionId GetSection() const override { return EditorMenuSectionId::Create; }
            int GetSortOrder() const override { return 0; }

            bool IsVisibleInMenu(const EditorMenuContext& ctx) const override
            {
                const HierarchyMenuContext* hierarchyCtx = ctx.Find<HierarchyMenuContext>();
                return hierarchyCtx != nullptr && hierarchyCtx->HitKind == HierarchyHitKind::Blank;
            }

            bool CanExecute(const EditorMenuContext& ctx) const override { return IsVisibleInMenu(ctx); }

            const char* GetDisabledReason(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                if (SceneEditor* sceneEditor = GetSceneEditor(&editor))
                {
                    sceneEditor->SubmitAddEmptyGOToScene(editor);
                }
            }
        };

        class RemoveComponentAction final : public IEditorAction
        {
        public:
            EditorActionId GetId() const override { return EditorActionId::RemoveComponent; }
            const char* GetLabel(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Remove";
            }
            EditorMenuSectionId GetSection() const override { return EditorMenuSectionId::Edit; }
            int GetSortOrder() const override { return 30; }

            bool IsVisibleInMenu(const EditorMenuContext& ctx) const override
            {
                const SceneInspectorMenuContext* inspectorCtx = ctx.Find<SceneInspectorMenuContext>();
                return inspectorCtx != nullptr
                    && inspectorCtx->SelectionKind == SceneInspectorSelectionKind::Component
                    && inspectorCtx->HoveredComponent != nullptr;
            }

            bool CanExecute(const EditorMenuContext& ctx) const override { return IsVisibleInMenu(ctx); }

            const char* GetDisabledReason(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "No component selected.";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                const SceneInspectorMenuContext* inspectorCtx = ctx.Find<SceneInspectorMenuContext>();
                if (!inspectorCtx || !inspectorCtx->HoveredComponent)
                {
                    return;
                }

                Component& component = *inspectorCtx->HoveredComponent;
                GameObject* owner = component.GetOwner();
                if (!owner)
                {
                    return;
                }

                SceneEditor* sceneEditor = GetSceneEditor(&editor);
                if (!sceneEditor)
                {
                    return;
                }

                sceneEditor->SubmitRemoveComponentFromGO(editor, *owner, component);
            }
        };

        class FocusInViewportEditorAction final : public SceneDeferredAction
        {
        public:
            EditorActionId GetId() const override { return EditorActionId::FocusInViewport; }
            const char* GetLabel(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Frame in Viewport";
            }
            EditorMenuSectionId GetSection() const override { return EditorMenuSectionId::View; }
            int GetSortOrder() const override { return 0; }

            bool IsVisibleInMenu(const EditorMenuContext& ctx) const override { return HasTarget(ctx); }

            bool CanExecute(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return false;
            }

            const char* GetDisabledReason(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Viewport focus is not implemented yet.";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                (void)editor;
                (void)ctx;
            }
        };
    }

    void RegisterSceneBuiltInActions(EditorActionRegistry& registry)
    {
        registry.Register(std::make_unique<CreateEmptyGameObjectAction>());
        registry.Register(std::make_unique<DuplicateEditorAction>());
        registry.Register(std::make_unique<RemoveComponentAction>());
        registry.Register(std::make_unique<FocusInViewportEditorAction>());
    }

} // namespace minEngine
