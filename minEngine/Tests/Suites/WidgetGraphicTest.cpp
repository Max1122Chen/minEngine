#include "WidgetGraphicTest.h"

#include "EngineTestFixture.h"

#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"
#include "Access/WidgetComponentTestAccess.h"

#include "Runtime/Core/Log/LogConsole.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/MEClass.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/Components/CanvasComponent.h"
#include "Runtime/Function/Framework/Components/ImageComponent.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/Components/TextComponent.h"
#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/Components/WidgetGraphicComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/UI/ScreenUIHitTester.h"
#include "Runtime/Function/UI/UILayoutPass.h"
#include "Runtime/Resource/Font.h"
#include "Runtime/Resource/Loaders/FontLoader.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "doctest.h"

namespace minEngine
{
    namespace
    {
        class WidgetGraphicTestScope
        {
        public:
            WidgetGraphicTestScope()
            {
                Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
                ObjectManager::Initialize();
                Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
                m_SceneManager.Initialize();
            }

            ~WidgetGraphicTestScope()
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

        std::shared_ptr<GameObject> MakeCanvas(Scene& scene, const Vector2& referenceResolution, std::shared_ptr<CanvasComponent>& outCanvas)
        {
            const std::shared_ptr<GameObject> canvasGo = scene.CreateGameObject();
            canvasGo->AddComponent<SceneComponent>();
            outCanvas = canvasGo->AddComponent<CanvasComponent>();
            outCanvas->SetReferenceResolution(referenceResolution);
            return canvasGo;
        }

        /** Widget node pinned to the parent's top-left corner with an explicit size. */
        std::shared_ptr<WidgetComponent> MakeWidgetNode(Scene& scene, GameObject& parent, const Vector2& size, const std::string& name)
        {
            const std::shared_ptr<GameObject> go = scene.CreateGameObject();
            go->Rename(name);
            go->AddComponent<SceneComponent>();
            const std::shared_ptr<WidgetComponent> widget = go->AddComponent<WidgetComponent>();
            widget->ApplyAnchorPreset(EUIAnchorPreset::TopLeft);
            widget->SetSize(size);
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

        bool LoadInterFontBytes(std::vector<uint8_t>& outBytes)
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

        /** Every warning logged since the last clear, joined for field-by-field inspection. */
        std::vector<std::string> WarningMessages()
        {
            std::vector<std::string> messages;
            for (const LogRecord& record : LogConsoleStorage::Snapshot())
            {
                if (record.severity == LogSeverity::Warn)
                {
                    messages.push_back(record.message);
                }
            }
            return messages;
        }

        /** First contested-graphics warning naming the given GameObject, or empty. */
        std::string FindContestedWarning(const std::string& gameObjectName)
        {
            for (const std::string& message : WarningMessages())
            {
                if (message.find(gameObjectName) != std::string::npos &&
                    message.find("graphic components: drawing") != std::string::npos)
                {
                    return message;
                }
            }
            return std::string();
        }
    }

    TEST_CASE("widget-graphic: every draw comes through the first contributing graphic [smoke]")
    {
        EngineReflectionFixture reflectionFixture;
        REQUIRE(reflectionFixture.IsReflectionReady());
        WidgetGraphicTestScope scope;
        LogSystem::Initialize();

        std::vector<uint8_t> fontBytes;
        REQUIRE(LoadInterFontBytes(fontBytes));
        const std::shared_ptr<Font> font = FontLoader::CreateFromBytes(std::move(fontBytes));
        REQUIRE(font != nullptr);

        const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("widget-graphic-first");

        std::shared_ptr<CanvasComponent> canvas;
        const std::shared_ptr<GameObject> canvasGo = MakeCanvas(*scene, Vector2(200.0f, 200.0f), canvas);
        const std::shared_ptr<WidgetComponent> widget = MakeWidgetNode(*scene, *canvasGo, Vector2(100.0f, 100.0f), "graphic-widget");

        const std::shared_ptr<ImageComponent> image = widget->GetOwner()->AddComponent<ImageComponent>();
        image->SetColor(LinearColor(1.0f, 0.0f, 0.0f, 1.0f));
        const std::shared_ptr<TextComponent> text = widget->GetOwner()->AddComponent<TextComponent>();
        text->SetFont(font);
        text->SetContent("ABC");
        REQUIRE(image->CanContribute());
        REQUIRE(text->CanContribute());

        UILayoutPass::LayoutCanvas(*canvas);

        // Component order decides: the image was added first, so it is the one that paints.
        REQUIRE(widget->GetGraphic() == image.get());
        CHECK(widget->HasGraphic());

        WidgetDrawData drawData;
        REQUIRE(Testing::TestAccess<WidgetComponent>::ResolveDrawSource(*widget, drawData) == image.get());
        CHECK_FALSE(drawData.bLocalPixelMesh);
        CHECK(drawData.Color.x == doctest::Approx(1.0f));
        CHECK(drawData.Color.y == doctest::Approx(0.0f));

        // The pointer path asks the same question, so the widget that paints is the widget that hits.
        const ScreenUIHitResult hit = HitTestReferencePoint(*scene, Vector2(50.0f, 50.0f), Vector2(200.0f, 200.0f));
        REQUIRE(hit.bHit);
        CHECK(hit.Widget == widget.get());
        CHECK(hit.Widget->GetGraphic() == image.get());

        // A contested widget says so on every rebuild, naming both sides and the way out.
        const std::string warning = FindContestedWarning(widget->GetOwner()->GetName());
        REQUIRE_FALSE(warning.empty());
        CHECK(warning.find("holds 2 graphic components") != std::string::npos);
        CHECK(warning.find("drawing 'minEngine::ImageComponent'") != std::string::npos);
        CHECK(warning.find("minEngine::TextComponent") != std::string::npos);
        CHECK(warning.find("in component order wins") != std::string::npos);
        CHECK(warning.find("reorder the components") != std::string::npos);
        MESSAGE("warning: " << warning);
    }

