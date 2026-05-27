#pragma once

#include "Core.h"
#include "Services/Inspector/InspectorAssetInspection.h"
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

        InspectorAssetInspection& GetAssetInspection() { return m_AssetInspection; }
        const InspectorAssetInspection& GetAssetInspection() const { return m_AssetInspection; }

        void SetInspectionTarget(const AssetMeta* meta);
        void ClearInspectionTarget();

    private:
        IEditorContext* m_Context = nullptr;
        InspectorAssetInspection m_AssetInspection;
    };
}
