#include "PointerConstraintState.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace minEngine
{
    namespace
    {
        // Clip geometry is TU-internal: the engine DLL sits at the PE export-ordinal ceiling
        // (see minEngine/CMakeLists.txt), so only the surface the tests really drive stays exported.
        bool IsValidRect(const WindowCursorClipRect& rect)
        {
            return rect.Right > rect.Left && rect.Bottom > rect.Top;
        }

        WindowCursorClipRect IntersectRects(const WindowCursorClipRect& lhs, const WindowCursorClipRect& rhs)
        {
            WindowCursorClipRect result;
            if (!lhs.bEnabled || !rhs.bEnabled)
            {
                return result;
            }

            result.Left = lhs.Left > rhs.Left ? lhs.Left : rhs.Left;
            result.Top = lhs.Top > rhs.Top ? lhs.Top : rhs.Top;
            result.Right = lhs.Right < rhs.Right ? lhs.Right : rhs.Right;
            result.Bottom = lhs.Bottom < rhs.Bottom ? lhs.Bottom : rhs.Bottom;
            result.bEnabled = IsValidRect(result);
            return result;
        }
    }

    namespace PointerConstraintOs
    {
        bool ApplyOsClipRect(const WindowCursorClipRect& rect)
        {
#if defined(_WIN32)
            if (!rect.bEnabled)
            {
                // Unconditional: a cached "already released" flag must never suppress the release.
                return ::ClipCursor(nullptr) != FALSE;
            }

            RECT osRect{};
            osRect.left = rect.Left;
            osRect.top = rect.Top;
            osRect.right = rect.Right;
            osRect.bottom = rect.Bottom;
            return ::ClipCursor(&osRect) != FALSE;
#else
            (void)rect;
            return false;
#endif
        }

        bool ReadOsClipRect(WindowCursorClipRect& outRect)
        {
#if defined(_WIN32)
            RECT osRect{};
            if (::GetClipCursor(&osRect) == FALSE)
            {
                return false;
            }

            outRect.bEnabled = true;
            outRect.Left = osRect.left;
            outRect.Top = osRect.top;
            outRect.Right = osRect.right;
            outRect.Bottom = osRect.bottom;
            return true;
#else
            (void)outRect;
            return false;
#endif
        }

        bool ReadAppliedClipState(bool& bOutActive, WindowCursorClipRect& outRect)
        {
            bOutActive = false;
            WindowCursorClipRect osClipRect;
            if (!ReadOsClipRect(osClipRect))
            {
                return false;
            }

            const WindowCursorClipRect desktop = GetDesktopScreenRect();
            const bool bDesktopValid = IsValidRect(desktop);
            bOutActive = bDesktopValid
                && !(osClipRect.Left <= desktop.Left && osClipRect.Top <= desktop.Top
                     && osClipRect.Right >= desktop.Right && osClipRect.Bottom >= desktop.Bottom);
            outRect = osClipRect;
            outRect.bEnabled = bOutActive;
            return true;
        }

        WindowCursorClipRect GetDesktopScreenRect()
        {
            WindowCursorClipRect rect;
#if defined(_WIN32)
            const int originX = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
            const int originY = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
            const int width = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
            const int height = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
            if (width <= 0 || height <= 0)
            {
                return rect;
            }

            rect.bEnabled = true;
            rect.Left = originX;
            rect.Top = originY;
            rect.Right = originX + width;
            rect.Bottom = originY + height;
#endif
            return rect;
        }
    }

    PointerConstraintState::PointerConstraintState(ICursorConstraintSink& sink)
        : m_Sink(sink)
    {
    }

    void PointerConstraintState::InitializeWindowActive(bool windowActive)
    {
        m_bWindowActive = windowActive;
    }

    void PointerConstraintState::SetCursorVisibleRequest(bool visible)
    {
        m_CursorModeRequest = visible ? EWindowCursorMode::Normal : EWindowCursorMode::Disabled;
        if (m_bWindowActive)
        {
            ApplyCurrentRequest();
        }
    }

    void PointerConstraintState::SetClipRectRequest(const WindowCursorClipRect& rect)
    {
        m_ClipRequest = rect;
        // Legacy pairing: clip on implies relative capture (Captured-style). Prefer SetPointerWindowCommandRequest.
        m_bRelativeCaptureRequest = rect.bEnabled;
        if (m_bWindowActive)
        {
            ApplyCurrentRequest();
        }
    }

    void PointerConstraintState::SetPointerWindowCommandRequest(
        EWindowCursorMode mode,
        const WindowCursorClipRect& clip,
        bool bRelativeCapture)
    {
        m_CursorModeRequest = mode;
        m_ClipRequest = clip;
        m_bRelativeCaptureRequest = bRelativeCapture;
        if (m_bWindowActive)
        {
            ApplyCurrentRequest();
        }
    }

    void PointerConstraintState::OnWindowFocusChanged(bool focused)
    {
        if (!focused)
        {
            // Immediate path: the constraint is revoked inside the focus callback, never on a later frame.
            m_bWindowActive = false;
            RevokeAll();
            ++m_FocusRevokeCount;
            return;
        }

        m_bWindowActive = true;
        // Replay the request in force right now; there is no history stack to rewind.
        ApplyCurrentRequest();
    }

    void PointerConstraintState::SyncPlatformFocus(bool platformActive)
    {
        if (platformActive == m_bWindowActive)
        {
            return;
        }

        if (platformActive)
        {
            m_bWindowActive = true;
            ApplyCurrentRequest();
            return;
        }

        // The focus-lost event never reached us. The platform says this window is inactive, so no
        // constraint may survive.
        m_bWindowActive = false;
        RevokeAll();
        ++m_WatchdogRevokeCount;
        ME_LOG(LogPlatform, Warn, "PointerConstraint watchdog: window is not active but a cursor constraint was still applied; revoked it now (a focus-lost event was missed).");
    }

    void PointerConstraintState::ReleaseAllAndForgetRequests()
    {
        RevokeAll();
        m_ClipRequest = WindowCursorClipRect{};
        m_CursorModeRequest = EWindowCursorMode::Normal;
        m_bRelativeCaptureRequest = false;
    }

    PointerConstraintDiagnostics PointerConstraintState::GetDiagnostics() const
    {
        PointerConstraintDiagnostics diagnostics;
        diagnostics.bWindowActive = m_bWindowActive;
        diagnostics.FocusRevokeCount = m_FocusRevokeCount;
        diagnostics.WatchdogRevokeCount = m_WatchdogRevokeCount;
        diagnostics.EmptyIntersectionCount = m_EmptyIntersectionCount;
        return diagnostics;
    }

    void PointerConstraintState::ApplyCurrentRequest()
    {
        if (!m_bWindowActive)
        {
            RevokeAll();
            return;
        }

        m_Sink.ApplyCursorMode(m_CursorModeRequest);

        WindowCursorClipRect applied = m_ClipRequest;
        if (applied.bEnabled)
        {
            const WindowCursorClipRect clientRect = m_Sink.GetClientScreenRect();
            if (IsValidRect(clientRect))
            {
                applied = IntersectRects(applied, clientRect);
            }

            if (!IsValidRect(applied))
            {
                // An empty intersection must not be applied, otherwise the cursor gets clamped to another
                // monitor or outside the window.
                ++m_EmptyIntersectionCount;
                ME_LOG(LogPlatform, Warn, "PointerConstraint: requested clip rect does not intersect the client area; not applying it.");
                applied = WindowCursorClipRect{};
            }
        }

        m_Sink.ApplyOsClip(applied);
        m_Sink.ApplyRelativeCapture(m_bRelativeCaptureRequest);
    }

    void PointerConstraintState::RevokeAll()
    {
        WindowCursorClipRect released;
        // Unconditional: the release must never be gated by any cached "we applied it" flag.
        m_Sink.ApplyOsClip(released);
        m_Sink.ApplyRelativeCapture(false);
        m_Sink.ApplyCursorMode(EWindowCursorMode::Normal);
    }
}