#include "MaterialRenderProxyDataTest.h"

#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "EngineTestFixture.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Core/Threading/RenderCommandPipe.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Material/MaterialCapability.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompiler.h"
#include "Runtime/Function/Render/Material/MaterialTestGraph.h"
#include "Runtime/Function/Render/MaterialInstance.h"
#include "Runtime/Function/Render/MaterialRenderProxy.h"
#include "Runtime/Function/Render/OpenGL/OpenGLRHI.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/PrimitiveSceneProxy.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RenderPipeline/RenderQueueBuild.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/SceneRenderContext.h"
#include "Runtime/Function/Render/ScreenUI/ScreenUIMaterialFactory.h"
#include "Runtime/Function/Render/EngineShaderUtils.h"
#include "Runtime/Function/Render/Sprite/SpriteMaterialFactory.h"
#include "Runtime/Function/Render/Sprite/SpriteQuadMesh.h"
#include "Runtime/Function/Render/Texture.h"

#include "doctest.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace
{
    using minEngine::LinearColor;
    using minEngine::Material;
    using minEngine::MaterialRenderProxy;
    using minEngine::MaterialRenderProxyDataRef;
    using minEngine::MeshDrawCommand;
    using minEngine::RHICommandList;
    using minEngine::SceneRenderContext;

    /** Headless GL 4.6 context; releases the engine's process-lifetime GPU caches while it lives. */
    class ScopedHeadlessGl46Context
    {
    public:
        ScopedHeadlessGl46Context()
        {
            if (glfwGetCurrentContext() != nullptr)
            {
                return;
            }

            if (!glfwInit())
            {
                return;
            }

            m_OwnsGlfw = true;
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            m_Window = glfwCreateWindow(1, 1, "MaterialRenderProxyData", nullptr, nullptr);
            if (m_Window == nullptr)
            {
                return;
            }

            glfwMakeContextCurrent(m_Window);
            if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
            {
                glfwDestroyWindow(m_Window);
                m_Window = nullptr;
            }
        }

        ~ScopedHeadlessGl46Context()
        {
            if (glfwGetCurrentContext() != nullptr)
            {
                minEngine::EngineShaderUtils::ClearProgramCache();
                minEngine::SpriteMaterialFactory::ClearTemplateCache();
                minEngine::ScreenUIMaterialFactory::ClearCachedResources();
                minEngine::SpriteQuadMesh::Get().Shutdown();
            }

            if (m_Window != nullptr)
            {
                glfwDestroyWindow(m_Window);
                m_Window = nullptr;
            }
            if (m_OwnsGlfw)
            {
                glfwTerminate();
            }
        }

        ScopedHeadlessGl46Context(const ScopedHeadlessGl46Context&) = delete;
        ScopedHeadlessGl46Context& operator=(const ScopedHeadlessGl46Context&) = delete;

        bool IsReady() const { return glfwGetCurrentContext() != nullptr; }

    private:
        GLFWwindow* m_Window = nullptr;
        bool m_OwnsGlfw = false;
    };

    /** Tests run from bin/; point the shader template root at the source tree explicitly. */
    class ScopedEngineDefaultAssetsRoot
    {
    public:
        ScopedEngineDefaultAssetsRoot()
        {
            const std::filesystem::path candidates[] = {
                std::filesystem::path("../Assets/EngineDefault"),
                std::filesystem::path("Assets/EngineDefault"),
                std::filesystem::path("../../minEngine/Assets/EngineDefault"),
            };
            for (const std::filesystem::path& candidate : candidates)
            {
                if (std::filesystem::exists(candidate / "Shaders" / "Template"))
                {
                    minEngine::PathRegistry::Get().SetEngineDefaultAssetsRootOverride(
                        std::filesystem::absolute(candidate));
                    m_bApplied = true;
                    return;
                }
            }
        }

        ~ScopedEngineDefaultAssetsRoot()
        {
            if (m_bApplied)
            {
                minEngine::PathRegistry::Get().ClearEngineDefaultAssetsRootOverride();
            }
        }

        ScopedEngineDefaultAssetsRoot(const ScopedEngineDefaultAssetsRoot&) = delete;
        ScopedEngineDefaultAssetsRoot& operator=(const ScopedEngineDefaultAssetsRoot&) = delete;

        bool IsApplied() const { return m_bApplied; }

    private:
        bool m_bApplied = false;
    };

    /** Registries a Scene destructor and the command pipe touch. */
    class MaterialProxyScope
    {
    public:
        MaterialProxyScope()
        {
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(&m_ObjectManager);
            minEngine::ObjectManager::Initialize();

            minEngine::Testing::TestAccess<minEngine::SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();

            m_Pipe.Initialize();
        }

        ~MaterialProxyScope()
        {
            m_Pipe.Shutdown();
            m_SceneManager.Shutdown();
            minEngine::Testing::TestAccess<minEngine::SceneManager>::SetInstance(nullptr);
            minEngine::ObjectManager::Shutdown();
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(nullptr);
        }

        MaterialProxyScope(const MaterialProxyScope&) = delete;
        MaterialProxyScope& operator=(const MaterialProxyScope&) = delete;

        minEngine::RenderCommandPipe& Pipe() { return m_Pipe; }

    private:
        minEngine::ObjectManager m_ObjectManager;
        minEngine::SceneManager m_SceneManager;
        minEngine::RenderCommandPipe m_Pipe;
    };

    class StubBuffer final : public minEngine::RHIBuffer
    {
    public:
        const minEngine::RHIBufferCreateDesc& GetDesc() const override { return m_Desc; }
        void UpdateSubresource(const void*, uint32_t, uint32_t) override {}

    private:
        minEngine::RHIBufferCreateDesc m_Desc;
    };

    class StubVertexInputLayout final : public minEngine::RHIVertexInputLayout
    {
    public:
        StubVertexInputLayout()
        {
            m_Elements.emplace_back("a_Position", minEngine::VertexElementType::Float3);
        }

        const std::vector<minEngine::RHIVertexElement>& GetElements() const override { return m_Elements; }
        uint32_t GetStride() const override { return 12u; }

    private:
        std::vector<minEngine::RHIVertexElement> m_Elements;
    };

    /**
     * A material with a compiled program and a published snapshot, built without going through
     * RenderSystem: the compiler takes the RHI it should link with as a parameter.
     */
    std::shared_ptr<Material> MakeCompiledMaterial(minEngine::RHI& rhi)
    {
        std::shared_ptr<Material> material = std::make_shared<Material>();
        minEngine::PopulateSmokeMaterialGraph(*material);
        // The smoke graph is wired for a shaded model; unlit would prune the Metallic scalar and a
        // material with no scalar at all cannot show a value-only publish.
        material->m_ShadingModel = minEngine::MaterialShadingModel::BlinnPhong;
        material->m_BlendMode = minEngine::MaterialBlendMode::Opaque;
        minEngine::MaterialCapabilityUtil::PruneInvalidMaterialOutputLinks(*material);

        minEngine::MaterialCompileContext ctx;
        ctx.RHI = &rhi;
        if (!minEngine::MaterialCompiler::Compile(*material, ctx))
        {
            return nullptr;
        }

        return material;
    }
}

