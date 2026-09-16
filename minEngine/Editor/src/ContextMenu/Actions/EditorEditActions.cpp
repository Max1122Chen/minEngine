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
                if (inspectorCtx->SelectionKind == SceneInspectorSelectionKind::GameObjectHeader)
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
                if (const ContentBrowserMenuContext* cbCtx = FindContentBrowserContext(ctx))
                {
                    return (cbCtx->HitKind == ContentBrowserHitKind::TreeAsset
                            || cbCtx->HitKind == ContentBrowserHitKind::TileAsset)
                        && !cbCtx->SelectedAssets.empty()
                        && cbCtx->SelectedAssets.front() != nullptr;
                }

                if (const SceneInspectorMenuContext* inspectorCtx = FindSceneInspectorContext(ctx))
                {
                    if (inspectorCtx->SelectionKind == SceneInspectorSelectionKind::Component)
                    {
                        return inspectorCtx->HoveredComponent != nullptr;
                    }
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
                return "Nothing to rename.";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                if (const ContentBrowserMenuContext* cbCtx = FindContentBrowserContext(ctx))
                {
                    if (!cbCtx->SelectedAssets.empty() && cbCtx->SelectedAssets.front() != nullptr)
                    {
                        const AssetMeta& meta = *cbCtx->SelectedAssets.front();
                        editor.GetAssetWorkflow().SetSelectedAsset(&meta);
                        editor.GetContentBrowser().RequestBeginAssetRename(meta.AssetPath);
                    }
                    return;
                }

                SceneEditor* sceneEditor = GetSceneEditor(&editor);
                if (!sceneEditor)
                {
                    return;
                }

                if (const SceneInspectorMenuContext* inspectorCtx = FindSceneInspectorContext(ctx))
                {
                    if (inspectorCtx->SelectionKind == SceneInspectorSelectionKind::Component
                        && inspectorCtx->HoveredComponent != nullptr)
                    {
                        sceneEditor->BeginRenameComponentInInspector(*inspectorCtx->HoveredComponent);
                        return;
                    }
                }

                const uint64_t gameObjectId = GetPrimarySceneGameObjectId(ctx);
                if (gameObjectId == kInvalidGameObjectId)
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

        class UnpackAndDeletePrefabEditorAction final : public IEditorAction
        {
        public:
            EditorActionId GetId() const override { return EditorActionId::UnpackAndDeletePrefab; }

            const char* GetLabel(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Unpack and Delete Prefab";
            }

            EditorMenuSectionId GetSection() const override { return EditorMenuSectionId::Asset; }
            int GetSortOrder() const override { return 20; }

            bool IsVisibleInMenu(const EditorMenuContext& ctx) const override
            {
                const ContentBrowserMenuContext* cbCtx = FindContentBrowserContext(ctx);
                if (cbCtx == nullptr || cbCtx->SelectedAssets.empty())
                {
                    return false;
                }

                for (const AssetMeta* meta : cbCtx->SelectedAssets)
                {
                    if (meta != nullptr && meta->AssetType == "Prefab")
                    {
                        return true;
                    }
                }
                return false;
            }

            bool CanExecute(const EditorMenuContext& ctx) const override
            {
                return IsVisibleInMenu(ctx);
            }

            const char* GetDisabledReason(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Select a Prefab asset.";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                editor.GetAssetWorkflow().DeleteSelectedAsset(true);
                RefreshContentBrowser(editor);
            }
        };
    }

    void RegisterEditorEditActions(EditorActionRegistry& registry)
    {
        registry.Register(std::make_unique<DeleteEditorAction>());
        registry.Register(std::make_unique<RenameEditorAction>());
        registry.Register(std::make_unique<UnpackAndDeletePrefabEditorAction>());
    }

} // namespace minEngine
