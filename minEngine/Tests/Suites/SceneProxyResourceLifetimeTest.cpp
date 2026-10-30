#include "SceneProxyResourceLifetimeTest.h"

#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Log/LogConsole.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Threading/RenderCommandPipe.h"
#include "Runtime/Function/Framework/Components/StaticMeshComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Render/DrawCommands/MeshBatchCollector.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawPacket.h"
#include "Runtime/Function/Render/DrawCommands/MeshPassProcessor.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/PrimitiveSceneProxy.h"
#include "Runtime/Function/Render/RenderPipeline/RenderQueueBuild.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/SceneRenderContext.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"
#include "Runtime/Function/Render/StaticMesh.h"

#include "doctest.h"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

using minEngine::MeshBatch;
using minEngine::MeshBatchCollector;
using minEngine::MeshCollectContext;
using minEngine::MeshDrawCommand;
using minEngine::MeshDrawPacket;
using minEngine::OpaqueMeshPassProcessor;
using minEngine::PrimitiveSceneProxy;
using minEngine::RHI;
using minEngine::RHIBuffer;
using minEngine::RHIBufferCreateDesc;
using minEngine::RHICommandList;
using minEngine::RHIGraphicsPSOStateFallback;
using minEngine::RHIVertexElement;
using minEngine::RHIVertexInputLayout;
using minEngine::StaticMesh;
using minEngine::StaticMeshComponent;
using minEngine::StaticMeshSectionInfo;

namespace
{
    /**
     * Buffer that reports how often its description was read. Draw submission must never need the
     * description: the draw range travels with the packet, so a submission-time read would show up
     * here as a count that moved.
     */
    class DescribedBuffer final : public RHIBuffer
    {
    public:
        explicit DescribedBuffer(uint32_t elementCount)
        {
            m_Desc.ElementCount = elementCount;
        }

        const RHIBufferCreateDesc& GetDesc() const override
        {
            ++DescQueryCount;
            return m_Desc;
        }

        void UpdateSubresource(const void*, uint32_t, uint32_t) override {}

        mutable uint32_t DescQueryCount = 0;

    private:
        RHIBufferCreateDesc m_Desc;
    };

    class StubVertexInputLayout final : public RHIVertexInputLayout
    {
    public:
        StubVertexInputLayout()
        {
            m_Elements.emplace_back("a_Position", minEngine::VertexElementType::Float3);
        }

        const std::vector<RHIVertexElement>& GetElements() const override { return m_Elements; }
        uint32_t GetStride() const override { return 12u; }

    private:
        std::vector<RHIVertexElement> m_Elements;
    };

    /** Records the draw ranges a submission actually issued. */
    class RecordingRHI final : public RHI
    {
    public:
        void Initialize() override {}
        void Shutdown() override {}

        std::shared_ptr<minEngine::RHITexture> RHICreateTexture2D(
            const minEngine::RHITextureCreateDesc&,
            const void*) override
        {
            return nullptr;
        }
        std::shared_ptr<minEngine::RHIShaderResourceView> RHICreateShaderResourceView(
            const minEngine::RHITextureSRVDesc&) override
        {
            return nullptr;
        }
        std::shared_ptr<RHIBuffer> RHICreateBuffer(const RHIBufferCreateDesc&, const void*) override
        {
            return nullptr;
        }
        std::shared_ptr<minEngine::RHIShader> RHICreateShader(
            const minEngine::RHIShaderCreateDesc&,
            std::string*) override
        {
            return nullptr;
        }
        std::shared_ptr<minEngine::RHIShader> RHICreateShader(
            const std::string&,
            const std::string&,
            std::string*) override
        {
            return nullptr;
        }
        std::shared_ptr<minEngine::RHIGraphicsPipelineState> RHICreateGraphicsPipelineState(
            const minEngine::RHIGraphicsPSODesc&) override
        {
            return nullptr;
        }
        std::shared_ptr<minEngine::RHIShaderBindingSetLayout> RHICreateShaderBindingSetLayout(
            const std::vector<minEngine::RHIShaderBindingSetLayoutEntry>&) override
        {
            return nullptr;
        }
        std::shared_ptr<minEngine::RHIPipelineLayout> RHICreatePipelineLayout(
            const std::vector<minEngine::RHIShaderBindingSetLayout*>&) override
        {
            return nullptr;
        }
        std::shared_ptr<minEngine::RHIShaderBindingSet> RHICreateShaderBindingSet(
            minEngine::RHIShaderBindingSetLayout*,
            const std::vector<minEngine::RHIShaderBinding>&) override
        {
            return nullptr;
        }
        std::shared_ptr<minEngine::RHISampler> RHIGetOrCreateSampler(
            const minEngine::RHISamplerDesc&) override
        {
            return nullptr;
        }
        std::shared_ptr<RHIVertexInputLayout> RHICreateVertexInputLayout(
            std::initializer_list<RHIVertexElement>) override
        {
            return nullptr;
        }

