#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"

namespace minEngine
{
    enum class InputActionValueType
    {
        Boolean,
        Axis1D,
        Axis2D,
        Axis3D
    };

    struct InputActionValue
    {
        // using Button = bool;
        using Axis1D = float;
        using Axis2D = Vector2;
        using Axis3D = Vector3;

        InputActionValue() = default;
        InputActionValue(const InputActionValue&) = default;
        ~InputActionValue() = default;

        InputActionValue(bool inValue)
            : Type(InputActionValueType::Boolean), Value(inValue ? Vector3(1.0f, 0.0f, 0.0f) : Vector3(0.0f, 0.0f, 0.0f))
        {}
        InputActionValue(float inValue)
            : Type(InputActionValueType::Axis1D), Value(Vector3(inValue, 0.0f, 0.0f))
        {}
        InputActionValue(const Vector2& inValue)
            : Type(InputActionValueType::Axis2D), Value(Vector3(inValue.x, inValue.y, 0.0f))
        {}
        InputActionValue(const Vector3& inValue)
            : Type(InputActionValueType::Axis3D), Value(inValue)
        {}

        InputActionValue(InputActionValueType inType, const Vector3& inValue)
            : Type(inType), Value(inValue)
        {
            switch(Type)
            {
                case InputActionValueType::Boolean:
                case InputActionValueType::Axis1D:
                    Value.y = 0.0f;
                case InputActionValueType::Axis2D:
                    Value.z = 0.0f;
                case InputActionValueType::Axis3D:
                default:
                    return;
            }
        }

        // Get magnitude squared
        float GetMagnitudeSq() const
        {
            switch(Type)
            {
                case InputActionValueType::Boolean:
                case InputActionValueType::Axis1D:
                    return Value.x * Value.x;
                case InputActionValueType::Axis2D:
                    return Value.x * Value.x + Value.y * Value.y;
                case InputActionValueType::Axis3D:
                    return Value.x * Value.x + Value.y * Value.y + Value.z * Value.z;
                default:
                    return 0.0f;
            }
        }

        bool IsNonZero(float tolerance = 0.0001f)
        {
            return GetMagnitudeSq() > (tolerance * tolerance);
        }

        float GetMagnitude() const
        {
            switch(Type)
            {
                case InputActionValueType::Boolean:
                case InputActionValueType::Axis1D:
                    return Math::abs(Value.x);
                case InputActionValueType::Axis2D:
                    return Math::sqrt(Value.x * Value.x + Value.y * Value.y);
                case InputActionValueType::Axis3D:
                    return Math::sqrt(Value.x * Value.x + Value.y * Value.y + Value.z * Value.z);
                default:
                    return 0.0f;
            }
        }

        // Operations
        bool operator==(const InputActionValue& rhs) const
        {
            return (Type == rhs.Type) && (Value == rhs.Value);
        }

        bool operator!=(const InputActionValue& rhs) const
        {
            return !(*this == rhs);
        }

        InputActionValue& operator+=(const InputActionValue& rhs)
        {
            Value += rhs.Value;
            Type = std::max(Type, rhs.Type);    // shall we use std::max or wrap it into minEngine::Math::Max?
            return *this;
        }

        friend InputActionValue operator+(const InputActionValue& lhs, const InputActionValue& rhs)
        {
            InputActionValue result(lhs);
            result += rhs;
            return result;
        }

        InputActionValue& operator *=(float scalar)
        {
            Value *= scalar;
            return *this;
        }

        InputActionValue operator*(float scalar) const
        {
            InputActionValue result(*this);
            result *= scalar;
            return result;
        }
        
        void Reset()
        {
            Value = Vector3(0.0f, 0.0f, 0.0f);
        }
        InputActionValueType GetType() const { return Type; }


        InputActionValueType Type = InputActionValueType::Boolean;

        Vector3 Value = {0.0f, 0.0f, 0.0f}; // Vector3::Zero
    };
}