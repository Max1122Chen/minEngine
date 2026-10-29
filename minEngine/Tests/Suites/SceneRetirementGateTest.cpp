#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Log/LogConsole.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Tasks/TaskSystem.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Core/Threading/RenderCommandPipe.h"
#include "Runtime/Function/Framework/Components/StaticMeshComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/MaterialRenderProxy.h"
#include "Runtime/Function/Render/RenderScene.h"

#include "EngineTestFixture.h"

#include "doctest.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using minEngine::FrameThreadMode;
using minEngine::FrameThreading;
using minEngine::Material;
using minEngine::ObjectManager;
using minEngine::Scene;
using minEngine::SceneManager;
using minEngine::StaticMeshComponent;
using minEngine::TaskSystem;

namespace
{
    /** The two registries a Scene destructor touches, plus the manager that owns the gate. */
    class SceneLifecycleScope
    {
    public:
        SceneLifecycleScope()
        {
            minEngine::Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            ObjectManager::Initialize();

            minEngine::Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
        }

        ~SceneLifecycleScope()
        {
            m_SceneManager.Shutdown();
            minEngine::Testing::TestAccess<SceneManager>::SetInstance(nullptr);

            ObjectManager::Shutdown();
            minEngine::Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
        }

        SceneLifecycleScope(const SceneLifecycleScope&) = delete;
        SceneLifecycleScope& operator=(const SceneLifecycleScope&) = delete;

    private:
        ObjectManager m_ObjectManager;
        SceneManager m_SceneManager;
    };

    /** Multi topology with the real Render/RHI role threads, so "which thread" is not simulated. */
    class FrameRoleScope
    {
    public:
        FrameRoleScope()
        {
            m_Threading.Initialize(FrameThreadMode::Multi);
            m_Tasks.Init(2);
        }

        ~FrameRoleScope()
        {
            m_Tasks.Shutdown();
            m_Threading.Shutdown();
        }

        FrameRoleScope(const FrameRoleScope&) = delete;
        FrameRoleScope& operator=(const FrameRoleScope&) = delete;

        TaskSystem& Tasks() { return m_Tasks; }

    private:
        FrameThreading m_Threading;
        TaskSystem m_Tasks;
    };

    /** A command pipe that unregisters itself even when a case fails part way through. */
    class RenderCommandPipeScope
    {
    public:
        RenderCommandPipeScope() { m_Pipe.Initialize(); }
        ~RenderCommandPipeScope() { m_Pipe.Shutdown(); }

        RenderCommandPipeScope(const RenderCommandPipeScope&) = delete;
        RenderCommandPipeScope& operator=(const RenderCommandPipeScope&) = delete;

        minEngine::RenderCommandPipe& Pipe() { return m_Pipe; }

    private:
        minEngine::RenderCommandPipe m_Pipe;
    };

    struct DestructionProbe
    {
        std::atomic<std::thread::id> ThreadOfDestruction{};
        std::atomic<bool> bDestroyed{ false };
    };

    /** A material (the live case for this channel) that records which thread destroyed it. */
    class ProbeMaterial final : public Material
    {
    public:
        std::shared_ptr<DestructionProbe> Probe;

        ~ProbeMaterial() override
        {
            if (Probe)
            {
                Probe->ThreadOfDestruction.store(std::this_thread::get_id(), std::memory_order_release);
                Probe->bDestroyed.store(true, std::memory_order_release);
            }
        }
    };
}

TEST_CASE("scene-retirement: a retired scene outlives the frame that may still reference it [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    FrameRoleScope frameRoles;
    SceneLifecycleScope lifecycle;

    std::shared_ptr<Scene> scene = minEngine::NewObject<Scene>("retire-gate");
    REQUIRE(scene != nullptr);
    scene->EnsureRenderScene();
    const std::weak_ptr<Scene> retired = scene;

    SceneManager::Get().RetireScene(std::move(scene));
    REQUIRE_FALSE(retired.expired());

    std::atomic<bool> aliveWhileRecording{ false };
    std::atomic<bool> aliveWhenReplayed{ false };
    frameRoles.Tasks().StartFrameRoles(
        [&](float) { aliveWhileRecording.store(!retired.expired(), std::memory_order_relaxed); },
        [&]() { aliveWhenReplayed.store(!retired.expired(), std::memory_order_relaxed); },
        /*combineGraphicsThreads=*/true);

    // The frame's completion signal: a submission that waits for its own RHI work.
    frameRoles.Tasks().SubmitRenderFrameTask(0.0f, /*waitForCompletion=*/true);

    CHECK(aliveWhileRecording.load(std::memory_order_relaxed));
    CHECK(aliveWhenReplayed.load(std::memory_order_relaxed));
    // Recorded and replayed, and still alive: only the game-role gate destroys it.
    CHECK_FALSE(retired.expired());

    SceneManager::Get().FlushRetiredScenes();
    CHECK(retired.expired());

    frameRoles.Tasks().StopFrameRoles();
}

