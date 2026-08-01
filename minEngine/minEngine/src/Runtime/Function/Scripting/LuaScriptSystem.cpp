#include "LuaScriptSystem.h"

#include "LuaManualBindings.h"

#include "Generated/ScriptBinding/ScriptBindingRegister.gen.h"
#include "Runtime/Core/Assert/Assert.h"
#include "Runtime/Core/Log/LogSystem.h"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace minEngine
{
    LuaScriptSystem* LuaScriptSystem::s_Instance = nullptr;

    LuaScriptSystem::LuaScriptSystem() = default;

    LuaScriptSystem::~LuaScriptSystem()
    {
        Shutdown();
    }

    void LuaScriptSystem::SetInstance(LuaScriptSystem* instance)
    {
        s_Instance = instance;
    }

    LuaScriptSystem& LuaScriptSystem::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "LuaScriptSystem is not initialized");
        return *s_Instance;
    }

    bool LuaScriptSystem::HasInstance()
    {
        return s_Instance != nullptr && s_Instance->m_Initialized;
    }

    void LuaScriptSystem::Initialize()
    {
        if (m_Initialized)
        {
            return;
        }

        m_State = std::make_unique<sol::state>();
        OpenStandardLibraries();
        LuaManualBindings::Register(*m_State);
        RegisterGeneratedLuaBindings(*m_State);
        m_Initialized = true;
        ME_CORE_INFO("LuaScriptSystem initialized.");
    }

    void LuaScriptSystem::Shutdown()
    {
        if (!m_Initialized)
        {
            return;
        }

        m_State.reset();
        m_Initialized = false;
        ME_CORE_INFO("LuaScriptSystem shut down.");
    }

    sol::state& LuaScriptSystem::GetState()
    {
        ME_ASSERT(m_Initialized && m_State != nullptr, "LuaScriptSystem state is not ready");
        return *m_State;
    }

    const sol::state& LuaScriptSystem::GetState() const
    {
        ME_ASSERT(m_Initialized && m_State != nullptr, "LuaScriptSystem state is not ready");
        return *m_State;
    }

    void LuaScriptSystem::ReportLuaError(std::string_view context, const sol::error& error)
    {
        ME_CORE_ERROR("Lua error [{}]: {}", context, error.what());
    }

    void LuaScriptSystem::ReportLuaError(std::string_view context, std::string_view message)
    {
        ME_CORE_ERROR("Lua error [{}]: {}", context, message);
    }

    bool LuaScriptSystem::RunString(std::string_view chunk, std::string_view chunkName)
    {
        if (!m_Initialized || m_State == nullptr)
        {
            ReportLuaError(chunkName, "LuaScriptSystem is not initialized.");
            return false;
        }

        const sol::protected_function_result result =
            m_State->safe_script(chunk, sol::script_pass_on_error, std::string(chunkName));
        if (!result.valid())
        {
            const sol::error error = result;
            ReportLuaError(chunkName, error);
            return false;
        }

        return true;
    }

    void LuaScriptSystem::OpenStandardLibraries()
    {
        m_State->open_libraries(
            sol::lib::base,
            sol::lib::package,
            sol::lib::string,
            sol::lib::table,
            sol::lib::math);
    }
}
