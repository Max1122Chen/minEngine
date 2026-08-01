#pragma once

#include "EngineAPI.h"

namespace sol
{
    class state;
}

namespace minEngine
{
    // Hand-written math/value bindings used by generated ScriptBinding usertypes.
    // S05: Vector2 / Vector3 / Vector4. Matrix* / quat deferred (awkward in Lua; not gameplay-critical yet).
    MINENGINE_API void RegisterLuaScriptBindingPrimitives(sol::state& state);
}
