#include "ScreenUIDragTest.h"

#include "EngineTestFixture.h"

#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/ButtonComponent.h"
#include "Runtime/Function/Framework/Components/CanvasComponent.h"
#include "Runtime/Function/Framework/Components/DragSourceComponent.h"
#include "Runtime/Function/Framework/Components/DropTargetComponent.h"
#include "Runtime/Function/Framework/Components/ImageComponent.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/UI/ScreenUIHitTypes.h"
#include "Runtime/Function/UI/UILayoutPass.h"
#include "Runtime/Function/UI/UISystem.h"
#include "Runtime/Function/UI/UITypes.h"

#include "doctest.h"

namespace minEngine
{
    class ScreenUIDragTestScope
    {
    public:
        ScreenUIDragTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();
            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
            UISystem::SetInstance(&m_UISystem);
            m_UISystem.Initialize();
            m_UISystem.SetPointerRoutingEnabled(true);
            ScreenUIPointerViewportContext ctx;
            ctx.ImageMin = Vector2(0.0f, 0.0f);
            ctx.ImageSize = Vector2(200.0f, 200.0f);
            ctx.bValid = true;
            m_UISystem.SetPointerViewportContext(ctx);
        }

        ~ScreenUIDragTestScope()
        {
            m_UISystem.Shutdown();
            UISystem::SetInstance(nullptr);
            m_SceneManager.Shutdown();
            Testing::TestAccess<SceneManager>::SetInstance(nullptr);
            m_ObjectManager.Shutdown();
            Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
        }

        UISystem& GetUISystem() { return m_UISystem; }

        void Process(
            Scene* scene,
            WidgetComponent* hitWidget,
            const Vector2& referencePx,
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
                pressed,
                down,
                released);
        }

    private:
        ObjectManager m_ObjectManager;
        SceneManager m_SceneManager;
        UISystem m_UISystem;
    };

    static std::shared_ptr<WidgetComponent> MakeSlot(
        Scene& scene,
        GameObject& canvasGo,
        const Vector2& margin,
        const Vector2& size,
        bool withSource,
        bool withTarget,
        int32_t userId)
    {
        const std::shared_ptr<GameObject> go = scene.CreateGameObject();
        go->AddComponent<SceneComponent>();
        const std::shared_ptr<WidgetComponent> widget = go->AddComponent<WidgetComponent>();
        go->AddComponent<ImageComponent>();
        widget->ApplyAnchorPreset(EUIAnchorPreset::TopLeft);
        widget->SetSize(size);
        widget->SetMargin(Vector4(margin.x, margin.y, 0.0f, 0.0f));
        if (withSource)
        {
            const std::shared_ptr<DragSourceComponent> source = go->AddComponent<DragSourceComponent>();
            source->SetUserId(userId);
            source->SetDragThresholdPx(6.0f);
        }
        if (withTarget)
        {
            go->AddComponent<DropTargetComponent>();
        }
        REQUIRE(go->AttachToParent(&canvasGo, AttachmentTransformRules::KeepRelativeTransform));
        return widget;
    }
}

TEST_CASE("screen-ui-drag: threshold promotes and suppresses click [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIDragTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-drag-threshold");
    const std::shared_ptr<GameObject> canvasGo = scene->CreateGameObject();
    canvasGo->AddComponent<SceneComponent>();
    const std::shared_ptr<CanvasComponent> canvas = canvasGo->AddComponent<CanvasComponent>();
    canvas->SetReferenceResolution(Vector2(200.0f, 200.0f));

    const std::shared_ptr<WidgetComponent> sourceWidget =
        MakeSlot(*scene, *canvasGo, Vector2(10.0f, 10.0f), Vector2(40.0f, 40.0f), true, true, 7);
    const std::shared_ptr<ButtonComponent> button =
        sourceWidget->GetOwner()->AddComponent<ButtonComponent>();
    UILayoutPass::LayoutCanvas(*canvas);

    int beginCount = 0;
    int clickCount = 0;
    DragSourceComponent* source =
        sourceWidget->GetOwner()->GetComponentsOfType<DragSourceComponent>().front().get();
    source->OnDragBegin().AddLambda([&beginCount]() { ++beginCount; });
    button->OnClicked().AddLambda([&clickCount]() { ++clickCount; });

    scope.Process(scene.get(), sourceWidget.get(), Vector2(20.0f, 20.0f), true, true, false);
    CHECK(scope.GetUISystem().GetDragSession().Phase == EUIDragPhase::Pending);
    CHECK(beginCount == 0);

    scope.Process(scene.get(), sourceWidget.get(), Vector2(22.0f, 20.0f), false, true, false);
    CHECK(scope.GetUISystem().GetDragSession().Phase == EUIDragPhase::Pending);

    scope.Process(scene.get(), sourceWidget.get(), Vector2(30.0f, 20.0f), false, true, false);
    CHECK(scope.GetUISystem().IsDragging());
    CHECK(beginCount == 1);

    scope.Process(scene.get(), sourceWidget.get(), Vector2(30.0f, 20.0f), false, false, true);
    CHECK_FALSE(scope.GetUISystem().IsDragging());
    CHECK(clickCount == 0);
}

