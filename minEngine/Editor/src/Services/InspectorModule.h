#pragma once

#include "Core.h"
#include "Shell/EditorServiceModule.h"

namespace minEngine
{
    /** Registers the shared Inspector dock panel (content from ActiveSubModule). */
    class InspectorModule : public EditorServiceModule
    {
    public:
        static constexpr const char* kModuleId = "Inspector";

        std::string_view GetModuleId() const override { return kModuleId; }
        void Register(IEditorContext& context) override;
        void Shutdown() override;
    };
}
