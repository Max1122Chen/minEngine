#include "Platform/EditorRHIImGuiTexture.h"

#include "Runtime/Core/Assert/Assert.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/RHI/RHIDeferredDestroy.h"
#include "Runtime/Function/Render/RHI/RHI.h"

#include <algorithm>
#include <mutex>
#include <utility>
#include <vector>

namespace minEngine
{
    namespace
    {
        /**
         * Everything the registry owns. One object rather than a pile of statics so the mutex that
         * guards each part is impossible to use against the wrong data.
         *
         * Two locks, never held together: the backend ops are installed twice in a process
         * (initialize, shutdown) while the binding list changes every time a panel appears.
         */
        struct RegistryState
        {
            std::mutex OpsMutex;
            EditorImGuiTextureBackendOps Ops;

            std::mutex BindingsMutex;
            /**
             * Live bindings as weak references. The bindings are owned by whichever pin registered
             * them, and a destroyed pin must not leave a raw pointer here for another thread to
             * walk: a weak_ptr that expired is simply not there any more.
             */
            std::vector<std::weak_ptr<EditorRHIImGuiTexture>> Bindings;

            /**
             * Bumped whenever the device objects - and with them the descriptor pool - are
             * recreated. A release posted before a bump names an identifier that no longer exists.
             */
            std::atomic<std::uint64_t> DeviceObjectsGeneration{ 0 };
            std::atomic<bool> FontAtlasRebuildRequested{ false };

            std::atomic<std::uint64_t> Registered{ 0 };
            std::atomic<std::uint64_t> RegistrationFailed{ 0 };
            std::atomic<std::uint64_t> ReleasesRetired{ 0 };
            std::atomic<std::uint64_t> ReleasesDropped{ 0 };
            std::atomic<std::uint64_t> IdentifiersDiscarded{ 0 };
            std::atomic<std::uint64_t> OffRoleRefusals{ 0 };
            std::atomic<std::uint64_t> FontAtlasRebuildRequests{ 0 };
            std::atomic<std::uint64_t> FontAtlasRebuilds{ 0 };
        };

        RegistryState& State()
        {
            static RegistryState s_State;
            return s_State;
        }

        /** Drops expired entries so a long session does not carry one dead slot per closed panel. */
        void PruneExpiredLocked(std::vector<std::weak_ptr<EditorRHIImGuiTexture>>& bindings)
        {
            bindings.erase(
                std::remove_if(
                    bindings.begin(),
                    bindings.end(),
                    [](const std::weak_ptr<EditorRHIImGuiTexture>& entry) { return entry.expired(); }),
                bindings.end());
        }

        /** A snapshot, because the callbacks below take the list's lock themselves. */
        std::vector<std::shared_ptr<EditorRHIImGuiTexture>> LiveBindings()
        {
            RegistryState& state = State();
            std::vector<std::shared_ptr<EditorRHIImGuiTexture>> live;
            std::lock_guard<std::mutex> lock(state.BindingsMutex);
            PruneExpiredLocked(state.Bindings);
            live.reserve(state.Bindings.size());
            for (const std::weak_ptr<EditorRHIImGuiTexture>& entry : state.Bindings)
            {
                if (std::shared_ptr<EditorRHIImGuiTexture> binding = entry.lock())
                {
                    live.push_back(std::move(binding));
                }
            }
            return live;
        }

        /**
         * The contract every device-touching entry of this file shares. A caller that is not the
         * graphics role is reported and counted, and gets "nothing was done" instead of a device
         * call on a thread the driver was never handed to.
         */
        bool EnsureGraphicsRole(const char* context)
        {
            if (EditorImGuiTextureRegistry::IsOnGraphicsRole())
            {
                return true;
            }

            ME_ENSURE(
                false,
                "ImGui device work must run on the graphics role (context: %s)",
                context != nullptr ? context : "(unspecified)");
            State().OffRoleRefusals.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
    }

