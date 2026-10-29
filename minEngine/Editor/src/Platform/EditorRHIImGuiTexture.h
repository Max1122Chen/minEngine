#pragma once

#include "imgui.h"
#include "Runtime/Function/Render/RHI/RHIDeferredCreate.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

namespace minEngine
{
    class RHI;
    class RHITexture;

    /**
     * The device calls behind an ImGui texture identifier.
     *
     * What an ImGui texture identifier is - a GL name, a VkDescriptorSet - belongs to the renderer
     * backend that draws with it, so the backend installs these and the layer below stays
     * API-neutral. Every one of them touches the device, which is why none of them may run where
     * the UI code that asks for it runs.
     */
    struct EditorImGuiTextureBackendOps
    {
        /** Build the identifier of a texture. Graphics role. Returns 0 when it cannot. */
        std::function<std::uint64_t(RHI*, RHITexture*)> CreateIdentifier;
        /** Give one identifier back. RHI role, at the submit batch tail. */
        std::function<void(RHI*, std::uint64_t)> ReleaseIdentifier;
        /** Destroy and then recreate the renderer device objects. Graphics role. */
        std::function<void(RHI*)> RebuildDeviceObjects;
    };

    /**
     * Texture identifiers and renderer device objects: asked for by the UI thread, performed by the
     * graphics role.
     *
     * Registering a texture with ImGui allocates a descriptor set, and rebuilding the font atlas
     * destroys the pool that set lives in. Both need the device, so neither may happen on the
     * thread that builds the UI:
     *
     * - A registration is posted to the RHI's deferred-create queue, which the graphics role drains
     *   at the start of the next render frame. The identifier therefore appears one frame after the
     *   UI first asks for it, so every caller has to cope with "not ready yet"
     *   (EditorRHIImGuiTexturePin::Pin returns ImTextureID_Invalid until then).
     * - A release is posted to the deferred-destroy queue, which runs it at the tail of the next
     *   submitted batch - after every batch that could still name the identifier.
     * - A font atlas rebuild is a request flag; the graphics role destroys and recreates the device
     *   objects at the next submit, which is also where the new atlas texture is uploaded.
     *
     * A device-objects rebuild destroys the descriptor pool, so every identifier registered before
     * it dies with it: the generation counter below is what tells a release posted earlier that its
     * identifier is already gone instead of freeing it from a pool that no longer exists.
     */
    class EditorImGuiTextureRegistry
    {
    public:
        struct Counters
        {
            /** Identifiers built on the graphics role. */
            std::uint64_t Registered = 0;
            /** Registrations the backend could not build an identifier for. */
            std::uint64_t RegistrationFailed = 0;
            /** Identifiers handed to the deferred-destroy queue. */
            std::uint64_t ReleasesRetired = 0;
            /** Retired identifiers whose device objects (and pool) were already destroyed. */
            std::uint64_t ReleasesDropped = 0;
            /** Identifiers dropped together with the device objects that owned them. */
            std::uint64_t IdentifiersDiscarded = 0;
            /** Graphics-role entries called from another thread. */
            std::uint64_t OffRoleRefusals = 0;
            std::uint64_t FontAtlasRebuildRequests = 0;
            std::uint64_t FontAtlasRebuilds = 0;
        };

        /** Reads the backend calls; installed by the renderer backend while it is initialized. */
        static void InstallBackendOps(EditorImGuiTextureBackendOps ops);
        static void UninstallBackendOps();

        /**
         * Any thread: the font atlas was rebuilt on the CPU and the GPU side has to follow. Takes
         * no lock and touches no device; the request is performed by the graphics role.
         */
        static void RequestFontAtlasRebuild();
        static bool IsFontAtlasRebuildPending();

        /**
         * Graphics role, once per frame before ImGui records anything: perform the device work the
         * UI thread asked for. The first thing it does is refuse a caller that is not the graphics
         * role - silently driving the device from the UI thread is the failure this exists to make
         * visible.
         */
        static void PerformPendingGpuWork(RHI* rhi);

        /** Any thread: every registered identifier is retired, i.e. destroyed past the batch tail. */
        static void RetireAllIdentifiers(const char* reason);

        /** True when this thread may drive the device (see RHIGraphicsRole). */
        static bool IsOnGraphicsRole();

        static Counters GetCounters();
        /** Bindings that still hold an identifier or wait for one. */
        static std::size_t GetLiveBindingCount();
        /** Bumped whenever the device objects - and the descriptor pool with them - are recreated. */
        static std::uint64_t GetDeviceObjectsGeneration();

