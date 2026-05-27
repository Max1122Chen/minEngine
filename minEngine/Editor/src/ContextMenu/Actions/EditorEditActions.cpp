#include "ContextMenu/Actions/EditorEditActions.h"

#include "ContextMenu/Contexts/ContentBrowserMenuContext.h"
#include "ContextMenu/Contexts/HierarchyMenuContext.h"
#include "ContextMenu/Contexts/SceneInspectorMenuContext.h"
#include "ContextMenu/EditorActionIds.h"
#include "ContextMenu/EditorMenuContext.h"
#include "ContextMenu/IEditorAction.h"
#include "Services/AssetWorkflowModule.h"
#include "Services/ContentBrowser/AssetTreeModel.h"
#include "Services/ContentBrowser/ContentBrowserModule.h"
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

        void RefreshContentBrowser(IEditorContext& editor)
        {
            AssetTreeModel& model = editor.GetContentBrowser().GetModel();
            model.RebuildDirectoryTree();
            model.RebuildCurrentDirectoryAssetList();
            editor.GetAssetWorkflow().SetSelectedAsset(nullptr);
        }

        const ContentBrowserMenuContext* FindContentBrowserContext(const EditorMenuContext& ctx)
        {
            return ctx.Find<ContentBrowserMenuContext>();
        }

        const HierarchyMenuContext* FindHierarchyContext(const EditorMenuContext& ctx)
        {
            return ctx.Find<HierarchyMenuContext>();
        }

        const SceneInspectorMenuContext* FindSceneInspectorContext(const EditorMenuContext& ctx)
        {
            return ctx.Find<SceneInspectorMenuContext>();
        }

        uint64_t GetPrimarySceneGameObjectId(const EditorMenuContext& ctx)
        {
            if (const HierarchyMenuContext* hierarchyCtx = FindHierarchyContext(ctx))
            {
                if (hierarchyCtx->HitKind == HierarchyHitKind::GameObjectItem
                    && !hierarchyCtx->SelectedGameObjectIds.empty())
                {
                    return hierarchyCtx->SelectedGameObjectIds.front();
                }
            }

            if (const SceneInspectorMenuContext* inspectorCtx = FindSceneInspectorContext(ctx))
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

        class DeleteEditorAction final : public IEditorAction
        {
        public:
            EditorActionId GetId() const override { return EditorActionId::Delete; }
            const char* GetLabel(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Delete";
            }
            EditorMenuSectionId GetSection() const override { return EditorMenuSectionId::Edit; }
            int GetSortOrder() const override { return 0; }

            bool IsVisibleInMenu(const EditorMenuContext& ctx) const override
            {
                if (const ContentBrowserMenuContext* cbCtx = FindContentBrowserContext(ctx))
                {
                    return !cbCtx->SelectedAssets.empty();
                }
                return HasSceneGameObjectTarget(ctx);
            }

            bool CanExecute(const EditorMenuContext& ctx) const override
            {
                return IsVisibleInMenu(ctx);
            }

            const char* GetDisabledReason(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Nothing to delete.";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                if (FindContentBrowserContext(ctx) != nullptr)
                {
                    editor.GetAssetWorkflow().DeleteSelectedAsset();
                    RefreshContentBrowser(editor);
                    return;
                }

                const uint64_t gameObjectId = GetPrimarySceneGameObjectId(ctx);
                if (gameObjectId == kInvalidGameObjectId)
                {
                    return;
                }

                SceneEditor* sceneEditor = GetSceneEditor(&editor);
                if (!sceneEditor)
                {
                    return;
                }

                sceneEditor->SubmitRemoveGameObjectFromScene(editor, gameObjectId);
            }
        };

        class RenameEditorAction final : public IEditorAction
        {
        public:
            EditorActionId GetId() const override { return EditorActionId::Rename; }
            const char* GetLabel(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Rename";
            }
            EditorMenuSectionId GetSection() const override { return EditorMenuSectionId::Edit; }
            int GetSortOrder() const override { return 10; }

            bool IsVisibleInMenu(const EditorMenuContext& ctx) const override
            {
                return HasSceneGameObjectTarget(ctx);
            }

            bool CanExecute(const EditorMenuContext& ctx) const override
            {
                return IsVisibleInMenu(ctx);
            }

            const char* GetDisabledReason(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "No GameObject selected.";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                const uint64_t gameObjectId = GetPrimarySceneGameObjectId(ctx);
                if (gameObjectId == kInvalidGameObjectId)
                {
                    return;
                }

                SceneEditor* sceneEditor = GetSceneEditor(&editor);
                if (!sceneEditor)
                {
                    return;
                }

                sceneEditor->SelectGameObject(gameObjectId);

                if (FindSceneInspectorContext(ctx) != nullptr)
                {
                    sceneEditor->BeginRenameGameObjectInInspector(gameObjectId);
                    return;
                }

                sceneEditor->RequestBeginRenameGameObject(gameObjectId);
            }
        };
    }

    void RegisterEditorEditActions(EditorActionRegistry& registry)
    {
        registry.Register(std::make_unique<DeleteEditorAction>());
        registry.Register(std::make_unique<RenameEditorAction>());
    }

} // namespace minEngine
