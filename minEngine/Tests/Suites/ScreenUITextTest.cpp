#include "ScreenUITextTest.h"

#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/CanvasComponent.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/Components/TextComponent.h"
#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/UI/Font/FontGlyphAtlas.h"
#include "Runtime/Function/UI/Font/TextMeshBuilder.h"
#include "Runtime/Function/UI/UITypes.h"
#include "Runtime/Resource/Loaders/FontLoader.h"

#include <filesystem>
#include <fstream>
#include <vector>

namespace minEngine
{
    class ScreenUITextTestScope
    {
    public:
        ScreenUITextTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            ObjectManager::Initialize();
            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
        }

        ~ScreenUITextTestScope()
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

    namespace
    {
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

        bool RunAtlasAsciiMeasure()
        {
            std::vector<uint8_t> bytes;
            if (!LoadInterFontBytes(bytes))
            {
                ME_LOG(LogTest, Error, "ScreenUIText: failed to locate Inter_18pt-Regular.ttf.");
                return false;
            }

            FontGlyphAtlas atlas;
            if (!atlas.Initialize(bytes, 32.0f))
            {
                ME_LOG(LogTest, Error, "ScreenUIText: atlas Initialize failed.");
                return false;
            }

            const float width = atlas.MeasureTextWidth("Hi");
            if (width <= 0.0f)
            {
                ME_LOG(LogTest, Error, "ScreenUIText: MeasureTextWidth(Hi) expected > 0.");
                return false;
            }

            FontGlyphInfo missing;
            if (atlas.EnsureGlyph(0x10FFFF, missing))
            {
                // Extremely unlikely to exist; if it does, still ok — just skip assert.
            }
            else
            {
                // Missing glyph must not crash; EnsureGlyph returns false (skip policy).
            }

            if (atlas.MeasureTextWidth("") != 0.0f)
            {
                ME_LOG(LogTest, Error, "ScreenUIText: empty string width should be 0.");
                return false;
            }

            TextMeshBuildResult mesh;
            if (!TextMeshBuilder::Build(atlas, "Hi", Vector2(200.0f, 64.0f), EUITextHorizontalAlignment::Left, mesh))
            {
                ME_LOG(LogTest, Error, "ScreenUIText: TextMeshBuilder failed.");
                return false;
            }
            if (mesh.QuadCount == 0)
            {
                ME_LOG(LogTest, Error, "ScreenUIText: expected glyph quads for 'Hi'.");
                return false;
            }
            return true;
        }

        bool RunTextComponentWithoutFont()
        {
            ScreenUITextTestScope scope;
            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-text-nofont");
            const std::shared_ptr<GameObject> go = scene->CreateGameObject();
            go->AddComponent<SceneComponent>();
            go->AddComponent<WidgetComponent>();
            const std::shared_ptr<TextComponent> text = go->AddComponent<TextComponent>();
            text->SetContent("Hello");
            if (text->CanAttemptDraw())
            {
                ME_LOG(LogTest, Error, "ScreenUIText: CanAttemptDraw should be false without Font.");
                return false;
            }
            return true;
        }

        bool RunTextComponentWithFont()
        {
            ScreenUITextTestScope scope;
            std::vector<uint8_t> bytes;
            if (!LoadInterFontBytes(bytes))
            {
                ME_LOG(LogTest, Error, "ScreenUIText: failed to locate font for TextComponent test.");
                return false;
            }

            const std::shared_ptr<Font> font = FontLoader::CreateFromBytes(std::move(bytes));
            if (!font || !font->IsValid())
            {
                ME_LOG(LogTest, Error, "ScreenUIText: CreateFromBytes failed.");
                return false;
            }

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-text-font");
            const std::shared_ptr<GameObject> canvasGo = scene->CreateGameObject();
            canvasGo->AddComponent<SceneComponent>();
            canvasGo->AddComponent<CanvasComponent>();

            const std::shared_ptr<GameObject> textGo = scene->CreateGameObject();
            textGo->AddComponent<SceneComponent>();
            const std::shared_ptr<WidgetComponent> widget = textGo->AddComponent<WidgetComponent>();
            widget->SetSize(Vector2(200.0f, 48.0f));
            const std::shared_ptr<TextComponent> text = textGo->AddComponent<TextComponent>();
            text->SetFont(font);
            text->SetContent("ABC");
            text->SetFontSize(24.0f);

            if (!text->CanAttemptDraw())
            {
                ME_LOG(LogTest, Error, "ScreenUIText: CanAttemptDraw should be true with Font.");
                return false;
            }
            return true;
        }
    }

    bool RunScreenUITextTests()
    {
        return RunAtlasAsciiMeasure() && RunTextComponentWithoutFont() && RunTextComponentWithFont();
    }
}

#include "doctest.h"
#include "EngineTestFixture.h"

TEST_CASE("screen-ui-text: atlas measure and text component [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunScreenUITextTests());
}
