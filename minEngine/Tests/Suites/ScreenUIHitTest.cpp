#include "ScreenUIHitTest.h"

#include "EngineTestFixture.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/CanvasComponent.h"
#include "Runtime/Function/Framework/Components/ImageComponent.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Render/ScreenUI/ScreenUICoords.h"
#include "Runtime/Function/UI/ScreenUIHitTester.h"
#include "Runtime/Function/UI/UILayoutPass.h"
#include "Runtime/Function/UI/UISystem.h"
#include "Runtime/Function/UI/UITypes.h"

#include "doctest.h"

namespace minEngine
{
    class ScreenUIHitTestScope
    {
    public:
        ScreenUIHitTestScope()
        {
            ObjectManager::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();
            SceneManager::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
        }

        ~ScreenUIHitTestScope()
        {
            m_SceneManager.Shutdown();
            SceneManager::SetInstance(nullptr);
            m_ObjectManager.Shutdown();
            ObjectManager::SetInstance(nullptr);
        }

        void InstallUISystem(UISystem& ui)
        {
            UISystem::SetInstance(&ui);
            ui.Initialize();
        }

        void UninstallUISystem(UISystem& ui)
        {
            ui.Shutdown();
            UISystem::SetInstance(nullptr);
        }

    private:
        ObjectManager m_ObjectManager;
        SceneManager m_SceneManager;
    };
}

TEST_CASE("screen-ui-hit: letterbox unmap rejects black bars [full]")
{
    using namespace minEngine;

    const ScreenUICoords::LetterboxMapping mapping =
        ScreenUICoords::MakeLetterboxMapping(1920.0f, 1080.0f, 1280.0f, 1000.0f);

    Vector2 refPoint{};
    CHECK_FALSE(mapping.TryUnmapPoint(Vector2(10.0f, 5.0f), 1920.0f, 1080.0f, refPoint));

    const Vector2 content = mapping.MapPoint(Vector2(100.0f, 50.0f));
    REQUIRE(mapping.TryUnmapPoint(content, 1920.0f, 1080.0f, refPoint));
    CHECK(refPoint.x == doctest::Approx(100.0f));
    CHECK(refPoint.y == doctest::Approx(50.0f));
}

TEST_CASE("screen-ui-hit: single canvas hits widget by computed rect [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIHitTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-hit-single");
    const std::shared_ptr<GameObject> canvasGo = scene->CreateGameObject();
    canvasGo->AddComponent<SceneComponent>();
    const std::shared_ptr<CanvasComponent> canvas = canvasGo->AddComponent<CanvasComponent>();
    canvas->SetReferenceResolution(Vector2(800.0f, 600.0f));

    const std::shared_ptr<GameObject> childGo = scene->CreateGameObject();
    childGo->AddComponent<SceneComponent>();
    const std::shared_ptr<WidgetComponent> widget = childGo->AddComponent<WidgetComponent>();
    childGo->AddComponent<ImageComponent>();
    widget->ApplyAnchorPreset(EUIAnchorPreset::TopLeft);
    widget->SetSize(Vector2(100.0f, 40.0f));
    widget->SetMargin(Vector4(20.0f, 30.0f, 0.0f, 0.0f));
    widget->SetStableOrder(1);
    REQUIRE(childGo->AttachToParent(canvasGo.get(), AttachmentTransformRules::KeepRelativeTransform));
    UILayoutPass::LayoutCanvas(*canvas);

    const ScreenUICoords::LetterboxMapping mapping =
        ScreenUICoords::MakeLetterboxMapping(800.0f, 600.0f, 800.0f, 600.0f);
    const Vector2 viewportPoint = mapping.MapPoint(Vector2(50.0f, 40.0f));

    ScreenUIHitQuery query;
    query.ViewportPoint = viewportPoint;
    query.ViewportWidth = 800.0f;
    query.ViewportHeight = 600.0f;
    query.Scene = scene.get();

    ScreenUIHitTester tester;
    const ScreenUIHitResult hit = tester.HitTestAt(query);
    CHECK(hit.bHit);
    CHECK(hit.Widget == widget.get());
    CHECK(hit.Canvas == canvas.get());
}

