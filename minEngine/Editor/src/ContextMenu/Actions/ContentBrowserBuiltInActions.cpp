#include "ContextMenu/Actions/ContentBrowserBuiltInActions.h"

#include "ContextMenu/Contexts/ContentBrowserMenuContext.h"
#include "ContextMenu/EditorActionIds.h"
#include "ContextMenu/EditorActionRegistry.h"
#include "ContextMenu/EditorMenuContext.h"
#include "ContextMenu/IEditorAction.h"
#include "Services/AssetWorkflowModule.h"
#include "Services/ContentBrowser/AssetTreeModel.h"
#include "Services/ContentBrowser/ContentBrowserModule.h"
#include "Shell/IEditorContext.h"

#include <memory>

namespace minEngine
{
    namespace
    {
        const ContentBrowserMenuContext* FindContentBrowserContext(const EditorMenuContext& ctx)
        {
            return ctx.Find<ContentBrowserMenuContext>();
        }

        void RefreshContentBrowser(IEditorContext& editor)
        {
            AssetTreeModel& model = editor.GetContentBrowser().GetModel();
            model.RebuildDirectoryTree();
            model.RebuildCurrentDirectoryAssetList();
            editor.GetAssetWorkflow().SetSelectedAsset(nullptr);
        }

        class ContentBrowserScopedAction : public IEditorAction
        {
        protected:
            static const ContentBrowserMenuContext* GetContext(const EditorMenuContext& ctx)
            {
                return FindContentBrowserContext(ctx);
            }
        };
        class ContentBrowserImportAction final : public ContentBrowserScopedAction
        {
        public:
            EditorActionId GetId() const override { return EditorActionId::ImportAsset; }
            const char* GetLabel(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Import...";
            }
            EditorMenuSectionId GetSection() const override { return EditorMenuSectionId::Asset; }
            int GetSortOrder() const override { return 10; }

            bool IsVisibleInMenu(const EditorMenuContext& ctx) const override
            {
                return GetContext(ctx) != nullptr;
            }

            bool CanExecute(const EditorMenuContext& ctx) const override
            {
                const ContentBrowserMenuContext* cbCtx = GetContext(ctx);
                if (cbCtx == nullptr)
                {
                    return false;
                }

                return cbCtx->HitKind == ContentBrowserHitKind::TreeDirectory ||
                       cbCtx->HitKind == ContentBrowserHitKind::ListBackground;
            }

            const char* GetDisabledReason(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Import is available on folders or empty list areas.";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                const ContentBrowserMenuContext* cbCtx = GetContext(ctx);
                const std::string_view destDir = cbCtx != nullptr ? cbCtx->CurrentDirectoryRel : std::string_view{};
                editor.GetAssetWorkflow().ImportAssetDialog(destDir);
            }
        };

        class ContentBrowserRefreshAction final : public ContentBrowserScopedAction
        {
        public:
            EditorActionId GetId() const override { return EditorActionId::Refresh; }
            const char* GetLabel(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Refresh";
            }
            EditorMenuSectionId GetSection() const override { return EditorMenuSectionId::Asset; }
            int GetSortOrder() const override { return 20; }

            bool IsVisibleInMenu(const EditorMenuContext& ctx) const override
            {
                return GetContext(ctx) != nullptr;
            }

            bool CanExecute(const EditorMenuContext& ctx) const override
            {
                return IsVisibleInMenu(ctx);
            }

            const char* GetDisabledReason(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                RefreshContentBrowser(editor);
            }
        };

        class ContentBrowserCreateSceneAction final : public ContentBrowserScopedAction
        {
        public:
            EditorActionId GetId() const override { return EditorActionId::OpenAsset; }
            const char* GetLabel(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Scene";
            }
            EditorMenuSectionId GetSection() const override { return EditorMenuSectionId::Create; }
            int GetSortOrder() const override { return 10; }

            bool IsVisibleInMenu(const EditorMenuContext& ctx) const override
            {
                const ContentBrowserMenuContext* cbCtx = GetContext(ctx);
                if (cbCtx == nullptr)
                {
                    return false;
                }

                return cbCtx->HitKind == ContentBrowserHitKind::TreeDirectory
                    || cbCtx->HitKind == ContentBrowserHitKind::ListBackground;
            }

            bool CanExecute(const EditorMenuContext& ctx) const override
            {
                return IsVisibleInMenu(ctx);
            }

            const char* GetDisabledReason(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                const ContentBrowserMenuContext* cbCtx = GetContext(ctx);
                const std::string_view destDir = cbCtx != nullptr ? cbCtx->CurrentDirectoryRel : std::string_view{};
                editor.GetAssetWorkflow().TryCreateSceneInDirectory(destDir);
            }
        };

