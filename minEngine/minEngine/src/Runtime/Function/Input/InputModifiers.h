#pragma once
#include "Core.h"
#include "InputActionValue.h"

namespace minEngine
{
    class InputAction;
    struct InputActionValue;
    struct InputActionInstance;

    class InputModifier
    {
    public:
        InputModifier() = default;
        virtual ~InputModifier() = default;

        virtual InputActionValue ModifyRaw(InputActionValue currentValue, float deltaTime) = 0;
    };

    class InputModifierNegate : public InputModifier
    {
    public:
        InputModifierNegate() = default;
        virtual ~InputModifierNegate() = default;

        virtual InputActionValue ModifyRaw(InputActionValue currentValue, float deltaTime) override
        {
            // Negate all the three components in the Value vector currently. TODO: refine later based on ValueType
            currentValue.Value = -currentValue.Value;
            return currentValue;
        }
    };

    enum class InputSwizzleAxisOrder
    {
        // Swap X and Y
        YXZ,
        // Swap X and Z
        ZYX,
        // Swap Y and Z
        XZY
    };

    class InputModifierSwizzleAxis : public InputModifier
    {
    public:
        InputModifierSwizzleAxis() = default;
        InputModifierSwizzleAxis(InputSwizzleAxisOrder order)
            : m_Order(order)
        {}

        virtual ~InputModifierSwizzleAxis() = default;

        InputSwizzleAxisOrder m_Order = InputSwizzleAxisOrder::YXZ;

        virtual InputActionValue ModifyRaw(InputActionValue currentValue, float deltaTime) override
        {
            Vector3 original = currentValue.Value;
            switch(m_Order)
            {
                case InputSwizzleAxisOrder::YXZ:
                    currentValue.Value = Vector3(original.y, original.x, original.z);
                    break;
                case InputSwizzleAxisOrder::ZYX:
                    currentValue.Value = Vector3(original.z, original.y, original.x);
                    break;
                case InputSwizzleAxisOrder::XZY:
                    currentValue.Value = Vector3(original.x, original.z, original.y);
                    break;
                default:
                    break;
            }
            return currentValue;
        }
    };
}