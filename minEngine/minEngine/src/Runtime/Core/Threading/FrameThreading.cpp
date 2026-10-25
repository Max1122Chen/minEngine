#include "FrameThreading.h"

#include "Runtime/Core/Assert/Assert.h"
#include "Runtime/Core/Log/LogSystem.h"

#include <cstdlib>
#include <cctype>

namespace minEngine
{
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
            m_ThreadBound[i] = false;
            m_ThreadIds[i] = {};
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
            m_ThreadBound[i] = false;
            m_ThreadIds[i] = {};
        }
        s_Instance = nullptr;
    }

    void FrameThreading::BindCurrentThread(FrameThreadKind kind)
    {
        ME_ASSERT(m_Initialized, "FrameThreading::BindCurrentThread before Initialize");
        const size_t index = static_cast<size_t>(kind);
        m_ThreadIds[index] = std::this_thread::get_id();
        m_ThreadBound[index] = true;
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
        const size_t index = static_cast<size_t>(kind);
        if (!m_ThreadBound[index])
        {
            return false;
        }
        return m_ThreadIds[index] == std::this_thread::get_id();
    }

    void FrameThreading::AssertIsFrameThread(FrameThreadKind kind, const char* context) const
    {
        if (IsInFrameThread(kind))
        {
            return;
        }

        const char* label = context != nullptr ? context : "(unspecified)";
        ME_LOG(
            LogCore,
            Error,
            "AssertIsFrameThread({}) failed at {}",
            KindToString(kind),
            label);
        ME_ASSERT(false, "Frame thread identity mismatch");
    }

    std::thread::id FrameThreading::GetThreadId(FrameThreadKind kind) const
    {
        return m_ThreadIds[static_cast<size_t>(kind)];
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
            hashId(m_ThreadIds[static_cast<size_t>(FrameThreadKind::Game)]),
            hashId(m_ThreadIds[static_cast<size_t>(FrameThreadKind::Render)]),
            hashId(m_ThreadIds[static_cast<size_t>(FrameThreadKind::RHI)]));
    }

    ScopedFrameThreadKind::ScopedFrameThreadKind(FrameThreadKind kind)
        : m_Kind(kind)
    {
        if (!FrameThreading::HasInstance())
        {
            return;
        }

        FrameThreading& threading = FrameThreading::Get();
        if (threading.IsInFrameThread(kind))
        {
            m_HadPrevious = false;
            return;
        }

        m_PreviousId = threading.GetThreadId(kind);
        m_HadPrevious = true;
        threading.BindCurrentThread(kind);
    }

    ScopedFrameThreadKind::~ScopedFrameThreadKind()
    {
        if (!m_HadPrevious || !FrameThreading::HasInstance())
        {
            return;
        }

        // Single-mode startup binds all roles to one tid; restoring a prior id is uncommon.
        // Re-bind current thread if we temporarily claimed this kind on another thread (future Multi).
        (void)m_Kind;
        (void)m_PreviousId;
    }
}