    bool EditorImGuiTextureRegistry::IsOnGraphicsRole()
    {
        // No frame roles installed - tests, offline tools - means there is no other thread to defer
        // to, so the caller is the graphics role. The convention the deferred queues already use.
        return RHIGraphicsRole::IsCurrent();
    }

    void EditorImGuiTextureRegistry::InstallBackendOps(EditorImGuiTextureBackendOps ops)
    {
        RegistryState& state = State();
        std::lock_guard<std::mutex> lock(state.OpsMutex);
        state.Ops = std::move(ops);
    }

    void EditorImGuiTextureRegistry::UninstallBackendOps()
    {
        // The identifiers were allocations of the backend being torn down; firing a release
        // callback into it after this point would call a device that is already gone, so they are
        // discarded instead - the pool dies with the device objects in every backend.
        DiscardAllIdentifiers("the ImGui renderer backend was shut down");

        RegistryState& state = State();
        // The releases already queued name the same dead pool. A shutdown drain runs their
        // callbacks after the backend is gone, so the generation bump is what turns them into
        // no-ops instead of calls into a freed backend.
        state.DeviceObjectsGeneration.fetch_add(1, std::memory_order_acq_rel);

        std::lock_guard<std::mutex> lock(state.OpsMutex);
        state.Ops = EditorImGuiTextureBackendOps{};
    }

    void EditorImGuiTextureRegistry::RequestFontAtlasRebuild()
    {
        RegistryState& state = State();
        if (!state.FontAtlasRebuildRequested.exchange(true, std::memory_order_acq_rel))
        {
            state.FontAtlasRebuildRequests.fetch_add(1, std::memory_order_relaxed);
        }
    }

    bool EditorImGuiTextureRegistry::IsFontAtlasRebuildPending()
    {
        return State().FontAtlasRebuildRequested.load(std::memory_order_acquire);
    }

    void EditorImGuiTextureRegistry::PerformPendingGpuWork(RHI* rhi)
    {
        if (!EnsureGraphicsRole("EditorImGuiTextureRegistry::PerformPendingGpuWork"))
        {
            return;
        }

        RegistryState& state = State();
        if (!state.FontAtlasRebuildRequested.exchange(false, std::memory_order_acq_rel))
        {
            return;
        }

        // The descriptor pool the registered identifiers live in is destroyed with the device
        // objects, so they are all dead at this point: bumping the generation turns the releases
        // still queued for them into no-ops, and dropping the bookkeeping keeps them from being
        // freed out of a pool that is about to stop existing.
        state.DeviceObjectsGeneration.fetch_add(1, std::memory_order_acq_rel);
        DiscardAllIdentifiers("the ImGui renderer device objects were recreated");

        std::function<void(RHI*)> rebuild;
        {
            std::lock_guard<std::mutex> lock(state.OpsMutex);
            rebuild = state.Ops.RebuildDeviceObjects;
        }
        if (!rebuild)
        {
            ME_LOG(
                LogEditor,
                Warn,
                "EditorImGuiTextureRegistry: the font atlas was rebuilt but no renderer backend is "
                "installed to rebuild its device objects.");
            return;
        }

        state.FontAtlasRebuilds.fetch_add(1, std::memory_order_relaxed);
        rebuild(rhi);

        // A release posted while the device objects were being rebuilt carries the generation from
        // before this second bump, so it cannot free into the pool that has just been replaced.
        state.DeviceObjectsGeneration.fetch_add(1, std::memory_order_acq_rel);
    }

    void EditorImGuiTextureRegistry::RetireAllIdentifiers(const char* reason)
    {
        ME_LOG(
            LogEditor,
            Trace,
            "EditorImGuiTextureRegistry: retiring every registered identifier ({}).",
            reason != nullptr ? reason : "(unspecified)");
        // Retirement is asynchronous by design, so nothing here needs the graphics role: the
        // deferred-destroy queue decides when the batch tail makes each one deletable.
        for (const std::shared_ptr<EditorRHIImGuiTexture>& binding : LiveBindings())
        {
            binding->RequestRelease();
        }
    }

