#pragma once

namespace sol
{
    class state;
}

namespace minEngine
{
    /// Lua ↔ Dynamic multicast bridge (sol allowed; Core stays sol-free).
    class LuaDynamicDelegateBindings
    {
    public:
        static void Register(sol::state& state);
    };

    inline void RegisterDynamicMulticastDelegateLuaUsertype(sol::state& state)
    {
        LuaDynamicDelegateBindings::Register(state);
    }
}
