#include "LuaComponent.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Scripting/LuaScriptSystem.h"

namespace minEngine
{
    LuaComponent::~LuaComponent()
    {
        UnloadScript();
    }

    void LuaComponent::SetScript(const std::shared_ptr<LuaScript>& script)
    {
        if (m_Script == script)
        {
            return;
        }

        UnloadScript();
        m_Script = script;
        m_SyncedScript = script.get();
        m_ScriptEnabled = true;
        m_HasLoggedTickError = false;
    }

    void LuaComponent::Tick(float deltaTime)
    {
        // Inspector may assign m_Script via reflection without calling SetScript.
        if (m_Script.get() != m_SyncedScript)
        {
            ClearLuaEnvironment();
            m_SyncedScript = m_Script.get();
            m_ScriptEnabled = true;
            m_HasLoggedTickError = false;
        }

        if (!m_ScriptEnabled || !IsActive())
        {
            return;
        }

        // No asset yet: idle (do not disable — editor often adds component before assigning script).
        if (m_Script == nullptr)
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

        if (m_Script == nullptr)
        {
            ClearLuaEnvironment();
            return false;
        }

        if (!m_Script->IsValid())
        {
            ME_CORE_ERROR("LuaComponent::LoadScript: LuaScript asset source is empty.");
            ClearLuaEnvironment();
            m_ScriptEnabled = false;
            return false;
        }

        ClearLuaEnvironment();

        sol::state& state = LuaScriptSystem::Get().GetState();
        m_Environment = sol::environment(state, sol::create, state.globals());
        // Inject host as LuaComponent* (ScriptType + sol::bases<Component>). Cleared in UnloadScript.
        m_Environment["self"] = this;

        const sol::protected_function_result loadResult = state.safe_script(
            m_Script->GetSource(),
            m_Environment,
            sol::script_pass_on_error,
            "LuaComponent");
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
        m_SyncedScript = m_Script.get();
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
