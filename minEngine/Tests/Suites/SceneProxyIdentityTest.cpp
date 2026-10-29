#include "SceneProxyIdentityTest.h"

#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Threading/RenderCommandPipe.h"
#include "Runtime/Function/Framework/Components/PointLightComponent.h"
#include "Runtime/Function/Framework/Components/SkyBoxComponent.h"
#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Render/LightSceneProxies/LightSceneProxy.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/SceneProxies/WidgetSceneProxy.h"
#include "Runtime/Function/Render/SkyBoxSceneProxies/SkyBoxSceneProxy.h"

#include "doctest.h"

#include <memory>

namespace
{
    using minEngine::Component;
    using minEngine::GameObject;
    using minEngine::LightSceneProxy;
    using minEngine::PointLightComponent;
    using minEngine::RenderCommandPipe;
    using minEngine::RenderScene;
    using minEngine::Scene;
    using minEngine::SkyBoxComponent;
    using minEngine::SkyBoxSceneProxy;
    using minEngine::WidgetComponent;
    using minEngine::WidgetSceneProxy;

    /** The registries a component tree needs, plus the command pipe a removal travels through. */
    class IdentityScope
    {
    public:
        IdentityScope()
        {
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(&m_ObjectManager);
            minEngine::ObjectManager::Initialize();

            minEngine::Testing::TestAccess<minEngine::SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();

            m_Pipe.Initialize();
        }

        ~IdentityScope()
        {
            m_Pipe.Shutdown();
            m_SceneManager.Shutdown();
            minEngine::Testing::TestAccess<minEngine::SceneManager>::SetInstance(nullptr);
            minEngine::ObjectManager::Shutdown();
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(nullptr);
        }

        IdentityScope(const IdentityScope&) = delete;
        IdentityScope& operator=(const IdentityScope&) = delete;

        RenderCommandPipe& Pipe() { return m_Pipe; }

    private:
        minEngine::ObjectManager m_ObjectManager;
        minEngine::SceneManager m_SceneManager;
        minEngine::RenderCommandPipe m_Pipe;
    };

    /**
     * The identity token of a scene proxy, read through a member pointer.
     *
     * A teardown path either writes that token or leaves it alone, so the value read before and
     * after the path ran is the probe: the removal commands match on this pointer, and nothing in
     * these scenes writes it but proxy creation and those commands.
     */
    template <typename TProxy, typename TToken>
    TToken ReadToken(const TProxy& proxy, TToken TProxy::* token)
    {
        return proxy.*token;
    }

    /**
     * A world whose components own render proxies on a render scene they cannot reach.
     *
     * The Scene never ensures a RenderScene of its own, so a component of it answers "no render
     * scene" exactly like one whose Scene has already been reset: it has a proxy on the render
     * side and no handle left to queue a removal of its own.
     */
    struct UnreachableWorld
    {
        std::shared_ptr<Scene> World;
        std::shared_ptr<GameObject> Owner;
    };

    UnreachableWorld MakeUnreachableWorld(const char* name)
    {
        UnreachableWorld made;
        made.World = minEngine::NewObject<Scene>(name);
        REQUIRE(made.World != nullptr);
        made.Owner = made.World->CreateGameObject();
        REQUIRE(made.Owner != nullptr);
        return made;
    }

    /**
     * Destroys a component without letting a deactivation run first.
     *
     * The proxy is registered while the component was never activated, so dropping its owner does
     * not reach RemoveActivationFromSystems: what remains is the destructor's own path, with the
     * proxy link still set and no render scene behind it.
     */
    template <typename TComponent>
    void DestroyWithoutDeactivation(const std::shared_ptr<GameObject>& owner, std::shared_ptr<TComponent>& component)
    {
        REQUIRE(owner->RemoveComponent(*component));
        component.reset();
    }
}

