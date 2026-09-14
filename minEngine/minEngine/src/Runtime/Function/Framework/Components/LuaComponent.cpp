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
            ME_LOG(LogScript, Error, "LuaComponent::LoadScript: LuaScriptSystem is not available.");
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
            ME_LOG(LogScript, Error, "LuaComponent::LoadScript: LuaScript asset source is empty.");
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

        m_Loaded = true;
        m_TickFn = TryGetFunction("tick");
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

    sol::protected_function LuaComponent::TryGetFunction(const char* name) const
    {
        if (!m_Loaded || name == nullptr || name[0] == '\0')
        {
            return sol::protected_function();
        }

        sol::object object = m_Environment[name];
        if (object.is<sol::protected_function>())
        {
            return object.as<sol::protected_function>();
        }

        return sol::protected_function();
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