TEST_CASE("scene-retirement: the gate refuses to destroy off the game role [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    FrameRoleScope frameRoles;
    SceneLifecycleScope lifecycle;

    std::shared_ptr<Scene> scene = minEngine::NewObject<Scene>("retire-off-role");
    REQUIRE(scene != nullptr);
    scene->EnsureRenderScene();
    const std::weak_ptr<Scene> retired = scene;
    SceneManager::Get().RetireScene(std::move(scene));

    std::thread renderRoleThread([&]() { SceneManager::Get().FlushRetiredScenes(); });
    renderRoleThread.join();
    CHECK_FALSE(retired.expired());

    SceneManager::Get().FlushRetiredScenes();
    CHECK(retired.expired());
}

TEST_CASE("scene-retirement: producers and the game-role drain do not race [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    SceneLifecycleScope lifecycle;

    constexpr int sceneCount = 64;
    std::vector<std::shared_ptr<Scene>> scenes;
    std::vector<std::weak_ptr<Scene>> retiredScenes;
    scenes.reserve(sceneCount);
    retiredScenes.reserve(sceneCount);
    for (int index = 0; index < sceneCount; ++index)
    {
        std::shared_ptr<Scene> scene =
            minEngine::NewObject<Scene>("retire-race-" + std::to_string(index));
        REQUIRE(scene != nullptr);
        scene->EnsureRenderScene();
        retiredScenes.push_back(scene);
        scenes.push_back(std::move(scene));
    }

    std::atomic<int> nextIndex{ 0 };
    std::vector<std::thread> producers;
    for (int producerIndex = 0; producerIndex < 4; ++producerIndex)
    {
        producers.emplace_back([&]() {
            for (;;)
            {
                const int index = nextIndex.fetch_add(1);
                if (index >= sceneCount)
                {
                    return;
                }
                // Each index is taken by exactly one producer, so no two threads touch the same
                // element of the vector the test filled in above.
                SceneManager::Get().RetireScene(std::move(scenes[static_cast<size_t>(index)]));
            }
        });
    }

    // The consumer keeps draining while the producers are still retiring scenes.
    while (nextIndex.load(std::memory_order_acquire) < sceneCount)
    {
        SceneManager::Get().FlushRetiredScenes();
        std::this_thread::yield();
    }

    for (std::thread& producer : producers)
    {
        producer.join();
    }
    SceneManager::Get().FlushRetiredScenes();

    for (const std::weak_ptr<Scene>& retiredScene : retiredScenes)
    {
        CHECK(retiredScene.expired());
    }
}