    TEST_CASE("widget-graphic: a graphic that is switched off hands the widget to the next one [smoke]")
    {
        EngineReflectionFixture reflectionFixture;
        REQUIRE(reflectionFixture.IsReflectionReady());
        WidgetGraphicTestScope scope;
        LogSystem::Initialize();

        std::vector<uint8_t> fontBytes;
        REQUIRE(LoadInterFontBytes(fontBytes));
        const std::shared_ptr<Font> font = FontLoader::CreateFromBytes(std::move(fontBytes));
        REQUIRE(font != nullptr);

        const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("widget-graphic-takeover");

        std::shared_ptr<CanvasComponent> canvas;
        const std::shared_ptr<GameObject> canvasGo = MakeCanvas(*scene, Vector2(200.0f, 200.0f), canvas);
        const std::shared_ptr<WidgetComponent> widget = MakeWidgetNode(*scene, *canvasGo, Vector2(100.0f, 100.0f), "graphic-widget");

        const std::shared_ptr<ImageComponent> image = widget->GetOwner()->AddComponent<ImageComponent>();
        const std::shared_ptr<TextComponent> text = widget->GetOwner()->AddComponent<TextComponent>();
        text->SetFont(font);
        text->SetContent("ABC");

        UILayoutPass::LayoutCanvas(*canvas);
        REQUIRE(widget->GetGraphic() == image.get());

        LogConsoleStorage::Clear();
        image->SetActive(false);
        CHECK_FALSE(image->CanContribute());
        CHECK(widget->GetGraphic() == text.get());

        // The text is the draw source now. No GPU here, so it produces no mesh and the widget
        // degrades to "nothing to paint" instead of drawing the switched-off image.
        WidgetDrawData drawData;
        CHECK(Testing::TestAccess<WidgetComponent>::ResolveDrawSource(*widget, drawData) == nullptr);

        const std::string warning = FindContestedWarning(widget->GetOwner()->GetName());
        REQUIRE_FALSE(warning.empty());
        CHECK(warning.find("drawing 'minEngine::TextComponent'") != std::string::npos);
        CHECK(warning.find("minEngine::ImageComponent") != std::string::npos);

        image->SetActive(true);
        CHECK(widget->GetGraphic() == image.get());
        REQUIRE(Testing::TestAccess<WidgetComponent>::ResolveDrawSource(*widget, drawData) == image.get());
    }

    TEST_CASE("widget-graphic: the graphic base is abstract and cannot be created [smoke]")
    {
        EngineReflectionFixture reflectionFixture;
        REQUIRE(reflectionFixture.IsReflectionReady());
        WidgetGraphicTestScope scope;

        const Reflection::MEClass* imageClass = ImageComponent::StaticClass();
        REQUIRE(imageClass != nullptr);
        const Reflection::MEClass* graphicClass = imageClass->GetSuperClass();
        REQUIRE(graphicClass != nullptr);
        CHECK(std::string(graphicClass->GetName()) == "minEngine::WidgetGraphicComponent");
        CHECK(graphicClass->HasSpecifier(Reflection::ClassSpecifier::Abstract));

        // The reflection factory refuses abstract types, so NewObject on the base yields null
        // instead of an object with unimplemented drawing.
        CHECK(graphicClass->CreateDefaultInstance() == nullptr);

        const Reflection::MEClass* textClass = TextComponent::StaticClass();
        REQUIRE(textClass != nullptr);
        CHECK(textClass->GetSuperClass() == graphicClass);
    }

