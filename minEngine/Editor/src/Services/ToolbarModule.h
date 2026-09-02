#pragma once

#include "Core.h"
#include "Shell/EditorServiceModule.h"

namespace minEngine
{
    class ToolbarModule : public EditorServiceModule
    {
    public:
        static constexpr const char* kModuleId = "Toolbar";

        std::string_view GetModuleId() const override { return kModuleId; }
        void Register(IEditorContext& context) override;
        void Shutdown() override;
    };
}
