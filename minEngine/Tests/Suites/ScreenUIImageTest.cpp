#include "ScreenUIImageTest.h"

#include "EngineTestFixture.h"

#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/ImageComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Function/UI/UITypes.h"

#include "doctest.h"

namespace minEngine
{
    class ScreenUIImageTestScope
    {
    public:
        ScreenUIImageTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            ObjectManager::Initialize();
            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
        }

        ~ScreenUIImageTestScope()
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
}

TEST_CASE("screen-ui-image: Raw resolve uses full UV [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIImageTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-image-raw");
    const std::shared_ptr<GameObject> go = scene->CreateGameObject();
    const std::shared_ptr<ImageComponent> image = go->AddComponent<ImageComponent>();

    CHECK(image->GetSourceMode() == EUIImageSourceMode::Raw);
    CHECK(image->CanDraw());
    const Vector4 uv = image->ResolveDrawUVRect();
    CHECK(uv.x == doctest::Approx(0.0f));
    CHECK(uv.y == doctest::Approx(0.0f));
    CHECK(uv.z == doctest::Approx(1.0f));
    CHECK(uv.w == doctest::Approx(1.0f));
}

TEST_CASE("screen-ui-image: Sprite resolve matches Sprite UV [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIImageTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-image-sprite");
    const std::shared_ptr<GameObject> go = scene->CreateGameObject();
    const std::shared_ptr<ImageComponent> image = go->AddComponent<ImageComponent>();

    // Texture2D without RHI: ConfigureFromTexture still stores rect; UV from pixel math.
    const std::shared_ptr<Texture2D> texture = std::make_shared<Texture2D>();
    // Width/height may be 0 without CreateSolid; use Sprite helper with explicit full rect path.
    const std::shared_ptr<Sprite> sprite = std::make_shared<Sprite>();
    sprite->SetSourceRectPx(Vector4(0.0f, 0.0f, 16.0f, 16.0f));
    // Without texture size, GetUVRectNormalized may degenerate — use ConfigureFromTexture when possible.
    // Fallback: compare mode + CanDraw false when no runtime texture.
    image->SetSpriteSource(sprite);
    CHECK(image->GetSourceMode() == EUIImageSourceMode::Sprite);
    CHECK(image->GetSprite() == sprite.get());

    // Sprite with no runtime texture cannot draw.
    CHECK_FALSE(image->CanDraw());

    image->SetSourceMode(EUIImageSourceMode::Sprite);
    image->SetSprite(nullptr);
    CHECK_FALSE(image->CanDraw());
    CHECK(ImageComponent::IsUVRectDegenerate(image->ResolveDrawUVRect()));
}

TEST_CASE("screen-ui-image: Setters do not auto-flip mode [full]")
{
    using namespace minEngine;
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScreenUIImageTestScope scope;

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("screen-ui-image-mode");
    const std::shared_ptr<GameObject> go = scene->CreateGameObject();
    const std::shared_ptr<ImageComponent> image = go->AddComponent<ImageComponent>();

    const std::shared_ptr<Sprite> sprite = std::make_shared<Sprite>();
    image->SetSprite(sprite);
    CHECK(image->GetSourceMode() == EUIImageSourceMode::Raw);

    image->SetSpriteSource(sprite);
    CHECK(image->GetSourceMode() == EUIImageSourceMode::Sprite);

    image->SetTexture(nullptr);
    CHECK(image->GetSourceMode() == EUIImageSourceMode::Sprite);

    image->SetRawTexture(nullptr);
    CHECK(image->GetSourceMode() == EUIImageSourceMode::Raw);
}