        void RHICmdBeginRenderPass(const minEngine::RHIRenderPassInfo&) override {}
        void RHICmdEndRenderPass() override {}
        void RHICmdSetGraphicsPipelineState(minEngine::RHIGraphicsPipelineState*) override {}
        void RHICmdSetShaderBindingSet(uint32_t, minEngine::RHIShaderBindingSet*) override {}
        void RHICmdTransition(const minEngine::RHITextureTransitionInfo&) override {}
        void RHICmdSetViewport(uint32_t, uint32_t, uint32_t, uint32_t, bool) override {}
        void RHICmdSetVertexBuffer(RHIBuffer*, uint32_t) override {}
        void RHICmdSetIndexBuffer(RHIBuffer*) override {}
        void RHICmdDrawIndexed(uint32_t indexCount, uint32_t, int32_t) override
        {
            ++IndexedDrawCount;
            LastIndexCount = indexCount;
        }
        void RHICmdDraw(uint32_t vertexCount, uint32_t) override
        {
            ++DrawCount;
            LastVertexCount = vertexCount;
        }
        void RHICmdGenerateMips(minEngine::RHITexture*) override {}
        void RHISetBackbufferClearColor(const minEngine::Math::Vector3&) override {}
        void RHIClearBackbuffer() override {}
        void RHIPresent() override {}

        uint32_t IndexedDrawCount = 0;
        uint32_t DrawCount = 0;
        uint32_t LastIndexCount = 0;
        uint32_t LastVertexCount = 0;
    };

    /** ObjectManager + SceneManager, the minimum the component tree needs to exist. */
    class ProxyLifetimeScope
    {
    public:
        ProxyLifetimeScope()
        {
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(&m_ObjectManager);
            minEngine::ObjectManager::Initialize();

            minEngine::Testing::TestAccess<minEngine::SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
        }

        ~ProxyLifetimeScope()
        {
            m_SceneManager.Shutdown();
            minEngine::Testing::TestAccess<minEngine::SceneManager>::SetInstance(nullptr);

            minEngine::ObjectManager::Shutdown();
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(nullptr);
        }

    private:
        minEngine::ObjectManager m_ObjectManager;
        minEngine::SceneManager m_SceneManager;
    };

    /** A mesh whose geometry ranges come from a section table, exactly like a loaded asset. */
    std::shared_ptr<StaticMesh> MakeSectionedMesh(
        uint32_t sectionIndexCount,
        uint32_t indexBufferElementCount,
        uint32_t vertexBufferElementCount,
        std::shared_ptr<DescribedBuffer>& outIndexBuffer)
    {
        std::shared_ptr<StaticMesh> mesh = std::make_shared<StaticMesh>();
        mesh->m_VertexBuffer = std::make_shared<DescribedBuffer>(vertexBufferElementCount);
        mesh->m_VertexInputLayout = std::make_shared<StubVertexInputLayout>();
        mesh->m_IndexBuffer = std::make_shared<DescribedBuffer>(indexBufferElementCount);
        outIndexBuffer = std::static_pointer_cast<DescribedBuffer>(mesh->m_IndexBuffer);

        StaticMeshSectionInfo section;
        section.MaterialIndex = 0;
        section.FirstIndex = 0;
        section.NumIndices = sectionIndexCount;
        mesh->m_Sections.push_back(section);
        return mesh;
    }
}

TEST_CASE("scene-proxy-lifetime: proxy keeps mesh buffers alive after the asset is released [smoke]")
{
    ProxyLifetimeScope scope;

    const std::shared_ptr<minEngine::Scene> scene =
        minEngine::SceneManager::Get().CreateNewScene("scene-proxy-lifetime-keepalive");
    REQUIRE(scene != nullptr);
    const std::shared_ptr<minEngine::GameObject> gameObject = scene->CreateGameObject();
    REQUIRE(gameObject != nullptr);
    const std::shared_ptr<StaticMeshComponent> component =
        gameObject->AddComponent<StaticMeshComponent>();
    REQUIRE(component != nullptr);

    std::shared_ptr<DescribedBuffer> indexBuffer;
    std::shared_ptr<StaticMesh> mesh = MakeSectionedMesh(36u, 36u, 24u, indexBuffer);
    const std::weak_ptr<RHIBuffer> vertexBufferWeak = mesh->m_VertexBuffer;
    const std::weak_ptr<RHIVertexInputLayout> layoutWeak = mesh->m_VertexInputLayout;
    const std::weak_ptr<RHIBuffer> indexBufferWeak = mesh->m_IndexBuffer;

    component->SetMesh(mesh);

    PrimitiveSceneProxy* proxy = component->CreateSceneProxy();
    REQUIRE(proxy != nullptr);
    REQUIRE(proxy->m_VertexBuffer != nullptr);
    REQUIRE(proxy->m_IndexBuffer != nullptr);
    REQUIRE(proxy->m_VertexInputLayout != nullptr);

    // The game thread drops every handle it owns: the component is re-pointed and the last asset
    // reference goes away. A proxy holding raw pointers only would now be pointing at freed GPU
    // memory while it is still queued for the render thread.
    component->SetMesh(nullptr);
    mesh.reset();
    indexBuffer.reset();

    CHECK_FALSE(vertexBufferWeak.expired());
    CHECK_FALSE(layoutWeak.expired());
    CHECK_FALSE(indexBufferWeak.expired());
    CHECK(proxy->m_VertexBuffer == vertexBufferWeak.lock().get());
    CHECK(proxy->m_IndexBuffer == indexBufferWeak.lock().get());

    // The render thread can still build a batch from what the proxy holds.
    const std::shared_ptr<minEngine::Material> material = std::make_shared<minEngine::Material>();
    proxy->SetMaterialSource(material);
    {
        MeshBatchCollector collector;
        MeshCollectContext context;
        proxy->GetMeshBatches(collector, context);
        CHECK(collector.GetBatches().size() == 1u);

        // The collected batch is an owner in its own right: a draw is built here and submitted
        // later, so the geometry it names has to outlive the proxy that produced it.
        component->DetachSceneProxy();
        delete proxy;
        CHECK_FALSE(vertexBufferWeak.expired());
        CHECK_FALSE(indexBufferWeak.expired());
    }

    // Releasing the last thing that named the geometry is what releases it.
    CHECK(vertexBufferWeak.expired());
    CHECK(layoutWeak.expired());
    CHECK(indexBufferWeak.expired());
}

