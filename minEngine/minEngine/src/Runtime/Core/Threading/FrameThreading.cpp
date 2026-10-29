#include "FrameThreading.h"

#include "Runtime/Core/Assert/Assert.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Threading/ThreadIdentity.h"

#include <cstdlib>
#include <cctype>

namespace minEngine
{
    namespace
    {
        /**
         * What a contract violation reports as the caller's role. Installed while an instance
         * exists, so a report raised on any thread names the role that thread held - the fact a
         * reader needs first, because the same call is correct on one role and a defect on another.
         */
        const char* CurrentFrameThreadRole()
        {
            if (!FrameThreading::HasInstance())
            {
                return "unbound (FrameThreading not initialized)";
            }
            return FrameThreadRoleName(FrameThreading::Get(), std::this_thread::get_id());
        }
    }

    FrameThreading* FrameThreading::s_Instance = nullptr;

    bool FrameThreading::HasInstance()
    {
        return s_Instance != nullptr;
    }

    FrameThreading& FrameThreading::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "FrameThreading is not initialized");
        return *s_Instance;
    }

    std::optional<FrameThreadMode> FrameThreading::TryParseMode(std::string_view text)
    {
        std::string lowered;
        lowered.reserve(text.size());
        for (char ch : text)
        {
            lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }

        if (lowered == "single" || lowered == "1" || lowered == "st")
        {
            return FrameThreadMode::Single;
        }
        if (lowered == "multi" || lowered == "3" || lowered == "mt")
        {
            return FrameThreadMode::Multi;
        }
        return std::nullopt;
    }

    std::optional<FrameThreadMode> FrameThreading::ResolveMode(std::optional<FrameThreadMode> commandLineOverride)
    {
        if (commandLineOverride.has_value())
        {
            return commandLineOverride;
        }

        const char* envValue = std::getenv("MINENGINE_FRAME_THREADS");
        if (envValue != nullptr && envValue[0] != '\0')
        {
            return TryParseMode(envValue);
        }

        return FrameThreadMode::Single;
    }

    const char* FrameThreading::ModeToString(FrameThreadMode mode)
    {
        switch (mode)
        {
        case FrameThreadMode::Single:
            return "Single";
        case FrameThreadMode::Multi:
            return "Multi";
        }
        return "Unknown";
    }

    const char* FrameThreading::KindToString(FrameThreadKind kind)
    {
        switch (kind)
        {
        case FrameThreadKind::Game:
            return "Game";
        case FrameThreadKind::Render:
            return "Render";
        case FrameThreadKind::RHI:
            return "RHI";
        }
        return "Unknown";
    }

    void FrameThreading::Initialize(FrameThreadMode mode)
    {
        ME_ASSERT(s_Instance == nullptr, "FrameThreading is already initialized");
        s_Instance = this;
        m_Initialized = true;
        m_Mode = mode;

        for (int i = 0; i < 3; ++i)
        {
            m_ThreadIds[i].store(std::thread::id{}, std::memory_order_relaxed);
        }

        if (m_Mode == FrameThreadMode::Multi)
        {
            // Render/RHI tids are claimed by TaskSystem::StartFrameRoles.
            BindCurrentThread(FrameThreadKind::Game);
        }
        else
        {
            BindCurrentThreadAsAllRoles();
        }

        SetContractRoleResolver(&CurrentFrameThreadRole);
        LogStartupIdentity();
    }

    void FrameThreading::Shutdown()
    {
        if (!m_Initialized)
        {
            return;
        }

        m_Initialized = false;
        for (int i = 0; i < 3; ++i)
        {
            m_ThreadIds[i].store(std::thread::id{}, std::memory_order_relaxed);
        }
        s_Instance = nullptr;
        // Nothing owns a frame role any more: a report from here on must say so rather than name a
        // role left over from a destroyed instance.
        SetContractRoleResolver(nullptr);
    }

    void FrameThreading::BindCurrentThread(FrameThreadKind kind)
    {
        // Binding into an instance that never initialised records a violation and continues: the
        // binding stays inert (every IsInFrameThread answers no), which is the same answer the
        // caller already gets from an unbound role.
        ME_ENSURE(m_Initialized, "FrameThreading::BindCurrentThread before Initialize");
        // An owner claims its role outright: the caller is the thread that runs this role's work -
        // TaskSystem starts every frame-role thread with this call - so there is nothing to arbitrate
        // and no earlier owner to protect. A temporary claim from a thread that does not own the
        // role goes through ScopedFrameThreadKind, which will not take a role another thread holds.
        const size_t index = static_cast<size_t>(kind);
        m_ThreadIds[index].store(std::this_thread::get_id(), std::memory_order_release);
    }

    void FrameThreading::BindCurrentThreadAsAllRoles()
    {
        BindCurrentThread(FrameThreadKind::Game);
        BindCurrentThread(FrameThreadKind::Render);
        BindCurrentThread(FrameThreadKind::RHI);
    }

    bool FrameThreading::IsInFrameThread(FrameThreadKind kind) const
    {
        if (!m_Initialized)
        {
            return false;
        }
        const std::thread::id owner =
            m_ThreadIds[static_cast<size_t>(kind)].load(std::memory_order_acquire);
        if (owner == std::thread::id{})
        {
            return false;
        }
        return owner == std::this_thread::get_id();
    }

    void FrameThreading::AssertIsFrameThread(FrameThreadKind kind, const char* context) const
    {
        if (IsInFrameThread(kind))
        {
            return;
        }

        // A role mismatch is recoverable: every role-gated path already answers "not this thread"
        // for a mismatched caller (the render pipe queues the work instead of running it inline), so
        // the frame stays correct and the violation only has to be counted and named.
        ME_ENSURE(
            false,
            "expected the %s frame role, caller is %s (context: %s)",
            KindToString(kind),
            FrameThreadRoleName(*this, std::this_thread::get_id()),
            context != nullptr ? context : "(unspecified)");
    }

    std::thread::id FrameThreading::GetThreadId(FrameThreadKind kind) const
    {
        return m_ThreadIds[static_cast<size_t>(kind)].load(std::memory_order_acquire);
    }

    void FrameThreading::LogStartupIdentity() const
    {
        const auto hashId = [](std::thread::id id) -> std::size_t
        {
            return std::hash<std::thread::id>{}(id);
        };

        ME_LOG(
            LogCore,
            Info,
            "FrameThreading: mode={} GameTid={:#x} RenderTid={:#x} RHITid={:#x}",
            ModeToString(m_Mode),
            hashId(GetThreadId(FrameThreadKind::Game)),
            hashId(GetThreadId(FrameThreadKind::Render)),
            hashId(GetThreadId(FrameThreadKind::RHI)));
    }

    ScopedFrameThreadKind::ScopedFrameThreadKind(FrameThreadKind kind)
        : m_Kind(kind)
    {
        if (!FrameThreading::HasInstance())
        {
            return;
        }

        FrameThreading& threading = FrameThreading::Get();
        std::atomic<std::thread::id>& slot = threading.m_ThreadIds[static_cast<size_t>(kind)];
        const std::thread::id self = std::this_thread::get_id();

        std::thread::id owner = slot.load(std::memory_order_acquire);
        if (owner == self)
        {
            // This thread already holds the role: the enclosing claim releases it, and a nested one
            // must not release what it did not take.
            return;
        }

        if (owner != std::thread::id{})
        {
            // Refused, not stolen. The role stays with the thread that runs it, so no role gate can
            // be answered by two threads at once.
            ME_ENSURE(
                false,
                "ScopedFrameThreadKind(%s) refused: the role is held by %s",
                FrameThreading::KindToString(kind),
                FrameThreadRoleName(threading, owner));
            return;
        }

        // The load above is a check, this exchange is the claim: together they let two threads race
        // for one free role and give it to exactly one of them.
        if (!slot.compare_exchange_strong(owner, self, std::memory_order_acq_rel, std::memory_order_acquire))
        {
            ME_ENSURE(
                false,
                "ScopedFrameThreadKind(%s) refused: another thread claimed the role while this one "
                "waited for it",
                FrameThreading::KindToString(kind));
            return;
        }

        m_Owner = &threading;
    }

    ScopedFrameThreadKind::~ScopedFrameThreadKind()
    {
        if (m_Owner == nullptr)
        {
            return;
        }

        // The claim is released against the table that granted it: a shutdown in between clears
        // every role, so there is nothing left to release and nothing that can be corrupted.
        FrameThreading* const grantingInstance =
            FrameThreading::HasInstance() ? &FrameThreading::Get() : nullptr;
        if (grantingInstance != m_Owner)
        {
            ME_ENSURE(
                false,
                "ScopedFrameThreadKind(%s) outlived the FrameThreading instance that granted it; the "
                "release was skipped",
                FrameThreading::KindToString(m_Kind));
            return;
        }

        std::atomic<std::thread::id>& slot = grantingInstance->m_ThreadIds[static_cast<size_t>(m_Kind)];
        std::thread::id owner = std::this_thread::get_id();
        if (slot.compare_exchange_strong(owner, std::thread::id{}, std::memory_order_acq_rel, std::memory_order_acquire))
        {
            return;
        }

        // Writing this scope's idea of the owner back would take the role from whoever holds it now,
        // so the role is left where it is and the broken pairing is reported instead.
        ME_ENSURE(
            false,
            "ScopedFrameThreadKind(%s) released nothing: the role is now held by %s",
            FrameThreading::KindToString(m_Kind),
            owner == std::thread::id{} ? "nobody (it was already released)"
                                       : FrameThreadRoleName(*grantingInstance, owner));
    }
}