TEST_CASE("scene-retirement: a removal on the render role releases the render object, not the material [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    FrameRoleScope frameRoles;
    SceneLifecycleScope lifecycle;

    std::shared_ptr<Scene> scene = minEngine::NewObject<Scene>("removal-release");
    REQUIRE(scene != nullptr);
    scene->EnsureRenderScene();
    minEngine::RenderScene* renderScene = scene->GetRenderScene();
    REQUIRE(renderScene != nullptr);

    std::shared_ptr<minEngine::GameObject> gameObject = scene->CreateGameObject();
    REQUIRE(gameObject != nullptr);
    std::shared_ptr<StaticMeshComponent> component = gameObject->AddComponent<StaticMeshComponent>();
    REQUIRE(component != nullptr);
    component->SetActive(true);
    component->ReconcileEffectiveActivation();
    REQUIRE(component->IsActive());

    std::shared_ptr<DestructionProbe> probe = std::make_shared<DestructionProbe>();
    std::shared_ptr<Material> material = std::make_shared<ProbeMaterial>();
    static_cast<ProbeMaterial*>(material.get())->Probe = probe;
    component->SetMaterial(material);

    // Registering on the game role is what makes the render object reachable from the proxy.
    renderScene->AddPrimitive(component.get());

    const std::weak_ptr<Material> handedOver = material;
    material.reset();
    component->SetMaterial(nullptr);

    // The asset is released here, on the role that owned it, and its destructor tells the render
    // object that the tables it published from are gone. The scene proxy still holds the render
    // object, so it keeps drawing from the snapshot it published before the asset died.
    REQUIRE(handedOver.expired());
    CHECK(probe->bDestroyed.load(std::memory_order_acquire));
    CHECK(probe->ThreadOfDestruction.load(std::memory_order_acquire) == std::this_thread::get_id());

    std::atomic<std::thread::id> removalThread{};
    frameRoles.Tasks().StartFrameRoles(
        [&](float) {
            removalThread.store(std::this_thread::get_id());
            // A removal applied on the render role parks the proxy there, and the frame end - the
            // same role, one step later - is what drops the render object's reference.
            renderScene->RemovePrimitive(component.get());
            minEngine::RenderScene::FlushPendingProxyDeletes();
        },
        []() {},
        /*combineGraphicsThreads=*/true);

    frameRoles.Tasks().SubmitRenderFrameTask(0.0f, /*waitForCompletion=*/true);

    CHECK(removalThread.load() != std::this_thread::get_id());
    // Nothing in the object system happened off the game role: what the render role released is a
    // render object with no registry entry, so the probe still reports the game-role destruction
    // that ran above.
    CHECK(probe->bDestroyed.load(std::memory_order_acquire));
    CHECK(probe->ThreadOfDestruction.load(std::memory_order_acquire) == std::this_thread::get_id());

    frameRoles.Tasks().StopFrameRoles();
}

TEST_CASE("scene-retirement: PIE exit and scene unload hand the last reference to the gate [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    SceneLifecycleScope lifecycle;
    SceneManager& sceneManager = SceneManager::Get();

    std::shared_ptr<Scene> pieScene = minEngine::NewObject<Scene>("pie-exit-gate");
    REQUIRE(pieScene != nullptr);
    sceneManager.RegisterPIEScene(pieScene, 0);
    const std::weak_ptr<Scene> retiredPIEScene = pieScene;
    pieScene.reset();
    REQUIRE_FALSE(retiredPIEScene.expired());

    sceneManager.UnregisterPIEScene(0);
    // Ended, but not destroyed in place: the play world outlives the frame that may still draw it.
    CHECK_FALSE(retiredPIEScene.expired());
    sceneManager.FlushRetiredScenes();
    CHECK(retiredPIEScene.expired());

    std::shared_ptr<Scene> activeScene = sceneManager.CreateNewScene("unload-gate");
    REQUIRE(activeScene != nullptr);
    const std::weak_ptr<Scene> retiredActiveScene = activeScene;
    activeScene.reset();
    REQUIRE_FALSE(retiredActiveScene.expired());

    sceneManager.UnloadActiveScene();
    CHECK_FALSE(retiredActiveScene.expired());
    sceneManager.FlushRetiredScenes();
    CHECK(retiredActiveScene.expired());
}

TEST_CASE("scene-retirement: the frame-end proxy delete refuses to run off the render role [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    FrameRoleScope frameRoles;
    SceneLifecycleScope lifecycle;

    std::shared_ptr<Scene> scene = minEngine::NewObject<Scene>("proxy-delete-off-role");
    REQUIRE(scene != nullptr);
    scene->EnsureRenderScene();
    minEngine::RenderScene* renderScene = scene->GetRenderScene();
    REQUIRE(renderScene != nullptr);

    std::shared_ptr<minEngine::GameObject> gameObject = scene->CreateGameObject();
    REQUIRE(gameObject != nullptr);
    std::shared_ptr<StaticMeshComponent> component = gameObject->AddComponent<StaticMeshComponent>();
    REQUIRE(component != nullptr);
    component->SetActive(true);
    component->ReconcileEffectiveActivation();
    REQUIRE(component->IsActive());

    std::shared_ptr<Material> material = std::make_shared<Material>();
    // Weak only: once the asset is gone the scene proxy is the render object's last owner, so this
    // reference answers "was the proxy destroyed".
    const std::weak_ptr<minEngine::MaterialRenderProxy> renderObjectWeak = material->GetRenderProxyRef();
    component->SetMaterial(material);
    material.reset();

    // No frame pipe here, so both commands apply inline on this role: the proxy ends up parked.
    renderScene->AddPrimitive(component.get());
    REQUIRE(component->GetSceneProxy() != nullptr);

    // The asset goes and the proxy keeps the render object it published, so the weak reference is
    // still answered by the proxy from here on.
    component->SetMaterial(nullptr);
    REQUIRE_FALSE(renderObjectWeak.expired());
    renderScene->RemovePrimitive(component.get());
    CHECK(renderScene->m_PrimitiveSceneProxies.empty());
    CHECK_FALSE(renderObjectWeak.expired());

    // Game role: refused. A proxy is destroyed only where the frame that recorded against it ends.
    minEngine::RenderScene::FlushPendingProxyDeletes();
    CHECK_FALSE(renderObjectWeak.expired());
    CHECK(renderScene->m_ProxyDeleteCount == 0u);

    // Render role: the same call is the frame end, and the only place the proxy may die.
    frameRoles.Tasks().StartFrameRoles(
        [&](float) { minEngine::RenderScene::FlushPendingProxyDeletes(); },
        []() {},
        /*combineGraphicsThreads=*/true);
    frameRoles.Tasks().SubmitRenderFrameTask(0.0f, /*waitForCompletion=*/true);

    CHECK(renderObjectWeak.expired());
    CHECK(renderScene->m_ProxyDeleteCount == 1u);

    frameRoles.Tasks().StopFrameRoles();
    component->DetachSceneProxy();
}

