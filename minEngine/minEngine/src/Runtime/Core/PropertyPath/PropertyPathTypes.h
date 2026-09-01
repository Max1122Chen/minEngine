#pragma once

#include "Core.h"

namespace minEngine::Reflection
{
    class MEClass;
}

namespace minEngine::Command
{
    struct ResolvedPropertyTarget
    {
        void* OwnerObject = nullptr;
        const Reflection::MEClass* OwnerClass = nullptr;
        std::string PropertySubPath;
    };
}