    void EditorImGuiTextureRegistry::DiscardAllIdentifiers(const char* reason)
    {
        RegistryState& state = State();
        std::uint64_t discarded = 0;
        for (const std::shared_ptr<EditorRHIImGuiTexture>& binding : LiveBindings())
        {
            if (binding->DiscardIdentifier())
            {
                ++discarded;
            }
        }
        if (discarded != 0)
        {
            state.IdentifiersDiscarded.fetch_add(discarded, std::memory_order_relaxed);
            ME_LOG(
                LogEditor,
                Trace,
                "EditorImGuiTextureRegistry: {} identifier(s) dropped with their device objects ({}).",
                discarded,
                reason != nullptr ? reason : "(unspecified)");
        }
    }

    EditorImGuiTextureRegistry::Counters EditorImGuiTextureRegistry::GetCounters()
    {
        const RegistryState& state = State();
        Counters counters;
        counters.Registered = state.Registered.load(std::memory_order_relaxed);
        counters.RegistrationFailed = state.RegistrationFailed.load(std::memory_order_relaxed);
        counters.ReleasesRetired = state.ReleasesRetired.load(std::memory_order_relaxed);
        counters.ReleasesDropped = state.ReleasesDropped.load(std::memory_order_relaxed);
        counters.IdentifiersDiscarded = state.IdentifiersDiscarded.load(std::memory_order_relaxed);
        counters.OffRoleRefusals = state.OffRoleRefusals.load(std::memory_order_relaxed);
        counters.FontAtlasRebuildRequests =
            state.FontAtlasRebuildRequests.load(std::memory_order_relaxed);
        counters.FontAtlasRebuilds = state.FontAtlasRebuilds.load(std::memory_order_relaxed);
        return counters;
    }

    std::size_t EditorImGuiTextureRegistry::GetLiveBindingCount()
    {
        return LiveBindings().size();
    }

    std::uint64_t EditorImGuiTextureRegistry::GetDeviceObjectsGeneration()
    {
        return State().DeviceObjectsGeneration.load(std::memory_order_acquire);
    }

    std::function<std::uint64_t(RHI*, RHITexture*)> EditorImGuiTextureRegistry::GetCreateIdentifierOp()
    {
        RegistryState& state = State();
        std::lock_guard<std::mutex> lock(state.OpsMutex);
        return state.Ops.CreateIdentifier;
    }

    std::function<void(RHI*, std::uint64_t)> EditorImGuiTextureRegistry::GetReleaseIdentifierOp()
    {
        RegistryState& state = State();
        std::lock_guard<std::mutex> lock(state.OpsMutex);
        return state.Ops.ReleaseIdentifier;
    }

    void EditorImGuiTextureRegistry::RegisterLiveBinding(
        const std::shared_ptr<EditorRHIImGuiTexture>& binding)
    {
        if (!binding)
        {
            return;
        }

        RegistryState& state = State();
        std::lock_guard<std::mutex> lock(state.BindingsMutex);
        PruneExpiredLocked(state.Bindings);
        state.Bindings.push_back(binding);
    }

    void EditorImGuiTextureRegistry::UnregisterLiveBinding(const EditorRHIImGuiTexture* binding)
    {
        if (binding == nullptr)
        {
            return;
        }

        RegistryState& state = State();
        std::lock_guard<std::mutex> lock(state.BindingsMutex);
        state.Bindings.erase(
            std::remove_if(
                state.Bindings.begin(),
                state.Bindings.end(),
                [binding](const std::weak_ptr<EditorRHIImGuiTexture>& entry) {
                    const std::shared_ptr<EditorRHIImGuiTexture> live = entry.lock();
                    return !live || live.get() == binding;
                }),
            state.Bindings.end());
    }

