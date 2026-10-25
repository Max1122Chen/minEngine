#pragma once

#include "Runtime/Core/EngineAPI.h"
#include "Runtime/Function/Render/WindowSystem.h"

#include <cstdint>

namespace minEngine
{
    enum class EPointerMode : uint8_t
    {
        Free = 0,
        Confined,
        Hidden,
        ConfinedHidden,
        Captured,
    };

    enum class EPointerTier : int
    {
        Default = 0,
        Game = 40,
        UI = 60,
        Play = 80,
        Editor = 100,
    };

    struct PointerClipSpec
    {
        bool bEnabled = false;
        WindowCursorClipRect Rect{};
    };

    struct PointerWindowCommand
    {
        EWindowCursorMode CursorMode = EWindowCursorMode::Normal;
        bool bClip = false;
        WindowCursorClipRect ClipRect{};
        bool bRelativeCapture = false;
    };

    struct PointerResolvedState
    {
        EPointerMode Mode = EPointerMode::Free;
        EPointerTier Tier = EPointerTier::Default;
        int StackDepth = 0;
        PointerClipSpec Clip{};
        bool bSuspendedByFocus = false;
    };

    struct PointerAppliedConstraint
    {
        EWindowCursorMode NativeCursorMode = EWindowCursorMode::Normal;
        bool bClipActive = false;
        WindowCursorClipRect ClipRect{};
        bool bRawCaptureActive = false;
        bool bWindowActive = false;
        bool bObservable = false;
    };

    struct PointerPositionSample
    {
        Vector2 WindowPx{0.0f, 0.0f};
        Vector2 ViewportPointPx{0.0f, 0.0f};
        Vector2 ReferencePx{0.0f, 0.0f};
        Vector2 RelativeDelta{0.0f, 0.0f};
        bool bAbsoluteValid = false;
        bool bReferenceValid = false;
        uint64_t FrameStamp = 0;
    };

    /** Editor / host settings for pointer policy (not serialized this slice). */
    struct PointerPolicySettings
    {
        /** Primary release chord: Left/Right Shift held + F1 pressed. */
        bool bKeepLeftAltAlias = true;
    };

    /** Pure mapping: mode + focus → window command. Independent of GLFW. */
    MINENGINE_API PointerWindowCommand MapModeToWindowCommand(
        EPointerMode mode,
        bool bWindowActive,
        const PointerClipSpec& clip);

    MINENGINE_API bool PointerClipSpecsEqual(const PointerClipSpec& a, const PointerClipSpec& b);
}
