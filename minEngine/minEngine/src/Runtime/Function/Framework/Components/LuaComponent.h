#pragma once

#include "Runtime/Function/Framework/Components/Component.h"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace minEngine
{
    // MVP feasibility component: hardcoded Lua chunk only (no script path / file IO).
    class LuaComponent : public Component
    {
    public:
        LuaComponent() = default;
        ~LuaComponent() override;

        void Tick(float deltaTime) override;

        bool IsScriptLoaded() const { return m_Loaded; }
        bool IsScriptEnabled() const { return m_ScriptEnabled; }

        bool LoadScript();
        void UnloadScript();

    private:
        static const char* GetHardcodedScript();

        bool EnsureLoaded();
        bool CallTick(float deltaTime);
        void ClearLuaEnvironment();

        sol::environment m_Environment;
        sol::protected_function m_TickFn;
        bool m_Loaded = false;
        bool m_ScriptEnabled = true;
        bool m_HasLoggedTickError = false;
    };
}