    TEST_CASE("widget-graphic: switching a canvas off invalidates the widgets below it [smoke]")
    {
        EngineReflectionFixture reflectionFixture;
        REQUIRE(reflectionFixture.IsReflectionReady());
        WidgetGraphicTestScope scope;

        const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("widget-graphic-canvas");

        std::shared_ptr<CanvasComponent> canvas;
        const std::shared_ptr<GameObject> canvasGo = MakeCanvas(*scene, Vector2(200.0f, 200.0f), canvas);
        const std::shared_ptr<WidgetComponent> panel = MakeWidgetNode(*scene, *canvasGo, Vector2(100.0f, 100.0f), "graphic-panel");
        const std::shared_ptr<GameObject> childGo = scene->CreateGameObject();
        childGo->AddComponent<SceneComponent>();
        const std::shared_ptr<WidgetComponent> child = childGo->AddComponent<WidgetComponent>();
        child->ApplyAnchorPreset(EUIAnchorPreset::TopLeft);
        child->SetSize(Vector2(50.0f, 50.0f));
        child->SetStableOrder(5u);
        REQUIRE(childGo->AttachToParent(panel->GetOwner(), AttachmentTransformRules::KeepRelativeTransform));
        panel->GetOwner()->AddComponent<ImageComponent>();
        childGo->AddComponent<ImageComponent>();

        UILayoutPass::LayoutCanvas(*canvas);

        const ScreenUIHitResult hitBefore = HitTestReferencePoint(*scene, Vector2(20.0f, 20.0f), Vector2(200.0f, 200.0f));
        REQUIRE(hitBefore.bHit);
        REQUIRE(hitBefore.Widget == child.get());

        const bool panelActiveSelf = panel->IsActiveSelf();
        const bool panelVisibleSelf = panel->IsVisibleSelf();
        const bool panelGoActive = panel->GetOwner()->IsActiveSelf();
        const bool panelGoVisible = panel->GetOwner()->IsVisibleSelf();

        Testing::TestAccess<WidgetComponent>::ClearRenderStateDirty(*panel);
        Testing::TestAccess<WidgetComponent>::ClearRenderStateDirty(*child);

        canvas->SetActive(false);

        // Without the subtree invalidation the widgets below keep their last proxy and the picture
        // stays on screen until something unrelated rebuilds it.
        CHECK(Testing::TestAccess<WidgetComponent>::IsRenderStateDirty(*panel));
        CHECK(Testing::TestAccess<WidgetComponent>::IsRenderStateDirty(*child));
        CHECK_FALSE(HitTestReferencePoint(*scene, Vector2(20.0f, 20.0f), Vector2(200.0f, 200.0f)).bHit);

        // Invalidation only: no authored flag moves, so switching the canvas back on restores the
        // author's choice untouched.
        CHECK(panel->IsActiveSelf() == panelActiveSelf);
        CHECK(panel->IsVisibleSelf() == panelVisibleSelf);
        CHECK(panel->GetOwner()->IsActiveSelf() == panelGoActive);
        CHECK(panel->GetOwner()->IsVisibleSelf() == panelGoVisible);

        Testing::TestAccess<WidgetComponent>::ClearRenderStateDirty(*panel);
        Testing::TestAccess<WidgetComponent>::ClearRenderStateDirty(*child);

        canvas->SetActive(true);

        CHECK(Testing::TestAccess<WidgetComponent>::IsRenderStateDirty(*panel));
        CHECK(Testing::TestAccess<WidgetComponent>::IsRenderStateDirty(*child));
        const ScreenUIHitResult hitBack = HitTestReferencePoint(*scene, Vector2(20.0f, 20.0f), Vector2(200.0f, 200.0f));
        REQUIRE(hitBack.bHit);
        CHECK(hitBack.Widget == child.get());

        // A widget with no graphic contributes no picture and takes no pointer.
        const std::shared_ptr<GameObject> emptyGo = scene->CreateGameObject();
        emptyGo->AddComponent<SceneComponent>();
        const std::shared_ptr<WidgetComponent> empty = emptyGo->AddComponent<WidgetComponent>();
        empty->ApplyAnchorPreset(EUIAnchorPreset::StretchAll);
        REQUIRE(emptyGo->AttachToParent(panel->GetOwner(), AttachmentTransformRules::KeepRelativeTransform));
        CHECK_FALSE(empty->HasGraphic());
        CHECK(empty->GetGraphic() == nullptr);
    }
}
