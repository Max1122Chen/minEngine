#include "MeshAssemblyDeterminismTest.h"

#include "Runtime/Core/Assert/Assert.h"
#include "Runtime/Core/Tasks/TaskSystem.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommandSort.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/MaterialRenderProxy.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/PrimitiveSceneProxy.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RenderPipeline/MeshAssemblyChunk.h"
#include "Runtime/Function/Render/RenderPipeline/RenderQueueBuild.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/SceneRenderContext.h"

#include "Support/HeadlessTestRHI.h"

#include "doctest.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace
{
    using minEngine::MaterialRenderProxy;
    using minEngine::Math::Geometry::AABB;
    using minEngine::Math::Geometry::Frustum;
    using minEngine::MeshAssemblyChunk;
    using minEngine::MeshAssemblyInput;
    using minEngine::MeshAssemblySchedule;
    using minEngine::MeshBatch;
    using minEngine::MeshBatchCollector;
    using minEngine::MeshCollectContext;
    using minEngine::MeshDrawCommand;
    using minEngine::PrimitiveSceneProxy;
    using minEngine::RenderScene;
    using minEngine::SceneRenderContext;
    using minEngine::TaskSystem;
    using minEngine::FrameThreadKind;
    using minEngine::FrameThreadMode;
    using minEngine::FrameThreading;
    using minEngine::ScopedFrameThreadKind;
    using minEngine::Math::Matrix4;
    using minEngine::Math::Vector3;

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
     * Counts the batches a proxy produced on the thread that called the assembly, and off it.
     *
     * This is what a fan-out is judged by: a body that ran on a worker produces its batches on a
     * thread that is not the caller's, and a body that was refused and repaired serially produces
     * them on the caller's thread. "The pool was asked" is not the same fact as either.
     */
    struct AssemblyThreadProbe
    {
        std::thread::id Caller = std::this_thread::get_id();
        std::atomic<uint32_t> OnCaller{ 0 };
        std::atomic<uint32_t> OffCaller{ 0 };

        void Reset()
        {
            OnCaller.store(0, std::memory_order_relaxed);
            OffCaller.store(0, std::memory_order_relaxed);
        }

        void Record()
        {
            std::atomic<uint32_t>& counter = std::this_thread::get_id() == Caller ? OnCaller : OffCaller;
            counter.fetch_add(1, std::memory_order_relaxed);
        }
    };

    /**
     * A primitive whose output the test decides.
     *
     * The proxy identity travels inside the command (vertex count and model translation), so a
     * merged queue can be read back as a sequence of proxy indices. Sub-culling by the supplied
     * frustum is honored, the way a multi-section proxy does it.
     */
    class AssemblyTestProxy final : public PrimitiveSceneProxy
    {
    public:
        AssemblyTestProxy(
            uint32_t identity,
            MaterialRenderProxy* material,
            const minEngine::RHIBufferRef& vertexBuffer,
            const minEngine::RHIVertexInputLayoutRef& vertexInputLayout,
            const minEngine::RHIBufferRef& indexBuffer)
            : m_Identity(identity)
            , m_Material(material)
            , m_VertexBuffer(vertexBuffer)
            , m_VertexInputLayout(vertexInputLayout)
            , m_IndexBuffer(indexBuffer)
        {
            m_bOutsideView = (m_Identity % 4u) == 3u;
            m_WorldBounds = SectionBounds(0);
            m_CastShadow = true;
        }

        uint32_t BatchCount = 1;

        /** Optional: records which thread this proxy produced its batches on. */
        AssemblyThreadProbe* ThreadProbe = nullptr;

        /**
         * Optional: runs while this proxy is collected, i.e. inside the assembly that is reading
         * the draw set. A case uses it to do to the set what the frame end does outside the frame.
         */
        std::function<void()> OnCollect;

        /** Moves the primitive out of the look-at view, so it is only collected for the shadows. */
        void SetOutsideView(bool bOutsideView)
        {
            m_bOutsideView = bOutsideView;
            m_WorldBounds = SectionBounds(0);
        }

        /**
         * Takes the translucent pass without asking the material. The material stays opaque, which
         * keeps the scene free of a material compile while both queues are still exercised.
         */
        void SetForceTranslucent(bool bForce) { m_bForceTranslucent = bForce; }

        void GetMeshBatches(MeshBatchCollector& collector, const MeshCollectContext& context) const override
        {
            if (ThreadProbe != nullptr)
            {
                ThreadProbe->Record();
            }
            if (OnCollect)
            {
                OnCollect();
            }

            for (uint32_t batchIndex = 0; batchIndex < BatchCount; ++batchIndex)
            {
                const AABB sectionBounds = SectionBounds(batchIndex);
                if (!context.AllowsWorldBounds(sectionBounds))
                {
                    continue;
                }

                MeshBatch batch;
                batch.m_VertexBufferRef = m_VertexBuffer;
                batch.m_VertexInputLayoutRef = m_VertexInputLayout;
                batch.m_IndexBufferRef = m_IndexBuffer;
                batch.m_VertexBuffer = m_VertexBuffer.get();
                batch.m_VertexInputLayout = m_VertexInputLayout.get();
                batch.m_IndexBuffer = m_IndexBuffer.get();
                batch.m_IndexCount = 6u * m_Identity + batchIndex + 3u;
                batch.m_VertexCount = m_Identity;
                batch.m_MaterialProxy = m_Material;
                batch.m_LocalToWorld = glm::translate(
                    Matrix4(1.0f),
                    Vector3(static_cast<float>(m_Identity), static_cast<float>(batchIndex), 0.0f));
                batch.m_BoundingBox = sectionBounds;
                batch.m_CastShadow = m_CastShadow;
                batch.m_bForceTranslucentPass = m_bForceTranslucent;
                batch.m_Proxy = this;
                collector.AddMesh(batch);
            }
        }

    private:
        AABB SectionBounds(uint32_t batchIndex) const
        {
            const bool bInsideView = !m_bOutsideView;
            const float centerX = bInsideView
                ? -1.2f + 0.1f * static_cast<float>(m_Identity % 24u)
                : 40.0f + static_cast<float>(m_Identity);
            const float centerZ = bInsideView ? 0.0f : -static_cast<float>(m_Identity);
            const float offset = 0.05f * static_cast<float>(batchIndex);

            const Vector3 center(centerX + offset, offset, centerZ);
            const Vector3 extent(0.3f, 0.3f, 0.3f);
            return AABB(center - extent, center + extent);
        }

        uint32_t m_Identity = 0;
        /** Every fourth identity starts outside the view, on the shadow-only route. */
        bool m_bOutsideView = false;
        bool m_bForceTranslucent = false;
        MaterialRenderProxy* m_Material = nullptr;
        minEngine::RHIBufferRef m_VertexBuffer;
        minEngine::RHIVertexInputLayoutRef m_VertexInputLayout;
        minEngine::RHIBufferRef m_IndexBuffer;
    };

    /** Proxies plus the resources they draw with, so no queue holds a dangling handle. */
    class AssemblyScene
    {
    public:
        /** Mixed scene: hidden, queued for removal, non-caster and multi-section proxies. */
        void Reset(uint32_t proxyCount)
        {
            m_Proxies.clear();
            m_Owned.clear();

            if (!m_VertexBuffer)
            {
                m_VertexBuffer = std::make_shared<StubBuffer>();
                m_IndexBuffer = std::make_shared<StubBuffer>();
                m_VertexInputLayout = std::make_shared<StubVertexInputLayout>();
                m_Material = std::make_shared<minEngine::Material>();
            }

            MaterialRenderProxy* materialProxy = m_Material->GetRenderProxy();

            m_Owned.reserve(proxyCount);
            for (uint32_t identity = 0; identity < proxyCount; ++identity)
            {
                std::unique_ptr<AssemblyTestProxy> proxy = std::make_unique<AssemblyTestProxy>(
                    identity,
                    materialProxy,
                    m_VertexBuffer,
                    m_VertexInputLayout,
                    m_IndexBuffer);

                proxy->SetForceTranslucent((identity % 3u) == 0u);
                proxy->BatchCount = (identity % 7u) == 0u ? 2u : 1u;
                proxy->m_CastShadow = (identity % 11u) != 0u;
                if ((identity % 5u) == 0u)
                {
                    proxy->m_Enabled = false;
                }
                if ((identity % 13u) == 0u)
                {
                    proxy->m_bPendingRemove = true;
                }

                m_Proxies.push_back(proxy.get());
                m_Owned.push_back(std::move(proxy));
            }

            // The draw set can hold a hole while a removal command is in flight; the assembly has
            // to skip it instead of producing anything for it.
            if (m_Proxies.size() > 7)
            {
                m_Proxies.insert(m_Proxies.begin() + 7, nullptr);
            }
        }

        /** Every proxy draws one batch and casts: one command per proxy, so a tampered range shows. */
        void ResetDense(uint32_t proxyCount)
        {
            Reset(proxyCount);
            m_Proxies.clear();
            for (std::unique_ptr<AssemblyTestProxy>& proxy : m_Owned)
            {
                proxy->m_Enabled = true;
                proxy->m_bPendingRemove = false;
                proxy->m_CastShadow = true;
                proxy->SetForceTranslucent(false);
                proxy->SetOutsideView(false);
                proxy->BatchCount = 1;
                m_Proxies.push_back(proxy.get());
            }
        }

        AssemblyTestProxy* GetProxy(uint32_t identity)
        {
            return identity < m_Owned.size() ? m_Owned[identity].get() : nullptr;
        }

        void SetThreadProbe(AssemblyThreadProbe* probe)
        {
            for (std::unique_ptr<AssemblyTestProxy>& proxy : m_Owned)
            {
                proxy->ThreadProbe = probe;
            }
        }

        std::vector<PrimitiveSceneProxy*> m_Proxies;

    private:
        std::vector<std::unique_ptr<AssemblyTestProxy>> m_Owned;
        minEngine::RHIBufferRef m_VertexBuffer;
        minEngine::RHIBufferRef m_IndexBuffer;
        std::shared_ptr<StubVertexInputLayout> m_VertexInputLayout;
        std::shared_ptr<minEngine::Material> m_Material;
    };

    Frustum MakeLookAtFrustum()
    {
        const Matrix4 view = glm::lookAt(
            Vector3(0.0f, 0.0f, 5.0f),
            Vector3(0.0f, 0.0f, 0.0f),
            Vector3(0.0f, 1.0f, 0.0f));
        const Matrix4 projection =
            glm::perspectiveRH_NO(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
        return Frustum::FromViewProjection(projection * view, -1.0f, 1.0f);
    }

    const Vector3 kCameraPosition(0.0f, 0.0f, 5.0f);

    /**
     * The assembly the chunked builder replaced, kept here as the reference: same loop, same two
     * collectors, same four dispatch calls, no RHI.
     */
    void BuildReferenceQueues(const MeshAssemblyInput& input, SceneRenderContext& ctx)
    {
        ctx.OpaqueQueue.clear();
        ctx.TranslucentQueue.clear();
        ctx.ShadowCasterQueue.clear();

        if (input.PrimitiveProxies == nullptr)
        {
            return;
        }

        const bool cullEnabled = input.MainFrustum != nullptr;

        MeshBatchCollector mainCollector;
        MeshBatchCollector shadowOnlyCollector;

        MeshCollectContext mainCollectContext;
        mainCollectContext.Purpose = minEngine::MeshCollectPurpose::MainView;
        if (cullEnabled)
        {
            mainCollectContext.ViewFrustum = input.MainFrustum;
        }

        MeshCollectContext shadowOnlyCollectContext;
        shadowOnlyCollectContext.Purpose = minEngine::MeshCollectPurpose::ShadowOnly;

        for (PrimitiveSceneProxy* primitiveProxy : *input.PrimitiveProxies)
        {
            if (!primitiveProxy || primitiveProxy->m_bPendingRemove)
            {
                continue;
            }

            if (!primitiveProxy->m_Enabled)
            {
                continue;
            }

            const AABB worldBounds = primitiveProxy->ResolveWorldBounds();

            bool bMainViewVisible = true;
            if (cullEnabled && worldBounds.IsValid())
            {
                bMainViewVisible = input.MainFrustum->Intersect(worldBounds);
            }

            const bool bWantShadowCollect = input.bEnableShadows && primitiveProxy->m_CastShadow;
            if (!bMainViewVisible && !bWantShadowCollect)
            {
                continue;
            }

            if (bMainViewVisible)
            {
                primitiveProxy->GetMeshBatches(mainCollector, mainCollectContext);
            }
            else
            {
                primitiveProxy->GetMeshBatches(shadowOnlyCollector, shadowOnlyCollectContext);
            }
        }

        minEngine::OpaqueMeshPassProcessor opaqueProcessor(ctx.OpaqueQueue);
        minEngine::TranslucentMeshPassProcessor translucentProcessor(ctx.TranslucentQueue);
        minEngine::ShadowDepthMeshPassProcessor shadowProcessor(ctx.ShadowCasterQueue);

        mainCollector.DispatchTo(opaqueProcessor);
        mainCollector.DispatchTo(translucentProcessor);
        mainCollector.DispatchTo(shadowProcessor);
        shadowOnlyCollector.DispatchTo(shadowProcessor);
    }

    /** The sorts the frame runs after assembly, applied to both sides so only assembly is compared. */
    void ApplyQueueSorts(SceneRenderContext& ctx)
    {
        minEngine::SortOpaqueMeshDraws(ctx.OpaqueQueue);
        minEngine::SortTranslucentMeshDraws(ctx.TranslucentQueue, kCameraPosition);
    }

    class Fnv1a64
    {
    public:
        void MixU32(uint32_t value) { MixBytes(&value, sizeof(value)); }
        void MixU64(uint64_t value) { MixBytes(&value, sizeof(value)); }
        void MixBool(bool value) { MixU32(value ? 1u : 0u); }
        void MixPointer(const void* pointer)
        {
            MixU64(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer)));
        }
        void MixFloat(float value)
        {
            uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            MixU32(bits);
        }

        uint64_t Get() const { return m_Value; }

    private:
        void MixBytes(const void* data, size_t size)
        {
            const unsigned char* bytes = static_cast<const unsigned char*>(data);
            for (size_t index = 0; index < size; ++index)
            {
                m_Value ^= static_cast<uint64_t>(bytes[index]);
                m_Value *= 1099511628211ull;
            }
        }

        uint64_t m_Value = 14695981039346656037ull;
    };

    /**
     * One digest over the fields that decide what a command draws and where it sits.
     *
     * Pointers are folded by value: both sides of a comparison run in this process against the same
     * objects, so an equal pointer is the same object and a different one is a different object.
     */
    uint64_t MakeQueueDigest(const std::vector<MeshDrawCommand>& commands)
    {
        Fnv1a64 hash;
        hash.MixU64(static_cast<uint64_t>(commands.size()));
        for (const MeshDrawCommand& command : commands)
        {
            hash.MixPointer(command.m_VertexBuffer);
            hash.MixPointer(command.m_IndexBuffer);
            hash.MixPointer(command.m_VertexInputLayout);
            hash.MixU32(command.m_IndexCount);
            hash.MixU32(command.m_VertexCount);
            hash.MixPointer(command.m_MaterialProxy);

            for (int column = 0; column < 4; ++column)
            {
                for (int row = 0; row < 4; ++row)
                {
                    hash.MixFloat(command.m_ModelMatrix[column][row]);
                }
            }

            hash.MixFloat(command.m_BoundingBox.Min.x);
            hash.MixFloat(command.m_BoundingBox.Min.y);
            hash.MixFloat(command.m_BoundingBox.Min.z);
            hash.MixFloat(command.m_BoundingBox.Max.x);
            hash.MixFloat(command.m_BoundingBox.Max.y);
            hash.MixFloat(command.m_BoundingBox.Max.z);

            hash.MixBool(command.m_CastShadow);
            hash.MixBool(command.m_bSkinned);
            hash.MixU32(command.m_BoneCount);
            hash.MixU64(command.m_SortKey.m_Key);
        }
        return hash.Get();
    }

    /**
     * Order-only digest over the identity each proxy stamps into its command.
     *
     * The scenario writes the proxy identity into `m_VertexCount`, so this reads the merged queue
     * back as a sequence of proxies without folding in a single pointer. That makes it a value a
     * case can pin: the sequence the shadow queue has to keep, whatever the storage does.
     */
    uint64_t MakeMarkerDigest(const std::vector<MeshDrawCommand>& commands)
    {
        Fnv1a64 hash;
        hash.MixU64(static_cast<uint64_t>(commands.size()));
        for (const MeshDrawCommand& command : commands)
        {
            hash.MixU32(command.m_VertexCount);
        }
        return hash.Get();
    }

    /**
     * Appends chunks in the order it is handed. Production code reaches it through
     * MergeMeshAssemblyChunks; the meta checks call it with a broken order on purpose, which is how
     * they show the equality above can fail.
     */
    void AppendChunks(const std::vector<const MeshAssemblyChunk*>& order, SceneRenderContext& ctx)
    {
        ctx.OpaqueQueue.clear();
        ctx.TranslucentQueue.clear();
        ctx.ShadowCasterQueue.clear();

        for (const MeshAssemblyChunk* chunk : order)
        {
            ctx.OpaqueQueue.insert(ctx.OpaqueQueue.end(), chunk->Opaque.begin(), chunk->Opaque.end());
            ctx.TranslucentQueue.insert(
                ctx.TranslucentQueue.end(),
                chunk->Translucent.begin(),
                chunk->Translucent.end());
        }
        for (const MeshAssemblyChunk* chunk : order)
        {
            ctx.ShadowCasterQueue.insert(
                ctx.ShadowCasterQueue.end(),
                chunk->ShadowFromMainVisible.begin(),
                chunk->ShadowFromMainVisible.end());
        }
        for (const MeshAssemblyChunk* chunk : order)
        {
            ctx.ShadowCasterQueue.insert(
                ctx.ShadowCasterQueue.end(),
                chunk->ShadowFromHidden.begin(),
                chunk->ShadowFromHidden.end());
        }
    }

    std::vector<const MeshAssemblyChunk*> ChunksInOrder(const std::vector<MeshAssemblyChunk>& chunks)
    {
        std::vector<const MeshAssemblyChunk*> order;
        order.reserve(chunks.size());
        for (const MeshAssemblyChunk& chunk : chunks)
        {
            order.push_back(&chunk);
        }
        return order;
    }

    uint32_t TotalVisited(const std::vector<MeshAssemblyChunk>& chunks)
    {
        uint32_t visited = 0;
        for (const MeshAssemblyChunk& chunk : chunks)
        {
            visited += chunk.VisitCount;
        }
        return visited;
    }

    struct FrameConfig
    {
        bool bCull = true;
        bool bEnableShadows = true;
    };

    /** The serial shape: one chunk, no pool named. */
    MeshAssemblySchedule MakeSchedule(int32_t chunkCount)
    {
        MeshAssemblySchedule schedule;
        schedule.ChunkCount = chunkCount;
        return schedule;
    }

    /** The same cut, handed to a pool. */
    MeshAssemblySchedule MakeParallelSchedule(int32_t chunkCount, TaskSystem& tasks)
    {
        MeshAssemblySchedule schedule = MakeSchedule(chunkCount);
        schedule.Tasks = &tasks;
        return schedule;
    }

    /**
     * FrameThreading is process-wide. Acquisition owns the release, so a case that fails between
     * initialise and shutdown cannot leave an identity behind for the next suite to compare against.
     */
    struct FrameThreadingScope
    {
        FrameThreading Threading;

        explicit FrameThreadingScope(FrameThreadMode mode) { Threading.Initialize(mode); }
        ~FrameThreadingScope() { Threading.Shutdown(); }

        FrameThreadingScope(const FrameThreadingScope&) = delete;
        FrameThreadingScope& operator=(const FrameThreadingScope&) = delete;
    };

    /**
     * A pool whose game role lives on another thread.
     *
     * The thread that initialises a task system becomes the role a fan-out is accepted from, so this
     * is how a case produces a refused fan-out: the assembly is called from here, where no frame
     * role is held, and the pool declines every body without telling the caller.
     */
    struct PoolOnAnotherThread
    {
        TaskSystem Tasks;
        std::thread::id ThreadId{};
        std::atomic<bool> bReady{ false };
        std::atomic<bool> bStop{ false };
        std::thread Thread;

        PoolOnAnotherThread()
        {
            Thread = std::thread([this]() {
                Tasks.Init(2);
                ThreadId = std::this_thread::get_id();
                bReady.store(true, std::memory_order_release);
                while (!bStop.load(std::memory_order_acquire))
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                Tasks.Shutdown();
            });

            while (!bReady.load(std::memory_order_acquire))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        ~PoolOnAnotherThread()
        {
            bStop.store(true, std::memory_order_release);
            Thread.join();
        }

        PoolOnAnotherThread(const PoolOnAnotherThread&) = delete;
        PoolOnAnotherThread& operator=(const PoolOnAnotherThread&) = delete;
    };

    /**
     * Registers a pool as the process instance the pipeline resolves its schedule from, the way
     * Engine does, and puts back whatever was registered before: a case that left its own pool
     * behind would hand the next case a pool it never created.
     */
    struct TaskSystemInstanceScope
    {
        explicit TaskSystemInstanceScope(TaskSystem* tasks)
            : m_Previous(TaskSystem::HasInstance() ? &TaskSystem::Get() : nullptr)
        {
            TaskSystem::SetInstance(tasks);
        }

        ~TaskSystemInstanceScope() { TaskSystem::SetInstance(m_Previous); }

        TaskSystemInstanceScope(const TaskSystemInstanceScope&) = delete;
        TaskSystemInstanceScope& operator=(const TaskSystemInstanceScope&) = delete;

    private:
        TaskSystem* m_Previous = nullptr;
    };

    /**
     * Turns the fan-out off for a scope and hands the decision back afterwards.
     *
     * The switch lives with the module that assembles, so a case that left it off would change
     * every later case in this binary.
     */
    struct MeshAssemblySwitchScope
    {
        explicit MeshAssemblySwitchScope(bool enabled) { minEngine::SetMeshAssemblyParallelEnabled(enabled); }
        ~MeshAssemblySwitchScope() { minEngine::ClearMeshAssemblyParallelOverride(); }

        MeshAssemblySwitchScope(const MeshAssemblySwitchScope&) = delete;
        MeshAssemblySwitchScope& operator=(const MeshAssemblySwitchScope&) = delete;
    };
}