TEST_CASE("material-proxy-data: drawing a batch reads no game object [smoke]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedEngineDefaultAssetsRoot engineAssets;
    REQUIRE(engineAssets.IsApplied());
    MaterialProxyScope scope;
    ScopedHeadlessGl46Context glContext;
    REQUIRE(glContext.IsReady());

    OpenGLRHI rhi;

    const std::shared_ptr<Material> material = ScreenUIMaterialFactory::CreateInstance(rhi);
    REQUIRE(material != nullptr);
    const std::shared_ptr<Texture2D> texture = ScreenUIMaterialFactory::GetOrCreateWhiteTexture(rhi);
    REQUIRE(texture != nullptr);
    ScreenUIMaterialFactory::ApplyColorAndTexture(
        *material, LinearColor(0.5f, 0.25f, 1.0f, 0.75f), texture, rhi);

    const std::shared_ptr<Scene> scene = NewObject<Scene>("material-proxy-data");
    REQUIRE(scene != nullptr);
    scene->EnsureRenderScene();
    RenderScene* renderScene = scene->GetRenderSceneShared().get();
    REQUIRE(renderScene != nullptr);

    std::unique_ptr<PrimitiveSceneProxy> proxy(new PrimitiveSceneProxy());
    proxy->m_Enabled = true;
    proxy->SetMeshResources(
        std::make_shared<StubBuffer>(),
        std::make_shared<StubVertexInputLayout>(),
        std::make_shared<StubBuffer>(),
        6u,
        4u);
    proxy->SetMaterialSource(material);
    REQUIRE(proxy->GetMaterialProxy() != nullptr);
    renderScene->m_PrimitiveSceneProxies.push_back(proxy.get());

    MaterialRenderProxy* materialProxy = proxy->GetMaterialProxy();

    // The window starts here: mesh collection, the pass processor and the material binding have to
    // answer from the proxy's own data. Nothing in it may reach the material.
    materialProxy->SetGameObjectAccessProbe(true);

    SceneRenderContext ctx;
    ctx.Scene = renderScene;
    BuildPrimitiveRenderQueue(ctx, nullptr);
    const size_t queued = ctx.OpaqueQueue.size() + ctx.TranslucentQueue.size();
    REQUIRE(queued == 1u);

    RHICommandList cmdList(&rhi);
    size_t submitted = 0;
    const auto submitQueue = [&submitted, materialProxy, &cmdList](
                                 const std::vector<MeshDrawCommand>& queue)
    {
        for (const MeshDrawCommand& command : queue)
        {
            REQUIRE(command.m_MaterialProxy == materialProxy);
            if (!command.m_MaterialProxy->IsCompiledForDraw())
            {
                continue;
            }

            RHIShader* shader = command.m_bSkinned ? command.m_MaterialProxy->GetGPUShaderSkinned()
                                                   : command.m_MaterialProxy->GetGPUShader();
            if (shader == nullptr || command.m_MaterialProxy->GetMaterialShaderBindingSetLayout() == nullptr)
            {
                continue;
            }

            command.m_MaterialProxy->BindForDraw(cmdList);
            if (command.m_MaterialProxy->GetMaterialShaderBindingSet() != nullptr)
            {
                ++submitted;
            }
        }
    };
    submitQueue(ctx.OpaqueQueue);
    submitQueue(ctx.TranslucentQueue);

    const MaterialGameObjectAccess access = materialProxy->ProbeGameObjectAccess();
    materialProxy->SetGameObjectAccessProbe(false);

    std::printf(
        "[material-proxy-data] render path reads: total=%u parameter=%u owner=%u publish=%u\n",
        access.Total,
        access.ParameterReads,
        access.OwnerReads,
        access.PublishReads);

    CHECK(submitted == 1u);
    CHECK(access.ParameterReads == 0u);
    CHECK(access.OwnerReads == 0u);
    CHECK(access.PublishReads == 0u);
    CHECK(access.Total == 0u);

    // The probe is not inert: a game-role read is counted, so a zero above means "not read", not
    // "not counted".
    materialProxy->SetGameObjectAccessProbe(true);
    float tint = 0.0f;
    CHECK(materialProxy->GetScalarParameterValue("TintR", tint));
    CHECK(material->IsCompiledForDraw());
    const MaterialGameObjectAccess control = materialProxy->ProbeGameObjectAccess();
    materialProxy->SetGameObjectAccessProbe(false);
    CHECK(control.ParameterReads == 1u);
    CHECK(control.Total >= 1u);

    renderScene->m_PrimitiveSceneProxies.clear();
}