TEST_CASE("scene-proxy-lifetime: a scene proxy keeps drawing after its material asset dies [smoke]")
{
    ProxyLifetimeScope scope;

    const std::shared_ptr<minEngine::Scene> scene =
        minEngine::SceneManager::Get().CreateNewScene("scene-proxy-lifetime-material");
    REQUIRE(scene != nullptr);
    const std::shared_ptr<minEngine::GameObject> gameObject = scene->CreateGameObject();
    REQUIRE(gameObject != nullptr);
    const std::shared_ptr<StaticMeshComponent> component =
        gameObject->AddComponent<StaticMeshComponent>();
    REQUIRE(component != nullptr);

    std::shared_ptr<DescribedBuffer> indexBuffer;
    const std::shared_ptr<StaticMesh> mesh = MakeSectionedMesh(6u, 6u, 4u, indexBuffer);
    component->SetMesh(mesh);

    std::shared_ptr<minEngine::Material> material = std::make_shared<minEngine::Material>();
    const std::weak_ptr<minEngine::MaterialInterface> materialWeak = material;
    component->SetMaterial(material);

    PrimitiveSceneProxy* proxy = component->CreateSceneProxy();
    REQUIRE(proxy != nullptr);
    minEngine::MaterialRenderProxy* const renderProxy = proxy->GetMaterialProxy();
    REQUIRE(renderProxy != nullptr);
    CHECK(renderProxy == material->GetRenderProxy());

    // The asset is released on the role that owns it, so it is destroyed right here. The render
    // object it published survives on the scene proxy's own reference and stops reading the tables
    // the asset took with it.
    component->SetMaterial(nullptr);
    material.reset();
    CHECK(materialWeak.expired());
    CHECK(proxy->GetMaterialProxy() == renderProxy);

    // A draw built after that point carries the render object, never the asset, and answers from
    // the snapshot it published while the asset was still alive.
    MeshBatchCollector collector;
    MeshCollectContext context;
    proxy->GetMeshBatches(collector, context);
    REQUIRE(collector.GetBatches().size() == 1u);
    CHECK(collector.GetBatches()[0].m_MaterialProxy == renderProxy);
    CHECK_FALSE(renderProxy->IsCompiledForDraw());
    CHECK(renderProxy->GetBaseMaterial() == nullptr);

    component->DetachSceneProxy();
    delete proxy;
}

TEST_CASE("scene-proxy-lifetime: index count reaches submission without reading the buffer [smoke]")
{
    ProxyLifetimeScope scope;

    const std::shared_ptr<minEngine::Scene> scene =
        minEngine::SceneManager::Get().CreateNewScene("scene-proxy-lifetime-index-count");
    REQUIRE(scene != nullptr);
    const std::shared_ptr<minEngine::GameObject> gameObject = scene->CreateGameObject();
    REQUIRE(gameObject != nullptr);
    const std::shared_ptr<StaticMeshComponent> component =
        gameObject->AddComponent<StaticMeshComponent>();
    REQUIRE(component != nullptr);

    // The section table says 36 indices; the index buffer claims 777. A draw that reads the buffer
    // instead of the metadata would submit 777 and show up as the wrong range here.
    std::shared_ptr<DescribedBuffer> indexBuffer;
    const std::shared_ptr<StaticMesh> mesh = MakeSectionedMesh(36u, 777u, 24u, indexBuffer);
    component->SetMesh(mesh);

    std::shared_ptr<minEngine::Material> material = std::make_shared<minEngine::Material>();
    component->SetMaterial(material);

    PrimitiveSceneProxy* proxy = component->CreateSceneProxy();
    REQUIRE(proxy != nullptr);
    CHECK(proxy->m_IndexCount == 36u);

    MeshBatchCollector collector;
    MeshCollectContext context;
    proxy->GetMeshBatches(collector, context);
    REQUIRE(collector.GetBatches().size() == 1u);
    CHECK(collector.GetBatches()[0].m_IndexCount == 36u);

    std::vector<MeshDrawCommand> commands;
    OpaqueMeshPassProcessor processor(commands);
    collector.DispatchTo(processor);
    REQUIRE(commands.size() == 1u);
    CHECK(commands[0].m_IndexCount == 36u);

    // The same conversion every pass uses to turn a draw command into a packet.
    MeshDrawPacket packet;
    packet.PipelineState = std::make_shared<RHIGraphicsPSOStateFallback>();
    packet.BindDrawCommand(commands[0]);

    const uint32_t descQueriesBeforeSubmit = indexBuffer->DescQueryCount;

    RecordingRHI rhi;
    RHICommandList cmdList(&rhi);
    cmdList.SubmitMeshDrawPacket(packet);

    CHECK(rhi.IndexedDrawCount == 1u);
    CHECK(rhi.LastIndexCount == 36u);
    CHECK(indexBuffer->DescQueryCount == descQueriesBeforeSubmit);

    // A packet with no recorded range draws nothing: submission has no fallback that would ask the
    // buffer how much it holds.
    MeshDrawPacket rangeless;
    rangeless.PipelineState = std::make_shared<RHIGraphicsPSOStateFallback>();
    rangeless.IndexBuffer = commands[0].m_IndexBuffer;
    cmdList.SubmitMeshDrawPacket(rangeless);
    CHECK(rhi.IndexedDrawCount == 1u);
    CHECK(indexBuffer->DescQueryCount == descQueriesBeforeSubmit);

    component->DetachSceneProxy();
    delete proxy;
}

