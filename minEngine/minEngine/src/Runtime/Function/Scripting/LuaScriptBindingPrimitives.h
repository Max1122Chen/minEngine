#pragma once

#include "EngineAPI.h"

namespace sol
{
    class state;
}

namespace minEngine
{
    // Hand-written math/value bindings used by generated ScriptBinding usertypes.
    MINENGINE_API void RegisterLuaScriptBindingPrimitives(sol::state& state);
}
