#pragma once

#include "Core.h"

#include <string_view>

namespace minEngine
{
    class IEditorContext;

    class EditorServiceModule
    {
    public:
        virtual ~EditorServiceModule() = default;

        virtual std::string_view GetModuleId() const = 0;
        virtual void Register(IEditorContext& context) = 0;
        virtual void Shutdown() = 0;
        virtual void Tick(float deltaTime) {}
    };
}