TEST_CASE("scene-proxy-identity: a light removal still matches after the component let go [smoke]")
{
    IdentityScope scope;
    RenderScene renderScene;

    UnreachableWorld world = MakeUnreachableWorld("identity-light");
    std::shared_ptr<PointLightComponent> component = world.Owner->AddComponent<PointLightComponent>();
    REQUIRE(component != nullptr);
    component->ReconcileEffectiveActivation();
    PointLightComponent* const rawComponent = component.get();
    REQUIRE(rawComponent->GetOwningRenderSceneIfPresent() == nullptr);

    renderScene.AddLight(rawComponent);
    scope.Pipe().PublishGameFrame(0u, 1u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(0));
    LightSceneProxy* const proxy = component->GetSceneProxy();
    REQUIRE(proxy != nullptr);
    REQUIRE(renderScene.m_PointLightSceneProxies.size() == 1u);
    REQUIRE(renderScene.HasPendingProxyWork());

    // The component leaves the render systems while it has no render scene handle: the only thing
    // tying the proxy to it is the token below, and the scene's removal matches on it.
    component->SetActive(false);
    CHECK(ReadToken(*proxy, &LightSceneProxy::m_LightComponent) == rawComponent);
    CHECK_FALSE(proxy->m_bPendingRemove);

    renderScene.RemoveLight(rawComponent);
    scope.Pipe().PublishGameFrame(1u, 2u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(1));

    // Matched: off the draw list, marked, and still owned by the scene until the frame ends.
    CHECK(renderScene.m_PointLightSceneProxies.empty());
    CHECK(proxy->m_bPendingRemove);
    CHECK(renderScene.m_ProxyDeleteCount == 0u);
    CHECK(renderScene.HasPendingProxyWork());

    minEngine::RenderScene::FlushPendingProxyDeletes();
    CHECK(renderScene.m_ProxyDeleteCount == 1u);
    CHECK_FALSE(renderScene.HasPendingProxyWork());
}

TEST_CASE("scene-proxy-identity: a light destroyed without a render scene keeps its token [smoke]")
{
    IdentityScope scope;
    RenderScene renderScene;

    UnreachableWorld world = MakeUnreachableWorld("identity-light-destructor");
    std::shared_ptr<PointLightComponent> component = world.Owner->AddComponent<PointLightComponent>();
    REQUIRE(component != nullptr);
    PointLightComponent* const rawComponent = component.get();
    REQUIRE(rawComponent->GetOwningRenderSceneIfPresent() == nullptr);

    renderScene.AddLight(rawComponent);
    scope.Pipe().PublishGameFrame(0u, 1u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(0));
    LightSceneProxy* const proxy = component->GetSceneProxy();
    REQUIRE(proxy != nullptr);

    // The removal is what the scene still owes this light; the component then goes away without
    // ever reaching a removal path of its own.
    renderScene.RemoveLight(rawComponent);
    DestroyWithoutDeactivation(world.Owner, component);
    CHECK(ReadToken(*proxy, &LightSceneProxy::m_LightComponent) == rawComponent);

    scope.Pipe().PublishGameFrame(1u, 2u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(1));
    CHECK(renderScene.m_PointLightSceneProxies.empty());
    CHECK(proxy->m_bPendingRemove);
    CHECK(renderScene.m_ProxyDeleteCount == 0u);

    minEngine::RenderScene::FlushPendingProxyDeletes();
    CHECK(renderScene.m_ProxyDeleteCount == 1u);
    CHECK_FALSE(renderScene.HasPendingProxyWork());
}

