#pragma once

#include "Core.h"
#include "Runtime/Core/Reflection/ReflectionAnnotations.h"

#include <cstdint>

namespace minEngine
{
    using PhysicsBodyId = uint32_t;

    inline constexpr PhysicsBodyId InvalidPhysicsBodyId = UINT32_MAX;

    ME_ENUM()
    enum class EBodyType : uint8_t
    {
        Static = 0,
        Dynamic,
        Kinematic,
    };
}

#include "Generated/Reflection/PhysicsTypes.gen.h"