    void EditorImGuiTextureRegistry::NoteRegistered()
    {
        State().Registered.fetch_add(1, std::memory_order_relaxed);
    }

    void EditorImGuiTextureRegistry::NoteRegistrationFailed()
    {
        State().RegistrationFailed.fetch_add(1, std::memory_order_relaxed);
    }

    void EditorImGuiTextureRegistry::NoteReleaseRetired()
    {
        State().ReleasesRetired.fetch_add(1, std::memory_order_relaxed);
    }

    void EditorImGuiTextureRegistry::NoteReleaseDropped()
    {
        State().ReleasesDropped.fetch_add(1, std::memory_order_relaxed);
    }

    EditorRHIImGuiTexture::EditorRHIImGuiTexture(RHI* rhi, RHITexture* texture)
        : m_Rhi(rhi)
        , m_Texture(texture)
    {
    }

    EditorRHIImGuiTexture::~EditorRHIImGuiTexture()
    {
        // A pin that never reset must not leak the identifier into the list of a device that
        // outlives it.
        RequestRelease();
    }

    std::shared_ptr<EditorRHIImGuiTexture> EditorRHIImGuiTexture::Request(RHI* rhi, RHITexture* texture)
    {
        if (rhi == nullptr || texture == nullptr)
        {
            return nullptr;
        }

        std::shared_ptr<EditorRHIImGuiTexture> binding(new EditorRHIImGuiTexture(rhi, texture));
        EditorImGuiTextureRegistry::RegisterLiveBinding(binding);
        // Runs the creation here when this thread is already the graphics role (single-thread
        // topology); otherwise it runs at the start of the next render frame, before that frame
        // records anything that could name the identifier.
        rhi->ScheduleNativeCreate(binding, "editor ImGui texture");
        return binding;
    }

    ImTextureID EditorRHIImGuiTexture::GetTextureId() const
    {
        const std::uint64_t identifier = m_Identifier.load(std::memory_order_acquire);
        return identifier != 0 ? static_cast<ImTextureID>(identifier) : ImTextureID_Invalid;
    }

    bool EditorRHIImGuiTexture::Matches(RHI* rhi, RHITexture* texture) const
    {
        if (m_IsInvalidated.load(std::memory_order_acquire))
        {
            return false;
        }
        // Both fields are written once, before the binding is visible to another thread, and never
        // again, so a plain read is the whole story.
        return m_Rhi == rhi && m_Texture == texture;
    }

    bool EditorRHIImGuiTexture::CreateNativeResources()
    {
        if (IsReady())
        {
            return true;
        }

        if (!EnsureGraphicsRole("EditorRHIImGuiTexture::CreateNativeResources"))
        {
            // Left queued: the request belongs to a frame that can actually create it.
            return false;
        }

        const std::function<std::uint64_t(RHI*, RHITexture*)> create =
            EditorImGuiTextureRegistry::GetCreateIdentifierOp();
        if (!create || m_Rhi == nullptr || m_Texture == nullptr)
        {
            m_HasFailed.store(true, std::memory_order_release);
            EditorImGuiTextureRegistry::NoteRegistrationFailed();
            ME_LOG(
                LogEditor,
                Error,
                "EditorRHIImGuiTexture: no ImGui renderer backend can register a texture; the "
                "viewport keeps reporting that its color texture is not ready.");
            return false;
        }

        const std::uint64_t identifier = create(m_Rhi, m_Texture);
        if (identifier == 0)
        {
            m_HasFailed.store(true, std::memory_order_release);
            EditorImGuiTextureRegistry::NoteRegistrationFailed();
            ME_LOG(
                LogEditor,
                Error,
                "EditorRHIImGuiTexture: the ImGui renderer backend could not register a texture "
                "(it is invalid, or the editor frame info is not ready yet).");
            return false;
        }

        m_Identifier.store(identifier, std::memory_order_release);
        m_IsReady.store(true, std::memory_order_release);
        EditorImGuiTextureRegistry::NoteRegistered();
        return true;
    }

