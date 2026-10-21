#include "ActivityConsumersTest.h"

#include "EngineTestFixture.h"

#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"
#include "Access/WidgetComponentTestAccess.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/CanvasComponent.h"
#include "Runtime/Function/Framework/Components/ImageComponent.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/Components/TextComponent.h"
#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/UI/ScreenUIHitTester.h"
#include "Runtime/Function/UI/UILayoutPass.h"
#include "Runtime/Function/UI/UITypes.h"
#include "Runtime/Resource/Font.h"
#include "Runtime/Resource/Loaders/FontLoader.h"

#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>

#include "doctest.h"

namespace minEngine
{
    namespace
    {
        class ActivityConsumersTestScope
        {
        public:
            ActivityConsumersTestScope()
            {
                Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
                ObjectManager::Initialize();
                Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
                m_SceneManager.Initialize();
            }

            ~ActivityConsumersTestScope()
            {
                m_SceneManager.Shutdown();
                Testing::TestAccess<SceneManager>::SetInstance(nullptr);
                ObjectManager::Shutdown();
                Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
            }

        private:
            ObjectManager m_ObjectManager;
            SceneManager m_SceneManager;
        };

        std::shared_ptr<GameObject> MakeCanvas(
            Scene& scene,
            const Vector2& referenceResolution,
            std::shared_ptr<CanvasComponent>& outCanvas)
        {
            const std::shared_ptr<GameObject> canvasGo = scene.CreateGameObject();
            canvasGo->AddComponent<SceneComponent>();
            outCanvas = canvasGo->AddComponent<CanvasComponent>();
            outCanvas->SetReferenceResolution(referenceResolution);
            return canvasGo;
        }

        /** Fixed-rect widget node: point anchor, explicit size, margin from the parent corner. */
        std::shared_ptr<WidgetComponent> MakeFixedNode(
            Scene& scene,
            GameObject& parent,
            const Vector2& size,
            const Vector4& margin,
            uint32_t stableOrder,
            bool withImage)
        {
            const std::shared_ptr<GameObject> go = scene.CreateGameObject();
            go->AddComponent<SceneComponent>();
            const std::shared_ptr<WidgetComponent> widget = go->AddComponent<WidgetComponent>();
            widget->ApplyAnchorPreset(EUIAnchorPreset::TopLeft);
            widget->SetSize(size);
            widget->SetMargin(margin);
            widget->SetStableOrder(stableOrder);
            if (withImage)
            {
                go->AddComponent<ImageComponent>();
            }
            REQUIRE(go->AttachToParent(&parent, AttachmentTransformRules::KeepRelativeTransform));
            return widget;
        }

        /** Stretched widget node: fills whatever rect its parent currently has. */
        std::shared_ptr<WidgetComponent> MakeStretchedNode(
            Scene& scene,
            GameObject& parent,
            uint32_t stableOrder,
            bool withImage)
        {
            const std::shared_ptr<GameObject> go = scene.CreateGameObject();
            go->AddComponent<SceneComponent>();
            const std::shared_ptr<WidgetComponent> widget = go->AddComponent<WidgetComponent>();
            widget->ApplyAnchorPreset(EUIAnchorPreset::StretchAll);
            widget->SetStableOrder(stableOrder);
            if (withImage)
            {
                go->AddComponent<ImageComponent>();
            }
            REQUIRE(go->AttachToParent(&parent, AttachmentTransformRules::KeepRelativeTransform));
            return widget;
        }

        ScreenUIHitResult HitTestReferencePoint(Scene& scene, const Vector2& point, const Vector2& viewport)
        {
            ScreenUIHitQuery query;
            query.ViewportPoint = point;
            query.ViewportWidth = viewport.x;
            query.ViewportHeight = viewport.y;
            query.Scene = &scene;

            const ScreenUIHitTester tester;
            return tester.HitTestAt(query);
        }

        bool LoadFontBytes(std::vector<uint8_t>& outBytes)
        {
            const std::filesystem::path candidates[] = {
                std::filesystem::path("minEngine/MyMEProject/Assets/Fonts/Inter_18pt-Regular.ttf"),
                std::filesystem::path("../MyMEProject/Assets/Fonts/Inter_18pt-Regular.ttf"),
                std::filesystem::path("../../MyMEProject/Assets/Fonts/Inter_18pt-Regular.ttf"),
                std::filesystem::path("MyMEProject/Assets/Fonts/Inter_18pt-Regular.ttf"),
            };

            for (const std::filesystem::path& path : candidates)
            {
                std::ifstream input(path, std::ios::binary | std::ios::ate);
                if (!input.is_open())
                {
                    continue;
                }

                const std::streamsize size = input.tellg();
                if (size <= 0)
                {
                    continue;
                }

                outBytes.resize(static_cast<size_t>(size));
                input.seekg(0, std::ios::beg);
                if (input.read(reinterpret_cast<char*>(outBytes.data()), size))
                {
                    return true;
                }
            }

            return false;
        }
    }

