#include "LuaScriptMvpTest.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Framework/Components/LuaComponent.h"
#include "Runtime/Function/Scripting/LuaBindProbe.h"
#include "Runtime/Function/Scripting/LuaScriptSystem.h"

#include "doctest.h"

#include "EngineTestFixture.h"

namespace minEngine
{
    namespace
    {
        bool TestSystemRunString()
        {
            LuaScriptSystem system;
            LuaScriptSystem::SetInstance(&system);
            system.Initialize();

            const bool ok = system.RunString("return 1 + 1", "mvp_add");
            system.Shutdown();
            LuaScriptSystem::SetInstance(nullptr);
            return ok;
        }

        bool TestProbeBindings()
        {
            LuaScriptSystem system;
            LuaScriptSystem::SetInstance(&system);
            system.Initialize();

            const bool ok = system.RunString(R"LUA(
local p = LuaBindProbe.new()
assert(p:Add(2, 3) == 5)
p:SetValue(42)
assert(p:GetValue() == 42)
LuaBindProbe.ResetStaticCounter()
LuaBindProbe.IncrementStaticCounter()
LuaBindProbe.IncrementStaticCounter()
assert(LuaBindProbe.GetStaticCounter() == 2)
me.log("probe ok")
)LUA",
                                             "mvp_probe");

            system.Shutdown();
            LuaScriptSystem::SetInstance(nullptr);
            return ok;
        }

        bool TestLuaComponentHardcodedTick()
        {
            LuaScriptSystem system;
            LuaScriptSystem::SetInstance(&system);
            system.Initialize();

            LuaBindProbe::ResetStaticCounter();
            LuaComponent component;
            component.Tick(0.016f);
            component.Tick(0.016f);
            component.Tick(0.016f);

            const int32_t counter = LuaBindProbe::GetStaticCounter();
            if (counter != 3)
            {
                ME_CORE_ERROR("LuaScriptMvpTest: expected static counter 3, got {}.", counter);
                component.UnloadScript();
                system.Shutdown();
                LuaScriptSystem::SetInstance(nullptr);
                return false;
            }

            component.UnloadScript();
            system.Shutdown();
            LuaScriptSystem::SetInstance(nullptr);
            return true;
        }

        bool TestLuaComponentDestroyClearsEnv()
        {
            LuaScriptSystem system;
            LuaScriptSystem::SetInstance(&system);
            system.Initialize();

            LuaBindProbe::ResetStaticCounter();
            {
                LuaComponent component;
                component.Tick(0.016f);
                if (LuaBindProbe::GetStaticCounter() != 1)
                {
                    ME_CORE_ERROR("LuaScriptMvpTest: destroy path preload failed.");
                    system.Shutdown();
                    LuaScriptSystem::SetInstance(nullptr);
                    return false;
                }
            }

            // Destructor UnloadScript must not crash; system still usable.
            const bool ok = system.RunString("return true", "mvp_after_destroy");
            system.Shutdown();
            LuaScriptSystem::SetInstance(nullptr);
            return ok;
        }
    } // namespace

    bool RunLuaScriptMvpTests()
    {
        if (!TestSystemRunString())
        {
            return false;
        }
        if (!TestProbeBindings())
        {
            return false;
        }
        if (!TestLuaComponentHardcodedTick())
        {
            return false;
        }
        if (!TestLuaComponentDestroyClearsEnv())
        {
            return false;
        }
        return true;
    }
} // namespace minEngine

TEST_CASE("lua-script-mvp: feasibility [mvp]")
{
    minEngine::EngineTestFixture fixture;
    CHECK(minEngine::RunLuaScriptMvpTests());
}