TEST_CASE("mesh-assembly-determinism: the chunked assembly reproduces the serial reference [smoke]")
{
    AssemblyScene scene;
    scene.Reset(200);
    const Frustum frustum = MakeLookAtFrustum();

    const FrameConfig configs[] = {
        FrameConfig{ true, true },
        FrameConfig{ true, false },
        FrameConfig{ false, true },
        FrameConfig{ false, false },
    };

    for (const FrameConfig& config : configs)
    {
        MeshAssemblyInput input;
        input.PrimitiveProxies = &scene.m_Proxies;
        input.MainFrustum = config.bCull ? &frustum : nullptr;
        input.bEnableShadows = config.bEnableShadows;

        SceneRenderContext reference;
        BuildReferenceQueues(input, reference);
        ApplyQueueSorts(reference);

        SceneRenderContext chunked;
        minEngine::BuildMeshAssemblyQueues(input, chunked, MeshAssemblySchedule{}, nullptr);
        ApplyQueueSorts(chunked);

        INFO("cull=" << config.bCull << " shadows=" << config.bEnableShadows);

        // The scenario has to exercise every route, or an equal digest proves less than it looks.
        CHECK_FALSE(reference.OpaqueQueue.empty());
        CHECK_FALSE(reference.TranslucentQueue.empty());
        CHECK_FALSE(reference.ShadowCasterQueue.empty());

        CHECK(chunked.OpaqueQueue.size() == reference.OpaqueQueue.size());
        CHECK(chunked.TranslucentQueue.size() == reference.TranslucentQueue.size());
        CHECK(chunked.ShadowCasterQueue.size() == reference.ShadowCasterQueue.size());

        CHECK(MakeQueueDigest(chunked.OpaqueQueue) == MakeQueueDigest(reference.OpaqueQueue));
        CHECK(MakeQueueDigest(chunked.TranslucentQueue) == MakeQueueDigest(reference.TranslucentQueue));
        CHECK(MakeQueueDigest(chunked.ShadowCasterQueue) == MakeQueueDigest(reference.ShadowCasterQueue));
    }
}

