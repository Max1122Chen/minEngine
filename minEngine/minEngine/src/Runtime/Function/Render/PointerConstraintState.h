#pragma once

#include "Core.h"
#include "WindowSystem.h"

namespace minEngine
{
    /**
     * Narrow sink the focus-scoped pointer constraint state applies through.
     * Implemented by GLFWWindowSystem (real platform) and by the pointer-policy tests (recording fake),
     * so the focus / replay / revoke rules run without a focusable window.
     */
    class ICursorConstraintSink
    {
    public:
        virtual ~ICursorConstraintSink() = default;

        /** Client area in screen pixels; the clip request is intersected with it before being applied. */
        virtual WindowCursorClipRect GetClientScreenRect() const = 0;

        /** Apply the OS clip. bEnabled == false must release it unconditionally (never gated by a cached flag). */
        virtual void ApplyOsClip(const WindowCursorClipRect& rect) = 0;

        /** Set the platform cursor mode (NORMAL / HIDDEN / DISABLED). */
        virtual void ApplyCursorMode(EWindowCursorMode mode) = 0;

        /** Toggle relative (raw) mouse capture. */
        virtual void ApplyRelativeCapture(bool enabled) = 0;
    };

    /**
     * Real OS cursor clip access. The read side always asks the OS (::GetClipCursor on Win32) instead of
     * trusting our own bookkeeping; non-Win32 reports "not observable" rather than pretending to be free.
     */
    namespace PointerConstraintOs
    {
        /** bEnabled == true -> ::ClipCursor(rect); bEnabled == false -> ::ClipCursor(nullptr), always. */
        bool ApplyOsClipRect(const WindowCursorClipRect& rect);
        bool ReadOsClipRect(WindowCursorClipRect& outRect);
        /** Whole virtual desktop in screen pixels; the reference rect for "no clip". */
        WindowCursorClipRect GetDesktopScreenRect();
        /**
         * Applied clip truth: bOutActive is true when the OS clip is narrower than the desktop, i.e. a real
         * constraint is applied. Returns false when the platform cannot report it (bOutActive then means nothing).
         */
        bool ReadAppliedClipState(bool& bOutActive, WindowCursorClipRect& outRect);
    }

    /** Diagnostic counters of the focus-scoped state (each one is also the "warn" counterpart of a rule). */
    struct PointerConstraintDiagnostics
    {
        bool bWindowActive = false;
        uint32_t FocusRevokeCount = 0;
        uint32_t WatchdogRevokeCount = 0;
        uint32_t EmptyIntersectionCount = 0;
    };

    /**
     * Focus-scoped pointer constraint state.
     *
     * Remembers the last requested constraint (clip rect + cursor visibility) and the platform focus truth,
     * and guarantees an OS-level constraint exists only while this window is active:
     *   - focus lost  -> immediate, unconditional revoke (clip off, raw off, cursor NORMAL) in the callback itself;
     *   - focus back  -> replay the request in force at that moment (no history replay);
     *   - request while inactive -> recorded only, never applied;
     *   - platform focus truth sampled each frame -> revoke + warn when a focus-lost event was missed.
     *
     * This class is also the single narrow test seam: tests drive OnWindowFocusChanged / SyncPlatformFocus
     * directly, while the real GLFW callback forwards into the very same entry points.
     */
    class PointerConstraintState
    {
    public:
        explicit PointerConstraintState(ICursorConstraintSink& sink);

        PointerConstraintState(const PointerConstraintState&) = delete;
        PointerConstraintState& operator=(const PointerConstraintState&) = delete;

        /** Record the focus truth observed at window creation without applying any request. */
        void InitializeWindowActive(bool windowActive);

        void SetCursorVisibleRequest(bool visible);
        void SetClipRectRequest(const WindowCursorClipRect& rect);

        /** Focus entry, shared by the platform callback and the tests. */
        void OnWindowFocusChanged(bool focused);

        /** Watchdog: platform focus truth, sampled once per frame. */
        void SyncPlatformFocus(bool platformActive);

        /** Hard revoke (Stop / Shutdown / crash): release everything and drop the recorded request. */
        void ReleaseAllAndForgetRequests();

        PointerConstraintDiagnostics GetDiagnostics() const;

    private:
        void ApplyCurrentRequest();
        void RevokeAll();

        ICursorConstraintSink& m_Sink;
        WindowCursorClipRect m_ClipRequest;
        bool m_bCursorVisibleRequest = true;
        bool m_bWindowActive = false;
        uint32_t m_FocusRevokeCount = 0;
        uint32_t m_WatchdogRevokeCount = 0;
        uint32_t m_EmptyIntersectionCount = 0;
    };
}