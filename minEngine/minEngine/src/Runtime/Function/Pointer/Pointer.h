#pragma once

#include "Runtime/Core/EngineAPI.h"
#include "Runtime/Function/Pointer/PointerModeRequest.h"
#include "Runtime/Function/Pointer/PointerPolicyTypes.h"

namespace minEngine::Pointer
{
    /** Game-tier requests (non-authoring). Hold the returned handle for the duration of the mode. */
    [[nodiscard]] MINENGINE_API PointerModeRequest Show();
    [[nodiscard]] MINENGINE_API PointerModeRequest Hide();
    [[nodiscard]] MINENGINE_API PointerModeRequest ConfineTo(const WindowCursorClipRect& rect);
    [[nodiscard]] MINENGINE_API PointerModeRequest ConfineHiddenTo(const WindowCursorClipRect& rect);
    [[nodiscard]] MINENGINE_API PointerModeRequest Capture();
    /** UI-tier confined visible cursor — beats Game-tier Captured (inventory / menus). */
    [[nodiscard]] MINENGINE_API PointerModeRequest ConfineUiTo(const WindowCursorClipRect& rect);
    [[nodiscard]] MINENGINE_API PointerModeRequest ShowUi();

    MINENGINE_API EPointerMode GetMode();
    MINENGINE_API bool IsCaptured();
    MINENGINE_API bool IsVisible();
    MINENGINE_API const PointerPositionSample& GetPosition();
}
