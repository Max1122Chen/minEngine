#pragma once
#include "Core.h"
#include "Math/Math.h"
#include "Runtime/Function/Render/RHI/RHIDeferredCreate.h"
#include "Runtime/Function/Render/RHI/RHIDeferredDestroy.h"

#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

namespace minEngine
{
    struct RHIVertexElement;

    class RHITexture;
    struct RHITextureCreateDesc;

    class RHIBuffer;
    struct RHIBufferCreateDesc;

    class RHIShader;
    struct RHIShaderCreateDesc;

    class RHIVertexInputLayout;

    class RHIGraphicsPipelineState;
    struct RHIGraphicsPSODesc;

    class RHIShaderBindingSetLayout;
    struct RHIShaderBindingSetLayoutEntry;

    class RHIPipelineLayout;

    class RHIShaderBindingSet;
    struct RHIShaderBinding;

    class RHIShaderResourceView;
    struct RHITextureSRVDesc;

    class RHISampler;
    struct RHISamplerDesc;

    class RHIRenderPassInfo;

    struct RHITextureTransitionInfo;

    class RHI
    {
    public:
        RHI() : m_InstanceSerial(NextInstanceSerial())
        {
            // A resource destructor does not know which device it belongs to, so the void left by
            // this RHI's death is the queue's own business: register the one queue late destructors
            // post to, and take it back down with the instance.
            RHIDeferredDestroyQueue::InstallActiveQueue(m_DeferredDestroyQueue);
        }

        /**
         * Handles that outlived their last reference are destroyed by the backend that created
         * them, not by the thread that dropped them; a queue still holding handles here means the
         * backend's own shutdown never drained them, which is worth a line in the log.
         */
        virtual ~RHI()
        {
            // Unregister first: a resource released from here on has no device to destroy it, and
            // the queue reports that instead of letting it post into a half-destroyed backend.
            RHIDeferredDestroyQueue::UninstallActiveQueue(m_DeferredDestroyQueue.get());

            if (m_DeferredCreateQueue)
            {
                m_DeferredCreateQueue->AbandonPending(
                    "the RHI was destroyed before the graphics role created them");
            }
            if (m_DeferredDestroyQueue)
            {
                m_DeferredDestroyQueue->AbandonPending(
                    "the RHI was destroyed before its backend drained the queue");
            }
        }

        /** The queue every resource of this RHI posts its native handle to. */
        RHIDeferredDestroyQueue& GetDeferredDestroyQueue() { return *m_DeferredDestroyQueue; }
        const RHIDeferredDestroyQueue& GetDeferredDestroyQueue() const { return *m_DeferredDestroyQueue; }

        /**
         * RHI role, immediately after a batch of recorded commands was handed to the platform.
         * Backends call it from their own present path, so Single and Multi topologies retire
         * through the same boundary instead of each call site deciding when it is safe: the
         * criterion is "this batch is with the platform", never "this many frames have passed".
         */
        size_t RHISubmitBatchCommitted() { return m_DeferredDestroyQueue->DrainAtSubmitBatchTail(); }

        /**
         * RHI role, for a frame that ended without submitting anything (minimized window, no
         * swapchain, failed submit). Destroys nothing - no batch was committed - but counts the
         * missed opportunity and lets the queue's stall watchdog fire.
         */
        size_t RHINoteFrameWithoutSubmit() { return m_DeferredDestroyQueue->NoteFrameWithoutSubmit(); }

        /** Backend shutdown, after the GPU is known idle. */
        size_t RHIDrainAllDeferredDestroys() { return m_DeferredDestroyQueue->DrainAll(); }

        /** The queue every create call posts to when it cannot create natively where it runs. */
        RHIDeferredCreateQueue& GetDeferredCreateQueue() { return *m_DeferredCreateQueue; }
        const RHIDeferredCreateQueue& GetDeferredCreateQueue() const { return *m_DeferredCreateQueue; }

        std::shared_ptr<RHIDeferredCreateQueue> GetSharedDeferredCreateQueue() const
        {
            return m_DeferredCreateQueue;
        }