TEST_CASE("mesh-assembly-determinism: the chunk count does not change the queues [smoke]")
{
    AssemblyScene scene;
    scene.Reset(200);
    const Frustum frustum = MakeLookAtFrustum();

    MeshAssemblyInput input;
    input.PrimitiveProxies = &scene.m_Proxies;
    input.MainFrustum = &frustum;
    input.bEnableShadows = true;

    const uint32_t proxyCount = static_cast<uint32_t>(scene.m_Proxies.size());

    std::vector<MeshAssemblyChunk> singleChunk;
    SceneRenderContext single;
    minEngine::BuildMeshAssemblyQueues(input, single, MakeSchedule(1), &singleChunk);
    ApplyQueueSorts(single);

    REQUIRE(singleChunk.size() == 1u);
    CHECK(singleChunk[0].FirstProxy == 0u);
    CHECK(singleChunk[0].ProxyCount == proxyCount);
    CHECK(TotalVisited(singleChunk) == proxyCount);

    const uint64_t opaqueDigest = MakeQueueDigest(single.OpaqueQueue);
    const uint64_t translucentDigest = MakeQueueDigest(single.TranslucentQueue);
    const uint64_t shadowDigest = MakeQueueDigest(single.ShadowCasterQueue);

    const uint32_t chunkCounts[] = { 2u, 3u, 7u, 17u, 400u };
    for (uint32_t chunkCount : chunkCounts)
    {
        std::vector<MeshAssemblyChunk> chunks;
        SceneRenderContext split;
        minEngine::BuildMeshAssemblyQueues(
            input,
            split,
            MakeSchedule(static_cast<int32_t>(chunkCount)),
            &chunks);
        ApplyQueueSorts(split);

        INFO("chunk count=" << chunkCount);

        REQUIRE(chunks.size() == std::min(chunkCount, proxyCount));
        CHECK(TotalVisited(chunks) == proxyCount);

        CHECK(MakeQueueDigest(split.OpaqueQueue) == opaqueDigest);
        CHECK(MakeQueueDigest(split.TranslucentQueue) == translucentDigest);
        CHECK(MakeQueueDigest(split.ShadowCasterQueue) == shadowDigest);
    }
}