        class ContentBrowserCreateMaterialAction final : public ContentBrowserScopedAction
        {
        public:
            EditorActionId GetId() const override { return EditorActionId::CreateEmptyGameObject; }
            const char* GetLabel(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Material";
            }
            EditorMenuSectionId GetSection() const override { return EditorMenuSectionId::Create; }
            int GetSortOrder() const override { return 20; }

            bool IsVisibleInMenu(const EditorMenuContext& ctx) const override
            {
                const ContentBrowserMenuContext* cbCtx = GetContext(ctx);
                if (cbCtx == nullptr)
                {
                    return false;
                }

                return cbCtx->HitKind == ContentBrowserHitKind::TreeDirectory
                    || cbCtx->HitKind == ContentBrowserHitKind::ListBackground;
            }

            bool CanExecute(const EditorMenuContext& ctx) const override
            {
                return IsVisibleInMenu(ctx);
            }

            const char* GetDisabledReason(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                const ContentBrowserMenuContext* cbCtx = GetContext(ctx);
                const std::string_view destDir = cbCtx != nullptr ? cbCtx->CurrentDirectoryRel : std::string_view{};
                editor.GetAssetWorkflow().TryCreateMaterialInDirectory(destDir);
            }
        };

        class ContentBrowserCreateMaterialInstanceAction final : public ContentBrowserScopedAction
        {
        public:
            EditorActionId GetId() const override { return EditorActionId::CreateMaterialInstance; }
            const char* GetLabel(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Material Instance";
            }
            EditorMenuSectionId GetSection() const override { return EditorMenuSectionId::Create; }
            int GetSortOrder() const override { return 21; }

            bool IsVisibleInMenu(const EditorMenuContext& ctx) const override
            {
                const ContentBrowserMenuContext* cbCtx = GetContext(ctx);
                if (cbCtx == nullptr)
                {
                    return false;
                }

                return cbCtx->HitKind == ContentBrowserHitKind::TreeDirectory
                    || cbCtx->HitKind == ContentBrowserHitKind::ListBackground;
            }

            bool CanExecute(const EditorMenuContext& ctx) const override
            {
                return IsVisibleInMenu(ctx);
            }

            const char* GetDisabledReason(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                const ContentBrowserMenuContext* cbCtx = GetContext(ctx);
                const std::string_view destDir = cbCtx != nullptr ? cbCtx->CurrentDirectoryRel : std::string_view{};
                editor.GetAssetWorkflow().TryCreateMaterialInstanceInDirectory(destDir);
            }
        };

        class ContentBrowserCreateSpriteAction final : public ContentBrowserScopedAction
        {
        public:
            EditorActionId GetId() const override { return EditorActionId::CreateEmptyGameObject; }
            const char* GetLabel(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Sprite";
            }
            EditorMenuSectionId GetSection() const override { return EditorMenuSectionId::Create; }
            int GetSortOrder() const override { return 22; }

            bool IsVisibleInMenu(const EditorMenuContext& ctx) const override
            {
                const ContentBrowserMenuContext* cbCtx = GetContext(ctx);
                if (cbCtx == nullptr)
                {
                    return false;
                }

                return cbCtx->HitKind == ContentBrowserHitKind::TreeDirectory
                    || cbCtx->HitKind == ContentBrowserHitKind::ListBackground;
            }

            bool CanExecute(const EditorMenuContext& ctx) const override
            {
                return IsVisibleInMenu(ctx);
            }

            const char* GetDisabledReason(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                const ContentBrowserMenuContext* cbCtx = GetContext(ctx);
                const std::string_view destDir = cbCtx != nullptr ? cbCtx->CurrentDirectoryRel : std::string_view{};
                editor.GetAssetWorkflow().TryCreateSpriteInDirectory(destDir);
            }
        };

        class ContentBrowserCreateSpriteAtlasAction final : public ContentBrowserScopedAction
        {
        public:
            EditorActionId GetId() const override { return EditorActionId::CreateEmptyGameObject; }
            const char* GetLabel(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Sprite Atlas";
            }
            EditorMenuSectionId GetSection() const override { return EditorMenuSectionId::Create; }
            int GetSortOrder() const override { return 23; }

            bool IsVisibleInMenu(const EditorMenuContext& ctx) const override
            {
                const ContentBrowserMenuContext* cbCtx = GetContext(ctx);
                if (cbCtx == nullptr)
                {
                    return false;
                }

                return cbCtx->HitKind == ContentBrowserHitKind::TreeDirectory
                    || cbCtx->HitKind == ContentBrowserHitKind::ListBackground;
            }

            bool CanExecute(const EditorMenuContext& ctx) const override
            {
                return IsVisibleInMenu(ctx);
            }

            const char* GetDisabledReason(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                const ContentBrowserMenuContext* cbCtx = GetContext(ctx);
                const std::string_view destDir = cbCtx != nullptr ? cbCtx->CurrentDirectoryRel : std::string_view{};
                editor.GetAssetWorkflow().TryCreateSpriteAtlasInDirectory(destDir);
            }
        };