TEST_CASE("material-proxy-data: parameter writes reach the render side only on publish [smoke]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    MaterialProxyScope scope;
    ScopedHeadlessGl46Context glContext;
    REQUIRE(glContext.IsReady());

    OpenGLRHI rhi;
    const std::shared_ptr<Texture2D> texture = Texture2D::CreateSolidRGBA(rhi, 255, 128, 0, 255);
    REQUIRE(texture != nullptr);
    REQUIRE(texture->GetRHITexture() != nullptr);

    std::shared_ptr<Material> material = std::make_shared<Material>();
    material->m_ScalarParameters.push_back({"Roughness", 0, "Roughness", 0.25f});
    material->m_TextureParameters.push_back({"BaseColor", 0, "BaseColor", texture});

    MaterialRenderProxy* proxy = material->GetRenderProxy();
    REQUIRE(proxy != nullptr);
    proxy->PublishGameData();

    const MaterialRenderProxyDataRef first = proxy->AcquirePublishedData();
    REQUIRE(first != nullptr);
    REQUIRE(first->FindScalarBySlot(0) != nullptr);
    CHECK(first->FindScalarBySlot(0)->Value == doctest::Approx(0.25f));
    REQUIRE(first->FindTextureBySlot(0) != nullptr);
    CHECK(first->FindTextureBySlot(0)->Texture.get() == texture->GetRHITexture());

    material->SetScalarParameter("Roughness", 0.75f);
    material->SetTextureParameter("BaseColor", nullptr);

    // The render side still reads the snapshot it had: writes are staged, not published.
    const MaterialRenderProxyDataRef staged = proxy->AcquirePublishedData();
    REQUIRE(staged != nullptr);
    CHECK(staged->Serial == first->Serial);
    CHECK(staged->FindScalarBySlot(0)->Value == doctest::Approx(0.25f));
    CHECK(staged->FindTextureBySlot(0)->Texture.get() == texture->GetRHITexture());

    proxy->PublishPendingGameData();

    const MaterialRenderProxyDataRef published = proxy->AcquirePublishedData();
    REQUIRE(published != nullptr);
    CHECK(published->Serial == first->Serial + 1u);
    CHECK(published->FindScalarBySlot(0)->Value == doctest::Approx(0.75f));
    CHECK(published->FindTextureBySlot(0)->Texture == nullptr);

    // Nothing changed since the last publish, so a second sweep publishes no new snapshot.
    proxy->PublishPendingGameData();
    CHECK(proxy->AcquirePublishedData()->Serial == published->Serial);
}