        /**
         * Graphics role, once per render frame, before that frame records anything: create the
         * native objects asked for since the previous frame. Returns how many were created.
         */
        size_t RHIDrainDeferredCreates() { return m_DeferredCreateQueue->DrainOnGraphicsRole(); }

        /**
         * The body shared by every resource factory: run the native creation on the graphics role
         * now, or hand the handle to the queue so the graphics role runs it at the start of the
         * next render frame.
         *
         * The handle is returned either way. A caller that creates from the game thread gets a
         * usable handle at once and a native object at the next frame start; the handle reports
         * that difference through IsReady, so a consumer that cannot wait skips the draw instead
         * of naming an object that does not exist yet. A creation that fails where it can run -
         * the graphics role - reports through IsReady, HasNativeCreateFailed and its own log, and
         * the queue does not keep asking.
         *
         * Objects that are ready by construction - no native half, or built inline just now -
         * never enter the queue.
         */
        template <typename ResourceType>
        std::shared_ptr<ResourceType> ScheduleNativeCreate(
            std::shared_ptr<ResourceType> resource,
            const char* kind)
        {
            if (!resource)
            {
                return resource;
            }

            // Nothing is left to create: either the object was built inline on this very call, or
            // it owns no native half at all (a pipeline state that is only a description, a
            // binding set layout that is a table). Queueing it would only add a frame-start no-op.
            if (resource->IsReady())
            {
                return resource;
            }

            if (RHIGraphicsRole::IsCurrent())
            {
                resource->CreateNativeResources();
                return resource;
            }

            // The queue holds the handle so the native object is created even if the caller drops
            // its own reference before the next frame starts.
            std::shared_ptr<RHIDeferredCreateTarget> target = resource;
            m_DeferredCreateQueue->Post(
                [target]() {
                    // Nobody else references the handle any more: creating the native object now
                    // would only queue it for destruction at the next present boundary.
                    if (target.use_count() == 1)
                    {
                        return RHICreateOutcome::NotNeeded;
                    }
                    if (target->CreateNativeResources())
                    {
                        return RHICreateOutcome::Created;
                    }
                    // A refusal no later frame can change leaves the queue instead of asking the
                    // driver again every frame start.
                    return target->HasNativeCreateFailed() ? RHICreateOutcome::Failed
                                                           : RHICreateOutcome::Retry;
                },
                kind);
            return resource;
        }

        /**
         * Monotonic per-process serial. GPU resources (shader programs, pipelines) are owned by one
         * RHI instance and must never be shared with another, so caches key on this value.
         */
        uint64_t GetInstanceSerial() const { return m_InstanceSerial; }

        virtual void Initialize() = 0;
        virtual void Shutdown() = 0;

        virtual std::shared_ptr<RHITexture> RHICreateTexture2D(
            const RHITextureCreateDesc& desc,
            const void* initialData = nullptr) = 0;

        virtual std::shared_ptr<RHIShaderResourceView> RHICreateShaderResourceView(
            const RHITextureSRVDesc& desc) = 0;

        virtual std::shared_ptr<RHIBuffer> RHICreateBuffer(
            const RHIBufferCreateDesc& desc,
            const void* initialData = nullptr) = 0;

        virtual std::shared_ptr<RHIShader> RHICreateShader(
            const RHIShaderCreateDesc& desc,
            std::string* outCompileLog = nullptr) = 0;

        /** Legacy GLSL source path (Material / unmigrated passes). Prefer bytecode overload. */
        virtual std::shared_ptr<RHIShader> RHICreateShader(
            const std::string& vertexSource,
            const std::string& fragmentSource,
            std::string* outCompileLog = nullptr) = 0;

        virtual std::shared_ptr<RHIGraphicsPipelineState> RHICreateGraphicsPipelineState(
            const RHIGraphicsPSODesc& desc) = 0;

        virtual std::shared_ptr<RHIShaderBindingSetLayout> RHICreateShaderBindingSetLayout(
            const std::vector<RHIShaderBindingSetLayoutEntry>& entries) = 0;

