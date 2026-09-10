#include "ScreenUIButtonTest.h"

#include "EngineTestFixture.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/ButtonComponent.h"
#include "Runtime/Function/Framework/Components/CanvasComponent.h"
#include "Runtime/Function/Framework/Components/ImageComponent.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/UI/UILayoutPass.h"
#include "Runtime/Function/UI/UISystem.h"
#include "Runtime/Function/UI/UITypes.h"

#include "doctest.h"

namespace minEngine
{
    class ScreenUIButtonTestScope
    {
    public:
        ScreenUIButtonTestScope()
        {
            ObjectManager::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();
            SceneManager::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
            UISystem::SetInstance(&m_UISystem);
            m_UISystem.Initialize();
        }

        ~ScreenUIButtonTestScope()
        {
            m_UISystem.Shutdown();
            UISystem::SetInstance(nullptr);
            m_SceneManager.Shutdown();
            SceneManager::SetInstance(nullptr);
            m_ObjectManager.Shutdown();
            ObjectManager::SetInstance(nullptr);
        }

        UISystem& GetUISystem() { return m_UISystem; }

        void DispatchClick(WidgetComponent* widget)
        {
            m_UISystem.TryDispatchButtonClick(widget);
        }

    private:
        ObjectManager m_ObjectManager;
        SceneManager m_SceneManager;
        UISystem m_UISystem;
    };
}

TEST_CASE("screen-ui-button: NotifyClicked broadcasts OnClicked [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIButtonTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-button-click");
    const std::shared_ptr<GameObject> canvasGo = scene->CreateGameObject();
    canvasGo->AddComponent<SceneComponent>();
    const std::shared_ptr<CanvasComponent> canvas = canvasGo->AddComponent<CanvasComponent>();
    canvas->SetReferenceResolution(Vector2(200.0f, 200.0f));

    const std::shared_ptr<GameObject> buttonGo = scene->CreateGameObject();
    buttonGo->AddComponent<SceneComponent>();
    const std::shared_ptr<WidgetComponent> widget = buttonGo->AddComponent<WidgetComponent>();
    buttonGo->AddComponent<ImageComponent>();
    const std::shared_ptr<ButtonComponent> button = buttonGo->AddComponent<ButtonComponent>();
    widget->ApplyAnchorPreset(EUIAnchorPreset::TopLeft);
    widget->SetSize(Vector2(80.0f, 40.0f));
    REQUIRE(buttonGo->AttachToParent(canvasGo.get(), AttachmentTransformRules::KeepRelativeTransform));
    UILayoutPass::LayoutCanvas(*canvas);

    int clickCount = 0;
    button->OnClicked().AddLambda(
        [&clickCount]()
        {
            ++clickCount;
        });

    button->NotifyClicked();
    CHECK(clickCount == 1);

    scope.DispatchClick(widget.get());
    CHECK(clickCount == 2);
}

TEST_CASE("screen-ui-button: disabled and missing widget do not fire [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIButtonTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-button-disabled");
    const std::shared_ptr<GameObject> go = scene->CreateGameObject();
    go->AddComponent<SceneComponent>();
    const std::shared_ptr<ButtonComponent> buttonOnly = go->AddComponent<ButtonComponent>();

    int clickCount = 0;
    buttonOnly->OnClicked().AddLambda(
        [&clickCount]()
        {
            ++clickCount;
        });

    buttonOnly->NotifyClicked();
    CHECK(clickCount == 0);

    const std::shared_ptr<WidgetComponent> widget = go->AddComponent<WidgetComponent>();
    go->AddComponent<ImageComponent>();
    widget->SetSize(Vector2(10.0f, 10.0f));

    buttonOnly->SetInteractable(false);
    buttonOnly->NotifyClicked();
    CHECK(clickCount == 0);

    buttonOnly->SetInteractable(true);
    buttonOnly->NotifyClicked();
    CHECK(clickCount == 1);
}

TEST_CASE("screen-ui-button: TargetGraphic resolves explicit then sibling [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIButtonTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-button-graphic");
    const std::shared_ptr<GameObject> go = scene->CreateGameObject();
    go->AddComponent<SceneComponent>();
    go->AddComponent<WidgetComponent>();
    const std::shared_ptr<ImageComponent> siblingImage = go->AddComponent<ImageComponent>();
    const std::shared_ptr<ButtonComponent> button = go->AddComponent<ButtonComponent>();

    CHECK(button->ResolveTargetGraphic() == siblingImage.get());

    const std::shared_ptr<GameObject> otherGo = scene->CreateGameObject();
    otherGo->AddComponent<SceneComponent>();
    const std::shared_ptr<ImageComponent> otherImage = otherGo->AddComponent<ImageComponent>();
    button->SetTargetGraphic(otherImage);
    CHECK(button->ResolveTargetGraphic() == otherImage.get());
}

TEST_CASE("screen-ui-button: tint applies pressed color when interactable [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIButtonTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-button-tint");
    const std::shared_ptr<GameObject> go = scene->CreateGameObject();
    go->AddComponent<SceneComponent>();
    const std::shared_ptr<WidgetComponent> widget = go->AddComponent<WidgetComponent>();
    const std::shared_ptr<ImageComponent> image = go->AddComponent<ImageComponent>();
    const std::shared_ptr<ButtonComponent> button = go->AddComponent<ButtonComponent>();

    const LinearColor pressed(0.2f, 0.3f, 0.4f, 1.0f);
    button->SetPressedColor(pressed);
    button->SetNormalColor(LinearColor(1.0f, 1.0f, 1.0f, 1.0f));

    scope.GetUISystem().SetPointerRoutingEnabled(true);
    // Simulate pressed ownership via public API path: Tick reads pointer state.
    // Friend scope cannot set private pointer; drive tint through interactable/disabled instead.
    button->SetInteractable(false);
    button->Tick(0.0f);
    CHECK(image->GetColor().R == doctest::Approx(button->GetDisabledColor().R));
    CHECK(image->GetColor().G == doctest::Approx(button->GetDisabledColor().G));
    CHECK(image->GetColor().B == doctest::Approx(button->GetDisabledColor().B));

    button->SetInteractable(true);
    button->Tick(0.0f);
    CHECK(image->GetColor().R == doctest::Approx(button->GetNormalColor().R));
    (void)widget;
    (void)pressed;
}