TEST_CASE("material-proxy-data: a published snapshot outlives its material [smoke]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedEngineDefaultAssetsRoot engineAssets;
    REQUIRE(engineAssets.IsApplied());
    MaterialProxyScope scope;
    ScopedHeadlessGl46Context glContext;
    REQUIRE(glContext.IsReady());

    OpenGLRHI rhi;

    MaterialRenderProxyDataRef snapshot;
    uint64_t serial = 0;
    uint32_t generation = 0;
    {
        const std::shared_ptr<Material> material = MakeCompiledMaterial(rhi);
        REQUIRE(material != nullptr);

        MaterialRenderProxy* proxy = material->GetRenderProxy();
        REQUIRE(proxy != nullptr);
        proxy->PublishGameData();
        snapshot = proxy->AcquirePublishedData();
        REQUIRE(snapshot != nullptr);
        REQUIRE(snapshot->bCompiledForDraw);
        REQUIRE(snapshot->Shader != nullptr);
        REQUIRE(snapshot->BindingSetLayout != nullptr);

        serial = snapshot->Serial;
        generation = snapshot->CompileGeneration;
        CHECK_FALSE(snapshot->Scalars.empty());
        CHECK_FALSE(snapshot->Textures.empty());
    }

    // The material - and the proxy it owned - is gone on this thread. The snapshot keeps every GPU
    // handle it names alive, so reading it is safe and still answers with what was drawn.
    REQUIRE(snapshot != nullptr);
    CHECK(snapshot->Serial == serial);
    CHECK(snapshot->CompileGeneration == generation);
    CHECK(snapshot->bCompiledForDraw);
    CHECK(snapshot->Shader != nullptr);
    CHECK(snapshot->Shader->IsValid());
    CHECK(snapshot->BindingSetLayout != nullptr);
    CHECK_FALSE(snapshot->Scalars.empty());
    CHECK_FALSE(snapshot->Textures.empty());
    CHECK(snapshot->Shader.use_count() >= 1);
}

