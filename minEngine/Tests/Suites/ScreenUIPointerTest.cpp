#include "ScreenUIPointerTest.h"

#include "EngineTestFixture.h"

#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/ButtonComponent.h"
#include "Runtime/Function/Framework/Components/CanvasComponent.h"
#include "Runtime/Function/Framework/Components/DragSourceComponent.h"
#include "Runtime/Function/Framework/Components/ImageComponent.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/UI/ScreenUIHitTypes.h"
#include "Runtime/Function/UI/ScreenUIPointerTypes.h"
#include "Runtime/Function/UI/UILayoutPass.h"
#include "Runtime/Function/UI/UISystem.h"
#include "Runtime/Function/UI/UITypes.h"

#include "doctest.h"

namespace minEngine
{
    class ScreenUIPointerTestScope
    {
    public:
        ScreenUIPointerTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            ObjectManager::Initialize();
            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
            UISystem::SetInstance(&m_UISystem);
            m_UISystem.Initialize();
            m_UISystem.SetPointerRoutingEnabled(true);
        }

        ~ScreenUIPointerTestScope()
        {
            m_UISystem.Shutdown();
            UISystem::SetInstance(nullptr);
            m_SceneManager.Shutdown();
            Testing::TestAccess<SceneManager>::SetInstance(nullptr);
            ObjectManager::Shutdown();
            Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
        }

        UISystem& GetUISystem() { return m_UISystem; }

        void Frame(
            Scene* scene,
            WidgetComponent* hitWidget,
            const Vector2& referencePx,
            EUIPointerButton button,
            bool pressed,
            bool down,
            bool released)
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
                true,
                button,
                pressed,
                down,
                released);
        }

    private:
        ObjectManager m_ObjectManager;
        SceneManager m_SceneManager;
        UISystem m_UISystem;
    };

    static std::shared_ptr<WidgetComponent> MakeWidget(
        Scene& scene,
        GameObject& canvasGo,
        bool withButton,
        bool withDragSource)
    {
        const std::shared_ptr<GameObject> go = scene.CreateGameObject();
        go->AddComponent<SceneComponent>();
        const std::shared_ptr<WidgetComponent> widget = go->AddComponent<WidgetComponent>();
        go->AddComponent<ImageComponent>();
        widget->ApplyAnchorPreset(EUIAnchorPreset::TopLeft);
        widget->SetSize(Vector2(40.0f, 40.0f));
        widget->SetMargin(Vector4(10.0f, 10.0f, 0.0f, 0.0f));
        if (withButton)
        {
            go->AddComponent<ButtonComponent>();
        }
        if (withDragSource)
        {
            const std::shared_ptr<DragSourceComponent> source = go->AddComponent<DragSourceComponent>();
            source->SetDragThresholdPx(6.0f);
        }
        REQUIRE(go->AttachToParent(&canvasGo, AttachmentTransformRules::KeepRelativeTransform));
        return widget;
    }
}

