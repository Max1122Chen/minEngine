#include "MeshAssemblyDeterminismTest.h"

#include "Runtime/Function/Render/DrawCommands/MeshDrawCommandSort.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/MaterialRenderProxy.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/PrimitiveSceneProxy.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RenderPipeline/MeshAssemblyChunk.h"
#include "Runtime/Function/Render/RenderPipeline/RenderQueueBuild.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/SceneRenderContext.h"

#include "doctest.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
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
    minEngine::BuildMeshAssemblyQueues(input, single, MeshAssemblySchedule{ 1 }, &singleChunk);
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
            MeshAssemblySchedule{ static_cast<int32_t>(chunkCount) },
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
    minEngine::BuildMeshAssemblyQueues(input, merged, MeshAssemblySchedule{ 3 }, &chunks);
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
    minEngine::BuildMeshAssemblyQueues(input, merged, MeshAssemblySchedule{ 3 }, &chunks);
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
            MeshAssemblySchedule{ static_cast<int32_t>(chunkCount) },
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
    minEngine::BuildMeshAssemblyQueues(input, noShadowCtx, MeshAssemblySchedule{ 2 }, nullptr);
    REQUIRE(noShadowCtx.ShadowCasterQueue.size() == 3u);
    CHECK(noShadowCtx.ShadowCasterQueue[0].m_VertexCount == 0u);
    CHECK(noShadowCtx.ShadowCasterQueue[1].m_VertexCount == 1u);
    CHECK(noShadowCtx.ShadowCasterQueue[2].m_VertexCount == 2u);
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
