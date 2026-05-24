#pragma once

#include "Core.h"
#include "Shell/EditorServiceModule.h"

namespace minEngine
{
    class MainMenuModule : public EditorServiceModule
    {
    public:
        static constexpr const char* kModuleId = "MainMenu";

        std::string_view GetModuleId() const override { return kModuleId; }
        void Register(IEditorContext& context) override;
        void Shutdown() override;
    };
}