namespace
{
    /** Records that a material died and which role ran the destructor, outside the object itself. */
    class MaterialDeathProbe
    {
    public:
        std::atomic<bool> bDestroyed{ false };
        std::atomic<std::thread::id> ThreadOfDestruction{};
    };

    class ProbedMaterial final : public minEngine::Material
    {
    public:
        ~ProbedMaterial() override
        {
            if (Probe)
            {
                Probe->ThreadOfDestruction.store(std::this_thread::get_id(), std::memory_order_release);
                Probe->bDestroyed.store(true, std::memory_order_release);
            }
        }

        std::shared_ptr<MaterialDeathProbe> Probe;
    };

    /** Registries a component tree needs, plus the command pipe a removal travels through. */
    class ProxyRemovalScope
    {
    public:
        ProxyRemovalScope()
        {
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(&m_ObjectManager);
            minEngine::ObjectManager::Initialize();

            minEngine::Testing::TestAccess<minEngine::SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();

            m_Pipe.Initialize();
        }

        ~ProxyRemovalScope()
        {
            m_Pipe.Shutdown();
            m_SceneManager.Shutdown();
            minEngine::Testing::TestAccess<minEngine::SceneManager>::SetInstance(nullptr);
            minEngine::ObjectManager::Shutdown();
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(nullptr);
        }

        ProxyRemovalScope(const ProxyRemovalScope&) = delete;
        ProxyRemovalScope& operator=(const ProxyRemovalScope&) = delete;

        minEngine::RenderCommandPipe& Pipe() { return m_Pipe; }

    private:
        minEngine::ObjectManager m_ObjectManager;
        minEngine::SceneManager m_SceneManager;
        minEngine::RenderCommandPipe m_Pipe;
    };
}

TEST_CASE("scene-proxy-lifetime: a draw command outlives the proxy that produced it [smoke]")
{
    ProxyRemovalScope scope;

    const std::shared_ptr<minEngine::Scene> scene =
        minEngine::SceneManager::Get().CreateNewScene("scene-proxy-lifetime-command-window");
    REQUIRE(scene != nullptr);
    scene->EnsureRenderScene();
    minEngine::RenderScene* renderScene = scene->GetRenderScene();
    REQUIRE(renderScene != nullptr);

    const std::shared_ptr<minEngine::GameObject> gameObject = scene->CreateGameObject();
    REQUIRE(gameObject != nullptr);
    const std::shared_ptr<StaticMeshComponent> component =
        gameObject->AddComponent<StaticMeshComponent>();
    REQUIRE(component != nullptr);

    std::shared_ptr<DescribedBuffer> indexBuffer;
    component->SetMesh(MakeSectionedMesh(36u, 36u, 24u, indexBuffer));
    component->SetActive(true);
    component->ReconcileEffectiveActivation();
    REQUIRE(component->IsActive());

    const std::shared_ptr<MaterialDeathProbe> probe = std::make_shared<MaterialDeathProbe>();
    std::shared_ptr<minEngine::Material> material = std::make_shared<ProbedMaterial>();
    static_cast<ProbedMaterial*>(material.get())->Probe = probe;
    const std::weak_ptr<minEngine::Material> materialWeak = material;
    component->SetMaterial(material);
    // Watched from here, and only through a weak reference: after the asset dies the scene proxy is
    // the render object's last owner, so this answers "is the proxy still alive".
    const std::weak_ptr<minEngine::MaterialRenderProxy> renderObjectWeak = material->GetRenderProxyRef();
    material.reset();

    renderScene->AddPrimitive(component.get());
    scope.Pipe().PublishGameFrame(0u, 1u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(0));
    REQUIRE(component->GetSceneProxy() != nullptr);

    // The component lets its asset go. The asset dies on this role - the role that owns it - and
    // what keeps drawing is the render object the scene proxy now holds on its own.
    component->SetMaterial(nullptr);
    REQUIRE(materialWeak.expired());
    CHECK(probe->bDestroyed.load(std::memory_order_acquire));
    CHECK(probe->ThreadOfDestruction.load(std::memory_order_acquire) == std::this_thread::get_id());

    // The frame records. A pass keeps the cells of its queue, so this command is exactly the state
    // submission walks in the frame that a removal lands after.
    minEngine::SceneRenderContext ctx;
    ctx.Scene = renderScene;
    minEngine::BuildPrimitiveRenderQueue(ctx, nullptr);
    REQUIRE(ctx.OpaqueQueue.size() == 1u);
    const MeshDrawCommand recordedCommand = ctx.OpaqueQueue[0];
    minEngine::MaterialRenderProxy* const recordedProxy = recordedCommand.m_MaterialProxy;
    REQUIRE(recordedProxy != nullptr);

    // The removal lands after the frame was recorded: it marks the proxy, takes it off the draw
    // list and parks it. That is the point where a command built from it used to be left pointing
    // at memory the removal had already freed.
    renderScene->RemovePrimitive(component.get());
    scope.Pipe().PublishGameFrame(1u, 2u);
    CHECK(scope.Pipe().FlushApplyForFrameChecked(1));
    CHECK(renderScene->m_PrimitiveSceneProxies.empty());
    CHECK(recordedCommand.m_MaterialProxy == recordedProxy);
    // The crash was this virtual call: it must answer, not fault.
    CHECK_FALSE(recordedProxy->IsCompiledForDraw());

    // The removal keeps the proxy alive for exactly one step longer than the command it produced,
    // and the frame end is that step: the render object goes then, as a reference count drop - no
    // material asset and no registry entry is involved.
    CHECK_FALSE(renderObjectWeak.expired());
    minEngine::RenderScene::FlushPendingProxyDeletes();
    CHECK(renderObjectWeak.expired());
    CHECK(probe->ThreadOfDestruction.load(std::memory_order_acquire) == std::this_thread::get_id());
}