TEST_CASE("material-proxy-data: an instance resolves parent slots from snapshots [smoke]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    MaterialProxyScope scope;

    std::shared_ptr<Material> parent = std::make_shared<Material>();
    parent->m_ScalarParameters.push_back({"Roughness", 0, "Roughness", 0.25f});
    parent->m_ScalarParameters.push_back({"Metallic", 1, "Metallic", 0.4f});

    const std::shared_ptr<MaterialInstance> instance = MaterialInstance::Create(parent);
    REQUIRE(instance != nullptr);
    instance->SetScalarParameter("Roughness", 0.9f);

    MaterialRenderProxy* parentProxy = parent->GetRenderProxy();
    MaterialRenderProxy* instanceProxy = instance->GetRenderProxy();
    REQUIRE(parentProxy != nullptr);
    REQUIRE(instanceProxy != nullptr);

    parentProxy->PublishGameData();
    instanceProxy->PublishPendingGameData();

    const MaterialRenderProxyDataRef first = instanceProxy->AcquirePublishedData();
    REQUIRE(first != nullptr);
    REQUIRE(first->FindScalarBySlot(0) != nullptr);
    CHECK(first->FindScalarBySlot(0)->Name == "Roughness");
    CHECK(first->FindScalarBySlot(0)->Value == doctest::Approx(0.9f));
    REQUIRE(first->FindScalarBySlot(1) != nullptr);
    CHECK(first->FindScalarBySlot(1)->Value == doctest::Approx(0.4f));

    // The parent gains a slot and changes a value; the instance follows on the next publish without
    // ever reading the parent material itself.
    parent->m_ScalarParameters[1].Value = 0.7f;
    parent->m_ScalarParameters.push_back({"Specular", 2, "Specular", 0.33f});
    parentProxy->PublishGameData();

    instanceProxy->PublishPendingGameData();
    const MaterialRenderProxyDataRef updated = instanceProxy->AcquirePublishedData();
    REQUIRE(updated != nullptr);
    CHECK(updated->Serial == first->Serial + 1u);
    REQUIRE(updated->FindScalarBySlot(1) != nullptr);
    CHECK(updated->FindScalarBySlot(1)->Value == doctest::Approx(0.7f));
    REQUIRE(updated->FindScalarBySlot(2) != nullptr);
    CHECK(updated->FindScalarBySlot(2)->Name == "Specular");
    CHECK(updated->FindScalarBySlot(2)->Value == doctest::Approx(0.33f));
    CHECK(updated->FindScalarBySlot(0)->Value == doctest::Approx(0.9f));
}

TEST_CASE("material-proxy-data: a compile generation change rebuilds the binding set once [smoke]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedEngineDefaultAssetsRoot engineAssets;
    REQUIRE(engineAssets.IsApplied());
    MaterialProxyScope scope;
    ScopedHeadlessGl46Context glContext;
    REQUIRE(glContext.IsReady());

    OpenGLRHI rhi;
    MaterialCompileContext ctx;
    ctx.RHI = &rhi;

    const std::shared_ptr<Material> material = MakeCompiledMaterial(rhi);
    REQUIRE(material != nullptr);

    MaterialRenderProxy* proxy = material->GetRenderProxy();
    REQUIRE(proxy != nullptr);
    REQUIRE(proxy->IsCompiledForDraw());

    RHICommandList cmdList(&rhi);
    CHECK(proxy->GetBindingSetRebuildCount() == 0u);

    proxy->BindForDraw(cmdList);
    CHECK(proxy->GetBindingSetRebuildCount() == 1u);
    CHECK(proxy->GetMaterialShaderBindingSet() != nullptr);

    // Same snapshot: the set built from it is reused.
    proxy->BindForDraw(cmdList);
    CHECK(proxy->GetBindingSetRebuildCount() == 1u);

    // A value-only publish rewrites the uniform buffer and keeps the set.
    const MaterialRenderProxyDataRef beforeRecompile = proxy->AcquirePublishedData();
    REQUIRE(beforeRecompile != nullptr);
    REQUIRE_FALSE(beforeRecompile->Scalars.empty());
    const std::string scalarName = beforeRecompile->Scalars.front().Name;
    material->SetScalarParameter(scalarName, 0.5f);
    proxy->PublishPendingGameData();
    proxy->BindForDraw(cmdList);
    CHECK(proxy->GetBindingSetRebuildCount() == 1u);

    // Recompiling moves the generation; the next draw rebuilds exactly one binding set.
    REQUIRE(MaterialCompiler::Compile(*material, ctx));
    proxy->BindForDraw(cmdList);
    CHECK(proxy->GetBindingSetRebuildCount() == 2u);
    CHECK(proxy->AcquirePublishedData()->CompileGeneration != beforeRecompile->CompileGeneration);

    proxy->BindForDraw(cmdList);
    CHECK(proxy->GetBindingSetRebuildCount() == 2u);
}

