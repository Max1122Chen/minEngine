#pragma once
#include "Core.h"

#include "Runtime/Function/Framework/Project/EditorSettings.h"

namespace minEngine
{
    ME_STRUCT()
    struct ProjectSettings
    {
        ME_GENERATED_BODY(ProjectSettings)

        ME_PROPERTY()
        std::string EditorDefaultSceneName;

        ME_PROPERTY()
        EditorSettings Editor;
    };
}

#include "ProjectSettings.gen.h"