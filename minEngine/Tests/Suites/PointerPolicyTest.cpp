#include "PointerPolicyTest.h"

#include "Runtime/Function/Pointer/IPointerModeSink.h"
#include "Runtime/Function/Pointer/Pointer.h"
#include "Runtime/Function/Pointer/PointerPolicyService.h"
#include "Runtime/Function/Pointer/PointerPolicyTypes.h"
#include "Runtime/Function/Render/PointerConstraintState.h"

#include "doctest.h"

namespace minEngine
{
    namespace
    {
        bool IsValidRect(const WindowCursorClipRect& rect)
        {
            return rect.Right > rect.Left && rect.Bottom > rect.Top;
        }

        WindowCursorClipRect MakeDesktopRect()
        {
            WindowCursorClipRect rect = PointerConstraintOs::GetDesktopScreenRect();
            if (!IsValidRect(rect))
            {
                // Fallback for platforms without a desktop readback; the geometry cases still run.
                rect.bEnabled = true;
                rect.Left = 0;
                rect.Top = 0;
                rect.Right = 1920;
                rect.Bottom = 1080;
            }
            return rect;
        }

        /** Client area used by the fake window: a strict inset of the desktop. */
        WindowCursorClipRect MakeClientRect()
        {
            const WindowCursorClipRect desktop = MakeDesktopRect();
            WindowCursorClipRect rect;
            rect.bEnabled = true;
            rect.Left = desktop.Left + 40;
            rect.Top = desktop.Top + 40;
            rect.Right = desktop.Right - 40;
            rect.Bottom = desktop.Bottom - 40;
            return rect;
        }

        WindowCursorClipRect MakeRect(int left, int top, int right, int bottom)
        {
            WindowCursorClipRect rect;
            rect.bEnabled = true;
            rect.Left = left;
            rect.Top = top;
            rect.Right = right;
            rect.Bottom = bottom;
            return rect;
        }

        void CheckRectEquals(const WindowCursorClipRect& actual, const WindowCursorClipRect& expected)
        {
            CHECK(actual.bEnabled == expected.bEnabled);
            CHECK(actual.Left == expected.Left);
            CHECK(actual.Top == expected.Top);
            CHECK(actual.Right == expected.Right);
            CHECK(actual.Bottom == expected.Bottom);
        }

        /**
         * Recording sink for the focus-scoped constraint state. It forwards the clip to the real OS
         * (::ClipCursor) so the assertions read the applied state back with ::GetClipCursor instead of
         * trusting the state machine's own bookkeeping.
         */
        class RecordingCursorSink : public ICursorConstraintSink
        {
        public:
            explicit RecordingCursorSink(const WindowCursorClipRect& clientRect)
                : m_ClientRect(clientRect)
            {
            }

            ~RecordingCursorSink() override
            {
                // Never leave a clip behind if a CHECK aborts the case.
                WindowCursorClipRect released;
                PointerConstraintOs::ApplyOsClipRect(released);
            }

            WindowCursorClipRect GetClientScreenRect() const override { return m_ClientRect; }

            void ApplyOsClip(const WindowCursorClipRect& rect) override
            {
                ++ApplyClipCount;
                if (!rect.bEnabled)
                {
                    ++ReleaseClipCount;
                }
                LastAppliedClip = rect;
                PointerConstraintOs::ApplyOsClipRect(rect);
            }

            void ApplyCursorMode(EWindowCursorMode mode) override { CursorMode = mode; }

            void ApplyRelativeCapture(bool enabled) override { bRawCapture = enabled; }

            /** Applied state as the platform reports it (real ::GetClipCursor readback for the clip factor). */
            WindowCursorConstraintState ReadState() const
            {
                WindowCursorConstraintState state;
                state.NativeCursorMode = CursorMode;
                state.bRawCaptureActive = bRawCapture;

                WindowCursorClipRect osClipRect;
                bool bClipActive = false;
                if (PointerConstraintOs::ReadAppliedClipState(bClipActive, osClipRect))
                {
                    state.bObservable = true;
                    state.bClipActive = bClipActive;
                    state.ClipRect = osClipRect;
                }
                return state;
            }

            WindowCursorClipRect LastAppliedClip{};
            EWindowCursorMode CursorMode = EWindowCursorMode::Normal;
            bool bRawCapture = false;
            int ApplyClipCount = 0;
            int ReleaseClipCount = 0;

        private:
            WindowCursorClipRect m_ClientRect;
        };

        void RequireClipAppliedForReal(const RecordingCursorSink& sink)
        {
            const WindowCursorConstraintState state = sink.ReadState();
#if defined(_WIN32)
            REQUIRE(state.bObservable == true);
#endif
            REQUIRE(state.bClipActive == true);
        }