TEST_CASE("material-proxy-data: a compiled render object draws after its material asset dies [smoke]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedEngineDefaultAssetsRoot engineAssets;
    REQUIRE(engineAssets.IsApplied());
    MaterialProxyScope scope;
    ScopedHeadlessGl46Context glContext;
    REQUIRE(glContext.IsReady());

    OpenGLRHI rhi;

    std::shared_ptr<Material> material = MakeCompiledMaterial(rhi);
    REQUIRE(material != nullptr);
    MaterialRenderProxyRef renderProxy = material->GetRenderProxyRef();
    REQUIRE(renderProxy != nullptr);
    const MaterialRenderProxyDataRef snapshot = renderProxy->AcquirePublishedData();
    REQUIRE(snapshot != nullptr);
    REQUIRE(snapshot->bCompiledForDraw);

    PrimitiveSceneProxy sceneProxy;
    sceneProxy.SetMaterialSource(material);
    REQUIRE(sceneProxy.GetMaterialProxy() == renderProxy.get());

    // The asset goes away on the role that owned it, taking its tables with it. What the scene
    // proxy holds is a render object, not the asset, so the draw is unaffected.
    const std::weak_ptr<Material> materialWeak = material;
    material.reset();
    CHECK(materialWeak.expired());

    CHECK(sceneProxy.GetMaterialProxy() == renderProxy.get());
    CHECK(renderProxy->IsCompiledForDraw());
    CHECK(renderProxy->GetGPUShader() != nullptr);
    CHECK(renderProxy->GetMaterialShaderBindingSetLayout() != nullptr);
    // The owner is gone, and the proxy answers so instead of walking a freed pointer.
    CHECK(renderProxy->GetBaseMaterial() == nullptr);

    // A publish with no owner keeps the last snapshot instead of replacing it with an empty one.
    renderProxy->PublishPendingGameData();
    CHECK(renderProxy->IsCompiledForDraw());
    CHECK(renderProxy->AcquirePublishedData()->Serial == snapshot->Serial);

    // Releasing every reference destroys the render object, and the snapshot it published stays
    // readable: nothing here was registered anywhere, so nothing has to be handed to another role.
    CHECK(snapshot.use_count() == 2);
    renderProxy.reset();
    // The scene proxy's slot is the last owner now, and it still holds the published snapshot.
    CHECK(snapshot.use_count() == 2);
    sceneProxy.m_MaterialSource.reset();
    sceneProxy.m_PreviousMaterialSource.reset();
    CHECK(snapshot.use_count() == 1);
}