TEST_CASE("scene-retirement: a retiring scene removes its primitives before it is destroyed [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    RenderCommandPipeScope pipeScope;
    SceneLifecycleScope lifecycle;
    minEngine::RenderCommandPipe& pipe = pipeScope.Pipe();

    std::shared_ptr<Scene> scene = minEngine::NewObject<Scene>("retire-removes-first");
    REQUIRE(scene != nullptr);
    scene->EnsureRenderScene();
    minEngine::RenderScene* renderScene = scene->GetRenderScene();
    REQUIRE(renderScene != nullptr);
    // Held past the Scene's death so the assertions below can still read it.
    const std::shared_ptr<minEngine::RenderScene> renderSceneRef = scene->GetRenderSceneShared();

    std::shared_ptr<minEngine::GameObject> gameObject = scene->CreateGameObject();
    REQUIRE(gameObject != nullptr);
    std::shared_ptr<StaticMeshComponent> component = gameObject->AddComponent<StaticMeshComponent>();
    REQUIRE(component != nullptr);
    component->SetActive(true);
    component->ReconcileEffectiveActivation();
    REQUIRE(component->IsActive());
    std::shared_ptr<Material> material = std::make_shared<Material>();
    component->SetMaterial(material);

    renderScene->AddPrimitive(component.get());
    pipe.PublishGameFrame(0u, 1u);
    REQUIRE(pipe.FlushApplyForFrameChecked(0));
    REQUIRE(renderScene->m_PrimitiveSceneProxies.size() == 1u);
    REQUIRE(component->GetSceneProxy() != nullptr);

    const std::weak_ptr<Scene> retiredScene = scene;
    SceneManager::Get().RetireScene(std::move(scene));
    REQUIRE_FALSE(retiredScene.expired());

    // Retiring queued a removal for the primitive and cleared the component's own handle, so the
    // teardown that follows cannot queue a second one. Nothing is freed yet: the removal is a
    // command the render role has not applied, which is why the proxy is still in the draw list.
    CHECK(component->GetSceneProxy() == nullptr);
    CHECK(renderScene->HasPendingProxyWork());
    CHECK(renderSceneRef->m_ProxyDeleteCount == 0u);

    // The next frame's apply takes it off the draw list and parks it; its frame end frees it.
    pipe.PublishGameFrame(1u, 2u);
    REQUIRE(pipe.FlushApplyForFrameChecked(1));
    CHECK(renderScene->m_PrimitiveSceneProxies.empty());
    CHECK(renderScene->HasPendingProxyWork());
    minEngine::RenderScene::FlushPendingProxyDeletes();
    CHECK_FALSE(renderScene->HasPendingProxyWork());
    CHECK(renderSceneRef->m_ProxyDeleteCount == 1u);
}