TEST_CASE("mesh-assembly-determinism: chunk ranges tile the draw set [full]")
{
    const uint32_t proxyCounts[] = { 0u, 1u, 2u, 7u, 200u, 257u };
    const uint32_t chunkCounts[] = { 1u, 2u, 3u, 8u, 17u, 300u };

    for (uint32_t proxyCount : proxyCounts)
    {
        for (uint32_t chunkCount : chunkCounts)
        {
            std::vector<MeshAssemblyChunk> chunks;
            minEngine::ComputeMeshAssemblyChunkRanges(proxyCount, chunkCount, chunks);

            INFO("proxies=" << proxyCount << " chunks=" << chunkCount);

            const uint32_t expectedChunks = proxyCount == 0u ? 0u : std::min(chunkCount, proxyCount);
            REQUIRE(chunks.size() == expectedChunks);

            uint32_t nextFirstProxy = 0;
            for (const MeshAssemblyChunk& chunk : chunks)
            {
                // Contiguous, non-empty, and together they cover exactly what the draw set holds.
                CHECK(chunk.FirstProxy == nextFirstProxy);
                CHECK(chunk.ProxyCount > 0u);
                nextFirstProxy += chunk.ProxyCount;
            }
            CHECK(nextFirstProxy == proxyCount);
        }
    }
}

TEST_CASE("mesh-assembly-determinism: merging in chunk order is what the equality rests on [full]")
{
    AssemblyScene scene;
    scene.ResetDense(9);
    const Frustum frustum = MakeLookAtFrustum();

    MeshAssemblyInput input;
    input.PrimitiveProxies = &scene.m_Proxies;
    input.MainFrustum = &frustum;
    input.bEnableShadows = true;

    std::vector<MeshAssemblyChunk> chunks;
    SceneRenderContext merged;
    minEngine::BuildMeshAssemblyQueues(input, merged, MakeSchedule(3), &chunks);
    ApplyQueueSorts(merged);
    REQUIRE(chunks.size() == 3u);

    const std::vector<const MeshAssemblyChunk*> inOrder = ChunksInOrder(chunks);
    SceneRenderContext rebuilt;
    AppendChunks(inOrder, rebuilt);
    ApplyQueueSorts(rebuilt);

    // The append used by the tampered run below is faithful: in chunk order it reproduces the merge.
    CHECK(MakeQueueDigest(rebuilt.OpaqueQueue) == MakeQueueDigest(merged.OpaqueQueue));
    CHECK(MakeQueueDigest(rebuilt.ShadowCasterQueue) == MakeQueueDigest(merged.ShadowCasterQueue));

    const std::vector<const MeshAssemblyChunk*> reversed(inOrder.rbegin(), inOrder.rend());
    SceneRenderContext tampered;
    AppendChunks(reversed, tampered);
    ApplyQueueSorts(tampered);

    // The digest is sensitive to insertion order, which is what makes the equality assertions able
    // to fail instead of passing on any order that produces the same set of commands.
    CHECK(MakeQueueDigest(tampered.OpaqueQueue) != MakeQueueDigest(merged.OpaqueQueue));
    // The shadow queue is never sorted, so a reversed merge has to show there unconditionally.
    CHECK(MakeQueueDigest(tampered.ShadowCasterQueue) != MakeQueueDigest(merged.ShadowCasterQueue));
}

TEST_CASE("mesh-assembly-determinism: a dropped or duplicated proxy changes the queues [full]")
{
    AssemblyScene scene;
    scene.ResetDense(6);
    const Frustum frustum = MakeLookAtFrustum();

    MeshAssemblyInput input;
    input.PrimitiveProxies = &scene.m_Proxies;
    input.MainFrustum = &frustum;
    input.bEnableShadows = true;

    std::vector<MeshAssemblyChunk> chunks;
    SceneRenderContext merged;
    minEngine::BuildMeshAssemblyQueues(input, merged, MakeSchedule(3), &chunks);
    ApplyQueueSorts(merged);
    REQUIRE(chunks.size() == 3u);
    REQUIRE(merged.OpaqueQueue.size() == 6u);

    const uint64_t opaqueDigest = MakeQueueDigest(merged.OpaqueQueue);

    std::vector<MeshAssemblyChunk> missing = chunks;
    missing[1].ProxyCount -= 1u;
    minEngine::AssembleMeshAssemblyChunk(input, missing[1]);
    SceneRenderContext missingCtx;
    minEngine::MergeMeshAssemblyChunks(missing, missingCtx);
    ApplyQueueSorts(missingCtx);
    CHECK(missingCtx.OpaqueQueue.size() == 5u);
    CHECK(MakeQueueDigest(missingCtx.OpaqueQueue) != opaqueDigest);

    std::vector<MeshAssemblyChunk> overlapping = chunks;
    overlapping[1].ProxyCount += 1u;
    minEngine::AssembleMeshAssemblyChunk(input, overlapping[1]);
    SceneRenderContext overlappingCtx;
    minEngine::MergeMeshAssemblyChunks(overlapping, overlappingCtx);
    ApplyQueueSorts(overlappingCtx);
    CHECK(overlappingCtx.OpaqueQueue.size() == 7u);
    CHECK(MakeQueueDigest(overlappingCtx.OpaqueQueue) != opaqueDigest);

    // A gap is the third way the tiling breaks: both counts stay plausible and one proxy in the
    // middle of the interval is never walked, so nothing but the digest shows it.
    std::vector<MeshAssemblyChunk> gapped = chunks;
    gapped[1].FirstProxy += 1u;
    gapped[1].ProxyCount -= 1u;
    minEngine::AssembleMeshAssemblyChunk(input, gapped[1]);
    CHECK(TotalVisited(gapped) == 5u);
    SceneRenderContext gappedCtx;
    minEngine::MergeMeshAssemblyChunks(gapped, gappedCtx);
    ApplyQueueSorts(gappedCtx);
    CHECK(gappedCtx.OpaqueQueue.size() == 5u);
    CHECK(MakeQueueDigest(gappedCtx.OpaqueQueue) != opaqueDigest);
}

TEST_CASE("mesh-assembly-determinism: shadow casters keep the main-view segment first [smoke]")
{
    AssemblyScene scene;
    scene.ResetDense(4);
    REQUIRE(scene.GetProxy(3) != nullptr);
    scene.GetProxy(3)->SetOutsideView(true);
    const Frustum frustum = MakeLookAtFrustum();

    MeshAssemblyInput input;
    input.PrimitiveProxies = &scene.m_Proxies;
    input.MainFrustum = &frustum;
    input.bEnableShadows = true;

    // Identities 0..2 sit inside the view; identity 3 sits outside it and is collected for the
    // shadow pass only.
    const uint32_t chunkCounts[] = { 1u, 2u, 4u };
    for (uint32_t chunkCount : chunkCounts)
    {
        SceneRenderContext ctx;
        minEngine::BuildMeshAssemblyQueues(
            input,
            ctx,
            MakeSchedule(static_cast<int32_t>(chunkCount)),
            nullptr);

        INFO("chunk count=" << chunkCount);

        REQUIRE(ctx.ShadowCasterQueue.size() == 4u);
        CHECK(ctx.ShadowCasterQueue[0].m_VertexCount == 0u);
        CHECK(ctx.ShadowCasterQueue[1].m_VertexCount == 1u);
        CHECK(ctx.ShadowCasterQueue[2].m_VertexCount == 2u);
        // The shadow-only caster is last however the chunks were cut.
        CHECK(ctx.ShadowCasterQueue[3].m_VertexCount == 3u);
    }

    // Without shadows the shadow-only caster produces nothing at all.
    input.bEnableShadows = false;
    SceneRenderContext noShadowCtx;
    minEngine::BuildMeshAssemblyQueues(input, noShadowCtx, MakeSchedule(2), nullptr);
    REQUIRE(noShadowCtx.ShadowCasterQueue.size() == 3u);
    CHECK(noShadowCtx.ShadowCasterQueue[0].m_VertexCount == 0u);
    CHECK(noShadowCtx.ShadowCasterQueue[1].m_VertexCount == 1u);
    CHECK(noShadowCtx.ShadowCasterQueue[2].m_VertexCount == 2u);
}

TEST_CASE("mesh-assembly-determinism: the shadow pass borrows the assembled casters [smoke]")
{
    AssemblyScene scene;
    scene.ResetDense(4);
    REQUIRE(scene.GetProxy(3) != nullptr);
    scene.GetProxy(3)->SetOutsideView(true);
    const Frustum frustum = MakeLookAtFrustum();

    MeshAssemblyInput input;
    input.PrimitiveProxies = &scene.m_Proxies;
    input.MainFrustum = &frustum;
    input.bEnableShadows = true;

    SceneRenderContext ctx;
    minEngine::BuildMeshAssemblyQueues(input, ctx, MakeSchedule(2), nullptr);

    // The assembly names the two runs in place: both index the queue that was just merged, so no
    // second container has to be built to say which caster came from which route.
    REQUIRE(ctx.ShadowCasterQueue.size() == 4u);
    CHECK(ctx.ShadowCasters.MainVisible.First == 0u);
    CHECK(ctx.ShadowCasters.MainVisible.Count == 3u);
    CHECK(ctx.ShadowCasters.Hidden.First == ctx.ShadowCasters.MainVisible.End());
    CHECK(ctx.ShadowCasters.Hidden.Count == 1u);
    CHECK(ctx.ShadowCasters.Tiles(ctx.ShadowCasterQueue.size()));

    // What the frame does: name the queue, record the shadow draws off it, and let go when the
    // frame ends. Nothing is copied, so the borrow has to be the queue itself.
    minEngine::ShadowCasterBorrow borrow;
    {
        minEngine::ShadowCasterBorrowScope scope(borrow, ctx.ShadowCasterQueue, ctx.ShadowCasters);

        REQUIRE(borrow.IsBound());
        CHECK(borrow.Queue() == &ctx.ShadowCasterQueue);
        CHECK(&borrow.Commands() == &ctx.ShadowCasterQueue);
        CHECK(borrow.Commands().data() == ctx.ShadowCasterQueue.data());
        CHECK(borrow.Segments().MainVisible.Count == ctx.ShadowCasters.MainVisible.Count);
        CHECK(borrow.Segments().Hidden.Count == ctx.ShadowCasters.Hidden.Count);
    }

    CHECK_FALSE(borrow.IsBound());
    CHECK(borrow.Queue() == nullptr);
}