TEST_CASE("screen-ui-drag: release under threshold still clicks [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIDragTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-drag-click");
    const std::shared_ptr<GameObject> canvasGo = scene->CreateGameObject();
    canvasGo->AddComponent<SceneComponent>();
    const std::shared_ptr<CanvasComponent> canvas = canvasGo->AddComponent<CanvasComponent>();
    canvas->SetReferenceResolution(Vector2(200.0f, 200.0f));

    const std::shared_ptr<WidgetComponent> sourceWidget =
        MakeSlot(*scene, *canvasGo, Vector2(10.0f, 10.0f), Vector2(40.0f, 40.0f), true, false, 1);
    const std::shared_ptr<ButtonComponent> button =
        sourceWidget->GetOwner()->AddComponent<ButtonComponent>();
    UILayoutPass::LayoutCanvas(*canvas);

    int clickCount = 0;
    button->OnClicked().AddLambda([&clickCount]() { ++clickCount; });

    scope.Process(scene.get(), sourceWidget.get(), Vector2(20.0f, 20.0f), true, true, false);
    scope.Process(scene.get(), sourceWidget.get(), Vector2(21.0f, 20.0f), false, true, false);
    scope.Process(scene.get(), sourceWidget.get(), Vector2(21.0f, 20.0f), false, false, true);

    CHECK(clickCount == 1);
    CHECK(scope.GetUISystem().GetDragSession().Phase == EUIDragPhase::None);
}

TEST_CASE("screen-ui-drag: drop on target broadcasts OnDrop [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIDragTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-drag-drop");
    const std::shared_ptr<GameObject> canvasGo = scene->CreateGameObject();
    canvasGo->AddComponent<SceneComponent>();
    const std::shared_ptr<CanvasComponent> canvas = canvasGo->AddComponent<CanvasComponent>();
    canvas->SetReferenceResolution(Vector2(200.0f, 200.0f));

    const std::shared_ptr<WidgetComponent> sourceWidget =
        MakeSlot(*scene, *canvasGo, Vector2(10.0f, 10.0f), Vector2(40.0f, 40.0f), true, false, 3);
    const std::shared_ptr<WidgetComponent> targetWidget =
        MakeSlot(*scene, *canvasGo, Vector2(100.0f, 10.0f), Vector2(40.0f, 40.0f), false, true, 0);
    UILayoutPass::LayoutCanvas(*canvas);

    int dropCount = 0;
    int endSuccess = 0;
    DropTargetComponent* target =
        targetWidget->GetOwner()->GetComponentsOfType<DropTargetComponent>().front().get();
    DragSourceComponent* source =
        sourceWidget->GetOwner()->GetComponentsOfType<DragSourceComponent>().front().get();
    target->OnDrop().AddLambda(
        [&dropCount]()
        {
            ++dropCount;
            REQUIRE(UISystem::Get().GetDragSession().Source != nullptr);
            CHECK(UISystem::Get().GetDragSession().Source->GetUserId() == 3);
        });
    source->OnDragEnd().AddLambda(
        [&endSuccess, source]()
        {
            if (source->ConsumeLastEndSuccess())
            {
                ++endSuccess;
            }
        });

    scope.Process(scene.get(), sourceWidget.get(), Vector2(20.0f, 20.0f), true, true, false);
    scope.Process(scene.get(), sourceWidget.get(), Vector2(40.0f, 20.0f), false, true, false);
    REQUIRE(scope.GetUISystem().IsDragging());
    scope.Process(scene.get(), targetWidget.get(), Vector2(110.0f, 20.0f), false, false, true);

    CHECK(dropCount == 1);
    CHECK(endSuccess == 1);
}

TEST_CASE("screen-ui-drag: release on empty cancels [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIDragTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-drag-cancel");
    const std::shared_ptr<GameObject> canvasGo = scene->CreateGameObject();
    canvasGo->AddComponent<SceneComponent>();
    const std::shared_ptr<CanvasComponent> canvas = canvasGo->AddComponent<CanvasComponent>();
    canvas->SetReferenceResolution(Vector2(200.0f, 200.0f));

    const std::shared_ptr<WidgetComponent> sourceWidget =
        MakeSlot(*scene, *canvasGo, Vector2(10.0f, 10.0f), Vector2(40.0f, 40.0f), true, false, 1);
    UILayoutPass::LayoutCanvas(*canvas);

    int cancelCount = 0;
    DragSourceComponent* source =
        sourceWidget->GetOwner()->GetComponentsOfType<DragSourceComponent>().front().get();
    source->OnDragCancel().AddLambda([&cancelCount]() { ++cancelCount; });

    scope.Process(scene.get(), sourceWidget.get(), Vector2(20.0f, 20.0f), true, true, false);
    scope.Process(scene.get(), sourceWidget.get(), Vector2(40.0f, 20.0f), false, true, false);
    scope.Process(scene.get(), nullptr, Vector2(150.0f, 150.0f), false, false, true);

    CHECK(cancelCount == 1);
    CHECK_FALSE(scope.GetUISystem().IsDragging());
}