TEST_CASE("scene-proxy-lifetime: re-pointing a proxy releases the material it drew with [smoke]")
{
    ProxyLifetimeScope scope;

    const std::shared_ptr<minEngine::Scene> scene =
        minEngine::SceneManager::Get().CreateNewScene("scene-proxy-lifetime-rebind");
    REQUIRE(scene != nullptr);
    const std::shared_ptr<minEngine::GameObject> gameObject = scene->CreateGameObject();
    REQUIRE(gameObject != nullptr);
    const std::shared_ptr<StaticMeshComponent> component =
        gameObject->AddComponent<StaticMeshComponent>();
    REQUIRE(component != nullptr);

    std::shared_ptr<DescribedBuffer> indexBuffer;
    component->SetMesh(MakeSectionedMesh(6u, 6u, 4u, indexBuffer));

    std::shared_ptr<minEngine::Material> first = std::make_shared<minEngine::Material>();
    const std::weak_ptr<minEngine::Material> firstWeak = first;
    const std::shared_ptr<minEngine::Material> second = std::make_shared<minEngine::Material>();

    PrimitiveSceneProxy* proxy = component->CreateSceneProxy();
    REQUIRE(proxy != nullptr);
    proxy->SetMaterialSource(first);

    minEngine::MaterialRenderProxy* const firstProxy = proxy->GetMaterialProxy();
    REQUIRE(firstProxy != nullptr);
    CHECK(firstProxy == first->GetRenderProxy());

    MeshBatchCollector collector;
    MeshCollectContext context;
    proxy->GetMeshBatches(collector, context);
    REQUIRE(collector.GetBatches().size() == 1u);
    CHECK(collector.GetBatches()[0].m_MaterialProxy == firstProxy);

    // What the proxy keeps for a frame that recorded against the old binding is the render object,
    // not the asset: this snapshot is the only handle on it from here on.
    const minEngine::MaterialRenderProxyDataRef firstSnapshot = firstProxy->AcquirePublishedData();
    REQUIRE(firstSnapshot != nullptr);
    CHECK(firstSnapshot.use_count() >= 2);

    // The asset dies here - the render side never owned it - while the render object it published
    // is still held by the proxy's own slots.
    first.reset();
    CHECK(firstWeak.expired());

    // The draw is re-pointed at another material. Everything re-read afterwards must be the new
    // proxy, not a cached copy of the old; the old one moves to the previous-binding slot.
    proxy->SetMaterialSource(second);
    CHECK(proxy->GetMaterialProxy() != nullptr);
    CHECK(proxy->GetMaterialProxy() == second->GetRenderProxy());
    CHECK(proxy->GetMaterialProxy() != firstProxy);
    CHECK(proxy->m_PreviousMaterialSource.get() == firstProxy);

    MeshBatchCollector recollected;
    proxy->GetMeshBatches(recollected, context);
    REQUIRE(recollected.GetBatches().size() == 1u);
    CHECK(recollected.GetBatches()[0].m_MaterialProxy == second->GetRenderProxy());

    // The previous binding is still alive, so a command recorded against it can still submit, and
    // the snapshot it published is still held by the object sitting in that slot.
    CHECK_FALSE(firstProxy->IsCompiledForDraw());
    CHECK(firstSnapshot.use_count() >= 2);

    // Dropping the proxy releases both references. Nothing here was registered anywhere, so the
    // release is a reference count drop on this role, with no owner left behind and no queue.
    component->DetachSceneProxy();
    delete proxy;

    CHECK(firstSnapshot.use_count() == 1);
}

namespace
{
    /**
     * One primitive whose scene proxy is the last owner of its render object.
     *
     * The material asset is released before the removal, so the render object it published has no
     * owner left but the scene proxy: a weak reference to it therefore answers exactly one
     * question - is the scene proxy still alive.
     */
    struct LoadedPrimitive
    {
        std::shared_ptr<minEngine::Scene> scene;
        std::shared_ptr<minEngine::GameObject> gameObject;
        std::shared_ptr<StaticMeshComponent> component;
        minEngine::RenderScene* renderScene = nullptr;

        std::shared_ptr<MaterialDeathProbe> materialProbe;
        minEngine::MaterialRenderProxy* renderObject = nullptr;
        std::weak_ptr<minEngine::MaterialRenderProxy> renderObjectWeak;
    };

