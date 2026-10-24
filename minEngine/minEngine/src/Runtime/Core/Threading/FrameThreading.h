#pragma once

#include "Core.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace minEngine
{
    enum class FrameThreadKind : uint8_t
    {
        Game = 0,
        Render = 1,
        RHI = 2,
    };

    enum class FrameThreadMode : uint8_t
    {
        Single = 0,
        Multi = 1,
    };

    /**
     * Frame-role identity and FrameThreadMode.
     * Single: one OS thread owns Game/Render/RHI kinds.
     * Multi: mode is stored now; dedicated Render/RHI OS threads land in a later slice.
     */
    class MINENGINE_API FrameThreading
    {
    public:
        static bool HasInstance();
        static FrameThreading& Get();

        /** Parse "single"|"multi" (case-insensitive). Invalid → nullopt. */
        static std::optional<FrameThreadMode> TryParseMode(std::string_view text);

        /**
         * Resolve mode: explicit CLI override if set, else MINENGINE_FRAME_THREADS, else Single.
         * Invalid env value → nullopt (caller should fail startup).
         */
        static std::optional<FrameThreadMode> ResolveMode(std::optional<FrameThreadMode> commandLineOverride);

        static const char* ModeToString(FrameThreadMode mode);
        static const char* KindToString(FrameThreadKind kind);

        void Initialize(FrameThreadMode mode);
        void Shutdown();

        FrameThreadMode GetMode() const { return m_Mode; }
        bool IsSingle() const { return m_Mode == FrameThreadMode::Single; }
        bool IsMulti() const { return m_Mode == FrameThreadMode::Multi; }

        /** Bind current OS thread as the owner of kind (Single: bind all three). */
        void BindCurrentThread(FrameThreadKind kind);
        void BindCurrentThreadAsAllRoles();

        bool IsInFrameThread(FrameThreadKind kind) const;
        bool IsInGameThread() const { return IsInFrameThread(FrameThreadKind::Game); }
        bool IsInRenderingThread() const { return IsInFrameThread(FrameThreadKind::Render); }
        bool IsInRHIThread() const { return IsInFrameThread(FrameThreadKind::RHI); }

        void AssertIsFrameThread(FrameThreadKind kind, const char* context) const;

        std::thread::id GetThreadId(FrameThreadKind kind) const;

        /** Info log: mode + tids (Single prints one tid for all kinds). */
        void LogStartupIdentity() const;

    private:
        static FrameThreading* s_Instance;

        bool m_Initialized = false;
        FrameThreadMode m_Mode = FrameThreadMode::Single;
        std::thread::id m_ThreadIds[3]{};
        bool m_ThreadBound[3]{false, false, false};
    };

    /** RAII: temporarily treat current thread as kind (for Editor split Logical/Renderer ticks). */
    class MINENGINE_API ScopedFrameThreadKind
    {
    public:
        explicit ScopedFrameThreadKind(FrameThreadKind kind);
        ~ScopedFrameThreadKind();

        ScopedFrameThreadKind(const ScopedFrameThreadKind&) = delete;
        ScopedFrameThreadKind& operator=(const ScopedFrameThreadKind&) = delete;

    private:
        FrameThreadKind m_Kind;
        std::thread::id m_PreviousId{};
        bool m_HadPrevious = false;
    };
}