TEST_CASE("screen-ui-pointer: left right and sugar [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIPointerTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-pointer-payload");
    const std::shared_ptr<GameObject> canvasGo = scene->CreateGameObject();
    canvasGo->AddComponent<SceneComponent>();
    const std::shared_ptr<CanvasComponent> canvas = canvasGo->AddComponent<CanvasComponent>();
    canvas->SetReferenceResolution(Vector2(200.0f, 200.0f));

    const std::shared_ptr<WidgetComponent> widget = MakeWidget(*scene, *canvasGo, true, false);
    UILayoutPass::LayoutCanvas(*canvas);

    const auto buttons = widget->GetOwner()->GetComponentsOfType<ButtonComponent>();
    REQUIRE_FALSE(buttons.empty());
    ButtonComponent* button = buttons[0].get();

    int widgetClicks = 0;
    int buttonPayload = 0;
    int buttonSugar = 0;
    EUIPointerButton lastButton = EUIPointerButton::None;
    widget->OnPointerClick().AddLambda(
        [&](const UIPointerEvent& event)
        {
            ++widgetClicks;
            lastButton = event.Button;
        });
    button->OnPointerClick().AddLambda(
        [&](const UIPointerEvent& event)
        {
            ++buttonPayload;
            lastButton = event.Button;
        });
    button->OnClicked().AddLambda([&]() { ++buttonSugar; });

    const Vector2 ref(20.0f, 20.0f);
    scope.Frame(scene.get(), widget.get(), ref, EUIPointerButton::Left, true, true, false);
    scope.Frame(scene.get(), widget.get(), ref, EUIPointerButton::Left, false, false, true);
    CHECK(widgetClicks == 1);
    CHECK(buttonPayload == 1);
    CHECK(buttonSugar == 1);
    CHECK(lastButton == EUIPointerButton::Left);

    scope.Frame(scene.get(), widget.get(), ref, EUIPointerButton::Right, true, true, false);
    scope.Frame(scene.get(), widget.get(), ref, EUIPointerButton::Right, false, false, true);
    CHECK(widgetClicks == 2);
    CHECK(buttonPayload == 2);
    CHECK(buttonSugar == 1);
    CHECK(lastButton == EUIPointerButton::Right);
    CHECK(scope.GetUISystem().GetPointerState().ClickButton == EUIPointerButton::Right);
}

TEST_CASE("screen-ui-pointer: right drag sets InitiatingButton [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIPointerTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-pointer-rdrag");
    const std::shared_ptr<GameObject> canvasGo = scene->CreateGameObject();
    canvasGo->AddComponent<SceneComponent>();
    const std::shared_ptr<CanvasComponent> canvas = canvasGo->AddComponent<CanvasComponent>();
    canvas->SetReferenceResolution(Vector2(200.0f, 200.0f));

    const std::shared_ptr<WidgetComponent> widget = MakeWidget(*scene, *canvasGo, false, true);
    UILayoutPass::LayoutCanvas(*canvas);

    int clicks = 0;
    widget->OnPointerClick().AddLambda([&](const UIPointerEvent&) { ++clicks; });

    scope.Frame(scene.get(), widget.get(), Vector2(20.0f, 20.0f), EUIPointerButton::Right, true, true, false);
    CHECK(scope.GetUISystem().GetDragSession().InitiatingButton == EUIPointerButton::Right);
    scope.Frame(scene.get(), widget.get(), Vector2(40.0f, 40.0f), EUIPointerButton::Right, false, true, false);
    CHECK(scope.GetUISystem().IsDragging());
    scope.Frame(scene.get(), widget.get(), Vector2(40.0f, 40.0f), EUIPointerButton::Right, false, false, true);
    CHECK(clicks == 0);
    CHECK_FALSE(scope.GetUISystem().IsDragging());
}

TEST_CASE("screen-ui-pointer: enter leave on hover change [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIPointerTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-pointer-hover");
    const std::shared_ptr<GameObject> canvasGo = scene->CreateGameObject();
    canvasGo->AddComponent<SceneComponent>();
    const std::shared_ptr<CanvasComponent> canvas = canvasGo->AddComponent<CanvasComponent>();
    canvas->SetReferenceResolution(Vector2(200.0f, 200.0f));

    const std::shared_ptr<WidgetComponent> a = MakeWidget(*scene, *canvasGo, false, false);
    const std::shared_ptr<WidgetComponent> b = MakeWidget(*scene, *canvasGo, false, false);
    b->SetMargin(Vector4(60.0f, 10.0f, 0.0f, 0.0f));
    UILayoutPass::LayoutCanvas(*canvas);

    int enters = 0;
    int leaves = 0;
    a->OnPointerEnter().AddLambda([&](const UIPointerEvent&) { ++enters; });
    a->OnPointerLeave().AddLambda([&](const UIPointerEvent&) { ++leaves; });

    scope.Frame(scene.get(), a.get(), Vector2(20.0f, 20.0f), EUIPointerButton::Left, false, false, false);
    CHECK(enters == 1);
    scope.Frame(scene.get(), b.get(), Vector2(70.0f, 20.0f), EUIPointerButton::Left, false, false, false);
    CHECK(leaves == 1);
}
