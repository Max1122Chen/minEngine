#include "UILayoutTest.h"

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
#include "Runtime/Function/UI/UILayoutPass.h"
#include "Runtime/Function/UI/UITypes.h"

#include "doctest.h"

namespace minEngine
{
    class UILayoutTestScope
    {
    public:
        UILayoutTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();
            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
        }

        ~UILayoutTestScope()
        {
            m_SceneManager.Shutdown();
            Testing::TestAccess<SceneManager>::SetInstance(nullptr);
            m_ObjectManager.Shutdown();
            Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
        }

    private:
        ObjectManager m_ObjectManager;
        SceneManager m_SceneManager;
    };
}

TEST_CASE("ui-layout: point anchor top-left with margin [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    UILayoutTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("ui-layout-point");
    const std::shared_ptr<GameObject> go = scene->CreateGameObject();
    const std::shared_ptr<WidgetComponent> widget = go->AddComponent<WidgetComponent>();
    widget->ApplyAnchorPreset(EUIAnchorPreset::TopLeft);
    widget->SetSize(Vector2(200.0f, 50.0f));
    widget->SetMargin(Vector4(10.0f, 20.0f, 0.0f, 0.0f));

    UIRect parent;
    parent.TopLeft = Vector2(0.0f, 0.0f);
    parent.Size = Vector2(1920.0f, 1080.0f);

    const UIRect rect = UILayoutPass::ComputeChildRect(parent, *widget);
    CHECK(rect.Left() == doctest::Approx(10.0f));
    CHECK(rect.Top() == doctest::Approx(20.0f));
    CHECK(rect.Width() == doctest::Approx(200.0f));
    CHECK(rect.Height() == doctest::Approx(50.0f));
}

TEST_CASE("ui-layout: stretch-all with margin inset [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    UILayoutTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("ui-layout-stretch");
    const std::shared_ptr<GameObject> go = scene->CreateGameObject();
    const std::shared_ptr<WidgetComponent> widget = go->AddComponent<WidgetComponent>();
    widget->ApplyAnchorPreset(EUIAnchorPreset::StretchAll);
    widget->SetMargin(Vector4(100.0f, 50.0f, 100.0f, 50.0f));

    UIRect parent;
    parent.TopLeft = Vector2(0.0f, 0.0f);
    parent.Size = Vector2(1000.0f, 500.0f);

    const UIRect rect = UILayoutPass::ComputeChildRect(parent, *widget);
    CHECK(rect.Left() == doctest::Approx(100.0f));
    CHECK(rect.Top() == doctest::Approx(50.0f));
    CHECK(rect.Width() == doctest::Approx(800.0f));
    CHECK(rect.Height() == doctest::Approx(400.0f));
}

TEST_CASE("ui-layout: center preset places top-left at parent center [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    UILayoutTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("ui-layout-center");
    const std::shared_ptr<GameObject> go = scene->CreateGameObject();
    const std::shared_ptr<WidgetComponent> widget = go->AddComponent<WidgetComponent>();
    widget->ApplyAnchorPreset(EUIAnchorPreset::Center);
    widget->SetSize(Vector2(40.0f, 20.0f));

    UIRect parent;
    parent.TopLeft = Vector2(0.0f, 0.0f);
    parent.Size = Vector2(200.0f, 100.0f);

    const UIRect rect = UILayoutPass::ComputeChildRect(parent, *widget);
    CHECK(rect.Left() == doctest::Approx(100.0f));
    CHECK(rect.Top() == doctest::Approx(50.0f));
    CHECK(rect.Width() == doctest::Approx(40.0f));
    CHECK(rect.Height() == doctest::Approx(20.0f));
}

TEST_CASE("ui-layout: canvas subtree layouts child image widget [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    UILayoutTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("ui-layout-tree");
    REQUIRE(scene);

    const std::shared_ptr<GameObject> canvasGo = scene->CreateGameObject();
    canvasGo->AddComponent<SceneComponent>();
    const std::shared_ptr<CanvasComponent> canvas = canvasGo->AddComponent<CanvasComponent>();
    canvas->SetReferenceResolution(Vector2(800.0f, 600.0f));

    const std::shared_ptr<GameObject> childGo = scene->CreateGameObject();
    childGo->AddComponent<SceneComponent>();
    const std::shared_ptr<WidgetComponent> widget = childGo->AddComponent<WidgetComponent>();
    childGo->AddComponent<ImageComponent>();
    widget->ApplyAnchorPreset(EUIAnchorPreset::TopLeft);
    widget->SetSize(Vector2(120.0f, 40.0f));
    widget->SetMargin(Vector4(16.0f, 24.0f, 0.0f, 0.0f));

    REQUIRE(childGo->AttachToParent(canvasGo.get(), AttachmentTransformRules::KeepRelativeTransform));

    UILayoutPass::LayoutCanvas(*canvas);

    const UIRect& rect = widget->GetComputedRect();
    CHECK(rect.Left() == doctest::Approx(16.0f));
    CHECK(rect.Top() == doctest::Approx(24.0f));
    CHECK(rect.Width() == doctest::Approx(120.0f));
    CHECK(rect.Height() == doctest::Approx(40.0f));

    CanvasComponent* found = CanvasComponent::FindOwningCanvas(childGo.get());
    CHECK(found == canvas.get());
}
