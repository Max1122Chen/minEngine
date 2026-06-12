#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Core/Math/Quaternion.h"

namespace minEngine
{
    class PhysicsConversion
    {
    public:
        // Engine (X forward, Y up, Z right) <-> Jolt (Y up, permuted axes per PHYS-F01 design §3.4).
        static Vector3 ToJoltPosition(const Vector3& enginePosition);
        static Vector3 FromJoltPosition(const Vector3& joltPosition);

        static Quaternion ToJoltQuaternion(const Quaternion& engineRotation);
        static Quaternion FromJoltQuaternion(const Quaternion& joltRotation);
    };
}
