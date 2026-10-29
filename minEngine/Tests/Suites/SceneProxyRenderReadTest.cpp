#include "SceneProxyRenderReadTest.h"

#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Math/Math.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Threading/RenderCommandPipe.h"
#include "Runtime/Function/Framework/Components/CanvasComponent.h"
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/Components/StaticMeshComponent.h"
#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/MaterialRenderProxy.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/PrimitiveSceneProxy.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RenderPipeline/RenderQueueBuild.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/SceneProxies/WidgetSceneProxy.h"
#include "Runtime/Function/Render/SceneRenderContext.h"
#include "Runtime/Function/Render/StaticMesh.h"

#include "EngineTestFixture.h"

#include "doctest.h"

#include <memory>
#include <vector>

namespace
{
    using minEngine::CanvasComponent;
    using minEngine::GameObject;
    using minEngine::Material;
    using minEngine::Math::Geometry::AABB;
    using minEngine::PrimitiveComponent;
    using minEngine::PrimitiveSceneProxy;
    using minEngine::RenderCommandPipe;
    using minEngine::RenderScene;
    using minEngine::SceneRenderContext;
    using minEngine::StaticMesh;
    using minEngine::StaticMeshComponent;
    using minEngine::Math::Vector2;
    using minEngine::Math::Vector3;
    using minEngine::WidgetComponent;
    using minEngine::WidgetSceneProxy;

    /**
     * Instrumentation for a component the game role destroyed while its proxy was still queued for
     * removal. The counters live outside the object on purpose: a query made after destruction has
     * to be observable even though the object that would have counted it is gone.
     */
    int s_BoundsQueryCount = 0;
    bool s_ProbeDestroyed = false;

    /**
     * A primitive whose box reads are counted, so "the render side never asked the component" is a
     * number instead of an absence of a crash.
     */
    class BoundsProbeComponent final : public PrimitiveComponent
    {
    public:
        ~BoundsProbeComponent() override { s_ProbeDestroyed = true; }

        AABB GetBoundingBox() const override
        {
            ++s_BoundsQueryCount;
            return AABB(Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 1.0f, 1.0f));
        }

        PrimitiveSceneProxy* CreateSceneProxy() override
        {
            PrimitiveSceneProxy* proxy = new PrimitiveSceneProxy();
            SyncSceneProxy(*proxy);
            m_SceneProxy = proxy;
            return proxy;
        }
    };

    class StubBuffer final : public minEngine::RHIBuffer
    {
    public:
        minEngine::RHIBufferCreateDesc& MutableDesc() { return m_Desc; }
        const minEngine::RHIBufferCreateDesc& GetDesc() const override { return m_Desc; }
        void UpdateSubresource(const void*, uint32_t, uint32_t) override {}

    private:
        minEngine::RHIBufferCreateDesc m_Desc;
    };

    class StubVertexInputLayout final : public minEngine::RHIVertexInputLayout
    {
    public:
        StubVertexInputLayout()
        {
            m_Elements.emplace_back("a_Position", minEngine::VertexElementType::Float3);
        }

        const std::vector<minEngine::RHIVertexElement>& GetElements() const override { return m_Elements; }
        uint32_t GetStride() const override { return 12u; }

    private:
        std::vector<minEngine::RHIVertexElement> m_Elements;
    };

    /** The registries a Scene destructor touches, plus a command pipe so queued work stays queued. */
    class RenderReadScope
    {
    public:
        RenderReadScope()
        {
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(&m_ObjectManager);
            minEngine::ObjectManager::Initialize();

            minEngine::Testing::TestAccess<minEngine::SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();

            m_Pipe.Initialize();
        }

        ~RenderReadScope()
        {
            m_Pipe.Shutdown();
            m_SceneManager.Shutdown();
            minEngine::Testing::TestAccess<minEngine::SceneManager>::SetInstance(nullptr);
            minEngine::ObjectManager::Shutdown();
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(nullptr);
        }

        RenderReadScope(const RenderReadScope&) = delete;
        RenderReadScope& operator=(const RenderReadScope&) = delete;

        RenderCommandPipe& Pipe() { return m_Pipe; }

    private:
        minEngine::ObjectManager m_ObjectManager;
        minEngine::SceneManager m_SceneManager;
        RenderCommandPipe m_Pipe;
    };

    /** A unit box whose geometry is as thin as a draw needs: a vertex buffer and a layout. */
    std::shared_ptr<StaticMesh> MakeUnitBoxMesh()
    {
        std::shared_ptr<StaticMesh> mesh = std::make_shared<StaticMesh>();
        mesh->m_BoundingBox = AABB(Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 1.0f, 1.0f));
        mesh->m_VertexBuffer = std::make_shared<StubBuffer>();
        mesh->m_VertexInputLayout = std::make_shared<StubVertexInputLayout>();
        mesh->m_IndexBuffer = std::make_shared<StubBuffer>();
        return mesh;
    }
}