TEST_CASE("scene-proxy-identity: a widget removal still matches after the component let go [smoke]")
{
    IdentityScope scope;
    RenderScene renderScene;

    UnreachableWorld world = MakeUnreachableWorld("identity-widget");
    std::shared_ptr<WidgetComponent> component = world.Owner->AddComponent<WidgetComponent>();
    REQUIRE(component != nullptr);
    component->ReconcileEffectiveActivation();
    WidgetComponent* const rawComponent = component.get();
    REQUIRE(rawComponent->GetOwningRenderSceneIfPresent() == nullptr);

    renderScene.AddWidget(rawComponent);
    scope.Pipe().PublishGameFrame(0u, 1u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(0));
    WidgetSceneProxy* const proxy = component->GetSceneProxy();
    REQUIRE(proxy != nullptr);
    REQUIRE(renderScene.m_WidgetSceneProxies.size() == 1u);
    REQUIRE(renderScene.HasPendingProxyWork());

    component->SetActive(false);
    CHECK(ReadToken(*proxy, &WidgetSceneProxy::m_WidgetComponent) == rawComponent);
    CHECK_FALSE(proxy->m_bPendingRemove);

    renderScene.RemoveWidget(rawComponent);
    scope.Pipe().PublishGameFrame(1u, 2u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(1));

    CHECK(renderScene.m_WidgetSceneProxies.empty());
    CHECK(proxy->m_bPendingRemove);
    CHECK(renderScene.m_ProxyDeleteCount == 0u);
    CHECK(renderScene.HasPendingProxyWork());

    minEngine::RenderScene::FlushPendingProxyDeletes();
    CHECK(renderScene.m_ProxyDeleteCount == 1u);
    CHECK_FALSE(renderScene.HasPendingProxyWork());
}

TEST_CASE("scene-proxy-identity: a widget destroyed without a render scene keeps its token [smoke]")
{
    IdentityScope scope;
    RenderScene renderScene;

    UnreachableWorld world = MakeUnreachableWorld("identity-widget-destructor");
    std::shared_ptr<WidgetComponent> component = world.Owner->AddComponent<WidgetComponent>();
    REQUIRE(component != nullptr);
    WidgetComponent* const rawComponent = component.get();
    REQUIRE(rawComponent->GetOwningRenderSceneIfPresent() == nullptr);

    renderScene.AddWidget(rawComponent);
    scope.Pipe().PublishGameFrame(0u, 1u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(0));
    WidgetSceneProxy* const proxy = component->GetSceneProxy();
    REQUIRE(proxy != nullptr);

    // The destructor is the only path left that could clear the token, and it may not: the removal
    // this widget is still owed - a scene teardown, an editor delete - matches on it.
    DestroyWithoutDeactivation(world.Owner, component);
    CHECK(ReadToken(*proxy, &WidgetSceneProxy::m_WidgetComponent) == rawComponent);

    renderScene.RemoveWidget(rawComponent);
    scope.Pipe().PublishGameFrame(1u, 2u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(1));
    CHECK(renderScene.m_WidgetSceneProxies.empty());
    CHECK(proxy->m_bPendingRemove);
    CHECK(renderScene.m_ProxyDeleteCount == 0u);

    minEngine::RenderScene::FlushPendingProxyDeletes();
    CHECK(renderScene.m_ProxyDeleteCount == 1u);
    CHECK_FALSE(renderScene.HasPendingProxyWork());
}

TEST_CASE("scene-proxy-identity: a skybox removal still matches after the component let go [smoke]")
{
    IdentityScope scope;
    RenderScene renderScene;

    UnreachableWorld world = MakeUnreachableWorld("identity-skybox");
    // Built through the class factory: the skybox component is not part of the engine's exported
    // surface, so the test never instantiates it itself.
    std::shared_ptr<Component> component = world.Owner->AddComponent("minEngine::SkyBoxComponent");
    REQUIRE(component != nullptr);
    component->ReconcileEffectiveActivation();
    SkyBoxComponent* const rawComponent = static_cast<SkyBoxComponent*>(component.get());
    REQUIRE(rawComponent->GetOwningRenderSceneIfPresent() == nullptr);

    renderScene.SetSkyBox(rawComponent);
    scope.Pipe().PublishGameFrame(0u, 1u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(0));
    SkyBoxSceneProxy* const proxy = rawComponent->GetSceneProxy();
    REQUIRE(proxy != nullptr);
    REQUIRE(renderScene.GetSkyBoxProxy() == proxy);

    component->SetActive(false);
    CHECK(ReadToken(*proxy, &SkyBoxSceneProxy::m_SkyBoxComponent) == rawComponent);
    CHECK_FALSE(proxy->m_bPendingRemove);

    renderScene.RemoveSkyBox(rawComponent);
    scope.Pipe().PublishGameFrame(1u, 2u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(1));

    CHECK(renderScene.GetSkyBoxProxy() == nullptr);
    CHECK(proxy->m_bPendingRemove);
    CHECK(renderScene.m_ProxyDeleteCount == 0u);
    CHECK(renderScene.HasPendingProxyWork());

    minEngine::RenderScene::FlushPendingProxyDeletes();
    CHECK(renderScene.m_ProxyDeleteCount == 1u);
    CHECK_FALSE(renderScene.HasPendingProxyWork());
}