TEST_CASE("mesh-assembly-determinism: the caster queue keeps its two runs and their order [smoke]")
{
    AssemblyScene scene;
    scene.ResetDense(4);
    REQUIRE(scene.GetProxy(3) != nullptr);
    scene.GetProxy(3)->SetOutsideView(true);
    const Frustum frustum = MakeLookAtFrustum();

    MeshAssemblyInput input;
    input.PrimitiveProxies = &scene.m_Proxies;
    input.MainFrustum = &frustum;
    input.bEnableShadows = true;

    SceneRenderContext reference;
    BuildReferenceQueues(input, reference);

    // The sequence the queue has always held: main-view casters in draw-set order, then the ones
    // collected for the shadow pass alone. Pinned, because a queue that is only compared against a
    // second copy of itself cannot report a changed order.
    constexpr uint64_t kExpectedCasterMarkerDigest = 10370375220131207297ull;

    const uint32_t chunkCounts[] = { 1u, 2u, 4u };
    for (uint32_t chunkCount : chunkCounts)
    {
        SceneRenderContext ctx;
        minEngine::BuildMeshAssemblyQueues(
            input,
            ctx,
            MakeSchedule(static_cast<int32_t>(chunkCount)),
            nullptr);

        INFO("chunk count=" << chunkCount);

        REQUIRE(ctx.ShadowCasterQueue.size() == 4u);
        CHECK(MakeMarkerDigest(ctx.ShadowCasterQueue) == kExpectedCasterMarkerDigest);

        // The first run is every main-view caster, the second run is what the shadow pass alone
        // collected, and both read into the same storage.
        const minEngine::ShadowCasterSegments segments = ctx.ShadowCasters;
        REQUIRE(segments.Tiles(ctx.ShadowCasterQueue.size()));

        const std::vector<MeshDrawCommand>& casters = ctx.ShadowCasterQueue;
        for (uint32_t index = 0; index < segments.MainVisible.Count; ++index)
        {
            INFO("main-view caster=" << index);
            CHECK(casters[segments.MainVisible.First + index].m_VertexCount < 3u);
        }
        for (uint32_t index = 0; index < segments.Hidden.Count; ++index)
        {
            INFO("shadow-only caster=" << index);
            CHECK(casters[segments.Hidden.First + index].m_VertexCount >= 3u);
        }

        // The order the shadow pass draws in, read through the traversal the pass itself uses.
        std::vector<uint32_t> drawn;
        minEngine::VisitShadowCasters(
            casters,
            segments,
            [&drawn](const MeshDrawCommand& command) { drawn.push_back(command.m_VertexCount); });
        CHECK(drawn == std::vector<uint32_t>{ 0u, 1u, 2u, 3u });

        // A run that names more than the storage holds draws what is there and stops.
        std::vector<uint32_t> clamped;
        minEngine::VisitShadowCasters(
            casters,
            minEngine::ShadowCasterSegments{
                minEngine::MeshQueueSegment{ 0u, 2u },
                minEngine::MeshQueueSegment{ 10u, 5u } },
            [&clamped](const MeshDrawCommand& command) { clamped.push_back(command.m_VertexCount); });
        CHECK(clamped == std::vector<uint32_t>{ 0u, 1u });

        // Byte for byte the queue the single-pass path produces - the same commands, the same order.
        CHECK(ctx.ShadowCasterQueue.size() == reference.ShadowCasterQueue.size());
        CHECK(MakeQueueDigest(ctx.ShadowCasterQueue) == MakeQueueDigest(reference.ShadowCasterQueue));
    }
}

TEST_CASE("mesh-assembly-determinism: the caster queue outlives the frame that borrows it [smoke]")
{
    AssemblyScene scene;
    scene.ResetDense(4);
    REQUIRE(scene.GetProxy(3) != nullptr);
    scene.GetProxy(3)->SetOutsideView(true);
    const Frustum frustum = MakeLookAtFrustum();

    MeshAssemblyInput input;
    input.PrimitiveProxies = &scene.m_Proxies;
    input.MainFrustum = &frustum;
    input.bEnableShadows = true;

    // The pass outlives every frame; the queue it reads does not. The counter is the detector: a
    // borrow that is still alive after its frame ended is a number a case can fail on, instead of a
    // dangling pointer nobody notices until it draws garbage.
    minEngine::ShadowCasterBorrow passSide;
    CHECK(minEngine::ShadowCasterBorrow::LiveCount() == 0u);

    {
        // One frame, as the pipeline owns it: the context is the queue's owner, and recording plus
        // submit both happen inside this scope.
        SceneRenderContext frame;
        minEngine::BuildMeshAssemblyQueues(input, frame, MakeSchedule(2), nullptr);
        minEngine::ShadowCasterBorrowScope scope(passSide, frame.ShadowCasterQueue, frame.ShadowCasters);

        REQUIRE(passSide.IsBound());
        CHECK(minEngine::ShadowCasterBorrow::LiveCount() == 1u);

        // "Recording": the pass reads the frame's own storage, in run order, twice.
        for (uint32_t pass = 0; pass < 2u; ++pass)
        {
            const std::vector<MeshDrawCommand>& casters = passSide.Commands();
            CHECK(&casters == &frame.ShadowCasterQueue);
            CHECK(casters.size() == frame.ShadowCasters.Total());
        }

        // "Submit": the queue is still the one the commands were recorded from.
        CHECK(passSide.Queue() == &frame.ShadowCasterQueue);
        CHECK(passSide.Commands().data() == frame.ShadowCasterQueue.data());
    }

    // The frame ended - by its own last statement or by any early return out of it - and nothing
    // still names its storage.
    CHECK_FALSE(passSide.IsBound());
    CHECK(minEngine::ShadowCasterBorrow::LiveCount() == 0u);
}

TEST_CASE("mesh-assembly-determinism: the frame entry point matches the reference too [smoke]")
{
    AssemblyScene scene;
    scene.Reset(40);
    const Frustum frustum = MakeLookAtFrustum();

    RenderScene renderScene;
    renderScene.m_PrimitiveSceneProxies = scene.m_Proxies;

    SceneRenderContext fromContext;
    fromContext.Scene = &renderScene;
    fromContext.bEnableShadows = true;
    minEngine::BuildPrimitiveRenderQueue(fromContext, &frustum);
    ApplyQueueSorts(fromContext);

    MeshAssemblyInput input;
    input.PrimitiveProxies = &renderScene.m_PrimitiveSceneProxies;
    input.MainFrustum = &frustum;
    input.bEnableShadows = true;

    SceneRenderContext reference;
    BuildReferenceQueues(input, reference);
    ApplyQueueSorts(reference);

    REQUIRE(fromContext.OpaqueQueue.size() == reference.OpaqueQueue.size());
    REQUIRE(fromContext.TranslucentQueue.size() == reference.TranslucentQueue.size());
    REQUIRE(fromContext.ShadowCasterQueue.size() == reference.ShadowCasterQueue.size());

    CHECK(MakeQueueDigest(fromContext.OpaqueQueue) == MakeQueueDigest(reference.OpaqueQueue));
    CHECK(MakeQueueDigest(fromContext.TranslucentQueue) == MakeQueueDigest(reference.TranslucentQueue));
    CHECK(MakeQueueDigest(fromContext.ShadowCasterQueue) == MakeQueueDigest(reference.ShadowCasterQueue));

    // A frame with no scene still empties the queues rather than keeping the previous frame's.
    SceneRenderContext emptyContext;
    emptyContext.OpaqueQueue.push_back(MeshDrawCommand{});
    minEngine::BuildPrimitiveRenderQueue(emptyContext, nullptr);
    CHECK(emptyContext.OpaqueQueue.empty());
    CHECK(emptyContext.TranslucentQueue.empty());
    CHECK(emptyContext.ShadowCasterQueue.empty());
}

