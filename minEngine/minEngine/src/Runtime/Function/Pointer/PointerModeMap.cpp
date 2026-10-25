#include "Runtime/Function/Pointer/PointerPolicyTypes.h"

namespace minEngine
{
    bool PointerClipSpecsEqual(const PointerClipSpec& a, const PointerClipSpec& b)
    {
        if (a.bEnabled != b.bEnabled)
        {
            return false;
        }
        if (!a.bEnabled)
        {
            return true;
        }
        return a.Rect.Left == b.Rect.Left && a.Rect.Top == b.Rect.Top && a.Rect.Right == b.Rect.Right
            && a.Rect.Bottom == b.Rect.Bottom && a.Rect.bEnabled == b.Rect.bEnabled;
    }

    PointerWindowCommand MapModeToWindowCommand(
        EPointerMode mode,
        bool bWindowActive,
        const PointerClipSpec& clip)
    {
        PointerWindowCommand command;
        if (!bWindowActive)
        {
            command.CursorMode = EWindowCursorMode::Normal;
            command.bClip = false;
            command.bRelativeCapture = false;
            return command;
        }

        switch (mode)
        {
        case EPointerMode::Confined:
            command.CursorMode = EWindowCursorMode::Normal;
            command.bClip = clip.bEnabled;
            command.ClipRect = clip.Rect;
            command.ClipRect.bEnabled = clip.bEnabled;
            command.bRelativeCapture = false;
            break;
        case EPointerMode::Hidden:
            command.CursorMode = EWindowCursorMode::Hidden;
            command.bClip = false;
            command.bRelativeCapture = false;
            break;
        case EPointerMode::ConfinedHidden:
            command.CursorMode = EWindowCursorMode::Hidden;
            command.bClip = clip.bEnabled;
            command.ClipRect = clip.Rect;
            command.ClipRect.bEnabled = clip.bEnabled;
            command.bRelativeCapture = false;
            break;
        case EPointerMode::Captured:
            command.CursorMode = EWindowCursorMode::Disabled;
            command.bClip = clip.bEnabled;
            command.ClipRect = clip.Rect;
            command.ClipRect.bEnabled = clip.bEnabled;
            command.bRelativeCapture = true;
            break;
        case EPointerMode::Free:
        default:
            command.CursorMode = EWindowCursorMode::Normal;
            command.bClip = false;
            command.bRelativeCapture = false;
            break;
        }
        return command;
    }
}
