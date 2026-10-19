#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/UI/ScreenUIPointerTypes.h"

#include <cstdint>

namespace minEngine
{
    class WidgetComponent;
    class CanvasComponent;
    class DragSourceComponent;
    class DropTargetComponent;

    enum class EUIDragPhase : uint8_t
    {
        None = 0,
        Pending,
        Dragging,
    };

    struct ScreenUIDragSession
    {
        EUIDragPhase Phase = EUIDragPhase::None;
        WidgetComponent* SourceWidget = nullptr;
        DragSourceComponent* Source = nullptr;
        CanvasComponent* SourceCanvas = nullptr;
        DropTargetComponent* HoveredTarget = nullptr;
        Vector2 PressReferencePx{ 0.0f, 0.0f };
        Vector2 CurrentReferencePx{ 0.0f, 0.0f };
        Vector2 PointerDeltaPx{ 0.0f, 0.0f };
        bool bSuppressClick = false;
        EUIPointerButton InitiatingButton = EUIPointerButton::None;

        void Reset()
        {
            Phase = EUIDragPhase::None;
            SourceWidget = nullptr;
            Source = nullptr;
            SourceCanvas = nullptr;
            HoveredTarget = nullptr;
            PressReferencePx = Vector2(0.0f, 0.0f);
            CurrentReferencePx = Vector2(0.0f, 0.0f);
            PointerDeltaPx = Vector2(0.0f, 0.0f);
            bSuppressClick = false;
            InitiatingButton = EUIPointerButton::None;
        }
    };
}