TEST_CASE("mesh-assembly-determinism: a fan-out runs the chunks off the calling thread [smoke]")
{
    FrameThreadingScope threading(FrameThreadMode::Multi);

    TaskSystem tasks;
    tasks.Init(2);
    REQUIRE(tasks.IsRunning());
    REQUIRE(tasks.GetWorkerCount() > 1);

    AssemblyScene scene;
    scene.Reset(1000);
    const Frustum frustum = MakeLookAtFrustum();

    MeshAssemblyInput input;
    input.PrimitiveProxies = &scene.m_Proxies;
    input.MainFrustum = &frustum;
    input.bEnableShadows = true;

    SceneRenderContext reference;
    BuildReferenceQueues(input, reference);
    ApplyQueueSorts(reference);
    REQUIRE_FALSE(reference.OpaqueQueue.empty());
    const uint64_t opaqueSerialDigest = MakeQueueDigest(reference.OpaqueQueue);
    const uint64_t translucentSerialDigest = MakeQueueDigest(reference.TranslucentQueue);
    const uint64_t shadowSerialDigest = MakeQueueDigest(reference.ShadowCasterQueue);

    AssemblyThreadProbe probe;
    scene.SetThreadProbe(&probe);

    // Assembly runs on the render role in a real frame; this thread also holds the game role, so the
    // pool accepts the fan-out under either identity.
    ScopedFrameThreadKind renderRole(FrameThreadKind::Render);

    const int32_t chunkCounts[] = { 2, 3, 8, 17 };
    for (int32_t chunkCount : chunkCounts)
    {
        INFO("chunk count=" << chunkCount);

        minEngine::ResetMeshAssemblyStats();
        probe.Reset();

        std::vector<MeshAssemblyChunk> chunks;
        SceneRenderContext parallel;
        minEngine::BuildMeshAssemblyQueues(
            input,
            parallel,
            MakeParallelSchedule(chunkCount, tasks),
            &chunks);
        ApplyQueueSorts(parallel);

        const minEngine::MeshAssemblyStats stats = minEngine::GetMeshAssemblyStats();

        // The fan-out was attempted: the capability gate passed and the pool was asked.
        REQUIRE(stats.LastSerialReason == nullptr);
        CHECK(stats.ParallelCalls == 1u);
        CHECK(stats.LastChunkCount == static_cast<uint32_t>(chunkCount));

        // It was also carried out: no chunk needed the repair pass, and every chunk walked its
        // whole range.
        CHECK(stats.ChunksNotRun == 0u);
        REQUIRE(chunks.size() == static_cast<size_t>(chunkCount));
        for (const MeshAssemblyChunk& chunk : chunks)
        {
            CHECK(chunk.VisitCount == chunk.ProxyCount);
        }

        // The batches were produced on threads that are not this one, and never on this one: the
        // work left the caller instead of quietly being done by it.
        CHECK(probe.OffCaller.load(std::memory_order_relaxed) > 0u);
        CHECK(probe.OnCaller.load(std::memory_order_relaxed) == 0u);

        CHECK(parallel.OpaqueQueue.size() == reference.OpaqueQueue.size());
        CHECK(parallel.TranslucentQueue.size() == reference.TranslucentQueue.size());
        CHECK(parallel.ShadowCasterQueue.size() == reference.ShadowCasterQueue.size());
        CHECK(MakeQueueDigest(parallel.OpaqueQueue) == opaqueSerialDigest);
        CHECK(MakeQueueDigest(parallel.TranslucentQueue) == translucentSerialDigest);
        CHECK(MakeQueueDigest(parallel.ShadowCasterQueue) == shadowSerialDigest);
    }

    tasks.Shutdown();
}

TEST_CASE("mesh-assembly-determinism: repeated fanned-out frames keep the queues [smoke]")
{
    FrameThreadingScope threading(FrameThreadMode::Multi);

    TaskSystem tasks;
    tasks.Init(2);
    REQUIRE(tasks.GetWorkerCount() > 1);

    AssemblyScene scene;
    scene.Reset(600);
    const Frustum frustum = MakeLookAtFrustum();

    MeshAssemblyInput input;
    input.PrimitiveProxies = &scene.m_Proxies;
    input.MainFrustum = &frustum;
    input.bEnableShadows = true;

    SceneRenderContext reference;
    BuildReferenceQueues(input, reference);
    ApplyQueueSorts(reference);
    const uint64_t opaqueSerialDigest = MakeQueueDigest(reference.OpaqueQueue);
    const uint64_t shadowSerialDigest = MakeQueueDigest(reference.ShadowCasterQueue);

    ScopedFrameThreadKind renderRole(FrameThreadKind::Render);

    // The cut changes from frame to frame: how the draw set is split may move the cost of a frame,
    // never its content or its order.
    const int32_t chunkCounts[] = { 2, 3, 8, 17 };
    minEngine::ResetMeshAssemblyStats();

    for (int32_t frame = 0; frame < 60; ++frame)
    {
        const int32_t chunkCount = chunkCounts[frame % 4];

        SceneRenderContext parallel;
        minEngine::BuildMeshAssemblyQueues(
            input,
            parallel,
            MakeParallelSchedule(chunkCount, tasks),
            nullptr);
        ApplyQueueSorts(parallel);

        INFO("frame=" << frame << " chunks=" << chunkCount);

        CHECK(MakeQueueDigest(parallel.OpaqueQueue) == opaqueSerialDigest);
        CHECK(MakeQueueDigest(parallel.ShadowCasterQueue) == shadowSerialDigest);
    }

    const minEngine::MeshAssemblyStats stats = minEngine::GetMeshAssemblyStats();
    CHECK(stats.Calls == 60u);
    CHECK(stats.ParallelCalls == 60u);
    CHECK(stats.ChunksNotRun == 0u);

    tasks.Shutdown();
}

TEST_CASE("mesh-assembly-determinism: a refused fan-out is repaired in place [smoke]")
{
    // Nothing on this thread holds a frame role, and the pool's game role lives on another thread,
    // so the pool declines every body it is handed - without telling the caller.
    REQUIRE_FALSE(FrameThreading::HasInstance());

    PoolOnAnotherThread pool;
    REQUIRE(pool.Tasks.IsRunning());
    REQUIRE(pool.Tasks.GetWorkerCount() > 1);
    REQUIRE(pool.ThreadId != std::this_thread::get_id());

    AssemblyScene scene;
    scene.Reset(1000);
    const Frustum frustum = MakeLookAtFrustum();

    MeshAssemblyInput input;
    input.PrimitiveProxies = &scene.m_Proxies;
    input.MainFrustum = &frustum;
    input.bEnableShadows = true;

    SceneRenderContext reference;
    BuildReferenceQueues(input, reference);
    ApplyQueueSorts(reference);

    AssemblyThreadProbe probe;
    scene.SetThreadProbe(&probe);
    minEngine::ResetMeshAssemblyStats();

    constexpr int32_t kChunkCount = 4;
    std::vector<MeshAssemblyChunk> chunks;
    SceneRenderContext repaired;
    minEngine::BuildMeshAssemblyQueues(
        input,
        repaired,
        MakeParallelSchedule(kChunkCount, pool.Tasks),
        &chunks);
    ApplyQueueSorts(repaired);

    const minEngine::MeshAssemblyStats stats = minEngine::GetMeshAssemblyStats();

    // The fan-out was attempted and refused, so every chunk has to be accounted for by the repair
    // pass - skipping it because the pool was "asked" is the failure this case exists for.
    REQUIRE(stats.LastSerialReason == nullptr);
    CHECK(stats.ParallelCalls == 1u);
    CHECK(stats.ChunksNotRun == static_cast<uint64_t>(kChunkCount));
    REQUIRE(chunks.size() == static_cast<size_t>(kChunkCount));
    for (const MeshAssemblyChunk& chunk : chunks)
    {
        CHECK(chunk.VisitCount == chunk.ProxyCount);
    }

    // The repair happened on this thread, which is the mirror image of the fanned-out case.
    CHECK(probe.OffCaller.load(std::memory_order_relaxed) == 0u);
    CHECK(probe.OnCaller.load(std::memory_order_relaxed) > 0u);

    // A refused fan-out costs the frame its parallelism, not its draws.
    CHECK(repaired.OpaqueQueue.size() == reference.OpaqueQueue.size());
    CHECK(repaired.TranslucentQueue.size() == reference.TranslucentQueue.size());
    CHECK(repaired.ShadowCasterQueue.size() == reference.ShadowCasterQueue.size());
    CHECK(MakeQueueDigest(repaired.OpaqueQueue) == MakeQueueDigest(reference.OpaqueQueue));
    CHECK(MakeQueueDigest(repaired.TranslucentQueue) == MakeQueueDigest(reference.TranslucentQueue));
    CHECK(MakeQueueDigest(repaired.ShadowCasterQueue) == MakeQueueDigest(reference.ShadowCasterQueue));
}

