#pragma once

#include "Core.h"
#include "Math/Math.h"

#include "Runtime/Function/Input/InputKeys.h"
#include "Runtime/Function/Framework/Transform/Transform.h"

#include <cstdint>

namespace minEngine
{
    struct ViewportFrameState
    {
        bool Hovered = false;
        bool Focused = false;
        Vector2 ContentSize;
        Vector2 ImageMin;
        Vector2 ImageSize;
    };

    struct GizmoState
    {
        bool Using = false;
        bool Hovering = false;
        bool Manipulated = false;
        struct DeltaTransform
        {
            Vector3 PositionDelta;
            glm::quat RotationDelta;
            Vector3 ScaleDelta;

            void Reset()
            {
                PositionDelta = Vector3(0.0f, 0.0f, 0.0f);
                RotationDelta = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                ScaleDelta = Vector3(1.0f, 1.0f, 1.0f);
            }
        } Delta;
        enum class Mode
        {
            None,
            Translate,
            Rotate,
            Scale
        } mode = Mode::None;
        enum class Axis
        {
            None = 0 << 0,
            X = 1 << 0,
            Y = 1 << 1,
            Z = 1 << 2
        } axis = Axis::None;
    };

    inline GizmoState::Axis operator|(GizmoState::Axis lhs, GizmoState::Axis rhs)
    {
        return static_cast<GizmoState::Axis>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
    }

    inline GizmoState::Axis operator&(GizmoState::Axis lhs, GizmoState::Axis rhs)
    {
        return static_cast<GizmoState::Axis>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
    }

    inline GizmoState::Axis& operator|=(GizmoState::Axis& lhs, GizmoState::Axis rhs)
    {
        lhs = lhs | rhs;
        return lhs;
    }

    enum class ViewportInputTriggerType : uint8_t
    {
        Pressed,
        Released,
        Down
    };

    enum class ViewportInputCommandType : uint8_t
    {
        BeginNavigate,
        EndNavigate,
        MoveForward,
        MoveBackward,
        MoveLeft,
        MoveRight,
        MoveUp,
        MoveDown,
        SpeedBoost,
        AdjustMoveSpeed,
        FocusSelection,
        Cancel,
        SetGizmoModeTranslate,
        SetGizmoModeRotate,
        SetGizmoModeScale,
        Select
    };

    struct ViewportInputBinding
    {
        InputKey Key;
        ViewportInputTriggerType Trigger = ViewportInputTriggerType::Pressed;
        ViewportInputCommandType Command = ViewportInputCommandType::Cancel;
    };

    struct ViewportInputCommand
    {
        ViewportInputCommandType Type = ViewportInputCommandType::Cancel;
        ViewportInputTriggerType Trigger = ViewportInputTriggerType::Pressed;
    };
}
