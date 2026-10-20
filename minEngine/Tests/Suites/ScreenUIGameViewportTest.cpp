#include "ScreenUIGameViewportTest.h"

#include "EngineTestFixture.h"

#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/CanvasComponent.h"
#include "Runtime/Function/Framework/Components/ImageComponent.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Render/ScreenUI/ScreenUICoords.h"
#include "Runtime/Function/UI/ScreenUIHitTypes.h"
#include "Runtime/Function/UI/UILayoutPass.h"
#include "Runtime/Function/UI/UISystem.h"
#include "Runtime/Function/UI/UITypes.h"

#include "doctest.h"

namespace minEngine
{
    /**
     * Headless host for the game-side read API. No platform window exists here, so the viewport
     * surface is injected exactly the way a host that renders ScreenUI into its own sub-rect does.
     */
    class ScreenUIGameViewportTestScope
    {
    public:
        ScreenUIGameViewportTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            ObjectManager::Initialize();
            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
            UISystem::SetInstance(&m_UISystem);
            m_UISystem.Initialize();
        }

        ~ScreenUIGameViewportTestScope()
        {
            m_UISystem.Shutdown();
            UISystem::SetInstance(nullptr);
            m_SceneManager.Shutdown();
            Testing::TestAccess<SceneManager>::SetInstance(nullptr);
            ObjectManager::Shutdown();
            Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
        }

        UISystem& GetUISystem() { return m_UISystem; }

        /** One pointer frame; the reference px is the one ScreenUI events must carry. */
        void Frame(
            Scene* scene,
            WidgetComponent* hitWidget,
            const Vector2& referencePx,
            bool pressed = false,
            bool down = false,
            bool released = false,
            bool bHaveReferencePx = true)
        {
            ScreenUIHitResult hit;
            hit.Reset();
            if (hitWidget != nullptr)
            {
                hit.bHit = true;
                hit.Widget = hitWidget;
                hit.GameObject = hitWidget->GetOwner();
                hit.PointInReferenceSpace = referencePx;
            }

            m_UISystem.UpdatePointerFrame(
                scene,
                hit,
                referencePx,
                bHaveReferencePx,
                EUIPointerButton::Left,
                pressed,
                down,
                released);
        }

    private:
        ObjectManager m_ObjectManager;
        SceneManager m_SceneManager;
        UISystem m_UISystem;
    };

    namespace
    {
        void InjectViewport(UISystem& ui, float widthWindowPx, float heightWindowPx)
        {
            ScreenUIPointerViewportContext context;
            context.ImageMin = Vector2(0.0f, 0.0f);
            context.ImageSize = Vector2(widthWindowPx, heightWindowPx);
            context.bValid = widthWindowPx > 0.0f && heightWindowPx > 0.0f;
            ui.SetPointerViewportContext(context);
        }

        std::shared_ptr<CanvasComponent> MakeCanvas(
            Scene& scene,
            const Vector2& referenceResolution,
            std::shared_ptr<GameObject>& outCanvasGo)
        {
            outCanvasGo = scene.CreateGameObject();
            outCanvasGo->AddComponent<SceneComponent>();
            const std::shared_ptr<CanvasComponent> canvas = outCanvasGo->AddComponent<CanvasComponent>();
            canvas->SetReferenceResolution(referenceResolution);
            return canvas;
        }

        std::shared_ptr<WidgetComponent> MakeWidget(Scene& scene, GameObject& canvasGo)
        {
            const std::shared_ptr<GameObject> go = scene.CreateGameObject();
            go->AddComponent<SceneComponent>();
            const std::shared_ptr<WidgetComponent> widget = go->AddComponent<WidgetComponent>();
            go->AddComponent<ImageComponent>();
            widget->ApplyAnchorPreset(EUIAnchorPreset::TopLeft);
            widget->SetSize(Vector2(40.0f, 40.0f));
            widget->SetMargin(Vector4(10.0f, 10.0f, 0.0f, 0.0f));
            REQUIRE(go->AttachToParent(&canvasGo, AttachmentTransformRules::KeepRelativeTransform));
            return widget;
        }
    }
}

TEST_CASE("screen-ui-game-viewport: viewport window px maps a canvas reference resolution [smoke]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIGameViewportTestScope scope;
    UISystem& ui = scope.GetUISystem();

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-game-viewport-map");
    std::shared_ptr<GameObject> canvasGo;
    const std::shared_ptr<CanvasComponent> canvas =
        MakeCanvas(*scene, Vector2(1920.0f, 1080.0f), canvasGo);

    InjectViewport(ui, 1280.0f, 1000.0f);

    const Vector2 viewportPx = ui.GetViewportSizeWindowPx();
    CHECK(viewportPx.x == doctest::Approx(1280.0f));
    CHECK(viewportPx.y == doctest::Approx(1000.0f));

    const ScreenUICoords::LetterboxMapping mapping = ui.GetReferencePxMapping(canvas.get());
    const float expectedScale = 1280.0f / 1920.0f;
    const float scaledHeight = 1080.0f * expectedScale;
    CHECK(mapping.Scale == doctest::Approx(expectedScale));
    CHECK(mapping.Offset.x == doctest::Approx(0.0f));
    CHECK(mapping.Offset.y == doctest::Approx((1000.0f - scaledHeight) * 0.5f));

    // The public inverse must agree with the mapping it came from (one conversion, no copy).
    Vector2 unmapped(0.0f, 0.0f);
    const Vector2 insidePx = mapping.MapPoint(Vector2(500.0f, 250.0f));
    REQUIRE(ui.TryUnmapViewportPointToReferencePx(canvas.get(), insidePx, unmapped));
    CHECK(unmapped.x == doctest::Approx(500.0f));
    CHECK(unmapped.y == doctest::Approx(250.0f));

    // A point in the top letterbox bar is not a coordinate.
    CHECK_FALSE(ui.TryUnmapViewportPointToReferencePx(canvas.get(), Vector2(10.0f, 2.0f), unmapped));

    // No canvas (or an unusable one) is "no mapping" (Scale 0), never a silent 1:1 mapping.
    CHECK(ui.GetReferencePxMapping(nullptr).Scale == 0.0f);
}