        void CheckReleasedToDesktop(const RecordingCursorSink& sink)
        {
            const WindowCursorConstraintState state = sink.ReadState();
#if defined(_WIN32)
            REQUIRE(state.bObservable == true);
#endif
            CHECK(state.bClipActive == false);
            CHECK(state.NativeCursorMode == EWindowCursorMode::Normal);
            CHECK(state.bRawCaptureActive == false);
        }
    }

    TEST_CASE("pointer-policy - focus loss releases os clip")
    {
        const WindowCursorClipRect clientRect = MakeClientRect();
        RecordingCursorSink sink(clientRect);
        PointerConstraintState state(sink);
        state.InitializeWindowActive(true);

        state.SetCursorVisibleRequest(false);
        state.SetClipRectRequest(MakeRect(clientRect.Left + 20, clientRect.Top + 20, clientRect.Right - 20, clientRect.Bottom - 20));
        RequireClipAppliedForReal(sink);
        CHECK(sink.ReadState().NativeCursorMode == EWindowCursorMode::Disabled);
        CHECK(sink.ReadState().bRawCaptureActive == true);

        // Same-frame revoke: asserted immediately after the focus event, no frame advance in between.
        state.OnWindowFocusChanged(false);

        CHECK(state.GetDiagnostics().bWindowActive == false);
        CHECK(state.GetDiagnostics().FocusRevokeCount == 1);
        CHECK(sink.LastAppliedClip.bEnabled == false);
        CHECK(sink.ReleaseClipCount >= 1);
        CheckReleasedToDesktop(sink);

        // De-sync guard (P7 root cause): an OS clip our bookkeeping never saw must still be released, i.e. the
        // revoke path may never be gated by a cached "we applied it" flag.
        PointerConstraintOs::ApplyOsClipRect(MakeRect(
            clientRect.Left + 30, clientRect.Top + 30, clientRect.Left + 120, clientRect.Top + 120));
        REQUIRE(sink.ReadState().bClipActive == true);

        state.OnWindowFocusChanged(false);
        CHECK(sink.ReadState().bClipActive == false);
    }

    TEST_CASE("pointer-policy - watchdog kills clip when inactive")
    {
        const WindowCursorClipRect clientRect = MakeClientRect();
        RecordingCursorSink sink(clientRect);
        PointerConstraintState state(sink);
        state.InitializeWindowActive(true);

        state.SetCursorVisibleRequest(false);
        state.SetClipRectRequest(MakeRect(clientRect.Left + 20, clientRect.Top + 20, clientRect.Right - 20, clientRect.Bottom - 20));
        RequireClipAppliedForReal(sink);

        // Simulate a dropped focus event: the platform reports "inactive" while the state machine still
        // believes the window is focused (no OnWindowFocusChanged call at all).
        REQUIRE(state.GetDiagnostics().bWindowActive == true);
        state.SyncPlatformFocus(false);

        CHECK(state.GetDiagnostics().bWindowActive == false);
        CHECK(state.GetDiagnostics().WatchdogRevokeCount == 1);
        CHECK(state.GetDiagnostics().FocusRevokeCount == 0);
        CheckReleasedToDesktop(sink);
    }

    TEST_CASE("pointer-policy - focus regain replays last request")
    {
        const WindowCursorClipRect clientRect = MakeClientRect();
        RecordingCursorSink sink(clientRect);
        PointerConstraintState state(sink);
        state.InitializeWindowActive(true);

        const WindowCursorClipRect request = MakeRect(clientRect.Left + 20, clientRect.Top + 20, clientRect.Right - 20, clientRect.Bottom - 20);
        state.SetCursorVisibleRequest(false);
        state.SetClipRectRequest(request);
        RequireClipAppliedForReal(sink);
        const WindowCursorClipRect appliedBeforeLoss = sink.LastAppliedClip;

        state.OnWindowFocusChanged(false);
        REQUIRE(sink.ReadState().bClipActive == false);

        state.OnWindowFocusChanged(true);

        const WindowCursorConstraintState replayed = sink.ReadState();
        CHECK(state.GetDiagnostics().bWindowActive == true);
        // Counter-example guard: a focus regain that ends up unconstrained must FAIL.
        CHECK(replayed.bClipActive == true);
        CHECK(replayed.NativeCursorMode == EWindowCursorMode::Disabled);
        CHECK(replayed.bRawCaptureActive == true);
        CheckRectEquals(sink.LastAppliedClip, appliedBeforeLoss);

        SUBCASE("the request in force at regain time wins over the pre-loss one")
        {
            state.OnWindowFocusChanged(false);
            REQUIRE(sink.ReadState().bClipActive == false);

            const WindowCursorClipRect changedWhileSuspended = MakeRect(clientRect.Left + 60, clientRect.Top + 60, clientRect.Left + 200, clientRect.Top + 200);
            state.SetClipRectRequest(changedWhileSuspended);
            // Suspended: recorded only, never applied.
            CHECK(sink.ReadState().bClipActive == false);

            state.OnWindowFocusChanged(true);
            CheckRectEquals(sink.LastAppliedClip, changedWhileSuspended);
            CHECK(sink.ReadState().bClipActive == true);
        }
    }

