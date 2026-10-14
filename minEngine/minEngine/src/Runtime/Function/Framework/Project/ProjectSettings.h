#pragma once
#include "Core.h"

#include "Runtime/Function/Framework/Project/EditorAppearanceSettings.h"
#include "Runtime/Function/Framework/Project/EditorSettings.h"

namespace minEngine
{
    ME_STRUCT()
    struct ProjectSettings
    {
        ME_GENERATED_BODY()

        ME_PROPERTY()
        std::string EditorDefaultSceneName;

        /**
         * Soft class path for the session GameInstance hub (UE GameMapsSettings::GameInstanceClass).
         * Empty = minEngine::GameInstance. Use reflection full name, e.g. GameModuleStub::StubGameInstance.
         */
        ME_PROPERTY()
        std::string GameInstanceClass;

        ME_PROPERTY()
        EditorSettings Editor;

        ME_PROPERTY()
        EditorAppearanceSettings Appearance;
    };
}

#include "ProjectSettings.gen.h"