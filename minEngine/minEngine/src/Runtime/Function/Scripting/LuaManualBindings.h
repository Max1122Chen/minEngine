#pragma once

#include <string_view>

namespace sol
{
    class state;
}

namespace minEngine
{
    class LuaManualBindings
    {
    public:
        static void Register(sol::state& state);

    private:
        static void Log(std::string_view message);
    };
}
