#include "LuaScriptBindingPrimitives.h"

#include "Runtime/Core/Math/Math.h"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace minEngine
{
    void RegisterLuaScriptBindingPrimitives(sol::state& state)
    {
        if (state["Vector3"].get_type() == sol::type::userdata ||
            state["Vector3"].get_type() == sol::type::table)
        {
            return;
        }

        state.new_usertype<Vector3>(
            "Vector3",
            sol::constructors<Vector3(), Vector3(float, float, float)>(),
            "x",
            &Vector3::x,
            "y",
            &Vector3::y,
            "z",
            &Vector3::z);
    }
}