TEST_CASE("scene-proxy-identity: a skybox destroyed without a render scene keeps its token [smoke]")
{
    IdentityScope scope;
    RenderScene renderScene;

    UnreachableWorld world = MakeUnreachableWorld("identity-skybox-destructor");
    std::shared_ptr<Component> component = world.Owner->AddComponent("minEngine::SkyBoxComponent");
    REQUIRE(component != nullptr);
    SkyBoxComponent* const rawComponent = static_cast<SkyBoxComponent*>(component.get());
    REQUIRE(rawComponent->GetOwningRenderSceneIfPresent() == nullptr);

    renderScene.SetSkyBox(rawComponent);
    scope.Pipe().PublishGameFrame(0u, 1u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(0));
    SkyBoxSceneProxy* const proxy = rawComponent->GetSceneProxy();
    REQUIRE(proxy != nullptr);

    DestroyWithoutDeactivation(world.Owner, component);
    CHECK(ReadToken(*proxy, &SkyBoxSceneProxy::m_SkyBoxComponent) == rawComponent);

    renderScene.RemoveSkyBox(rawComponent);
    scope.Pipe().PublishGameFrame(1u, 2u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(1));
    CHECK(renderScene.GetSkyBoxProxy() == nullptr);
    CHECK(proxy->m_bPendingRemove);
    CHECK(renderScene.m_ProxyDeleteCount == 0u);

    minEngine::RenderScene::FlushPendingProxyDeletes();
    CHECK(renderScene.m_ProxyDeleteCount == 1u);
    CHECK_FALSE(renderScene.HasPendingProxyWork());
}