    TEST_CASE("activity-consumers: inactive node skips its whole UI layout subtree [smoke]")
    {
        EngineReflectionFixture reflectionFixture;
        REQUIRE(reflectionFixture.IsReflectionReady());
        ActivityConsumersTestScope scope;

        const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("activity-consumers-layout");

        std::shared_ptr<CanvasComponent> canvas;
        const std::shared_ptr<GameObject> canvasGo = MakeCanvas(*scene, Vector2(800.0f, 600.0f), canvas);
        const std::shared_ptr<WidgetComponent> panel = MakeFixedNode(
            *scene, *canvasGo, Vector2(400.0f, 300.0f), Vector4(100.0f, 100.0f, 0.0f, 0.0f), 1u, false);
        const std::shared_ptr<WidgetComponent> button =
            MakeStretchedNode(*scene, *panel->GetOwner(), 1u, false);

        UILayoutPass::LayoutCanvas(*canvas);

        const UIRect laidOut = button->GetComputedRect();
        CHECK(laidOut.TopLeft.x == doctest::Approx(100.0f));
        CHECK(laidOut.TopLeft.y == doctest::Approx(100.0f));
        CHECK(laidOut.Size.x == doctest::Approx(400.0f));
        CHECK(laidOut.Size.y == doctest::Approx(300.0f));

        panel->GetOwner()->SetActive(false);
        REQUIRE_FALSE(panel->GetOwner()->IsActive());
        REQUIRE_FALSE(button->IsActive());

        UILayoutPass::LayoutCanvas(*canvas);

        const UIRect skipped = button->GetComputedRect();
        CHECK(skipped.TopLeft.x == doctest::Approx(laidOut.TopLeft.x));
        CHECK(skipped.TopLeft.y == doctest::Approx(laidOut.TopLeft.y));
        CHECK(skipped.Size.x == doctest::Approx(laidOut.Size.x));
        CHECK(skipped.Size.y == doctest::Approx(laidOut.Size.y));

        panel->GetOwner()->SetActive(true);
        UILayoutPass::LayoutCanvas(*canvas);
        CHECK(button->GetComputedRect().Size.x == doctest::Approx(400.0f));
        CHECK(button->GetComputedRect().Size.y == doctest::Approx(300.0f));
    }

    TEST_CASE("activity-consumers: inactive node skips its whole hit-test subtree [smoke]")
    {
        EngineReflectionFixture reflectionFixture;
        REQUIRE(reflectionFixture.IsReflectionReady());
        ActivityConsumersTestScope scope;

        const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("activity-consumers-hit");

        std::shared_ptr<CanvasComponent> canvas;
        const std::shared_ptr<GameObject> canvasGo = MakeCanvas(*scene, Vector2(800.0f, 600.0f), canvas);
        const std::shared_ptr<WidgetComponent> panel = MakeFixedNode(
            *scene, *canvasGo, Vector2(400.0f, 300.0f), Vector4(100.0f, 100.0f, 0.0f, 0.0f), 1u, true);
        const std::shared_ptr<WidgetComponent> button =
            MakeStretchedNode(*scene, *panel->GetOwner(), 5u, true);

        UILayoutPass::LayoutCanvas(*canvas);

        const ScreenUIHitResult hitBefore =
            HitTestReferencePoint(*scene, Vector2(200.0f, 200.0f), Vector2(800.0f, 600.0f));
        REQUIRE(hitBefore.bHit);
        REQUIRE(hitBefore.Widget == button.get());

        panel->GetOwner()->SetActive(false);
        UILayoutPass::LayoutCanvas(*canvas);

        const ScreenUIHitResult hitAfter =
            HitTestReferencePoint(*scene, Vector2(200.0f, 200.0f), Vector2(800.0f, 600.0f));
        CHECK_FALSE(hitAfter.bHit);
    }

