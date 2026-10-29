#pragma once

#include "Core.h"

#include <atomic>
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
     * Multi: OS Render/RHI (or combined graphics) threads are owned by TaskSystem.
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
        friend class ScopedFrameThreadKind;

        static FrameThreading* s_Instance;

        bool m_Initialized = false;
        FrameThreadMode m_Mode = FrameThreadMode::Single;

        /**
         * One atomic per role, holding the owning thread's id, or a default-constructed id when
         * nobody owns the role. Ownership and identity are one value on purpose: as two fields a
         * reader could see "owned" next to the id from before the claim, and every role gate would
         * answer for a thread that no longer runs that role. A default-constructed std::thread::id
         * is unequal to every running thread's id, so it carries "unowned" without a second flag.
         */
        std::atomic<std::thread::id> m_ThreadIds[3]{};
    };

    /**
     * RAII: hold a frame role for the length of a scope, on a thread that is not its owner (Editor
     * split logical/renderer ticks, and code that has to run graphics work where no graphics thread
     * exists yet).
     *
     * Contract: a claim is granted only for a role nobody holds. Until the scope ends this thread is
     * that role, and at its end the role is unowned again - never left pointing at this thread, so a
     * thread cannot end up permanently passing a role's contracts. Scopes nest: claiming a role this
     * thread already holds changes nothing, and the role is released by the outermost claim, so the
     * roles unwind in the order they were taken.
     *
     * A claim on a role another thread holds is refused and counted (ME_ENSURE), not stolen: two
     * threads answering "yes" to the same role turns every contract written for that role into a
     * statement nobody can trust.
     */
    class MINENGINE_API ScopedFrameThreadKind
    {
    public:
        explicit ScopedFrameThreadKind(FrameThreadKind kind);
        ~ScopedFrameThreadKind();

        ScopedFrameThreadKind(const ScopedFrameThreadKind&) = delete;
        ScopedFrameThreadKind& operator=(const ScopedFrameThreadKind&) = delete;

    private:
        FrameThreadKind m_Kind;
        /** The instance that granted the claim; null when nothing was claimed and nothing is owed. */
        FrameThreading* m_Owner = nullptr;
    };
}