    LoadedPrimitive LoadOnePrimitive(ProxyRemovalScope& scope, const char* sceneName)
    {
        LoadedPrimitive loaded;
        loaded.scene = minEngine::SceneManager::Get().CreateNewScene(sceneName);
        REQUIRE(loaded.scene != nullptr);
        loaded.scene->EnsureRenderScene();
        loaded.renderScene = loaded.scene->GetRenderScene();
        REQUIRE(loaded.renderScene != nullptr);

        loaded.gameObject = loaded.scene->CreateGameObject();
        REQUIRE(loaded.gameObject != nullptr);
        loaded.component = loaded.gameObject->AddComponent<StaticMeshComponent>();
        REQUIRE(loaded.component != nullptr);

        std::shared_ptr<DescribedBuffer> indexBuffer;
        loaded.component->SetMesh(MakeSectionedMesh(36u, 36u, 24u, indexBuffer));
        loaded.component->SetActive(true);
        loaded.component->ReconcileEffectiveActivation();
        REQUIRE(loaded.component->IsActive());

        loaded.materialProbe = std::make_shared<MaterialDeathProbe>();
        std::shared_ptr<minEngine::Material> material = std::make_shared<ProbedMaterial>();
        static_cast<ProbedMaterial*>(material.get())->Probe = loaded.materialProbe;
        loaded.renderObject = material->GetRenderProxy();
        loaded.renderObjectWeak = material->GetRenderProxyRef();
        loaded.component->SetMaterial(material);
        material.reset();

        loaded.renderScene->AddPrimitive(loaded.component.get());
        scope.Pipe().PublishGameFrame(0u, 1u);
        REQUIRE(scope.Pipe().FlushApplyForFrameChecked(0));
        REQUIRE(loaded.component->GetSceneProxy() != nullptr);

        // The asset goes; the render object it published does not, because the scene proxy holds
        // it. From here the proxy is its only owner, so its death is the proxy's death.
        loaded.component->SetMaterial(nullptr);
        REQUIRE(loaded.materialProbe->bDestroyed.load(std::memory_order_acquire));
        REQUIRE_FALSE(loaded.renderObjectWeak.expired());
        return loaded;
    }
}

TEST_CASE("scene-proxy-lifetime: a removal parks the proxy and the frame end deletes it [smoke]")
{
    ProxyRemovalScope scope;
    LoadedPrimitive loaded = LoadOnePrimitive(scope, "scene-proxy-parked-removal");

    // One earlier frame recorded this primitive, so a command naming it exists while the removal
    // below is on its way.
    minEngine::SceneRenderContext ctx;
    ctx.Scene = loaded.renderScene;
    minEngine::BuildPrimitiveRenderQueue(ctx, nullptr);
    REQUIRE(ctx.OpaqueQueue.size() == 1u);
    REQUIRE(ctx.OpaqueQueue[0].m_MaterialProxy == loaded.renderObject);

    const uint64_t deletesBefore = loaded.renderScene->m_ProxyDeleteCount;
    loaded.renderScene->RemovePrimitive(loaded.component.get());
    scope.Pipe().PublishGameFrame(1u, 2u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(1));

    // Removal did its three things and nothing more: marked, off the draw list, still owned. The
    // command recorded above can therefore still be read through the proxy that produced it.
    CHECK(loaded.renderScene->m_PrimitiveSceneProxies.empty());
    CHECK(loaded.renderScene->m_ProxyDeleteCount == deletesBefore);
    CHECK_FALSE(loaded.renderObjectWeak.expired());
    CHECK_FALSE(loaded.renderObject->IsCompiledForDraw());

    // Nothing but the frame end destroys a scene proxy, and it destroys it once.
    minEngine::RenderScene::FlushPendingProxyDeletes();
    CHECK(loaded.renderObjectWeak.expired());
    CHECK(loaded.renderScene->m_ProxyDeleteCount == deletesBefore + 1);

    minEngine::RenderScene::FlushPendingProxyDeletes();
    CHECK(loaded.renderScene->m_ProxyDeleteCount == deletesBefore + 1);
}

TEST_CASE("scene-proxy-lifetime: a frame that draws nothing still reaches the frame-end delete [smoke]")
{
    ProxyRemovalScope scope;
    LoadedPrimitive loaded = LoadOnePrimitive(scope, "scene-proxy-no-draw-frame");

    // No queue is built for this frame and there is no camera or target: the frame produces no draw
    // command at all. Its removals still have to be applied and its parked proxies still have to
    // die, so the delete point cannot be something that only a drawn frame reaches.
    loaded.renderScene->RemovePrimitive(loaded.component.get());
    scope.Pipe().PublishGameFrame(1u, 2u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(1));
    REQUIRE(loaded.renderScene->m_PrimitiveSceneProxies.empty());
    REQUIRE_FALSE(loaded.renderObjectWeak.expired());
    // Parked, not destroyed: the container holds it and the scene reports work in flight.
    CHECK(loaded.renderScene->HasPendingProxyWork());

    minEngine::RenderScene::FlushPendingProxyDeletes();
    CHECK(loaded.renderObjectWeak.expired());
    CHECK_FALSE(loaded.renderScene->HasPendingProxyWork());
}