    TEST_CASE("pointer-policy - stop and shutdown leave no clip")
    {
        const WindowCursorClipRect clientRect = MakeClientRect();
        const WindowCursorClipRect request = MakeRect(clientRect.Left + 20, clientRect.Top + 20, clientRect.Right - 20, clientRect.Bottom - 20);

        SUBCASE("stop clears the applied constraint")
        {
            RecordingCursorSink sink(clientRect);
            PointerConstraintState state(sink);
            state.InitializeWindowActive(true);
            state.SetCursorVisibleRequest(false);
            state.SetClipRectRequest(request);
            RequireClipAppliedForReal(sink);

            // Exact call sequence of PlayInEditorSession::Stop (editor call sites stay untouched).
            WindowCursorClipRect clearRect;
            state.SetClipRectRequest(clearRect);
            state.SetCursorVisibleRequest(true);

            CheckReleasedToDesktop(sink);
        }

        SUBCASE("shutdown revokes unconditionally")
        {
            RecordingCursorSink sink(clientRect);
            PointerConstraintState state(sink);
            state.InitializeWindowActive(true);
            state.SetCursorVisibleRequest(false);
            state.SetClipRectRequest(request);
            RequireClipAppliedForReal(sink);

            state.ReleaseAllAndForgetRequests();

            CheckReleasedToDesktop(sink);

            // The forgotten request must not come back on the next focus gain.
            state.OnWindowFocusChanged(true);
            CHECK(sink.ReadState().bClipActive == false);
            CHECK(sink.ReadState().NativeCursorMode == EWindowCursorMode::Normal);
        }
    }

    TEST_CASE("pointer-policy - clip rect intersects client area")
    {
        const WindowCursorClipRect clientRect = MakeClientRect();
        RecordingCursorSink sink(clientRect);
        PointerConstraintState state(sink);
        state.InitializeWindowActive(true);

        // Partially outside: the applied value must be request ∩ client.
        const WindowCursorClipRect partiallyOutside = MakeRect(
            clientRect.Left + 10, clientRect.Top + 10, clientRect.Right + 300, clientRect.Bottom + 300);
        const WindowCursorClipRect expected = MakeRect(
            clientRect.Left + 10, clientRect.Top + 10, clientRect.Right, clientRect.Bottom);

        state.SetClipRectRequest(partiallyOutside);
        CheckRectEquals(sink.LastAppliedClip, expected);
#if defined(_WIN32)
        WindowCursorClipRect osClipRect;
        REQUIRE(PointerConstraintOs::ReadOsClipRect(osClipRect) == true);
        CheckRectEquals(osClipRect, expected);
#endif

        // Fully outside: nothing applied, one warn.
        const WindowCursorClipRect fullyOutside = MakeRect(
            clientRect.Right + 5, clientRect.Bottom + 5, clientRect.Right + 205, clientRect.Bottom + 205);
        state.SetClipRectRequest(fullyOutside);

        CHECK(state.GetDiagnostics().EmptyIntersectionCount == 1);
        CHECK(sink.LastAppliedClip.bEnabled == false);
        CHECK(sink.ReadState().bClipActive == false);
    }

    namespace
    {
        class RecordingPointerModeSink : public IPointerModeSink
        {
        public:
            void Apply(const PointerWindowCommand& command) override
            {
                ++ApplyCount;
                Last = command;
            }

            PointerWindowCommand Last{};
            int ApplyCount = 0;
        };

        struct PointerPolicyServiceScope
        {
            PointerPolicyServiceScope()
            {
                PointerPolicyService::SetInstance(&Service);
                Service.SetSink(&Sink);
                Service.Initialize();
            }

            ~PointerPolicyServiceScope()
            {
                Service.Shutdown();
                PointerPolicyService::SetInstance(nullptr);
            }

            RecordingPointerModeSink Sink;
            PointerPolicyService Service;
        };
    }

