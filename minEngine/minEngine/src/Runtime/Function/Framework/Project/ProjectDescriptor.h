#pragma once

#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace minEngine
{
    ME_STRUCT()
    struct ProjectDescriptor
    {
        ME_REFLECTION_FRIEND(ProjectDescriptor)

        ME_PROPERTY()
        uint32_t SchemaVersion = 1;

        ME_PROPERTY()
        std::string ProjectName;

        ME_PROPERTY()
        GUID ProjectId;

        ME_PROPERTY()
        std::string EngineVersion;

        ME_PROPERTY()
        std::string ContentRoot;

        ME_PROPERTY()
        std::string ConfigRoot;

        ME_PROPERTY()
        std::string EditorDefaultScene;
    };
}

#include "Generated/Reflection/ProjectDescriptor.gen.h"