TEST_CASE("screen-ui-game-viewport: missing viewport reports invalid instead of fake coords [smoke]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIGameViewportTestScope scope;
    UISystem& ui = scope.GetUISystem();

    // Nothing injected and no platform window in this headless process.
    CHECK_FALSE(ui.GetPointerViewportContext().bValid);
    const Vector2 viewportPx = ui.GetViewportSizeWindowPx();
    CHECK(viewportPx.x == 0.0f);
    CHECK(viewportPx.y == 0.0f);

    const ScreenUIPointerViewportSample sample = ui.GetPointerPosition();
    CHECK_FALSE(sample.bValid);
    CHECK(sample.ReferencePx.x == 0.0f);
    CHECK(sample.ReferencePx.y == 0.0f);
    CHECK(sample.ReferenceDeltaPx.x == 0.0f);
    CHECK(sample.ReferenceDeltaPx.y == 0.0f);

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-game-viewport-invalid");
    std::shared_ptr<GameObject> canvasGo;
    const std::shared_ptr<CanvasComponent> canvas =
        MakeCanvas(*scene, Vector2(1920.0f, 1080.0f), canvasGo);

    // A real canvas with no viewport still yields no mapping.
    CHECK(ui.GetReferencePxMapping(canvas.get()).Scale == 0.0f);
    Vector2 untouched(7.0f, 7.0f);
    CHECK_FALSE(ui.TryUnmapViewportPointToReferencePx(canvas.get(), Vector2(1.0f, 1.0f), untouched));
    CHECK(untouched.x == 7.0f);
    CHECK(untouched.y == 7.0f);

    // With a surface, a resolvable frame is valid ...
    InjectViewport(ui, 200.0f, 200.0f);
    scope.Frame(scene.get(), nullptr, Vector2(40.0f, 30.0f));
    REQUIRE(ui.GetPointerPosition().bValid);

    // ... and a frame that cannot resolve a reference point reports invalid, and drops the stale
    // coordinate instead of re-serving it.
    scope.Frame(scene.get(), nullptr, Vector2(40.0f, 30.0f), false, false, false, false);
    CHECK_FALSE(ui.GetPointerPosition().bValid);
    CHECK(ui.GetPointerPosition().ReferencePx.x == 0.0f);
    CHECK(ui.GetPointerPosition().ReferencePx.y == 0.0f);

    // An unusable injected surface is no surface: mapping and pointer both go invalid again.
    InjectViewport(ui, 0.0f, 0.0f);
    CHECK(ui.GetReferencePxMapping(canvas.get()).Scale == 0.0f);
    CHECK(ui.GetViewportSizeWindowPx().x == 0.0f);
}

TEST_CASE("screen-ui-game-viewport: pointer sample matches UIPointerEvent reference px [smoke]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIGameViewportTestScope scope;
    UISystem& ui = scope.GetUISystem();

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-game-viewport-pointer");
    std::shared_ptr<GameObject> canvasGo;
    const std::shared_ptr<CanvasComponent> canvas =
        MakeCanvas(*scene, Vector2(200.0f, 200.0f), canvasGo);
    const std::shared_ptr<WidgetComponent> widget = MakeWidget(*scene, *canvasGo);
    UILayoutPass::LayoutCanvas(*canvas);

    InjectViewport(ui, 200.0f, 200.0f);

    int downCount = 0;
    Vector2 eventReferencePx(0.0f, 0.0f);
    widget->OnPointerDown().AddLambda(
        [&](const UIPointerEvent& event)
        {
            ++downCount;
            eventReferencePx = event.ReferencePx;
        });

    const Vector2 firstRef(20.0f, 20.0f);
    scope.Frame(scene.get(), widget.get(), firstRef);
    REQUIRE(ui.GetPointerPosition().bValid);
    CHECK(ui.GetPointerPosition().ReferencePx.x == doctest::Approx(firstRef.x));
    CHECK(ui.GetPointerPosition().ReferencePx.y == doctest::Approx(firstRef.y));

    scope.Frame(scene.get(), widget.get(), firstRef, true, true, false);
    REQUIRE(downCount == 1);
    // Same call, same variable: the dispatched event and the readable sample are one coordinate.
    CHECK(ui.GetPointerPosition().ReferencePx.x == eventReferencePx.x);
    CHECK(ui.GetPointerPosition().ReferencePx.y == eventReferencePx.y);

    const Vector2 secondRef(30.0f, 25.0f);
    scope.Frame(scene.get(), widget.get(), secondRef, false, true, false);
    CHECK(ui.GetPointerPosition().ReferencePx.x == doctest::Approx(secondRef.x));
    CHECK(ui.GetPointerPosition().ReferencePx.y == doctest::Approx(secondRef.y));
    CHECK(ui.GetPointerPosition().ReferenceDeltaPx.x == doctest::Approx(secondRef.x - firstRef.x));
    CHECK(ui.GetPointerPosition().ReferenceDeltaPx.y == doctest::Approx(secondRef.y - firstRef.y));

    scope.Frame(scene.get(), widget.get(), secondRef, false, false, true);
    CHECK(downCount == 1);
    CHECK(ui.GetPointerPosition().bValid);
}
