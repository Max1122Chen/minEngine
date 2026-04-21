#pragma once

#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"

#include <cstdint>
#include <filesystem>
#include <string>

/**
 * A minEngine project's directory structure should be like this:
 * MyProject/
 * ├── MyProject.meproject       // Project descriptor file (e.g., JSON format) that contains project metadata like name, unique ID, asset directory, etc.
 * ├── Assets/                   // All project assets (models, textures, materials, scenes, etc.) are stored here. This can be further organized into subdirectories if needed.
 * │   ├── Textures/
 * │   ├── Models/
 * │   ├── Materials/
 * │   └── Scenes/
 * ├── Code/                     // (Optional) Source code for game-specific logic, editor extensions, etc.
 * └── Binaries/                 // (Optional) Compiled binaries, build outputs, etc.
 */

namespace minEngine
{
    ME_STRUCT()
    struct ProjectDescriptor
    {
        ME_REFLECTION_FRIEND(ProjectDescriptor)

        ME_PROPERTY()
        std::string ProjectName;

        ME_PROPERTY()
        GUID ProjectId;

        ME_PROPERTY()
        std::string ProjectRoot;    // Absolute path to the project root directory
    };
}

#include "ProjectDescriptor.gen.h"