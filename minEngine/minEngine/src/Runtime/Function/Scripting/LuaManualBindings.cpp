#include "LuaManualBindings.h"

#include "LuaBindProbe.h"

#include "Runtime/Core/Log/LogSystem.h"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace minEngine
{
    void LuaManualBindings::Log(std::string_view message)
    {
        ME_CORE_INFO("[Lua] {}", message);
    }

    void LuaManualBindings::Register(sol::state& state)
    {
        sol::table me = state.create_named_table("me");
        me.set_function("log", &LuaManualBindings::Log);

        state.new_usertype<LuaBindProbe>(
            "LuaBindProbe",
            sol::constructors<LuaBindProbe()>(),
            "Add",
            &LuaBindProbe::Add,
            "SetValue",
            &LuaBindProbe::SetValue,
            "GetValue",
            &LuaBindProbe::GetValue,
            "ResetStaticCounter",
            &LuaBindProbe::ResetStaticCounter,
            "GetStaticCounter",
            &LuaBindProbe::GetStaticCounter,
            "IncrementStaticCounter",
            &LuaBindProbe::IncrementStaticCounter);
    }
}
