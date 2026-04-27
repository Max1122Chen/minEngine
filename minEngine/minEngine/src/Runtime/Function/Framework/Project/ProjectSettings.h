#pragma once
#include "Core.h"

namespace minEngine
{
    ME_STRUCT()
    struct ProjectSettings
    {
        ME_GENERATED_BODY(ProjectSettings)

        ME_PROPERTY()
        std::string EditorDefaultSceneName;
    };
}

#include "ProjectSettings.gen.h"