TEST_CASE("scene-proxy-lifetime: a recreate adds the new proxy without the frame end touching it [smoke]")
{
    ProxyRemovalScope scope;
    LoadedPrimitive loaded = LoadOnePrimitive(scope, "scene-proxy-recreate-same-frame");

    // Recreate in one frame: the old proxy is removed and the new one registered by two commands
    // applied in that order. The old one is parked, the new one is in the draw list.
    std::shared_ptr<minEngine::Material> replacement = std::make_shared<minEngine::Material>();
    minEngine::MaterialRenderProxy* const newRenderObject = replacement->GetRenderProxy();
    loaded.component->SetMaterial(replacement);
    loaded.renderScene->RecreatePrimitive(loaded.component.get());
    scope.Pipe().PublishGameFrame(1u, 2u);
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(1));

    minEngine::PrimitiveSceneProxy* const newProxy = loaded.component->GetSceneProxy();
    REQUIRE(newProxy != nullptr);
    REQUIRE(newProxy != nullptr);
    REQUIRE_FALSE(loaded.renderObjectWeak.expired());
    REQUIRE(loaded.renderScene->m_PrimitiveSceneProxies.size() == 1u);
    CHECK(loaded.renderScene->m_PrimitiveSceneProxies[0] == newProxy);

    minEngine::SceneRenderContext ctx;
    ctx.Scene = loaded.renderScene;
    minEngine::BuildPrimitiveRenderQueue(ctx, nullptr);
    REQUIRE(ctx.OpaqueQueue.size() == 1u);
    CHECK(ctx.OpaqueQueue[0].m_MaterialProxy == newRenderObject);

    // The frame end deletes the proxy removed in this same frame and leaves the new one, which is
    // in the draw list, untouched.
    minEngine::RenderScene::FlushPendingProxyDeletes();
    CHECK(loaded.renderObjectWeak.expired());
    CHECK(loaded.renderScene->m_PrimitiveSceneProxies.size() == 1u);
    CHECK(loaded.renderScene->m_PrimitiveSceneProxies[0] == newProxy);
    CHECK(newProxy->GetMaterialProxy() == newRenderObject);

    // Leave the scene empty: the replacement goes the same way, one frame later.
    loaded.renderScene->RemovePrimitive(loaded.component.get());
    scope.Pipe().PublishGameFrame(2u, 3u);
    CHECK(scope.Pipe().FlushApplyForFrameChecked(2));
    minEngine::RenderScene::FlushPendingProxyDeletes();
    CHECK(loaded.renderScene->m_PrimitiveSceneProxies.empty());
}

TEST_CASE("scene-proxy-lifetime: a scene destroyed with a live proxy reports it [smoke]")
{
    minEngine::LogSystem::Initialize();
    minEngine::LogConsoleStorage::Clear();

    ProxyRemovalScope scope;
    const std::shared_ptr<minEngine::Scene> scene =
        minEngine::SceneManager::Get().CreateNewScene("scene-proxy-destroyed-with-primitive");
    REQUIRE(scene != nullptr);

    const std::shared_ptr<minEngine::GameObject> gameObject = scene->CreateGameObject();
    REQUIRE(gameObject != nullptr);
    const std::shared_ptr<StaticMeshComponent> component =
        gameObject->AddComponent<StaticMeshComponent>();
    REQUIRE(component != nullptr);
    std::shared_ptr<DescribedBuffer> indexBuffer;
    component->SetMesh(MakeSectionedMesh(36u, 36u, 24u, indexBuffer));
    component->SetActive(true);
    component->ReconcileEffectiveActivation();
    REQUIRE(component->IsActive());

    {
        // Its own scene, destroyed with a primitive nobody removed: the contract violation is
        // reported when it goes, and it is not swept up by a bulk delete that would hide it.
        minEngine::RenderScene renderScene;
        renderScene.AddPrimitive(component.get());
        scope.Pipe().PublishGameFrame(0u, 1u);
        REQUIRE(scope.Pipe().FlushApplyForFrameChecked(0));
        REQUIRE(renderScene.m_PrimitiveSceneProxies.size() == 1u);
    }
    component->DetachSceneProxy();

    bool reported = false;
    for (const minEngine::LogRecord& record : minEngine::LogConsoleStorage::Snapshot())
    {
        if (record.severity != minEngine::LogSeverity::Error)
        {
            continue;
        }
        if (std::string(record.GetChannelName()) != "Render")
        {
            continue;
        }
        if (record.message.find("still registered") != std::string::npos)
        {
            reported = true;
        }
    }
    CHECK(reported);
}

TEST_CASE("scene-proxy-lifetime: a change made during the game frame is in that frame's recording [smoke]")
{
    ProxyRemovalScope scope;
    LoadedPrimitive loaded = LoadOnePrimitive(scope, "scene-proxy-same-frame-change");

    // The game role moves the primitive and hands the change over as a command of its frame. It
    // must not touch the proxy itself: the render role owns it.
    minEngine::Transform moved = loaded.component->GetTransform();
    moved.Position = minEngine::Math::Vector3(4.0f, 0.0f, 0.0f);
    loaded.component->SetTransform(moved);
    loaded.renderScene->UpdatePrimitiveTransform(loaded.component.get());
    scope.Pipe().PublishGameFrame(1u, 2u);

    // Until the frame applies its commands, the proxy still carries the state of the frame before.
    REQUIRE(loaded.renderScene->m_PrimitiveSceneProxies.size() == 1u);
    CHECK(loaded.renderScene->m_PrimitiveSceneProxies[0]->m_Transform.Position.x == doctest::Approx(0.0f));

    // The frame applies first and records afterwards, so the change is in this frame's recording -
    // there is no frame in which the proxy still draws the old position.
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(1));

    minEngine::SceneRenderContext ctx;
    ctx.Scene = loaded.renderScene;
    minEngine::BuildPrimitiveRenderQueue(ctx, nullptr);
    REQUIRE(ctx.OpaqueQueue.size() == 1u);
    CHECK(ctx.OpaqueQueue[0].m_ModelMatrix[3].x == doctest::Approx(4.0f));

    minEngine::RenderScene::FlushPendingProxyDeletes();
}