TEST_CASE("scene-retirement: PIE exit leaves no primitive behind for the scene destructor [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::LogSystem::Initialize();
    minEngine::LogConsoleStorage::Clear();

    RenderCommandPipeScope pipeScope;
    SceneLifecycleScope lifecycle;
    minEngine::RenderCommandPipe& pipe = pipeScope.Pipe();

    SceneManager& sceneManager = SceneManager::Get();

    std::shared_ptr<Scene> pieScene = minEngine::NewObject<Scene>("pie-exit-primitives");
    REQUIRE(pieScene != nullptr);
    pieScene->EnsureRenderScene();
    minEngine::RenderScene* renderScene = pieScene->GetRenderScene();
    REQUIRE(renderScene != nullptr);
    const std::shared_ptr<minEngine::RenderScene> renderSceneRef = pieScene->GetRenderSceneShared();

    std::shared_ptr<minEngine::GameObject> gameObject = pieScene->CreateGameObject();
    REQUIRE(gameObject != nullptr);
    std::shared_ptr<StaticMeshComponent> component = gameObject->AddComponent<StaticMeshComponent>();
    REQUIRE(component != nullptr);
    component->SetActive(true);
    component->ReconcileEffectiveActivation();
    REQUIRE(component->IsActive());
    std::shared_ptr<Material> material = std::make_shared<Material>();
    component->SetMaterial(material);

    renderScene->AddPrimitive(component.get());
    pipe.PublishGameFrame(0u, 1u);
    REQUIRE(pipe.FlushApplyForFrameChecked(0));
    REQUIRE(renderScene->m_PrimitiveSceneProxies.size() == 1u);

    sceneManager.RegisterPIEScene(pieScene, 0);
    const std::weak_ptr<Scene> retiredPIEScene = pieScene;
    pieScene.reset();
    REQUIRE_FALSE(retiredPIEScene.expired());

    sceneManager.UnregisterPIEScene(0);
    // Retired, not destroyed: the removal it queued has not been applied, so the proxy is still
    // registered. A flush may not destroy the scene in that state.
    CHECK_FALSE(retiredPIEScene.expired());
    CHECK(renderSceneRef->HasPendingProxyWork());
    sceneManager.FlushRetiredScenes();
    CHECK_FALSE(retiredPIEScene.expired());
    CHECK_FALSE(renderSceneRef->m_PrimitiveSceneProxies.empty());

    // The next frame applies the removal and its frame end frees the proxy.
    pipe.PublishGameFrame(1u, 2u);
    REQUIRE(pipe.FlushApplyForFrameChecked(1));
    CHECK(renderSceneRef->m_PrimitiveSceneProxies.empty());
    CHECK(renderSceneRef->HasPendingProxyWork());
    minEngine::RenderScene::FlushPendingProxyDeletes();
    CHECK_FALSE(renderSceneRef->HasPendingProxyWork());
    CHECK(renderSceneRef->m_ProxyDeleteCount == 1u);

    // Only now may the play world go, and it goes without reporting anything.
    sceneManager.FlushRetiredScenes();
    CHECK(retiredPIEScene.expired());

    bool reported = false;
    for (const minEngine::LogRecord& record : minEngine::LogConsoleStorage::Snapshot())
    {
        if (record.severity != minEngine::LogSeverity::Error)
        {
            continue;
        }
        if (std::string(record.GetChannelName()) != "Render")
        {
            continue;
        }
        if (record.message.find("still registered") != std::string::npos)
        {
            reported = true;
        }
    }
    CHECK_FALSE(reported);
}