TEST_CASE("mesh-assembly-determinism: the pipeline resolves the pool the process owns [smoke]")
{
    FrameThreadingScope threading(FrameThreadMode::Multi);

    TaskSystem tasks;
    tasks.Init(2);
    REQUIRE(tasks.GetWorkerCount() > 1);

    AssemblyScene scene;
    scene.Reset(1000);
    AssemblyThreadProbe probe;
    scene.SetThreadProbe(&probe);
    const Frustum frustum = MakeLookAtFrustum();

    RenderScene renderScene;
    renderScene.m_PrimitiveSceneProxies = scene.m_Proxies;

    SceneRenderContext viaPipeline;
    viaPipeline.Scene = &renderScene;
    viaPipeline.bEnableShadows = true;

    minEngine::ResetMeshAssemblyStats();
    probe.Reset();

    // The pool in this case was never registered as the process instance, so the entry point -
    // which resolves the instance, the way the engine stands it up - cannot see it. A host that owns
    // no pool looks the same from here: one chunk, the serial shape, and a reason that says why.
    minEngine::BuildPrimitiveRenderQueue(viaPipeline, &frustum);
    ApplyQueueSorts(viaPipeline);

    const minEngine::MeshAssemblyStats stats = minEngine::GetMeshAssemblyStats();
    REQUIRE(stats.LastSerialReason != nullptr);
    CHECK(std::string(stats.LastSerialReason) == "no task system");
    CHECK(stats.ParallelCalls == 0u);
    CHECK(stats.ChunksNotRun == 0u);
    CHECK(stats.LastChunkCount == 1u);
    CHECK(probe.OffCaller.load(std::memory_order_relaxed) == 0u);
    CHECK(probe.OnCaller.load(std::memory_order_relaxed) > 0u);

    MeshAssemblyInput input;
    input.PrimitiveProxies = &renderScene.m_PrimitiveSceneProxies;
    input.MainFrustum = &frustum;
    input.bEnableShadows = true;

    SceneRenderContext reference;
    BuildReferenceQueues(input, reference);
    ApplyQueueSorts(reference);
    CHECK(MakeQueueDigest(viaPipeline.OpaqueQueue) == MakeQueueDigest(reference.OpaqueQueue));
    CHECK(MakeQueueDigest(viaPipeline.ShadowCasterQueue) == MakeQueueDigest(reference.ShadowCasterQueue));

    // Naming the pool is not enough either: a draw set under the threshold is not worth spreading,
    // and the two ends of a fan-out are paid in full whatever a chunk holds.
    AssemblyScene small;
    small.Reset(100);
    MeshAssemblyInput smallInput;
    smallInput.PrimitiveProxies = &small.m_Proxies;
    smallInput.MainFrustum = &frustum;
    smallInput.bEnableShadows = true;
    REQUIRE(smallInput.GetProxyCount() < minEngine::kMeshAssemblyMinProxiesPerChunk * 2u);

    minEngine::ResetMeshAssemblyStats();
    SceneRenderContext smallCtx;
    minEngine::BuildMeshAssemblyQueues(smallInput, smallCtx, MakeParallelSchedule(4, tasks), nullptr);

    const minEngine::MeshAssemblyStats smallStats = minEngine::GetMeshAssemblyStats();
    REQUIRE(smallStats.LastSerialReason != nullptr);
    CHECK(std::string(smallStats.LastSerialReason) == "too few proxies to spread");
    CHECK(smallStats.ParallelCalls == 0u);
    CHECK(smallStats.ChunksNotRun == 0u);

    tasks.Shutdown();
}

TEST_CASE("mesh-assembly-determinism: a single-topology frame still fans out [smoke]")
{
    // Single topology: one thread owns the game, render and RHI role and no frame-role thread is
    // running. The pool is real, so the workers exist - and the fan-out has to happen anyway,
    // because who runs the frame and who may run a chunk are two different questions.
    FrameThreadingScope threading(FrameThreadMode::Single);
    REQUIRE(FrameThreading::Get().IsSingle());
    REQUIRE(FrameThreading::Get().IsInGameThread());
    REQUIRE(FrameThreading::Get().IsInRenderingThread());

    TaskSystem tasks;
    tasks.Init(2);
    REQUIRE(tasks.IsRunning());
    REQUIRE(tasks.GetWorkerCount() > 1);
    REQUIRE_FALSE(tasks.AreFrameRolesRunning());

    AssemblyScene scene;
    scene.Reset(1000);
    AssemblyThreadProbe probe;
    scene.SetThreadProbe(&probe);
    const Frustum frustum = MakeLookAtFrustum();

    RenderScene renderScene;
    renderScene.m_PrimitiveSceneProxies = scene.m_Proxies;

    MeshAssemblyInput input;
    input.PrimitiveProxies = &renderScene.m_PrimitiveSceneProxies;
    input.MainFrustum = &frustum;
    input.bEnableShadows = true;

    SceneRenderContext reference;
    BuildReferenceQueues(input, reference);
    ApplyQueueSorts(reference);
    const uint64_t opaqueDigest = MakeQueueDigest(reference.OpaqueQueue);
    const uint64_t translucentDigest = MakeQueueDigest(reference.TranslucentQueue);
    const uint64_t shadowDigest = MakeQueueDigest(reference.ShadowCasterQueue);

    SceneRenderContext viaPipeline;
    viaPipeline.Scene = &renderScene;
    viaPipeline.bEnableShadows = true;

    TaskSystemInstanceScope poolScope(&tasks);

    minEngine::ResetMeshAssemblyStats();
    probe.Reset();

    minEngine::BuildPrimitiveRenderQueue(viaPipeline, &frustum);
    ApplyQueueSorts(viaPipeline);

    const minEngine::MeshAssemblyStats stats = minEngine::GetMeshAssemblyStats();

    // The gate is the pool and the draw set, never the topology: a frame that runs on a single
    // thread still spreads its chunks over the workers.
    REQUIRE(stats.LastSerialReason == nullptr);
    CHECK(stats.ParallelCalls == 1u);
    CHECK(stats.ChunksNotRun == 0u);
    CHECK(stats.LastChunkCount > 1u);
    CHECK(stats.LastProxyCount ==
          static_cast<uint32_t>(renderScene.m_PrimitiveSceneProxies.size()));

    // "The pool was asked" is not the fact that matters: the chunks were produced elsewhere.
    CHECK(probe.OffCaller.load(std::memory_order_relaxed) > 0u);
    CHECK(probe.OnCaller.load(std::memory_order_relaxed) == 0u);

    CHECK(MakeQueueDigest(viaPipeline.OpaqueQueue) == opaqueDigest);
    CHECK(MakeQueueDigest(viaPipeline.TranslucentQueue) == translucentDigest);
    CHECK(MakeQueueDigest(viaPipeline.ShadowCasterQueue) == shadowDigest);

    tasks.Shutdown();
}

TEST_CASE("mesh-assembly-determinism: turning the fan-out off reproduces the serial shape [smoke]")
{
    REQUIRE_FALSE(FrameThreading::HasInstance());

    TaskSystem tasks;
    tasks.Init(2);
    REQUIRE(tasks.GetWorkerCount() > 1);

    AssemblyScene scene;
    scene.Reset(1000);
    AssemblyThreadProbe probe;
    scene.SetThreadProbe(&probe);
    const Frustum frustum = MakeLookAtFrustum();

    RenderScene renderScene;
    renderScene.m_PrimitiveSceneProxies = scene.m_Proxies;

    MeshAssemblyInput input;
    input.PrimitiveProxies = &renderScene.m_PrimitiveSceneProxies;
    input.MainFrustum = &frustum;
    input.bEnableShadows = true;

    SceneRenderContext reference;
    BuildReferenceQueues(input, reference);
    ApplyQueueSorts(reference);
    const uint64_t opaqueDigest = MakeQueueDigest(reference.OpaqueQueue);
    const uint64_t translucentDigest = MakeQueueDigest(reference.TranslucentQueue);
    const uint64_t shadowDigest = MakeQueueDigest(reference.ShadowCasterQueue);

    SceneRenderContext off;
    off.Scene = &renderScene;
    off.bEnableShadows = true;

    minEngine::ResetMeshAssemblyStats();
    probe.Reset();

    {
        MeshAssemblySwitchScope switchScope(false);
        TaskSystemInstanceScope poolScope(&tasks);
        minEngine::BuildPrimitiveRenderQueue(off, &frustum);
    }
    ApplyQueueSorts(off);

    const minEngine::MeshAssemblyStats stats = minEngine::GetMeshAssemblyStats();

    // Off means the single-chunk shape: one loop, nothing scheduled, and the reason says so.
    REQUIRE(stats.LastSerialReason != nullptr);
    CHECK(std::string(stats.LastSerialReason) == "parallel assembly turned off");
    CHECK(stats.ParallelCalls == 0u);
    CHECK(stats.ChunksNotRun == 0u);
    CHECK(stats.LastChunkCount == 1u);
    CHECK(probe.OffCaller.load(std::memory_order_relaxed) == 0u);
    CHECK(probe.OnCaller.load(std::memory_order_relaxed) > 0u);

    // ... and byte for byte the reference: the switch changes how the queues are built, never what
    // they contain.
    CHECK(off.OpaqueQueue.size() == reference.OpaqueQueue.size());
    CHECK(off.TranslucentQueue.size() == reference.TranslucentQueue.size());
    CHECK(off.ShadowCasterQueue.size() == reference.ShadowCasterQueue.size());
    CHECK(MakeQueueDigest(off.OpaqueQueue) == opaqueDigest);
    CHECK(MakeQueueDigest(off.TranslucentQueue) == translucentDigest);
    CHECK(MakeQueueDigest(off.ShadowCasterQueue) == shadowDigest);

    tasks.Shutdown();
}

