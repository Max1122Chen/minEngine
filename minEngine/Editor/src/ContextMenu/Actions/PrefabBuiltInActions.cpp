#include "ContextMenu/Actions/PrefabBuiltInActions.h"

#include "ContextMenu/Contexts/HierarchyMenuContext.h"
#include "ContextMenu/EditorActionIds.h"
#include "ContextMenu/EditorActionRegistry.h"
#include "ContextMenu/EditorMenuContext.h"
#include "ContextMenu/IEditorAction.h"
#include "Shell/EditorContextHelpers.h"
#include "Shell/IEditorContext.h"
#include "SubEditor/Prefab/PrefabEditConstraints.h"
#include "SubEditor/Scene/SceneEditor.h"

#include <limits>
#include <memory>

namespace minEngine
{
    namespace
    {
        bool AllowsPrefabLevelWorkflow(const EditorMenuContext& ctx)
        {
            const HierarchyMenuContext* hierarchyCtx = ctx.Find<HierarchyMenuContext>();
            return hierarchyCtx != nullptr && hierarchyCtx->bAllowPrefabLevelWorkflow;
        }

        class CreatePrefabAction final : public IEditorAction
        {
        public:
            EditorActionId GetId() const override { return EditorActionId::CreatePrefab; }
            const char* GetLabel(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Create Prefab...";
            }
            EditorMenuSectionId GetSection() const override { return EditorMenuSectionId::Asset; }
            int GetSortOrder() const override { return 10; }

            bool IsVisibleInMenu(const EditorMenuContext& ctx) const override
            {
                const HierarchyMenuContext* hierarchyCtx = ctx.Find<HierarchyMenuContext>();
                return AllowsPrefabLevelWorkflow(ctx)
                    && hierarchyCtx->HitKind == HierarchyHitKind::GameObjectItem
                    && !hierarchyCtx->SelectedGameObjectIds.empty();
            }

            bool CanExecute(const EditorMenuContext& ctx) const override { return IsVisibleInMenu(ctx); }

            const char* GetDisabledReason(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Create Prefab is only available in a Level Scene.";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                const HierarchyMenuContext* hierarchyCtx = ctx.Find<HierarchyMenuContext>();
                SceneEditor* sceneEditor = GetSceneEditor(&editor);
                if (!hierarchyCtx || !sceneEditor || hierarchyCtx->SelectedGameObjectIds.empty())
                {
                    return;
                }

                if (!PrefabEditConstraints::AllowCreatePrefab(*sceneEditor, nullptr) || editor.IsPlaying())
                {
                    return;
                }

                sceneEditor->CreatePrefabFromSelectedGameObject(
                    editor,
                    hierarchyCtx->SelectedGameObjectIds.front());
            }
        };

        class InstantiatePrefabAction final : public IEditorAction
        {
        public:
            EditorActionId GetId() const override { return EditorActionId::InstantiatePrefab; }
            const char* GetLabel(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Instantiate Prefab...";
            }
            EditorMenuSectionId GetSection() const override { return EditorMenuSectionId::Create; }
            int GetSortOrder() const override { return 20; }

            bool IsVisibleInMenu(const EditorMenuContext& ctx) const override
            {
                return AllowsPrefabLevelWorkflow(ctx);
            }

            bool CanExecute(const EditorMenuContext& ctx) const override { return IsVisibleInMenu(ctx); }

            const char* GetDisabledReason(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Instantiate Prefab is only available in a Level Scene.";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                SceneEditor* sceneEditor = GetSceneEditor(&editor);
                if (!sceneEditor)
                {
                    return;
                }

                if (!PrefabEditConstraints::AllowInstantiatePrefab(*sceneEditor, nullptr) || editor.IsPlaying())
                {
                    return;
                }

                sceneEditor->SubmitInstantiatePrefab(editor, std::numeric_limits<uint64_t>::max());
            }
        };

        class InstantiatePrefabAsChildAction final : public IEditorAction
        {
        public:
            EditorActionId GetId() const override { return EditorActionId::InstantiatePrefabAsChild; }
            const char* GetLabel(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Instantiate Prefab as Child...";
            }
            EditorMenuSectionId GetSection() const override { return EditorMenuSectionId::Create; }
            int GetSortOrder() const override { return 21; }

            bool IsVisibleInMenu(const EditorMenuContext& ctx) const override
            {
                const HierarchyMenuContext* hierarchyCtx = ctx.Find<HierarchyMenuContext>();
                return AllowsPrefabLevelWorkflow(ctx)
                    && hierarchyCtx->HitKind == HierarchyHitKind::GameObjectItem
                    && !hierarchyCtx->SelectedGameObjectIds.empty();
            }

            bool CanExecute(const EditorMenuContext& ctx) const override { return IsVisibleInMenu(ctx); }

            const char* GetDisabledReason(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Instantiate Prefab is only available in a Level Scene.";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                const HierarchyMenuContext* hierarchyCtx = ctx.Find<HierarchyMenuContext>();
                SceneEditor* sceneEditor = GetSceneEditor(&editor);
                if (!hierarchyCtx || !sceneEditor || hierarchyCtx->SelectedGameObjectIds.empty())
                {
                    return;
                }

                if (!PrefabEditConstraints::AllowInstantiatePrefab(*sceneEditor, nullptr) || editor.IsPlaying())
                {
                    return;
                }

                sceneEditor->SubmitInstantiatePrefab(editor, hierarchyCtx->SelectedGameObjectIds.front());
            }
        };
    }

    void RegisterPrefabBuiltInActions(EditorActionRegistry& registry)
    {
        registry.Register(std::make_unique<CreatePrefabAction>());
        registry.Register(std::make_unique<InstantiatePrefabAction>());
        registry.Register(std::make_unique<InstantiatePrefabAsChildAction>());
    }
}