    TEST_CASE("pointer-policy - map mode captured vs confined")
    {
        PointerClipSpec clip;
        clip.bEnabled = true;
        clip.Rect = MakeRect(10, 10, 100, 100);

        const PointerWindowCommand captured = MapModeToWindowCommand(EPointerMode::Captured, true, clip);
        CHECK(captured.CursorMode == EWindowCursorMode::Disabled);
        CHECK(captured.bClip == true);
        CHECK(captured.bRelativeCapture == true);

        const PointerWindowCommand confined = MapModeToWindowCommand(EPointerMode::Confined, true, clip);
        CHECK(confined.CursorMode == EWindowCursorMode::Normal);
        CHECK(confined.bClip == true);
        CHECK(confined.bRelativeCapture == false);

        const PointerWindowCommand inactive =
            MapModeToWindowCommand(EPointerMode::Captured, false, clip);
        CHECK(inactive.CursorMode == EWindowCursorMode::Normal);
        CHECK(inactive.bClip == false);
        CHECK(inactive.bRelativeCapture == false);
    }

    TEST_CASE("pointer-policy - resolve prefers higher tier")
    {
        PointerPolicyServiceScope scope;
        PointerModeRequest game =
            scope.Service.Request(EPointerTier::Game, EPointerMode::Captured);
        PointerModeRequest ui = scope.Service.Request(EPointerTier::UI, EPointerMode::Confined);

        CHECK(scope.Service.GetResolvedMode() == EPointerMode::Confined);
        CHECK(scope.Service.GetResolvedState().Tier == EPointerTier::UI);
        CHECK(scope.Service.GetResolvedState().StackDepth == 2);
        CHECK(scope.Sink.Last.CursorMode == EWindowCursorMode::Normal);

        ui.Release();
        CHECK(scope.Service.GetResolvedMode() == EPointerMode::Captured);
        CHECK(scope.Sink.Last.CursorMode == EWindowCursorMode::Disabled);

        game.Release();
        CHECK(scope.Service.GetResolvedMode() == EPointerMode::Free);
    }

    TEST_CASE("pointer-policy - refcount coalesces")
    {
        PointerPolicyServiceScope scope;
        PointerModeRequest a = scope.Service.Request(EPointerTier::Game, EPointerMode::Captured);
        PointerModeRequest b = scope.Service.Request(EPointerTier::Game, EPointerMode::Captured);
        CHECK(scope.Service.GetResolvedState().StackDepth == 1);

        a.Release();
        CHECK(scope.Service.GetResolvedMode() == EPointerMode::Captured);

        b.Release();
        CHECK(scope.Service.GetResolvedMode() == EPointerMode::Free);
    }

    TEST_CASE("pointer-policy - out-of-order release is inert")
    {
        PointerPolicyServiceScope scope;
        PointerModeRequest editor =
            scope.Service.Request(EPointerTier::Editor, EPointerMode::Captured);
        PointerModeRequest game =
            scope.Service.Request(EPointerTier::Game, EPointerMode::Hidden);

        editor.Release();
        CHECK(scope.Service.GetResolvedMode() == EPointerMode::Hidden);
        CHECK(scope.Service.GetResolvedState().Tier == EPointerTier::Game);

        game.Release();
        CHECK(scope.Service.GetResolvedMode() == EPointerMode::Free);
    }

    TEST_CASE("pointer-policy - ui show beats game captured")
    {
        PointerPolicyServiceScope scope;
        PointerModeRequest capture =
            scope.Service.Request(EPointerTier::Game, EPointerMode::Captured);
        CHECK(scope.Service.IsPointerCaptured());

        PointerModeRequest menu = Pointer::ShowUi();
        CHECK(scope.Service.GetResolvedMode() == EPointerMode::Free);
        CHECK(scope.Service.IsPointerVisible());
        CHECK(scope.Sink.Last.CursorMode == EWindowCursorMode::Normal);

        menu.Release();
        CHECK(scope.Service.GetResolvedMode() == EPointerMode::Captured);

        capture.Release();
    }

    TEST_CASE("pointer-policy - focus suspends resolved mode")
    {
        PointerPolicyServiceScope scope;
        PointerModeRequest capture =
            scope.Service.Request(EPointerTier::Game, EPointerMode::Captured);
        scope.Service.OnWindowFocusLost();
        CHECK(scope.Service.GetResolvedState().bSuspendedByFocus == true);
        CHECK(scope.Service.GetResolvedMode() == EPointerMode::Free);
        CHECK(scope.Sink.Last.CursorMode == EWindowCursorMode::Normal);

        scope.Service.OnWindowFocusGained();
        CHECK(scope.Service.GetResolvedState().bSuspendedByFocus == false);
        CHECK(scope.Service.GetResolvedMode() == EPointerMode::Captured);
        CHECK(scope.Sink.Last.CursorMode == EWindowCursorMode::Disabled);

        capture.Release();
    }
}
