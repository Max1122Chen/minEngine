#pragma once

#include "EngineAPI.h"

#include <memory>
#include <string_view>

namespace sol
{
    class state;
    class error;
}

namespace minEngine
{
    class MINENGINE_API LuaScriptSystem
    {
    public:
        // Out-of-line: unique_ptr<sol::state> needs complete sol::state in the .cpp.
        LuaScriptSystem();
        ~LuaScriptSystem();

        LuaScriptSystem(const LuaScriptSystem&) = delete;
        LuaScriptSystem& operator=(const LuaScriptSystem&) = delete;

        void Initialize();
        void Shutdown();

        static void SetInstance(LuaScriptSystem* instance);
        static LuaScriptSystem& Get();
        static bool HasInstance();

        sol::state& GetState();
        const sol::state& GetState() const;

        void ReportLuaError(std::string_view context, const sol::error& error);
        void ReportLuaError(std::string_view context, std::string_view message);

        bool RunString(std::string_view chunk, std::string_view chunkName = "RunString");

    private:
        void OpenStandardLibraries();

        static LuaScriptSystem* s_Instance;

        std::unique_ptr<sol::state> m_State;
        bool m_Initialized = false;
    };
}