TEST_CASE("material-proxy-data: the last render-object reference may be dropped on another thread [smoke]")
{
    using namespace minEngine;

    // A game object would unregister from ObjectManager when destroyed, and that registry is game
    // role state. This object is not one, which is what makes the release below role-agnostic.
    static_assert(
        !std::is_base_of<MEObject, MaterialRenderProxy>::value,
        "the render-side material object must not be a game object");

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedEngineDefaultAssetsRoot engineAssets;
    REQUIRE(engineAssets.IsApplied());
    MaterialProxyScope scope;
    ScopedHeadlessGl46Context glContext;
    REQUIRE(glContext.IsReady());

    OpenGLRHI rhi;

    std::shared_ptr<Material> material = MakeCompiledMaterial(rhi);
    REQUIRE(material != nullptr);
    MaterialRenderProxyRef renderProxy = material->GetRenderProxyRef();
    REQUIRE(renderProxy != nullptr);
    const MaterialRenderProxyDataRef snapshot = renderProxy->AcquirePublishedData();
    REQUIRE(snapshot != nullptr);
    CHECK(snapshot.use_count() >= 2);

    // The asset dies on the game role, while the render object is still held.
    const std::weak_ptr<Material> materialWeak = material;
    material.reset();
    REQUIRE(materialWeak.expired());

    std::atomic<std::thread::id> dropThread{};
    std::atomic<bool> bAnsweredOnThatThread{ false };
    std::thread worker(
        [proxy = std::move(renderProxy), &dropThread, &bAnsweredOnThatThread]() mutable
        {
            dropThread.store(std::this_thread::get_id());
            // Reading the published state needs nothing from the thread that built it.
            bAnsweredOnThatThread.store(proxy->IsCompiledForDraw(), std::memory_order_release);
            // Whatever role drops the last reference destroys this object; it holds no game object
            // and leaves no registry entry behind.
            proxy.reset();
        });
    worker.join();

    CHECK(dropThread.load() != std::this_thread::get_id());
    CHECK(bAnsweredOnThatThread.load(std::memory_order_acquire));
    CHECK(materialWeak.expired());
    // The render object is gone: the snapshot it published is the only thing left of it.
    CHECK(snapshot.use_count() == 1);
}
TEST_CASE("material-proxy-data: a material compiled on the game role is drawable once its program exists [smoke]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedEngineDefaultAssetsRoot engineAssets;
    REQUIRE(engineAssets.IsApplied());
    MaterialProxyScope scope;
    ScopedHeadlessGl46Context glContext;
    REQUIRE(glContext.IsReady());

    // Multi: the game role holds no drawing context, so a program cannot be created where the
    // material is compiled.
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    OpenGLRHI rhi;

    const std::shared_ptr<Material> material = MakeCompiledMaterial(rhi);
    // The compile itself succeeds: producing the GPU program is the graphics role's job.
    REQUIRE(material != nullptr);

    MaterialRenderProxy* renderProxy = material->GetRenderProxy();
    REQUIRE(renderProxy != nullptr);
    REQUIRE(material->GetGPUShader() != nullptr);

    // Compiled, not created, not failed: the state between the two, carried by data a caller can
    // read instead of guessed from a zero program name.
    CHECK(material->HasCompiledProgram());
    CHECK(material->IsProgramPendingMaterialization());
    CHECK(!material->HasProgramCreateFailed());
    CHECK(!material->GetGPUShader()->IsReady());
    CHECK(!material->GetGPUShader()->IsValid());
    CHECK(!material->IsCompiledForDraw());
    CHECK(!renderProxy->IsCompiledForDraw());
    CHECK(renderProxy->HasCompiledProgram());
    CHECK(renderProxy->IsProgramPendingMaterialization());
    // A caller that asked for the log gets an expression for "nothing has run yet" rather than
    // an empty string that reads like success.
    CHECK(!material->GetShaderCompileLog().empty());
    CHECK(rhi.GetDeferredCreateQueue().GetPendingCount() >= 1);

    // Frame start, before anything is recorded: this is the only moment with both the context and
    // the guarantee that nothing has named the program yet.
    {
        ScopedFrameThreadKind graphicsRole(FrameThreadKind::Render);
        CHECK(rhi.RHIDrainDeferredCreates() >= 1);
    }

    REQUIRE(material->GetGPUShader()->IsReady());
    CHECK(material->GetGPUShader()->IsValid());
    // The snapshot was published before the program existed and is not republished here: the
    // readiness question is asked of the handle it names, so the same snapshot turns drawable.
    CHECK(renderProxy->IsCompiledForDraw());
    CHECK(material->IsCompiledForDraw());
    CHECK(!renderProxy->IsProgramPendingMaterialization());
    CHECK(rhi.GetDeferredCreateQueue().GetFailedCount() == 0);

    threading.Shutdown();
}

