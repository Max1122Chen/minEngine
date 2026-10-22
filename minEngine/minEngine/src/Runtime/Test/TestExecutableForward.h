#pragma once

#include "Runtime/Core/EngineAPI.h"

namespace minEngine
{
    /** Run minEngineTests.exe beside the current executable; returns process exit code. */
    MINENGINE_API int ForwardToMinEngineTestsExecutable(int argc, char** argv);
}