TEST_CASE("scene-proxy-render-read: the render queue never asks a destroyed component for its bounds [smoke]")
{
    minEngine::EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());

    RenderReadScope scope;

    const std::shared_ptr<minEngine::Scene> scene = minEngine::NewObject<minEngine::Scene>("render-read-destroyed");
    REQUIRE(scene != nullptr);
    scene->EnsureRenderScene();
    RenderScene* renderScene = scene->GetRenderSceneShared().get();
    REQUIRE(renderScene != nullptr);

    const std::shared_ptr<GameObject> gameObject = scene->CreateGameObject();
    REQUIRE(gameObject != nullptr);

    s_BoundsQueryCount = 0;
    s_ProbeDestroyed = false;

    std::unique_ptr<BoundsProbeComponent> component(new BoundsProbeComponent());
    component->SetOwner(gameObject.get());
    component->SetPosition(Vector3(5.0f, 0.0f, 0.0f));

    PrimitiveSceneProxy* proxy = component->CreateSceneProxy();
    REQUIRE(proxy != nullptr);
    // The sync on the game role is the one place that may read the box.
    CHECK(s_BoundsQueryCount == 1);
    CHECK(proxy->ResolveWorldBounds().Min.x == doctest::Approx(4.0f));
    CHECK(proxy->ResolveWorldBounds().Max.x == doctest::Approx(6.0f));

    proxy->m_Enabled = true;
    const std::shared_ptr<Material> material = std::make_shared<Material>();
    proxy->SetMaterialSource(material);
    std::shared_ptr<StubBuffer> vertexBuffer = std::make_shared<StubBuffer>();
    std::shared_ptr<StubBuffer> indexBuffer = std::make_shared<StubBuffer>();
    std::shared_ptr<StubVertexInputLayout> layout = std::make_shared<StubVertexInputLayout>();
    proxy->SetMeshResources(vertexBuffer, layout, indexBuffer, 36u, 24u);

    renderScene->m_PrimitiveSceneProxies.push_back(proxy);

    // The world tears down: the component goes away on the game role while the proxy is still in
    // the render-side list, because the removal command only runs after this frame is recorded.
    component.reset();
    REQUIRE(s_ProbeDestroyed);
    CHECK(renderScene->m_PrimitiveSceneProxies.size() == 1u);

    s_BoundsQueryCount = 0;
    SceneRenderContext ctx;
    ctx.Scene = renderScene;
    minEngine::BuildPrimitiveRenderQueue(ctx, nullptr);

    // The whole point: building the queue resolved the cached box and never reached the component.
    CHECK(s_BoundsQueryCount == 0);
    REQUIRE(ctx.OpaqueQueue.size() == 1u);
    CHECK(ctx.OpaqueQueue[0].m_BoundingBox.Min.x == doctest::Approx(4.0f));
    CHECK(ctx.OpaqueQueue[0].m_BoundingBox.Max.x == doctest::Approx(6.0f));
    REQUIRE(ctx.ShadowCasterQueue.size() == 1u);

    // The queued removal then runs and takes the proxy out of the draw set through its own flag,
    // which is the only thing the queue builder reads to decide a proxy is gone.
    CHECK(scope.Pipe().FlushApplyForFrameChecked(0));
    CHECK(renderScene->m_PrimitiveSceneProxies.empty());
    CHECK(proxy->m_bPendingRemove);

    SceneRenderContext afterRemoval;
    afterRemoval.Scene = renderScene;
    minEngine::BuildPrimitiveRenderQueue(afterRemoval, nullptr);
    CHECK(afterRemoval.OpaqueQueue.empty());
    CHECK(s_BoundsQueryCount == 0);

    delete proxy;
}