TEST_CASE("mesh-assembly-determinism: the chunk count follows the workers and the threshold [full]")
{
    struct Row
    {
        uint32_t ProxyCount;
        int32_t WorkerCount;
        uint32_t ExpectedChunks;
    };

    // Two chunks per worker, capped by how many chunks the per-chunk threshold allows.
    const Row rows[] = {
        { 0u, 8, 1u },
        { 1u, 8, 1u },
        { 255u, 8, 2u },
        { 256u, 8, 2u },
        { 1000u, 1, 1u },
        { 1000u, 0, 1u },
        { 1000u, 2, 4u },
        { 1000u, 4, 8u },
        { 1000u, 16, 8u },
    };

    for (const Row& row : rows)
    {
        INFO("proxies=" << row.ProxyCount << " workers=" << row.WorkerCount);
        CHECK(minEngine::ComputeMeshAssemblyChunkCount(row.ProxyCount, row.WorkerCount) ==
              row.ExpectedChunks);
    }

    // A schedule with no pool is one chunk however large the draw set is.
    CHECK(minEngine::ResolveMeshAssemblySchedule(nullptr, 100000).GetChunkCount() == 1u);
    CHECK_FALSE(minEngine::ResolveMeshAssemblySchedule(nullptr, 100000).bParallelDisabled);

    // The switch is what marks the shape, so the reason and the chunk count cannot disagree.
    {
        MeshAssemblySwitchScope switchScope(false);
        const MeshAssemblySchedule schedule = minEngine::ResolveMeshAssemblySchedule(nullptr, 100000);
        CHECK(schedule.GetChunkCount() == 1u);
        CHECK(schedule.bParallelDisabled);
    }
}

TEST_CASE("mesh-assembly-determinism: an unrun chunk reaches the frame summary [smoke]")
{
    REQUIRE_FALSE(FrameThreading::HasInstance());

    AssemblyScene scene;
    scene.Reset(1000);
    const Frustum frustum = MakeLookAtFrustum();

    MeshAssemblyInput input;
    input.PrimitiveProxies = &scene.m_Proxies;
    input.MainFrustum = &frustum;
    input.bEnableShadows = true;

    // A frame that fanned out and ran every chunk moves nothing.
    const uint64_t quietBefore = TaskSystem::GetMeshAssemblyChunksNotRunTotal();
    {
        TaskSystem tasks;
        tasks.Init(2);
        REQUIRE(tasks.GetWorkerCount() > 1);
        ScopedFrameThreadKind renderRole(FrameThreadKind::Render);

        SceneRenderContext fannedOut;
        minEngine::BuildMeshAssemblyQueues(input, fannedOut, MakeParallelSchedule(4, tasks), nullptr);
        minEngine::ResetMeshAssemblyStats();
        tasks.Shutdown();
    }
    CHECK(TaskSystem::GetMeshAssemblyChunksNotRunTotal() == quietBefore);

    // A pool on another thread refuses every body. The queues are repaired, and the number the
    // frame summary reads is what records the parallel work the frame did not get.
    PoolOnAnotherThread pool;
    REQUIRE(pool.Tasks.GetWorkerCount() > 1);

    constexpr int32_t kChunkCount = 4;
    SceneRenderContext refused;
    minEngine::BuildMeshAssemblyQueues(
        input,
        refused,
        MakeParallelSchedule(kChunkCount, pool.Tasks),
        nullptr);

    const uint64_t quietAfter = TaskSystem::GetMeshAssemblyChunksNotRunTotal();
    REQUIRE(quietAfter > quietBefore);
    CHECK(quietAfter - quietBefore == static_cast<uint64_t>(kChunkCount));

    minEngine::FrameWorkLossCounters previous;
    previous.MeshAssemblyChunksNotRun = quietBefore;
    minEngine::FrameWorkLossCounters current;
    current.MeshAssemblyChunksNotRun = quietAfter;

    const std::string line = minEngine::DescribeFrameWorkLossDelta(previous, current);
    CHECK(line.find("mesh assembly chunk(s) did not run") != std::string::npos);
    CHECK(line.find("4 mesh assembly") != std::string::npos);

    // A frame with nothing lost stays silent, which is what keeps the line worth reading.
    CHECK(minEngine::DescribeFrameWorkLossDelta(current, current).empty());
}

TEST_CASE("mesh-assembly-determinism: a frame end during assembly is a reported violation [smoke]")
{
    REQUIRE_FALSE(FrameThreading::HasInstance());

    // The draw set is read on the promise that no frame ends while it is being read. The frame-end
    // delete is what ends one, so a case makes it happen inside the collection and checks that the
    // promise is enforced rather than merely written down.
    AssemblyScene scene;
    scene.ResetDense(200);
    const Frustum frustum = MakeLookAtFrustum();

    minEngine::RenderScene flushScene;

    MeshAssemblyInput input;
    input.PrimitiveProxies = &scene.m_Proxies;
    input.MainFrustum = &frustum;
    input.bEnableShadows = true;
    input.Scene = &flushScene;

    // The predicate itself, on a window nobody disturbed.
    const uint64_t serial = minEngine::GetMeshAssemblySceneFlushSerial(input);
    CHECK(minEngine::DescribeMeshAssemblySetInstability(input, serial) == nullptr);

    // A caller with no scene has nothing to watch and must not report what it cannot see.
    MeshAssemblyInput noScene;
    CHECK(minEngine::DescribeMeshAssemblySetInstability(noScene, 0u) == nullptr);

    minEngine::RenderScene::FlushPendingProxyDeletes();
    CHECK(minEngine::RenderScene::GetProxyDeleteFlushCount() > serial);
    CHECK(minEngine::DescribeMeshAssemblySetInstability(input, serial) != nullptr);

    // Now the whole entry point: the first proxy ends the frame while the assembly walks the set.
    AssemblyTestProxy* const first = scene.GetProxy(0);
    REQUIRE(first != nullptr);
    first->OnCollect = []() {
        minEngine::RenderScene::FlushPendingProxyDeletes();
    };

    const uint64_t violationsBefore = minEngine::GetContractViolationCounters().Total();
    SceneRenderContext ctx;
    minEngine::BuildMeshAssemblyQueues(input, ctx, MakeSchedule(1), nullptr);
    first->OnCollect = nullptr;

    CHECK(minEngine::GetContractViolationCounters().Total() > violationsBefore);

    // Reported, not refused: the frame still gets the queues it asked for.
    CHECK_FALSE(ctx.OpaqueQueue.empty());
}

TEST_CASE("mesh-assembly-determinism: an assembly inside a recording window is reported [smoke]")
{
    // The assembly reads the proxy set and produces plain queue data, so it must not run while this
    // thread holds a recorded stream that has not been replayed: the two streams would interleave.
    REQUIRE_FALSE(FrameThreading::HasInstance());

    AssemblyScene scene;
    scene.Reset(300);
    const Frustum frustum = MakeLookAtFrustum();

    MeshAssemblyInput input;
    input.PrimitiveProxies = &scene.m_Proxies;
    input.MainFrustum = &frustum;
    input.bEnableShadows = true;

    minEngine::Testing::HeadlessTestRHI device;
    minEngine::RHICommandList cmdList(&device);
    cmdList.SetImmediateOverride(false);
    REQUIRE_FALSE(minEngine::RHICommandList::IsRecordingWindowOpenOnThisThread());

    // Outside a window the entry point has nothing to report, and the queues are built.
    const uint64_t closedBefore = minEngine::GetContractViolationCounters().Total();
    SceneRenderContext closed;
    minEngine::BuildMeshAssemblyQueues(input, closed, MakeSchedule(1), nullptr);
    CHECK(minEngine::GetContractViolationCounters().Total() == closedBefore);
    CHECK_FALSE(closed.OpaqueQueue.empty());

    // Inside one it reports the broken contract rather than assembling on in silence.
    cmdList.BeginRecording();
    REQUIRE(minEngine::RHICommandList::IsRecordingWindowOpenOnThisThread());

    const uint64_t openBefore = minEngine::GetContractViolationCounters().Total();
    SceneRenderContext open;
    minEngine::BuildMeshAssemblyQueues(input, open, MakeSchedule(1), nullptr);
    CHECK(minEngine::GetContractViolationCounters().Total() > openBefore);

    // Reporting is not skipping: the queue is still the one the frame asked for.
    CHECK(open.OpaqueQueue.size() == closed.OpaqueQueue.size());
    CHECK(MakeQueueDigest(open.OpaqueQueue) == MakeQueueDigest(closed.OpaqueQueue));

    cmdList.FinishRecording();
    CHECK_FALSE(minEngine::RHICommandList::IsRecordingWindowOpenOnThisThread());
}