        class ContentBrowserCreateDataAssetAction final : public ContentBrowserScopedAction
        {
        public:
            EditorActionId GetId() const override { return EditorActionId::CreateEmptyGameObject; }
            const char* GetLabel(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Data Asset";
            }
            EditorMenuSectionId GetSection() const override { return EditorMenuSectionId::Create; }
            int GetSortOrder() const override { return 24; }

            bool IsVisibleInMenu(const EditorMenuContext& ctx) const override
            {
                const ContentBrowserMenuContext* cbCtx = GetContext(ctx);
                if (cbCtx == nullptr)
                {
                    return false;
                }

                return cbCtx->HitKind == ContentBrowserHitKind::TreeDirectory
                    || cbCtx->HitKind == ContentBrowserHitKind::ListBackground;
            }

            bool CanExecute(const EditorMenuContext& ctx) const override
            {
                return IsVisibleInMenu(ctx);
            }

            const char* GetDisabledReason(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                const ContentBrowserMenuContext* cbCtx = GetContext(ctx);
                const std::string_view destDir = cbCtx != nullptr ? cbCtx->CurrentDirectoryRel : std::string_view{};
                editor.GetAssetWorkflow().TryCreateDataAssetInDirectory(destDir);
            }
        };

        class ContentBrowserCreateAtlasFromSpritesAction final : public ContentBrowserScopedAction
        {
        public:
            EditorActionId GetId() const override { return EditorActionId::CreateEmptyGameObject; }
            const char* GetLabel(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "Create Sprite Atlas From Selection";
            }
            EditorMenuSectionId GetSection() const override { return EditorMenuSectionId::Create; }
            int GetSortOrder() const override { return 25; }

            bool IsVisibleInMenu(const EditorMenuContext& ctx) const override
            {
                const ContentBrowserMenuContext* cbCtx = GetContext(ctx);
                if (cbCtx == nullptr || cbCtx->SelectedAssets.empty())
                {
                    return false;
                }

                for (const AssetMeta* meta : cbCtx->SelectedAssets)
                {
                    if (meta != nullptr && meta->AssetType == "Sprite")
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
                return "";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                const ContentBrowserMenuContext* cbCtx = GetContext(ctx);
                if (cbCtx == nullptr)
                {
                    return;
                }
                editor.GetAssetWorkflow().TryCreateSpriteAtlasFromSelection(cbCtx->SelectedAssets);
            }
        };

        class ContentBrowserCreateFolderAction final : public ContentBrowserScopedAction
        {
        public:
            EditorActionId GetId() const override { return EditorActionId::CreateFolder; }
            const char* GetLabel(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "New Folder";
            }
            EditorMenuSectionId GetSection() const override { return EditorMenuSectionId::Create; }
            int GetSortOrder() const override { return 5; }

            bool IsVisibleInMenu(const EditorMenuContext& ctx) const override
            {
                return GetContext(ctx) != nullptr;
            }

            bool CanExecute(const EditorMenuContext& ctx) const override
            {
                const ContentBrowserMenuContext* cbCtx = GetContext(ctx);
                if (cbCtx == nullptr)
                {
                    return false;
                }
                return cbCtx->HitKind == ContentBrowserHitKind::TreeDirectory
                    || cbCtx->HitKind == ContentBrowserHitKind::ListBackground;
            }

            const char* GetDisabledReason(const EditorMenuContext& ctx) const override
            {
                (void)ctx;
                return "New Folder is available on folders or empty list areas.";
            }

            void Execute(IEditorContext& editor, const EditorMenuContext& ctx) const override
            {
                const ContentBrowserMenuContext* cbCtx = GetContext(ctx);
                const std::string parent =
                    cbCtx != nullptr ? cbCtx->CurrentDirectoryRel : std::string{};
                editor.GetContentBrowser().RequestCreateFolderDialog(parent);
            }
        };
    }

    void RegisterContentBrowserBuiltInActions(EditorActionRegistry& registry)
    {
        registry.Register(std::make_unique<ContentBrowserImportAction>());
        registry.Register(std::make_unique<ContentBrowserRefreshAction>());
        registry.Register(std::make_unique<ContentBrowserCreateFolderAction>());
        registry.Register(std::make_unique<ContentBrowserCreateSceneAction>());
        registry.Register(std::make_unique<ContentBrowserCreateMaterialAction>());
        registry.Register(std::make_unique<ContentBrowserCreateMaterialInstanceAction>());
        registry.Register(std::make_unique<ContentBrowserCreateSpriteAction>());
        registry.Register(std::make_unique<ContentBrowserCreateSpriteAtlasAction>());
        registry.Register(std::make_unique<ContentBrowserCreateDataAssetAction>());
        registry.Register(std::make_unique<ContentBrowserCreateAtlasFromSpritesAction>());
    }

} // namespace minEngine
