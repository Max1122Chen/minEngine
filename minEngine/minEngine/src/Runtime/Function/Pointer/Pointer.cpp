#include "Runtime/Function/Pointer/Pointer.h"

#include "Runtime/Function/Pointer/PointerPolicyService.h"

namespace minEngine::Pointer
{
    namespace
    {
        PointerClipSpec MakeClip(const WindowCursorClipRect& rect)
        {
            PointerClipSpec clip;
            clip.bEnabled = rect.bEnabled;
            clip.Rect = rect;
            return clip;
        }
    }

    PointerModeRequest Show()
    {
        if (!PointerPolicyService::HasInstance())
        {
            return {};
        }
        return PointerPolicyService::Get().Request(EPointerTier::Game, EPointerMode::Free);
    }

    PointerModeRequest Hide()
    {
        if (!PointerPolicyService::HasInstance())
        {
            return {};
        }
        return PointerPolicyService::Get().Request(EPointerTier::Game, EPointerMode::Hidden);
    }

    PointerModeRequest ConfineTo(const WindowCursorClipRect& rect)
    {
        if (!PointerPolicyService::HasInstance())
        {
            return {};
        }
        return PointerPolicyService::Get().Request(EPointerTier::Game, EPointerMode::Confined, MakeClip(rect));
    }

    PointerModeRequest ConfineHiddenTo(const WindowCursorClipRect& rect)
    {
        if (!PointerPolicyService::HasInstance())
        {
            return {};
        }
        return PointerPolicyService::Get().Request(
            EPointerTier::Game,
            EPointerMode::ConfinedHidden,
            MakeClip(rect));
    }

    PointerModeRequest Capture()
    {
        if (!PointerPolicyService::HasInstance())
        {
            return {};
        }
        PointerClipSpec clip;
        return PointerPolicyService::Get().Request(EPointerTier::Game, EPointerMode::Captured, clip);
    }

    PointerModeRequest ConfineUiTo(const WindowCursorClipRect& rect)
    {
        if (!PointerPolicyService::HasInstance())
        {
            return {};
        }
        return PointerPolicyService::Get().Request(EPointerTier::UI, EPointerMode::Confined, MakeClip(rect));
    }

    PointerModeRequest ShowUi()
    {
        if (!PointerPolicyService::HasInstance())
        {
            return {};
        }
        return PointerPolicyService::Get().Request(EPointerTier::UI, EPointerMode::Free);
    }

    EPointerMode GetMode()
    {
        if (!PointerPolicyService::HasInstance())
        {
            return EPointerMode::Free;
        }
        return PointerPolicyService::Get().GetResolvedMode();
    }

    bool IsCaptured()
    {
        return PointerPolicyService::HasInstance() && PointerPolicyService::Get().IsPointerCaptured();
    }

    bool IsVisible()
    {
        return !PointerPolicyService::HasInstance() || PointerPolicyService::Get().IsPointerVisible();
    }

    const PointerPositionSample& GetPosition()
    {
        static PointerPositionSample s_Empty;
        if (!PointerPolicyService::HasInstance())
        {
            return s_Empty;
        }
        return PointerPolicyService::Get().GetPosition();
    }
}