        virtual std::shared_ptr<RHIPipelineLayout> RHICreatePipelineLayout(
            const std::vector<RHIShaderBindingSetLayout*>& setLayouts) = 0;

        virtual std::shared_ptr<RHIShaderBindingSet> RHICreateShaderBindingSet(
            RHIShaderBindingSetLayout* layout,
            const std::vector<RHIShaderBinding>& resources) = 0;

        /** create or return cached sampler for desc (shared across textures). */
        virtual std::shared_ptr<RHISampler> RHIGetOrCreateSampler(const RHISamplerDesc& desc) = 0;

        virtual std::shared_ptr<RHIVertexInputLayout> RHICreateVertexInputLayout(
            std::initializer_list<RHIVertexElement> elements) = 0;

        virtual void RHICmdBeginRenderPass(const RHIRenderPassInfo& info) = 0;
        virtual void RHICmdEndRenderPass() = 0;

        virtual void RHICmdSetGraphicsPipelineState(RHIGraphicsPipelineState* pipelineState) = 0;
        virtual void RHICmdSetShaderBindingSet(uint32_t setIndex, RHIShaderBindingSet* bindingSet) = 0;
        virtual void RHICmdTransition(const RHITextureTransitionInfo& transition) = 0;

        /**
         * Set viewport/scissor.
         * @param flipY When true (default), Vulkan flips Y (negative height) for GLM/OpenGL-style
         *              clip space. Offline cubemap bake should pass false so face images match
         *              samplerCube conventions without polar seams.
         */
        virtual void RHICmdSetViewport(
            uint32_t x,
            uint32_t y,
            uint32_t width,
            uint32_t height,
            bool flipY = true) = 0;

        /** Minimum uniform-buffer offset alignment (Vulkan device limit / GL UBO alignment). */
        virtual uint32_t RHIGetMinUniformBufferOffsetAlignment() const { return 256; }
        virtual void RHICmdSetVertexBuffer(RHIBuffer* vertexBuffer, uint32_t slot = 0) = 0;
        virtual void RHICmdSetIndexBuffer(RHIBuffer* indexBuffer) = 0;

        virtual void RHICmdDrawIndexed(
            uint32_t indexCount,
            uint32_t firstIndex = 0,
            int32_t vertexOffset = 0) = 0;

        virtual void RHICmdDraw(uint32_t vertexCount, uint32_t firstVertex = 0) = 0;

        /** Generate mip chain from mip 0 (backend-owned). Prefer after filling base level. */
        virtual void RHICmdGenerateMips(RHITexture* texture) = 0;

        /** Backbuffer clear color / clear (swapchain surface). */
        virtual void RHISetBackbufferClearColor(const Vector3& color) = 0;
        virtual void RHIClearBackbuffer() = 0;

        /**
         * Present the backbuffer / swapchain.
         * OpenGL: glfwSwapBuffers. Vulkan: submit + present (semaphores/fences internal).
         */
        virtual void RHIPresent() = 0;

        /**
         * Offline / load-time GPU work outside the swapchain frame (EnvMap bake, etc.).
         * OpenGL: no-op. Vulkan: one-shot command buffer + queue wait.
         */
        virtual void RHIBeginImmediateCommands() {}
        virtual void RHIEndImmediateCommands() {}

        /** Drop cached attachment views before RDG physical textures are destroyed/recreated (Vulkan). */
        virtual void NotifyAttachmentResourcesDiscarded() {}

    private:
        std::shared_ptr<RHIDeferredDestroyQueue> m_DeferredDestroyQueue =
            std::make_shared<RHIDeferredDestroyQueue>();
        std::shared_ptr<RHIDeferredCreateQueue> m_DeferredCreateQueue =
            std::make_shared<RHIDeferredCreateQueue>();

        static uint64_t NextInstanceSerial()
        {
            static std::atomic<uint64_t> s_NextSerial{0};
            return ++s_NextSerial;
        }

        uint64_t m_InstanceSerial = 0;
    };
}
