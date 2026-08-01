#include "LuaScriptMvpTest.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Function/Framework/Components/LuaComponent.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Scripting/LuaBindProbe.h"
#include "Runtime/Function/Scripting/LuaScriptSystem.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/LuaScript.h"

#include "doctest.h"

#include "EngineTestFixture.h"

#include <filesystem>
#include <fstream>

namespace minEngine
{
    class LuaScriptMvpTestScope
    {
    public:
        LuaScriptMvpTestScope(bool withAssets)
        {
            ObjectManager::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();

            if (withAssets)
            {
                SceneManager::SetInstance(&m_SceneManager);
                m_SceneManager.Initialize();

                AssetManager::SetInstance(&m_AssetManager);
                m_AssetManager.Initialize();
                m_OwnsAssets = true;
            }
        }

        ~LuaScriptMvpTestScope()
        {
            if (m_OwnsAssets)
            {
                m_AssetManager.Shutdown();
                AssetManager::SetInstance(nullptr);

                m_SceneManager.Shutdown();
                SceneManager::SetInstance(nullptr);
            }

            m_ObjectManager.Shutdown();
            ObjectManager::SetInstance(nullptr);
        }

    private:
        ObjectManager m_ObjectManager;
        AssetManager m_AssetManager;
        SceneManager m_SceneManager;
        bool m_OwnsAssets = false;
    };

    namespace
    {
        class TempProjectScope
        {
        public:
            TempProjectScope()
            {
                const std::filesystem::path tempRoot =
                    std::filesystem::temp_directory_path() / "minEngine_LuaScriptS05Test";
                std::error_code removeError;
                std::filesystem::remove_all(tempRoot, removeError);

                m_ProjectRoot = tempRoot / "Project";
                m_ContentDirectory = m_ProjectRoot / "Assets" / "_LuaS05";
                std::filesystem::create_directories(m_ContentDirectory);
                PathRegistry::Get().SetProjectRoots(m_ProjectRoot);
            }

            ~TempProjectScope()
            {
                PathRegistry::Get().ClearProjectRoots();
                std::error_code removeError;
                std::filesystem::remove_all(m_ProjectRoot.parent_path(), removeError);
            }

            const std::filesystem::path& GetContentDirectory() const { return m_ContentDirectory; }

        private:
            std::filesystem::path m_ProjectRoot;
            std::filesystem::path m_ContentDirectory;
        };

        std::shared_ptr<LuaScript> MakeTickProbeScript(const std::string& name)
        {
            std::shared_ptr<LuaScript> script = NewObject<LuaScript>(name);
            script->SetSource(R"LUA(
function tick(dt)
  LuaBindProbe.IncrementStaticCounter()
end
)LUA");
            return script;
        }

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

        bool TestLuaComponentAssetTick()
        {
            LuaScriptMvpTestScope scope(false);
            LuaScriptSystem system;
            LuaScriptSystem::SetInstance(&system);
            system.Initialize();

            LuaBindProbe::ResetStaticCounter();
            LuaComponent component;
            component.SetScript(MakeTickProbeScript("TickProbe"));
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
            LuaScriptMvpTestScope scope(false);
            LuaScriptSystem system;
            LuaScriptSystem::SetInstance(&system);
            system.Initialize();

            LuaBindProbe::ResetStaticCounter();
            {
                LuaComponent component;
                component.SetScript(MakeTickProbeScript("DestroyProbe"));
                component.Tick(0.016f);
                if (LuaBindProbe::GetStaticCounter() != 1)
                {
                    ME_CORE_ERROR("LuaScriptMvpTest: destroy path preload failed.");
                    system.Shutdown();
                    LuaScriptSystem::SetInstance(nullptr);
                    return false;
                }
            }

            const bool ok = system.RunString("return true", "mvp_after_destroy");
            system.Shutdown();
            LuaScriptSystem::SetInstance(nullptr);
            return ok;
        }