TEST_CASE("scene-proxy-render-read: a transform update carries a recomputed world box [smoke]")
{
    minEngine::EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());

    RenderReadScope scope;

    const std::shared_ptr<minEngine::Scene> scene = minEngine::NewObject<minEngine::Scene>("render-read-transform");
    REQUIRE(scene != nullptr);
    scene->EnsureRenderScene();
    RenderScene* renderScene = scene->GetRenderSceneShared().get();
    REQUIRE(renderScene != nullptr);

    const std::shared_ptr<GameObject> gameObject = scene->CreateGameObject();
    REQUIRE(gameObject != nullptr);
    const std::shared_ptr<StaticMeshComponent> component = gameObject->AddComponent<StaticMeshComponent>();
    REQUIRE(component != nullptr);
    component->SetMesh(MakeUnitBoxMesh());
    component->SetMaterial(std::make_shared<Material>());
    component->SetPosition(Vector3(10.0f, 0.0f, 0.0f));

    PrimitiveSceneProxy* proxy = component->CreateSceneProxy();
    REQUIRE(proxy != nullptr);
    CHECK(proxy->ResolveWorldBounds().Min.x == doctest::Approx(9.0f));
    CHECK(proxy->ResolveWorldBounds().Max.x == doctest::Approx(11.0f));

    // Moving the component only publishes a transform packet; the box has to travel with it.
    component->SetPosition(Vector3(20.0f, 0.0f, 0.0f));
    renderScene->UpdatePrimitiveTransform(component.get());
    CHECK(proxy->ResolveWorldBounds().Min.x == doctest::Approx(9.0f));

    CHECK(scope.Pipe().FlushApplyForFrameChecked(0));
    CHECK(proxy->ResolveWorldBounds().Min.x == doctest::Approx(19.0f));
    CHECK(proxy->ResolveWorldBounds().Max.x == doctest::Approx(21.0f));

    SceneRenderContext ctx;
    ctx.Scene = renderScene;
    renderScene->m_PrimitiveSceneProxies.push_back(proxy);
    minEngine::BuildPrimitiveRenderQueue(ctx, nullptr);
    REQUIRE(ctx.OpaqueQueue.size() == 1u);
    CHECK(ctx.OpaqueQueue[0].m_BoundingBox.Min.x == doctest::Approx(19.0f));

    renderScene->m_PrimitiveSceneProxies.clear();
    component->DetachSceneProxy();
    delete proxy;
}