    TEST_CASE("activity-consumers: widget without contributing content does not take the pointer [smoke]")
    {
        EngineReflectionFixture reflectionFixture;
        REQUIRE(reflectionFixture.IsReflectionReady());
        ActivityConsumersTestScope scope;

        const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("activity-consumers-empty-widget");

        std::shared_ptr<CanvasComponent> canvas;
        const std::shared_ptr<GameObject> canvasGo = MakeCanvas(*scene, Vector2(200.0f, 200.0f), canvas);
        const std::shared_ptr<WidgetComponent> plate =
            MakeStretchedNode(*scene, *canvasGo, 9u, false);
        const std::shared_ptr<WidgetComponent> board = MakeFixedNode(
            *scene, *canvasGo, Vector2(100.0f, 100.0f), Vector4(0.0f, 0.0f, 0.0f, 0.0f), 1u, true);

        UILayoutPass::LayoutCanvas(*canvas);

        const ScreenUIHitResult hit = HitTestReferencePoint(*scene, Vector2(50.0f, 50.0f), Vector2(200.0f, 200.0f));
        REQUIRE(hit.bHit);
        CHECK(hit.Widget == board.get());
        CHECK(hit.Widget != plate.get());
    }

    TEST_CASE("activity-consumers: content source switch gates its contribution and dirties the widget [smoke]")
    {
        EngineReflectionFixture reflectionFixture;
        REQUIRE(reflectionFixture.IsReflectionReady());
        ActivityConsumersTestScope scope;

        const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("activity-consumers-source");

        std::shared_ptr<CanvasComponent> canvas;
        const std::shared_ptr<GameObject> canvasGo = MakeCanvas(*scene, Vector2(200.0f, 200.0f), canvas);
        const std::shared_ptr<WidgetComponent> imageWidget = MakeFixedNode(
            *scene, *canvasGo, Vector2(100.0f, 100.0f), Vector4(0.0f, 0.0f, 0.0f, 0.0f), 1u, true);
        const std::shared_ptr<ImageComponent> image =
            imageWidget->GetOwner()->GetComponentsOfType<ImageComponent>()[0];
        REQUIRE(image != nullptr);

        UILayoutPass::LayoutCanvas(*canvas);

        REQUIRE(image->CanDraw());
        const ScreenUIHitResult hitBefore =
            HitTestReferencePoint(*scene, Vector2(50.0f, 50.0f), Vector2(200.0f, 200.0f));
        REQUIRE(hitBefore.bHit);
        REQUIRE(hitBefore.Widget == imageWidget.get());

        Testing::TestAccess<WidgetComponent>::ClearRenderStateDirty(*imageWidget);
        image->SetActive(false);

        CHECK_FALSE(image->CanDraw());
        // The widget has to be told, not wait for an unrelated rebuild.
        CHECK(Testing::TestAccess<WidgetComponent>::IsRenderStateDirty(*imageWidget));

        const ScreenUIHitResult hitWhileOff =
            HitTestReferencePoint(*scene, Vector2(50.0f, 50.0f), Vector2(200.0f, 200.0f));
        CHECK_FALSE(hitWhileOff.bHit);

        image->SetActive(true);
        CHECK(image->CanDraw());
        CHECK(Testing::TestAccess<WidgetComponent>::IsRenderStateDirty(*imageWidget));

        const ScreenUIHitResult hitBackOn =
            HitTestReferencePoint(*scene, Vector2(50.0f, 50.0f), Vector2(200.0f, 200.0f));
        REQUIRE(hitBackOn.bHit);
        CHECK(hitBackOn.Widget == imageWidget.get());

        std::vector<uint8_t> fontBytes;
        if (!LoadFontBytes(fontBytes))
        {
            MESSAGE("font asset unavailable; the text source contribution is not exercised");
            return;
        }

        const std::shared_ptr<Font> font = FontLoader::CreateFromBytes(std::move(fontBytes));
        REQUIRE(font != nullptr);

        const std::shared_ptr<GameObject> textGo = scene->CreateGameObject();
        textGo->AddComponent<SceneComponent>();
        const std::shared_ptr<WidgetComponent> textWidget = textGo->AddComponent<WidgetComponent>();
        const std::shared_ptr<TextComponent> text = textGo->AddComponent<TextComponent>();
        text->SetFont(font);
        REQUIRE(text->CanAttemptDraw());

        Testing::TestAccess<WidgetComponent>::ClearRenderStateDirty(*textWidget);
        text->SetActive(false);

        CHECK_FALSE(text->CanAttemptDraw());
        CHECK(Testing::TestAccess<WidgetComponent>::IsRenderStateDirty(*textWidget));

        text->SetActive(true);
        CHECK(text->CanAttemptDraw());
    }
}