TEST_CASE("scene-retirement: a scene released outside the gate hands its proxies to the frame end [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::LogSystem::Initialize();
    minEngine::LogConsoleStorage::Clear();

    RenderCommandPipeScope pipeScope;
    SceneLifecycleScope lifecycle;
    minEngine::RenderCommandPipe& pipe = pipeScope.Pipe();

    std::shared_ptr<Scene> scene = minEngine::NewObject<Scene>("released-by-owner");
    REQUIRE(scene != nullptr);
    scene->EnsureRenderScene();
    // Weak only: nothing outside the engine may keep the render side alive, so "still there after
    // the Scene went" is a question this test can ask.
    const std::weak_ptr<minEngine::RenderScene> renderSceneWeak = scene->GetRenderSceneShared();
    minEngine::RenderScene* const renderScene = scene->GetRenderScene();
    REQUIRE(renderScene != nullptr);

    std::shared_ptr<minEngine::GameObject> gameObject = scene->CreateGameObject();
    REQUIRE(gameObject != nullptr);
    std::shared_ptr<StaticMeshComponent> component = gameObject->AddComponent<StaticMeshComponent>();
    REQUIRE(component != nullptr);
    component->SetActive(true);
    component->ReconcileEffectiveActivation();
    REQUIRE(component->IsActive());
    std::shared_ptr<Material> material = std::make_shared<Material>();
    component->SetMaterial(material);

    renderScene->AddPrimitive(component.get());
    pipe.PublishGameFrame(0u, 1u);
    REQUIRE(pipe.FlushApplyForFrameChecked(0));
    REQUIRE(renderScene->m_PrimitiveSceneProxies.size() == 1u);

    // The path this test is about: an owner drops the last strong reference to the Scene instead of
    // retiring it. The Scene goes, its render side does not - the components queued their removals
    // as commands of this frame, and nothing may free a proxy before that frame ended.
    const std::weak_ptr<Scene> releasedScene = scene;
    scene.reset();
    CHECK(releasedScene.expired());
    CHECK_FALSE(renderSceneWeak.expired());
    CHECK(renderScene->HasPendingProxyWork());
    CHECK(renderScene->m_ProxyDeleteCount == 0u);
    CHECK(renderScene->m_PrimitiveSceneProxies.size() == 1u);

    // The next frame applies the removal, its frame end frees the proxy, and only then does the
    // render side go.
    pipe.PublishGameFrame(1u, 2u);
    REQUIRE(pipe.FlushApplyForFrameChecked(1));
    CHECK(renderScene->m_PrimitiveSceneProxies.empty());
    minEngine::RenderScene::FlushPendingProxyDeletes();
    CHECK(renderSceneWeak.expired());

    bool reported = false;
    for (const minEngine::LogRecord& record : minEngine::LogConsoleStorage::Snapshot())
    {
        if (record.severity != minEngine::LogSeverity::Error)
        {
            continue;
        }
        if (std::string(record.GetChannelName()) != "Render")
        {
            continue;
        }
        if (record.message.find("still registered") != std::string::npos)
        {
            reported = true;
        }
    }
    CHECK_FALSE(reported);
}

TEST_CASE("scene-retirement: shutdown frees the proxies of a scene no frame can drain [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    minEngine::LogSystem::Initialize();
    minEngine::LogConsoleStorage::Clear();

    RenderCommandPipeScope pipeScope;
    SceneLifecycleScope lifecycle;
    minEngine::RenderCommandPipe& pipe = pipeScope.Pipe();
    SceneManager& sceneManager = SceneManager::Get();

    std::shared_ptr<Scene> scene = sceneManager.CreateNewScene("shutdown-drain");
    REQUIRE(scene != nullptr);
    minEngine::RenderScene* renderScene = scene->GetRenderScene();
    REQUIRE(renderScene != nullptr);
    const std::shared_ptr<minEngine::RenderScene> renderSceneRef = scene->GetRenderSceneShared();

    std::shared_ptr<minEngine::GameObject> gameObject = scene->CreateGameObject();
    REQUIRE(gameObject != nullptr);
    std::shared_ptr<StaticMeshComponent> component = gameObject->AddComponent<StaticMeshComponent>();
    REQUIRE(component != nullptr);
    component->SetActive(true);
    component->ReconcileEffectiveActivation();
    REQUIRE(component->IsActive());
    std::shared_ptr<Material> material = std::make_shared<Material>();
    component->SetMaterial(material);

    renderScene->AddPrimitive(component.get());
    pipe.PublishGameFrame(0u, 1u);
    REQUIRE(pipe.FlushApplyForFrameChecked(0));
    REQUIRE(renderSceneRef->m_PrimitiveSceneProxies.size() == 1u);

    const std::weak_ptr<Scene> shutdownScene = scene;
    scene.reset();
    REQUIRE_FALSE(shutdownScene.expired());

    // Shutdown after the roles stopped: the render role is gone, so no drain will apply the
    // removal the teardown queued and no frame end will run the delete. Shutdown has to do both
    // itself, before it destroys the scene.
    minEngine::FrameThreading threading;
    threading.Initialize(minEngine::FrameThreadMode::Multi);

    sceneManager.Shutdown();

    threading.Shutdown();

    CHECK(shutdownScene.expired());
    CHECK_FALSE(renderSceneRef->HasPendingProxyWork());
    CHECK(renderSceneRef->m_ProxyDeleteCount == 1u);

    bool reported = false;
    for (const minEngine::LogRecord& record : minEngine::LogConsoleStorage::Snapshot())
    {
        if (record.severity != minEngine::LogSeverity::Error)
        {
            continue;
        }
        if (std::string(record.GetChannelName()) != "Render")
        {
            continue;
        }
        if (record.message.find("still registered") != std::string::npos)
        {
            reported = true;
        }
    }
    CHECK_FALSE(reported);
}

