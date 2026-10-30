#pragma once

// A device double for suites that reason about GPU-handle lifetime without a graphics context.
//
// It is a real RHI subclass: it carries the deferred-destroy queue every handle posts to, so a
// suite sees the same hand-off a backend performs - the handle's destructor posts, and the posted
// callback is what counts as the native object dying. Resource factories are hooks, so a suite
// decides what a created object looks like and whether it was created at all.

#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHIDeferredDestroy.h"
#include "Runtime/Function/Render/RHI/RHIGraphicsPipelineState.h"
#include "Runtime/Function/Render/RHI/RHIPipelineLayout.h"
#include "Runtime/Function/Render/RHI/RHISampler.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Function/Render/RHI/RHIShaderBinding.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

#include <atomic>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace minEngine::Testing
{
    /**
     * What happened to the handles a device double handed out.
     *
     * "Alive" counts the C++ handle objects, "Destroyed" counts the native object dying. The gap
     * between them is the leak every suite here is about: a handle that outlives the device can no
     * longer hand its native object over, and the native object is never destroyed.
     */
    struct GpuHandleProbe
    {
        std::atomic<int> HandlesCreated{ 0 };
        std::atomic<int> HandlesAlive{ 0 };
        std::atomic<int> NativesDestroyed{ 0 };
        /** The handle was released while no device was registered: nothing could destroy it. */
        std::atomic<int> NativesRefused{ 0 };
    };

    inline void PostProbeDestroy(
        const std::shared_ptr<GpuHandleProbe>& probe,
        const char* kind)
    {
        const bool posted = PostNativeDestroy(
            [probe]() {
                probe->NativesDestroyed.fetch_add(1, std::memory_order_relaxed);
                return true;
            },
            kind);
        if (!posted)
        {
            probe->NativesRefused.fetch_add(1, std::memory_order_relaxed);
        }
    }

    /** A vertex input layout whose native object is a counter. */
    class ProbeVertexInputLayout final : public RHIVertexInputLayout
    {
    public:
        ProbeVertexInputLayout(
            std::shared_ptr<GpuHandleProbe> probe,
            std::vector<RHIVertexElement> elements)
            : m_Probe(std::move(probe))
            , m_Elements(std::move(elements))
        {
            m_Probe->HandlesCreated.fetch_add(1, std::memory_order_relaxed);
            m_Probe->HandlesAlive.fetch_add(1, std::memory_order_relaxed);
        }

        ~ProbeVertexInputLayout() override
        {
            m_Probe->HandlesAlive.fetch_sub(1, std::memory_order_relaxed);
            PostProbeDestroy(m_Probe, "probe vertex input layout");
        }

        ProbeVertexInputLayout(const ProbeVertexInputLayout&) = delete;
        ProbeVertexInputLayout& operator=(const ProbeVertexInputLayout&) = delete;

        const std::vector<RHIVertexElement>& GetElements() const override { return m_Elements; }
        uint32_t GetStride() const override { return 32u; }

    private:
        std::shared_ptr<GpuHandleProbe> m_Probe;
        std::vector<RHIVertexElement> m_Elements;
    };

    /**
     * A binding set whose native object is a counter.
     *
     * The serial says which build of the set it is: a producer that rebuilt its set hands out a
     * new object with a new serial, and a consumer that still names the previous one is drawing
     * with state the producer already replaced. That is the only way to tell the two apart without
     * reading memory that may already be gone.
     */
    class ProbeShaderBindingSet final : public RHIShaderBindingSet
    {
    public:
        ProbeShaderBindingSet(
            std::shared_ptr<GpuHandleProbe> probe,
            const RHIShaderBindingSetLayout* layout,
            uint32_t serial)
            : m_Probe(std::move(probe))
            , m_Layout(layout)
            , m_Serial(serial)
        {
            m_Probe->HandlesCreated.fetch_add(1, std::memory_order_relaxed);
            m_Probe->HandlesAlive.fetch_add(1, std::memory_order_relaxed);
        }

        ~ProbeShaderBindingSet() override
        {
            m_Probe->HandlesAlive.fetch_sub(1, std::memory_order_relaxed);
            PostProbeDestroy(m_Probe, "probe shader binding set");
        }

        ProbeShaderBindingSet(const ProbeShaderBindingSet&) = delete;
        ProbeShaderBindingSet& operator=(const ProbeShaderBindingSet&) = delete;

        const RHIShaderBindingSetLayout* GetLayout() const override { return m_Layout; }
        const std::vector<RHIShaderBinding>& GetBindings() const override { return m_Bindings; }

        uint32_t GetSerial() const { return m_Serial; }

    private:
        std::shared_ptr<GpuHandleProbe> m_Probe;
        const RHIShaderBindingSetLayout* m_Layout = nullptr;
        uint32_t m_Serial = 0;
        std::vector<RHIShaderBinding> m_Bindings;
    };

    /** A binding-set layout that only carries the entry table. */
    class ProbeShaderBindingSetLayout final : public RHIShaderBindingSetLayout
    {
    public:
        explicit ProbeShaderBindingSetLayout(std::vector<RHIShaderBindingSetLayoutEntry> entries)
            : m_Entries(std::move(entries))
        {
        }

        const std::vector<RHIShaderBindingSetLayoutEntry>& GetEntries() const override
        {
            return m_Entries;
        }

    private:
        std::vector<RHIShaderBindingSetLayoutEntry> m_Entries;
    };

    /** A pipeline state that carries its description, like the backend fallback. */
    class ProbePipelineState final : public RHIGraphicsPSOStateFallback
    {
    public:
        explicit ProbePipelineState(const RHIGraphicsPSODesc& desc)
            : RHIGraphicsPSOStateFallback(desc)
        {
        }
    };

    /**
     * The device half a suite needs: the queue, the bind record and the hooks.
     *
     * Every command the tests do not exercise is a no-op; the ones they do observe - binding set
     * binds above all - are recorded so a suite can ask what actually reached the device at replay
     * time, which is the moment a dangling handle would have been dereferenced for real.
     */
    class HeadlessTestRHI final : public RHI
    {
    public:
        explicit HeadlessTestRHI(std::shared_ptr<GpuHandleProbe> probe = std::make_shared<GpuHandleProbe>())
            : m_Probe(std::move(probe))
        {
        }

        const std::shared_ptr<GpuHandleProbe>& GetProbe() const { return m_Probe; }

        std::function<std::shared_ptr<RHIVertexInputLayout>(std::initializer_list<RHIVertexElement>)>
            CreateVertexInputLayoutHook;
        std::function<std::shared_ptr<RHIShaderBindingSet>(RHIShaderBindingSetLayout*)>
            CreateShaderBindingSetHook;
        /**
         * Buffer factory hook. A suite that reasons about geometry lifetime installs a probe that
         * goes through RHI::ScheduleNativeCreate, so the handle exists before its native object and
         * the queue's "nobody wants it" decision is exercised for real.
         */
        std::function<std::shared_ptr<RHIBuffer>(const RHIBufferCreateDesc&, const void*)> CreateBufferHook;

        /** Every bind the device was handed, in order: the index and the set the stream named. */
        std::vector<std::pair<uint32_t, RHIShaderBindingSet*>> BoundSets;
        /** Binds whose set named nothing at all. */
        int NullSetBinds = 0;

        /**
         * One draw the device was handed, with the geometry bound when it arrived.
         *
         * A real backend refuses a draw whose geometry has no native object instead of handing it
         * to the driver, so the double answers the same question: a recorded draw whose bound
         * buffer is not ready is the state that must never reach the device, and it is counted
         * here so a suite can assert on it rather than on a log line.
         */
        struct RecordedDraw
        {
            bool bIndexed = false;
            uint32_t Count = 0;
            RHIBuffer* VertexBuffer = nullptr;
            RHIBuffer* IndexBuffer = nullptr;
            bool bNamelessGeometry = false;
        };

        std::vector<RecordedDraw> Draws;
        /** Draws refused-by-contract: the geometry they named had no native object. */
        int NamelessDraws = 0;

        /** The geometry last bound, so a draw can be attributed to what it named. */
        RHIBuffer* BoundVertexBuffer = nullptr;
        RHIBuffer* BoundIndexBuffer = nullptr;

        void Initialize() override {}
        void Shutdown() override {}

        std::shared_ptr<RHITexture> RHICreateTexture2D(
            const RHITextureCreateDesc&,
            const void*) override
        {
            return nullptr;
        }
        std::shared_ptr<RHIShaderResourceView> RHICreateShaderResourceView(
            const RHITextureSRVDesc&) override
        {
            return nullptr;
        }
        std::shared_ptr<RHIBuffer> RHICreateBuffer(
            const RHIBufferCreateDesc& desc,
            const void* initialData) override
        {
            if (CreateBufferHook)
            {
                return CreateBufferHook(desc, initialData);
            }
            return nullptr;
        }
        std::shared_ptr<RHIShader> RHICreateShader(
            const RHIShaderCreateDesc&,
            std::string*) override
        {
            return nullptr;
        }
        std::shared_ptr<RHIShader> RHICreateShader(
            const std::string&,
            const std::string&,
            std::string*) override
        {
            return nullptr;
        }
        std::shared_ptr<RHIGraphicsPipelineState> RHICreateGraphicsPipelineState(
            const RHIGraphicsPSODesc& desc) override
        {
            return std::make_shared<ProbePipelineState>(desc);
        }
        std::shared_ptr<RHIShaderBindingSetLayout> RHICreateShaderBindingSetLayout(
            const std::vector<RHIShaderBindingSetLayoutEntry>&) override
        {
            return nullptr;
        }
        std::shared_ptr<RHIPipelineLayout> RHICreatePipelineLayout(
            const std::vector<RHIShaderBindingSetLayout*>&) override
        {
            return nullptr;
        }
        std::shared_ptr<RHIShaderBindingSet> RHICreateShaderBindingSet(
            RHIShaderBindingSetLayout* layout,
            const std::vector<RHIShaderBinding>&) override
        {
            if (CreateShaderBindingSetHook)
            {
                return CreateShaderBindingSetHook(layout);
            }
            return std::make_shared<ProbeShaderBindingSet>(m_Probe, layout, m_NextSetSerial++);
        }
        std::shared_ptr<RHISampler> RHIGetOrCreateSampler(const RHISamplerDesc&) override
        {
            return nullptr;
        }
        std::shared_ptr<RHIVertexInputLayout> RHICreateVertexInputLayout(
            std::initializer_list<RHIVertexElement> elements) override
        {
            if (CreateVertexInputLayoutHook)
            {
                return CreateVertexInputLayoutHook(elements);
            }
            return std::make_shared<ProbeVertexInputLayout>(
                m_Probe,
                std::vector<RHIVertexElement>(elements));
        }

        void RHICmdBeginRenderPass(const RHIRenderPassInfo&) override {}
        void RHICmdEndRenderPass() override {}
        void RHICmdSetGraphicsPipelineState(RHIGraphicsPipelineState*) override {}
        void RHICmdSetShaderBindingSet(uint32_t setIndex, RHIShaderBindingSet* bindingSet) override
        {
            if (bindingSet == nullptr)
            {
                ++NullSetBinds;
            }
            BoundSets.emplace_back(setIndex, bindingSet);
        }
        void RHICmdTransition(const RHITextureTransitionInfo&) override {}
        void RHICmdSetViewport(uint32_t, uint32_t, uint32_t, uint32_t, bool) override {}
        void RHICmdSetVertexBuffer(RHIBuffer* vertexBuffer, uint32_t) override
        {
            BoundVertexBuffer = vertexBuffer;
        }
        void RHICmdSetIndexBuffer(RHIBuffer* indexBuffer) override
        {
            BoundIndexBuffer = indexBuffer;
        }
        void RHICmdDrawIndexed(uint32_t indexCount, uint32_t, int32_t) override
        {
            RecordDraw(true, indexCount, BoundVertexBuffer, BoundIndexBuffer);
        }
        void RHICmdDraw(uint32_t vertexCount, uint32_t) override
        {
            RecordDraw(false, vertexCount, BoundVertexBuffer, nullptr);
        }
        void RHICmdGenerateMips(RHITexture*) override {}
        void RHISetBackbufferClearColor(const minEngine::Math::Vector3&) override {}
        void RHIClearBackbuffer() override {}
        void RHIPresent() override {}

    private:
        /** The backend's own answer to "would this draw name data": no object, no draw. */
        void RecordDraw(bool bIndexed, uint32_t count, RHIBuffer* vertexBuffer, RHIBuffer* indexBuffer)
        {
            RecordedDraw draw;
            draw.bIndexed = bIndexed;
            draw.Count = count;
            draw.VertexBuffer = vertexBuffer;
            draw.IndexBuffer = indexBuffer;

            const RHIBuffer* named = indexBuffer != nullptr ? indexBuffer : vertexBuffer;
            draw.bNamelessGeometry = named != nullptr && !named->IsReady();
            if (draw.bNamelessGeometry)
            {
                ++NamelessDraws;
            }
            Draws.push_back(draw);
        }

        std::shared_ptr<GpuHandleProbe> m_Probe;
        uint32_t m_NextSetSerial = 1;
    };
}