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
    }

    void RegisterContentBrowserBuiltInActions(EditorActionRegistry& registry)
    {
        registry.Register(std::make_unique<ContentBrowserImportAction>());
        registry.Register(std::make_unique<ContentBrowserRefreshAction>());
    }

} // namespace minEngine