TEST_CASE("scene-proxy-identity: the probe sees the token write that strands a proxy [smoke]")
{
    // The reverse control: the same scenes, with the game-role write the teardown paths used to
    // make put back by hand. It proves the probe above measures something - the token really
    // moves - and that a moved token is what leaves a removal matching nothing.
    IdentityScope scope;
    RenderScene lightScene;
    RenderScene widgetScene;
    RenderScene skyBoxScene;

    UnreachableWorld lightWorld = MakeUnreachableWorld("identity-reverse-light");
    std::shared_ptr<PointLightComponent> light = lightWorld.Owner->AddComponent<PointLightComponent>();
    REQUIRE(light != nullptr);
    lightScene.AddLight(light.get());
    scope.Pipe().PublishGameFrame(0u, 1u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(0));
    LightSceneProxy* const lightProxy = light->GetSceneProxy();
    REQUIRE(lightProxy != nullptr);

    UnreachableWorld widgetWorld = MakeUnreachableWorld("identity-reverse-widget");
    std::shared_ptr<WidgetComponent> widget = widgetWorld.Owner->AddComponent<WidgetComponent>();
    REQUIRE(widget != nullptr);
    widgetScene.AddWidget(widget.get());
    scope.Pipe().PublishGameFrame(1u, 2u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(1));
    WidgetSceneProxy* const widgetProxy = widget->GetSceneProxy();
    REQUIRE(widgetProxy != nullptr);

    UnreachableWorld skyBoxWorld = MakeUnreachableWorld("identity-reverse-skybox");
    std::shared_ptr<Component> skyBoxComponent = skyBoxWorld.Owner->AddComponent("minEngine::SkyBoxComponent");
    REQUIRE(skyBoxComponent != nullptr);
    SkyBoxComponent* const rawSkyBox = static_cast<SkyBoxComponent*>(skyBoxComponent.get());
    skyBoxScene.SetSkyBox(rawSkyBox);
    scope.Pipe().PublishGameFrame(2u, 3u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(2));
    SkyBoxSceneProxy* const skyBoxProxy = rawSkyBox->GetSceneProxy();
    REQUIRE(skyBoxProxy != nullptr);

    // The old game-role fallback, written where a teardown path used to write it.
    lightProxy->m_LightComponent = nullptr;
    widgetProxy->m_WidgetComponent = nullptr;
    skyBoxProxy->m_SkyBoxComponent = nullptr;

    CHECK(ReadToken(*lightProxy, &LightSceneProxy::m_LightComponent) != light.get());
    CHECK(ReadToken(*widgetProxy, &WidgetSceneProxy::m_WidgetComponent) != widget.get());
    CHECK(ReadToken(*skyBoxProxy, &SkyBoxSceneProxy::m_SkyBoxComponent) != rawSkyBox);

    lightScene.RemoveLight(light.get());
    widgetScene.RemoveWidget(widget.get());
    skyBoxScene.RemoveSkyBox(rawSkyBox);
    scope.Pipe().PublishGameFrame(3u, 4u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(3));

    // Nothing matched: every proxy is still listed and still owned, which is the strand this suite
    // is about - the light keeps contributing, the widget keeps drawing, the sky stays installed.
    CHECK(lightScene.m_PointLightSceneProxies.size() == 1u);
    CHECK(widgetScene.m_WidgetSceneProxies.size() == 1u);
    CHECK(skyBoxScene.GetSkyBoxProxy() == skyBoxProxy);
    CHECK(lightScene.HasPendingProxyWork());
    CHECK(widgetScene.HasPendingProxyWork());
    CHECK(skyBoxScene.HasPendingProxyWork());
    CHECK(lightScene.m_ProxyDeleteCount == 0u);
    CHECK(widgetScene.m_ProxyDeleteCount == 0u);
    CHECK(skyBoxScene.m_ProxyDeleteCount == 0u);
    CHECK_FALSE(lightProxy->m_bPendingRemove);
    CHECK_FALSE(widgetProxy->m_bPendingRemove);
    CHECK_FALSE(skyBoxProxy->m_bPendingRemove);

    // Putting the token back - which is what the teardown paths now guarantee - lets the very same
    // removal match, so these scenes end clean instead of stranding their proxies.
    lightProxy->m_LightComponent = light.get();
    widgetProxy->m_WidgetComponent = widget.get();
    skyBoxProxy->m_SkyBoxComponent = rawSkyBox;

    lightScene.RemoveLight(light.get());
    widgetScene.RemoveWidget(widget.get());
    skyBoxScene.RemoveSkyBox(rawSkyBox);
    scope.Pipe().PublishGameFrame(4u, 5u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(4));
    CHECK(lightScene.m_PointLightSceneProxies.empty());
    CHECK(widgetScene.m_WidgetSceneProxies.empty());
    CHECK(skyBoxScene.GetSkyBoxProxy() == nullptr);
    CHECK(lightProxy->m_bPendingRemove);
    CHECK(widgetProxy->m_bPendingRemove);
    CHECK(skyBoxProxy->m_bPendingRemove);

    minEngine::RenderScene::FlushPendingProxyDeletes();
    CHECK_FALSE(lightScene.HasPendingProxyWork());
    CHECK_FALSE(widgetScene.HasPendingProxyWork());
    CHECK_FALSE(skyBoxScene.HasPendingProxyWork());
}
