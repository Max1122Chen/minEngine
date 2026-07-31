#include "LuaComponent.h"

#include "Runtime/Function/Scripting/LuaScriptSystem.h"

#include "Runtime/Core/Log/LogSystem.h"

namespace minEngine
{
    LuaComponent::~LuaComponent()
    {
        UnloadScript();
    }

    const char* LuaComponent::GetHardcodedScript()
    {
        return R"LUA(
function tick(dt)
  LuaBindProbe.IncrementStaticCounter()
end
)LUA";
    }

    void LuaComponent::Tick(float deltaTime)
    {
        if (!m_ScriptEnabled)
        {
            return;
        }

        if (!EnsureLoaded())
        {
            return;
        }

        CallTick(deltaTime);
    }

    bool LuaComponent::LoadScript()
    {
        if (!LuaScriptSystem::HasInstance())
        {
            ME_CORE_ERROR("LuaComponent::LoadScript: LuaScriptSystem is not available.");
            m_ScriptEnabled = false;
            return false;
        }

        ClearLuaEnvironment();

        sol::state& state = LuaScriptSystem::Get().GetState();
        m_Environment = sol::environment(state, sol::create, state.globals());

        // Order: code, env, on_error, chunkname (sol2 state_view::safe_script).
        const sol::protected_function_result loadResult = state.safe_script(
            GetHardcodedScript(),
            m_Environment,
            sol::script_pass_on_error,
            "LuaComponentHardcoded");
        if (!loadResult.valid())
        {
            const sol::error error = loadResult;
            LuaScriptSystem::Get().ReportLuaError("LuaComponent::LoadScript", error);
            ClearLuaEnvironment();
            m_ScriptEnabled = false;
            return false;
        }

        sol::object tickObject = m_Environment["tick"];
        if (tickObject.is<sol::protected_function>())
        {
            m_TickFn = tickObject.as<sol::protected_function>();
        }
        else
        {
            m_TickFn = sol::protected_function();
        }

        m_Loaded = true;
        m_ScriptEnabled = true;
        m_HasLoggedTickError = false;
        return true;
    }

    void LuaComponent::UnloadScript()
    {
        ClearLuaEnvironment();
        m_Loaded = false;
    }

    bool LuaComponent::EnsureLoaded()
    {
        if (m_Loaded)
        {
            return true;
        }

        return LoadScript();
    }

    bool LuaComponent::CallTick(float deltaTime)
    {
        if (!m_TickFn.valid())
        {
            return true;
        }

        const sol::protected_function_result result = m_TickFn(deltaTime);
        if (!result.valid())
        {
            if (!m_HasLoggedTickError)
            {
                const sol::error error = result;
                if (LuaScriptSystem::HasInstance())
                {
                    LuaScriptSystem::Get().ReportLuaError("LuaComponent::Tick", error);
                }
                m_HasLoggedTickError = true;
            }

            m_ScriptEnabled = false;
            return false;
        }

        return true;
    }

    void LuaComponent::ClearLuaEnvironment()
    {
        m_TickFn = sol::protected_function();
        m_Environment = sol::environment();
        m_Loaded = false;
    }
}