TEST_CASE("scene-proxy-lifetime: a frame that applied a removal records nothing for that proxy and keeps it alive [smoke]")
{
    ProxyRemovalScope scope;
    LoadedPrimitive loaded = LoadOnePrimitive(scope, "scene-proxy-removal-during-recording");

    const uint64_t deletesBefore = loaded.renderScene->m_ProxyDeleteCount;
    loaded.renderScene->RemovePrimitive(loaded.component.get());
    scope.Pipe().PublishGameFrame(1u, 2u);

    // The removal is applied at the start of the frame, before anything is recorded.
    REQUIRE(scope.Pipe().FlushApplyForFrameChecked(1));
    REQUIRE(loaded.renderScene->m_PrimitiveSceneProxies.empty());
    REQUIRE(loaded.renderScene->HasPendingProxyWork());

    // Recording the frame produces no draw command from it - the proxy is parked, not deleted, so
    // the recording window is never a window onto freed memory.
    minEngine::SceneRenderContext ctx;
    ctx.Scene = loaded.renderScene;
    minEngine::BuildPrimitiveRenderQueue(ctx, nullptr);
    CHECK(ctx.OpaqueQueue.empty());
    CHECK_FALSE(loaded.renderObjectWeak.expired());
    CHECK(loaded.renderScene->m_ProxyDeleteCount == deletesBefore);

    // The frame end frees it, exactly once.
    minEngine::RenderScene::FlushPendingProxyDeletes();
    CHECK(loaded.renderObjectWeak.expired());
    CHECK(loaded.renderScene->m_ProxyDeleteCount == deletesBefore + 1);
    CHECK_FALSE(loaded.renderScene->HasPendingProxyWork());
}


TEST_CASE("scene-proxy-lifetime: a draw command and its packet keep the vertex layout the asset dropped [smoke]")
{
    ProxyLifetimeScope scope;

    const std::shared_ptr<minEngine::Scene> scene =
        minEngine::SceneManager::Get().CreateNewScene("scene-proxy-lifetime-layout-handle");
    REQUIRE(scene != nullptr);
    const std::shared_ptr<minEngine::GameObject> gameObject = scene->CreateGameObject();
    REQUIRE(gameObject != nullptr);
    const std::shared_ptr<StaticMeshComponent> component =
        gameObject->AddComponent<StaticMeshComponent>();
    REQUIRE(component != nullptr);

    std::shared_ptr<DescribedBuffer> indexBuffer;
    std::shared_ptr<StaticMesh> mesh = MakeSectionedMesh(36u, 36u, 24u, indexBuffer);
    const std::weak_ptr<RHIVertexInputLayout> layoutWeak = mesh->m_VertexInputLayout;
    component->SetMesh(mesh);

    std::shared_ptr<minEngine::Material> material = std::make_shared<minEngine::Material>();
    component->SetMaterial(material);

    PrimitiveSceneProxy* proxy = component->CreateSceneProxy();
    REQUIRE(proxy != nullptr);

    // Collect and convert inside one scope: from here on the command and the packet are the only
    // things naming the layout, so whichever of them holds it is what keeps it alive.
    std::vector<MeshDrawCommand> commands;
    {
        MeshBatchCollector collector;
        MeshCollectContext context;
        proxy->GetMeshBatches(collector, context);
        REQUIRE(collector.GetBatches().size() == 1u);

        OpaqueMeshPassProcessor processor(commands);
        collector.DispatchTo(processor);
    }
    REQUIRE(commands.size() == 1u);
    REQUIRE(commands[0].m_VertexInputLayout != nullptr);
    const RHIVertexInputLayout* const layoutAddress = commands[0].m_VertexInputLayout;

    MeshDrawPacket packet;
    packet.PipelineState = std::make_shared<RHIGraphicsPSOStateFallback>();
    packet.BindDrawCommand(commands[0]);
    CHECK(packet.VertexInputLayout == layoutAddress);

    // The proxy is removed on the render role and the asset is released on the game one, exactly as
    // they are when a mesh component is destroyed mid-frame. The draw built from it was already
    // handed to the frame, so the draw is what has to keep the layout.
    component->DetachSceneProxy();
    delete proxy;
    component->SetMesh(nullptr);
    mesh.reset();

    CHECK_FALSE(layoutWeak.expired());
    CHECK(commands[0].m_VertexInputLayout == layoutAddress);
    CHECK(packet.VertexInputLayout == layoutAddress);
    CHECK(packet.VertexInputLayoutRef.get() == layoutAddress);

    // A submission that named a layout nobody holds would hand the device an address that was
    // already released. The packet holding it is what makes this draw valid.
    RecordingRHI rhi;
    RHICommandList cmdList(&rhi);
    cmdList.SubmitMeshDrawPacket(packet);
    CHECK(rhi.IndexedDrawCount == 1u);
    CHECK(rhi.LastIndexCount == 36u);

    const RHIVertexInputLayout* const drainedAddress = layoutAddress;
    packet = MeshDrawPacket();
    commands.clear();
    CHECK(layoutWeak.expired());
    (void)drainedAddress;
}