TEST_CASE("scene-proxy-render-read: the screen UI queue draws from the synced canvas snapshot [smoke]")
{
    minEngine::EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());

    RenderReadScope scope;

    const std::shared_ptr<minEngine::Scene> scene = minEngine::NewObject<minEngine::Scene>("render-read-screen-ui");
    REQUIRE(scene != nullptr);
    scene->EnsureRenderScene();
    RenderScene* renderScene = scene->GetRenderSceneShared().get();
    REQUIRE(renderScene != nullptr);

    const std::shared_ptr<GameObject> canvasGameObject = scene->CreateGameObject();
    REQUIRE(canvasGameObject != nullptr);
    canvasGameObject->AddComponent<minEngine::SceneComponent>();
    const std::shared_ptr<CanvasComponent> canvas = canvasGameObject->AddComponent<CanvasComponent>();
    REQUIRE(canvas != nullptr);
    canvas->SetScaleMode(minEngine::EUICanvasScaleMode::Letterbox);
    canvas->SetReferenceResolution(Vector2(1920.0f, 1080.0f));
    canvas->SetSortOrder(7);

    const std::shared_ptr<GameObject> widgetGameObject = scene->CreateGameObject();
    REQUIRE(widgetGameObject != nullptr);
    widgetGameObject->AddComponent<minEngine::SceneComponent>();
    const std::shared_ptr<WidgetComponent> widget = widgetGameObject->AddComponent<WidgetComponent>();
    REQUIRE(widget != nullptr);
    widget->SetStableOrder(3);
    widget->SetComputedRect(minEngine::UIRect::FromLTRB(100.0f, 50.0f, 300.0f, 150.0f));
    REQUIRE(widgetGameObject->AttachToParent(canvasGameObject.get(), minEngine::AttachmentTransformRules::KeepRelativeTransform));

    WidgetSceneProxy proxy;
    widget->UpdateSceneProxy(proxy);

    // What the components published is what the render side will use.
    CHECK(proxy.m_bHasActiveCanvas);
    CHECK(proxy.m_CanvasSortOrder == 7);
    CHECK(proxy.m_CanvasReferenceResolution.x == doctest::Approx(1920.0f));
    CHECK(proxy.m_CanvasReferenceResolution.y == doctest::Approx(1080.0f));

    // Stand in for what a live RHI would have handed over; the queue only copies those pointers.
    proxy.m_bVisible = true;
    proxy.SetMaterialSource(std::make_shared<Material>());
    proxy.m_VertexBuffer = reinterpret_cast<minEngine::RHIBuffer*>(static_cast<uintptr_t>(0x2000));
    proxy.m_VertexInputLayout = reinterpret_cast<minEngine::RHIVertexInputLayout*>(static_cast<uintptr_t>(0x3000));
    // The queue must not need the widget behind the proxy: it is null exactly like a component the
    // game role already destroyed.
    proxy.m_WidgetComponent = nullptr;

    renderScene->m_WidgetSceneProxies.push_back(&proxy);

    SceneRenderContext ctx;
    ctx.Scene = renderScene;
    minEngine::BuildScreenUIDrawQueue(ctx, 960u, 540u);
    REQUIRE(ctx.ScreenUIQueue.size() == 1u);
    const minEngine::UIDrawCommand& command = ctx.ScreenUIQueue[0];
    CHECK(command.CanvasSortOrder == 7);
    CHECK(command.StableOrder == 3u);
    // Letterbox maps 1920x1080 reference pixels onto a 960x540 viewport at half scale: the
    // reference rect (100,50)+(200x100) becomes (50,25)+(100x50), whose centre is (100,50).
    CHECK(command.Draw.m_ModelMatrix[3][0] == doctest::Approx(100.0f));
    CHECK(command.Draw.m_ModelMatrix[3][1] == doctest::Approx(50.0f));
    CHECK(command.Draw.m_ModelMatrix[0][0] == doctest::Approx(100.0f));
    CHECK(command.Draw.m_ModelMatrix[1][1] == doctest::Approx(-50.0f));

    // Changing the canvas reference resolution and re-syncing is what the queue follows; the canvas
    // component itself is never consulted while the queue is built.
    canvas->SetReferenceResolution(Vector2(960.0f, 540.0f));
    canvas->SetSortOrder(4);
    widget->UpdateSceneProxy(proxy);
    proxy.m_bVisible = true;
    proxy.SetMaterialSource(std::make_shared<Material>());
    proxy.m_VertexBuffer = reinterpret_cast<minEngine::RHIBuffer*>(static_cast<uintptr_t>(0x2000));
    proxy.m_VertexInputLayout = reinterpret_cast<minEngine::RHIVertexInputLayout*>(static_cast<uintptr_t>(0x3000));
    proxy.m_WidgetComponent = nullptr;

    SceneRenderContext updated;
    updated.Scene = renderScene;
    minEngine::BuildScreenUIDrawQueue(updated, 960u, 540u);
    REQUIRE(updated.ScreenUIQueue.size() == 1u);
    // A 960x540 canvas maps one to one: the same reference rect now reaches the queue unscaled,
    // so the centre is (200,100) and the quad keeps its authored size.
    CHECK(updated.ScreenUIQueue[0].CanvasSortOrder == 4);
    CHECK(updated.ScreenUIQueue[0].Draw.m_ModelMatrix[3][0] == doctest::Approx(200.0f));
    CHECK(updated.ScreenUIQueue[0].Draw.m_ModelMatrix[3][1] == doctest::Approx(100.0f));
    CHECK(updated.ScreenUIQueue[0].Draw.m_ModelMatrix[0][0] == doctest::Approx(200.0f));
    CHECK(updated.ScreenUIQueue[0].Draw.m_ModelMatrix[1][1] == doctest::Approx(-100.0f));

    renderScene->m_WidgetSceneProxies.clear();
    widget->DetachSceneProxy();
}
