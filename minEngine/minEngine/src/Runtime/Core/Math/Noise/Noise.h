#pragma once

#include "EngineAPI.h"

#include "Math/Math.h"

namespace minEngine::Math::Noise
{
    /**
     * Sampleable continuous pseudo-random field.
     * Config mutation and Sample* must not run concurrently.
     * Output of concrete types is approximately in [-1, 1] unless documented otherwise.
     */
    class MINENGINE_API Noise
    {
    public:
        virtual ~Noise() = default;

        virtual float Sample1D(float x) const = 0;
        virtual float Sample2D(float x, float y) const = 0;
        virtual float Sample3D(float x, float y, float z) const = 0;

        float Sample(float x) const { return Sample1D(x); }
        float Sample(const Vector2& p) const { return Sample2D(p.x, p.y); }
        float Sample(const Vector3& p) const { return Sample3D(p.x, p.y, p.z); }
    };
}