        // ---------------------------------------------------------------- implementation detail
        //
        // Called by EditorRHIImGuiTexture. They are public because that class is not a friend, and
        // because a test needs to read the same counters the code acts on.

        static std::function<std::uint64_t(RHI*, RHITexture*)> GetCreateIdentifierOp();
        static std::function<void(RHI*, std::uint64_t)> GetReleaseIdentifierOp();
        static void RegisterLiveBinding(const std::shared_ptr<class EditorRHIImGuiTexture>& binding);
        static void UnregisterLiveBinding(const class EditorRHIImGuiTexture* binding);
        static void NoteRegistered();
        static void NoteRegistrationFailed();
        static void NoteReleaseRetired();
        static void NoteReleaseDropped();
        /** Graphics role: every identifier dies with the device objects, without being freed. */
        static void DiscardAllIdentifiers(const char* reason);
    };

    /**
     * One texture registered with ImGui.
     *
     * The identifier is created on the graphics role, so a binding created on another thread is a
     * pending request for one frame: IsReady() answers false until the graphics role has run
     * CreateNativeResources(). Creating it is safe to repeat, and a binding nobody references any
     * more is dropped by the queue instead of being created.
     */
    class EditorRHIImGuiTexture final : public RHIDeferredCreateTarget
    {
    public:
        ~EditorRHIImGuiTexture() override;

        EditorRHIImGuiTexture(const EditorRHIImGuiTexture&) = delete;
        EditorRHIImGuiTexture& operator=(const EditorRHIImGuiTexture&) = delete;

        /**
         * Any thread: ask for the ImGui identifier of a texture. The binding references the
         * texture, it does not own it - the RHI does.
         */
        static std::shared_ptr<EditorRHIImGuiTexture> Request(RHI* rhi, RHITexture* texture);

        /** The identifier, or ImTextureID_Invalid until the graphics role has built it. */
        ImTextureID GetTextureId() const;
        bool IsValid() const { return GetTextureId() != ImTextureID_Invalid; }
        bool Matches(RHI* rhi, RHITexture* texture) const;

        /** Any thread: retire the identifier. Lives on past the batch tail, not past this call. */
        void RequestRelease();

        bool IsReady() const override { return m_IsReady.load(std::memory_order_acquire); }
        bool CreateNativeResources() override;
        bool HasNativeCreateFailed() const override { return m_HasFailed.load(std::memory_order_acquire); }

        /**
         * The identifier died with the descriptor pool that owned it, so there is nothing left to
         * free. Called by the registry when the device objects are recreated. True when an
         * identifier was actually dropped.
         */
        bool DiscardIdentifier();

    private:
        EditorRHIImGuiTexture(RHI* rhi, RHITexture* texture);

        RHI* m_Rhi = nullptr;
        RHITexture* m_Texture = nullptr;
        std::atomic<std::uint64_t> m_Identifier{ 0 };
        std::atomic<bool> m_IsReady{ false };
        std::atomic<bool> m_HasFailed{ false };
        /**
         * The identifier was dropped with its descriptor pool: the binding is a registration that
         * no longer exists, so a pin has to ask for a new one instead of reusing this.
         */
        std::atomic<bool> m_IsInvalidated{ false };
    };

    /**
     * Per-frame pin: keeps an ImGui texture registration alive while Image() is drawn.
     *
     * The pin is UI-thread state only. It holds the binding, and the binding - not the pin - is
     * what the graphics role and the tracking list know about, so a destroyed pin can never leave
     * a dangling entry behind for another thread to walk.
     */
    class EditorRHIImGuiTexturePin
    {
    public:
        EditorRHIImGuiTexturePin() = default;
        ~EditorRHIImGuiTexturePin();

        EditorRHIImGuiTexturePin(const EditorRHIImGuiTexturePin&) = delete;
        EditorRHIImGuiTexturePin& operator=(const EditorRHIImGuiTexturePin&) = delete;

        /**
         * The identifier to draw with this frame, or ImTextureID_Invalid while the graphics role
         * still has the registration queued. Textures registered before keep their identifier.
         */
        ImTextureID Pin(RHI* rhi, RHITexture* texture);
        void Reset(RHI* rhi);

        /** Retire every registered binding, e.g. after the swapchain - the samplers - changed. */
        static void InvalidateAllBindings();

    private:
        std::shared_ptr<EditorRHIImGuiTexture> m_Binding;
        RHI* m_BoundRhi = nullptr;
        RHITexture* m_BoundTexture = nullptr;
    };
}
