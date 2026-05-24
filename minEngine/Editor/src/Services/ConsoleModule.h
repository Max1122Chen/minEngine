#pragma once

#include "Core.h"
#include "Shell/EditorServiceModule.h"

namespace minEngine
{
    class ConsoleModule : public EditorServiceModule
    {
    public:
        static constexpr const char* kModuleId = "Console";

        std::string_view GetModuleId() const override { return kModuleId; }
        void Register(IEditorContext& context) override;
        void Shutdown() override;
    };
}
