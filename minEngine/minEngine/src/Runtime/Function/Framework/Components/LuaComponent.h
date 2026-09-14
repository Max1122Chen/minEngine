#pragma once

#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Scripting/LuaScriptSystem.h"
#include "Runtime/Resource/LuaScript.h"

#include <memory>
#include <utility>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace minEngine
{
    // Runs a LuaScript asset's source in a per-component environment; calls tick(dt).
    ME_CLASS(ScriptType)
    class LuaComponent : public Component
    {
        ME_GENERATED_BODY()

    public:
        LuaComponent() = default;
        ~LuaComponent() override;

        void SetScript(const std::shared_ptr<LuaScript>& script);
        LuaScript* GetScript() const { return m_Script.get(); }

        void Tick(float deltaTime) override;

        ME_FUNCTION(ScriptPure)
        bool IsScriptLoaded() const { return m_Loaded; }
        bool IsScriptEnabled() const { return m_ScriptEnabled; }

        bool LoadScript();
        void UnloadScript();

        /// Looks up a named function in the loaded environment (empty if missing / not loaded).
        sol::protected_function TryGetFunction(const char* name) const;

        /// Invokes a named Lua function with protected call semantics. Returns false on miss or error.
        template <typename... TArgs>
        bool Call(const char* name, TArgs&&... args)
        {
            sol::protected_function fn = TryGetFunction(name);
            if (!fn.valid())
            {
                return false;
            }

            const sol::protected_function_result result = fn(std::forward<TArgs>(args)...);
            if (!result.valid())
            {
                const sol::error error = result;
                if (LuaScriptSystem::HasInstance())
                {
                    LuaScriptSystem::Get().ReportLuaError("LuaComponent::Call", error);
                }
                return false;
            }

            return true;
        }

    private:
        bool EnsureLoaded();
        bool CallTick(float deltaTime);
        void ClearLuaEnvironment();

        ME_PROPERTY()
        std::shared_ptr<LuaScript> m_Script;

        sol::environment m_Environment;
        sol::protected_function m_TickFn;
        // Last m_Script.get() acknowledged by Tick/SetScript (inspector may mutate m_Script directly).
        LuaScript* m_SyncedScript = nullptr;
        bool m_Loaded = false;
        bool m_ScriptEnabled = true;
        bool m_HasLoggedTickError = false;
    };
}

#include "Generated/Reflection/LuaComponent.gen.h"
