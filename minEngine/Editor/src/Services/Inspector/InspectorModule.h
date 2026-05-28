#pragma once

#include "Core.h"
#include "Services/Thumbnail/AssetThumbnailService.h"
#include "Shell/EditorServiceModule.h"

namespace minEngine
{
    class AssetMeta;
    class IEditorContext;

    /** Registers the shared Inspector dock panel (content from ActiveSubModule). */
    class InspectorModule : public EditorServiceModule
    {
    public:
        static constexpr const char* kModuleId = "Inspector";

        std::string_view GetModuleId() const override { return kModuleId; }
        void Register(IEditorContext& context) override;
        void Shutdown() override;

        AssetThumbnailService& GetThumbnailService() { return m_ThumbnailService; }
        const AssetThumbnailService& GetThumbnailService() const { return m_ThumbnailService; }

        void SetInspectionTarget(const AssetMeta* meta);
        void ClearInspectionTarget();

    private:
        IEditorContext* m_Context = nullptr;
        AssetThumbnailService m_ThumbnailService;
    };
}