        bool TestLuaScriptLoaderFromDisk()
        {
            TempProjectScope project;
            LuaScriptMvpTestScope scope(true);

            const std::filesystem::path scriptPath = project.GetContentDirectory() / "tick_probe.lua";
            {
                std::ofstream output(scriptPath, std::ios::binary);
                if (!output.is_open())
                {
                    ME_CORE_ERROR("LuaScriptMvpTest: failed to write fixture .lua");
                    return false;
                }
                output << "function tick(dt)\n  LuaBindProbe.IncrementStaticCounter()\nend\n";
            }

            const AssetMeta meta = AssetManager::Get().RegisterAsset(scriptPath.string(), "LuaScript");
            if (meta.AssetPath.empty())
            {
                ME_CORE_ERROR("LuaScriptMvpTest: RegisterAsset failed for .lua fixture.");
                return false;
            }

            std::shared_ptr<LuaScript> script = AssetManager::Get().LoadAsset<LuaScript>(meta.AssetPath);
            if (script == nullptr || !script->IsValid())
            {
                ME_CORE_ERROR("LuaScriptMvpTest: LoadAsset<LuaScript> failed.");
                return false;
            }

            if (script->GetSource().find("IncrementStaticCounter") == std::string::npos)
            {
                ME_CORE_ERROR("LuaScriptMvpTest: loaded source missing expected body.");
                return false;
            }

            LuaScriptSystem system;
            LuaScriptSystem::SetInstance(&system);
            system.Initialize();

            LuaBindProbe::ResetStaticCounter();
            LuaComponent component;
            component.SetScript(script);
            component.Tick(0.016f);
            component.Tick(0.016f);

            const bool ok = LuaBindProbe::GetStaticCounter() == 2;
            if (!ok)
            {
                ME_CORE_ERROR(
                    "LuaScriptMvpTest: disk-loaded script tick expected 2, got {}.",
                    LuaBindProbe::GetStaticCounter());
            }

            component.UnloadScript();
            system.Shutdown();
            LuaScriptSystem::SetInstance(nullptr);
            return ok;
        }

        bool TestGeneratedTransformBinding()
        {
            LuaScriptSystem system;
            LuaScriptSystem::SetInstance(&system);
            system.Initialize();

            const bool ok = system.RunString(R"LUA(
local t = Transform.MakeIdentity()
assert(t.Position.x == 0 and t.Position.y == 0 and t.Position.z == 0)
t.Position = Vector3.new(1, 2, 3)
assert(t.Position.x == 1 and t.Position.y == 2 and t.Position.z == 3)
t:SetPosition(Vector3.new(4, 5, 6))
assert(t.Position.x == 4 and t.Position.y == 5 and t.Position.z == 6)
t:Translate(Vector3.new(1, 1, 1))
assert(t.Position.x == 5 and t.Position.y == 6 and t.Position.z == 7)
)LUA",
                                             "script_binding_transform");

            system.Shutdown();
            LuaScriptSystem::SetInstance(nullptr);
            return ok;
        }

        bool TestMathPrimitiveBindings()
        {
            LuaScriptSystem system;
            LuaScriptSystem::SetInstance(&system);
            system.Initialize();

            const bool ok = system.RunString(R"LUA(
local v2 = Vector2.new(1.5, -2.5)
assert(v2.x == 1.5 and v2.y == -2.5)
v2.x = 3
assert(v2.x == 3)

local v4 = Vector4.new(1, 2, 3, 4)
assert(v4.x == 1 and v4.y == 2 and v4.z == 3 and v4.w == 4)
v4.w = 9
assert(v4.w == 9)
)LUA",
                                             "script_binding_math_primitives");

            system.Shutdown();
            LuaScriptSystem::SetInstance(nullptr);
            return ok;
        }

        bool TestSceneEntrySelfOwnerTranslate()
        {
            // SceneComponent::SetPosition marks EOF updates via SceneManager.
            LuaScriptMvpTestScope scope(true);
            LuaScriptSystem system;
            LuaScriptSystem::SetInstance(&system);
            system.Initialize();

            std::shared_ptr<GameObject> gameObject = NewObject<GameObject>("LuaSceneEntryGO");
            gameObject->AddComponent<SceneComponent>();
            std::shared_ptr<LuaComponent> luaComponent = gameObject->AddComponent<LuaComponent>();

            std::shared_ptr<LuaScript> script = NewObject<LuaScript>("SceneEntryScript");
            script->SetSource(R"LUA(
function tick(dt)
  local owner = self:GetOwner()
  assert(owner ~= nil)
  owner:SetPosition(Vector3.new(0, 0, 0))
  owner:Translate(Vector3.new(2, 3, 4))
end
)LUA");
            luaComponent->SetScript(script);
            luaComponent->Tick(0.016f);

            const Vector3 position = gameObject->GetPosition();
            const bool ok =
                std::abs(position.x - 2.0f) < 1e-4f &&
                std::abs(position.y - 3.0f) < 1e-4f &&
                std::abs(position.z - 4.0f) < 1e-4f;
            if (!ok)
            {
                ME_CORE_ERROR(
                    "LuaScriptMvpTest: scene entry expected (2,3,4), got ({},{},{}).",
                    position.x,
                    position.y,
                    position.z);
            }

            luaComponent->UnloadScript();
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
        if (!TestLuaComponentAssetTick())
        {
            return false;
        }
        if (!TestLuaComponentDestroyClearsEnv())
        {
            return false;
        }
        if (!TestLuaScriptLoaderFromDisk())
        {
            return false;
        }
        if (!TestGeneratedTransformBinding())
        {
            return false;
        }
        if (!TestMathPrimitiveBindings())
        {
            return false;
        }
        if (!TestSceneEntrySelfOwnerTranslate())
        {
            return false;
        }
        return true;
    }
} // namespace minEngine

TEST_CASE("lua-script-mvp: feasibility [mvp]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunLuaScriptMvpTests());
}