TEST_CASE("screen-ui-hit: higher stable order wins within canvas [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIHitTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-hit-order");
    const std::shared_ptr<GameObject> canvasGo = scene->CreateGameObject();
    canvasGo->AddComponent<SceneComponent>();
    const std::shared_ptr<CanvasComponent> canvas = canvasGo->AddComponent<CanvasComponent>();
    canvas->SetReferenceResolution(Vector2(400.0f, 300.0f));

    auto addWidget = [&](uint32_t order) -> WidgetComponent*
    {
        const std::shared_ptr<GameObject> go = scene->CreateGameObject();
        go->AddComponent<SceneComponent>();
        const std::shared_ptr<WidgetComponent> widget = go->AddComponent<WidgetComponent>();
        go->AddComponent<ImageComponent>();
        widget->ApplyAnchorPreset(EUIAnchorPreset::TopLeft);
        widget->SetSize(Vector2(100.0f, 100.0f));
        widget->SetMargin(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
        widget->SetStableOrder(order);
        REQUIRE(go->AttachToParent(canvasGo.get(), AttachmentTransformRules::KeepRelativeTransform));
        return widget.get();
    };

    WidgetComponent* back = addWidget(1);
    WidgetComponent* front = addWidget(5);
    UILayoutPass::LayoutCanvas(*canvas);

    ScreenUIHitQuery query;
    query.ViewportPoint = Vector2(10.0f, 10.0f);
    query.ViewportWidth = 400.0f;
    query.ViewportHeight = 300.0f;
    query.Scene = scene.get();

    ScreenUIHitTester tester;
    const ScreenUIHitResult hit = tester.HitTestAt(query);
    CHECK(hit.bHit);
    CHECK(hit.Widget == front);
    CHECK(hit.Widget != back);
}

TEST_CASE("screen-ui-hit: higher canvas sort order wins [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIHitTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-hit-canvas");

    auto makeCanvas = [&](int32_t sortOrder, const Vector2& margin) -> WidgetComponent*
    {
        const std::shared_ptr<GameObject> canvasGo = scene->CreateGameObject();
        canvasGo->AddComponent<SceneComponent>();
        const std::shared_ptr<CanvasComponent> canvas = canvasGo->AddComponent<CanvasComponent>();
        canvas->SetReferenceResolution(Vector2(200.0f, 200.0f));
        canvas->SetSortOrder(sortOrder);

        const std::shared_ptr<GameObject> go = scene->CreateGameObject();
        go->AddComponent<SceneComponent>();
        const std::shared_ptr<WidgetComponent> widget = go->AddComponent<WidgetComponent>();
        go->AddComponent<ImageComponent>();
        widget->ApplyAnchorPreset(EUIAnchorPreset::TopLeft);
        widget->SetSize(Vector2(80.0f, 80.0f));
        widget->SetMargin(Vector4(margin.x, margin.y, 0.0f, 0.0f));
        widget->SetStableOrder(1);
        REQUIRE(go->AttachToParent(canvasGo.get(), AttachmentTransformRules::KeepRelativeTransform));
        UILayoutPass::LayoutCanvas(*canvas);
        return widget.get();
    };

    WidgetComponent* low = makeCanvas(0, Vector2(10.0f, 10.0f));
    WidgetComponent* high = makeCanvas(10, Vector2(10.0f, 10.0f));

    ScreenUIHitQuery query;
    query.ViewportPoint = Vector2(20.0f, 20.0f);
    query.ViewportWidth = 200.0f;
    query.ViewportHeight = 200.0f;
    query.Scene = scene.get();

    ScreenUIHitTester tester;
    const ScreenUIHitResult hit = tester.HitTestAt(query);
    CHECK(hit.bHit);
    CHECK(hit.Widget == high);
    CHECK(hit.Widget != low);
}

TEST_CASE("screen-ui-hit: hit-test invisible skips widget [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIHitTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-hit-invisible");
    const std::shared_ptr<GameObject> canvasGo = scene->CreateGameObject();
    canvasGo->AddComponent<SceneComponent>();
    const std::shared_ptr<CanvasComponent> canvas = canvasGo->AddComponent<CanvasComponent>();
    canvas->SetReferenceResolution(Vector2(200.0f, 200.0f));

    const std::shared_ptr<GameObject> go = scene->CreateGameObject();
    go->AddComponent<SceneComponent>();
    const std::shared_ptr<WidgetComponent> widget = go->AddComponent<WidgetComponent>();
    go->AddComponent<ImageComponent>();
    widget->ApplyAnchorPreset(EUIAnchorPreset::TopLeft);
    widget->SetSize(Vector2(100.0f, 100.0f));
    widget->SetHitTestVisible(false);
    REQUIRE(go->AttachToParent(canvasGo.get(), AttachmentTransformRules::KeepRelativeTransform));
    UILayoutPass::LayoutCanvas(*canvas);

    ScreenUIHitQuery query;
    query.ViewportPoint = Vector2(10.0f, 10.0f);
    query.ViewportWidth = 200.0f;
    query.ViewportHeight = 200.0f;
    query.Scene = scene.get();

    ScreenUIHitTester tester;
    const ScreenUIHitResult hit = tester.HitTestAt(query);
    CHECK_FALSE(hit.bHit);
}

TEST_CASE("screen-ui-hit: ShouldBlockWorldPointer follows routing and hover [full]")
{
    using namespace minEngine;
    ScreenUIHitTestScope scope;

    UISystem ui;
    scope.InstallUISystem(ui);

    CHECK_FALSE(ui.ShouldBlockWorldPointer());
    ui.SetPointerRoutingEnabled(true);
    CHECK_FALSE(ui.ShouldBlockWorldPointer());

    ui.SetPointerRoutingEnabled(false);
    CHECK_FALSE(ui.ShouldBlockWorldPointer());

    scope.UninstallUISystem(ui);
}