    void EditorRHIImGuiTexture::RequestRelease()
    {
        const std::uint64_t identifier = m_Identifier.exchange(0, std::memory_order_acq_rel);
        m_IsReady.store(false, std::memory_order_release);
        EditorImGuiTextureRegistry::UnregisterLiveBinding(this);

        if (identifier == 0)
        {
            return;
        }

        RHI* rhi = m_Rhi;
        const std::function<void(RHI*, std::uint64_t)> release =
            EditorImGuiTextureRegistry::GetReleaseIdentifierOp();
        if (rhi == nullptr || !release)
        {
            // No backend left that could free it: the device objects it belonged to are the only
            // thing that will, and the count says so instead of pretending it was released.
            EditorImGuiTextureRegistry::NoteReleaseDropped();
            return;
        }

        const std::uint64_t generation = EditorImGuiTextureRegistry::GetDeviceObjectsGeneration();
        const bool bPosted = PostNativeDestroy(
            [release, rhi, identifier, generation]() {
                if (EditorImGuiTextureRegistry::GetDeviceObjectsGeneration() != generation)
                {
                    EditorImGuiTextureRegistry::NoteReleaseDropped();
                    return true;
                }
                release(rhi, identifier);
                return true;
            },
            "editor ImGui texture identifier");
        if (bPosted)
        {
            EditorImGuiTextureRegistry::NoteReleaseRetired();
        }
        else
        {
            // No RHI alive to destroy it; the queue has reported the leak it cannot defer.
            EditorImGuiTextureRegistry::NoteReleaseDropped();
        }
    }

    bool EditorRHIImGuiTexture::DiscardIdentifier()
    {
        const std::uint64_t identifier = m_Identifier.exchange(0, std::memory_order_acq_rel);
        m_IsReady.store(false, std::memory_order_release);
        // Never reuse this binding: the pool that owned the identifier is gone, so the next pin has
        // to ask the rebuilt device objects for a new one.
        m_IsInvalidated.store(true, std::memory_order_release);
        EditorImGuiTextureRegistry::UnregisterLiveBinding(this);
        return identifier != 0;
    }

    EditorRHIImGuiTexturePin::~EditorRHIImGuiTexturePin()
    {
        if (m_Binding)
        {
            m_Binding->RequestRelease();
            m_Binding.reset();
        }
    }

    ImTextureID EditorRHIImGuiTexturePin::Pin(RHI* rhi, RHITexture* texture)
    {
        if (rhi == nullptr || texture == nullptr)
        {
            Reset(rhi);
            return ImTextureID_Invalid;
        }

        if (m_Binding && m_Binding->Matches(rhi, texture))
        {
            // Same texture as last frame: the identifier is whatever the graphics role settled on,
            // including "not yet" for the frame in which it was first asked for.
            return m_Binding->GetTextureId();
        }

        Reset(rhi);
        m_BoundRhi = rhi;
        m_BoundTexture = texture;
        m_Binding = EditorRHIImGuiTexture::Request(rhi, texture);
        return m_Binding != nullptr ? m_Binding->GetTextureId() : ImTextureID_Invalid;
    }

    void EditorRHIImGuiTexturePin::Reset(RHI* rhi)
    {
        (void)rhi;
        if (m_Binding)
        {
            // The identifier outlives this call by one submitted batch on purpose: the frame that
            // drew it may still be in flight.
            m_Binding->RequestRelease();
            m_Binding.reset();
        }
        m_BoundRhi = nullptr;
        m_BoundTexture = nullptr;
    }

    void EditorRHIImGuiTexturePin::InvalidateAllBindings()
    {
        EditorImGuiTextureRegistry::RetireAllIdentifiers("the ImGui renderer invalidated its batching");
    }
}
