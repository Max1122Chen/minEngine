#include "Math/Noise/CellularNoise.h"
#include "Math/Noise/FractalNoise.h"
#include "Math/Noise/PerlinNoise.h"
#include "Math/Noise/ValueNoise.h"

#include "doctest.h"

#include <cmath>
#include <memory>

namespace
{
    using minEngine::Math::Noise::CellularNoise;
    using minEngine::Math::Noise::FractalNoise;
    using minEngine::Math::Noise::PerlinNoise;
    using minEngine::Math::Noise::ValueNoise;

    constexpr float kEps = 1.0e-5f;
    constexpr float kRangeSlack = 0.05f;

    bool NearlyEqual(float a, float b)
    {
        return std::fabs(a - b) <= kEps;
    }

    template <typename TNoise>
    void CheckDeterminism2D(TNoise& a, TNoise& b)
    {
        CHECK(NearlyEqual(a.Sample2D(1.25f, -3.5f), b.Sample2D(1.25f, -3.5f)));
        CHECK(NearlyEqual(a.Sample2D(0.0f, 0.0f), a.Sample2D(0.0f, 0.0f)));
    }

    template <typename TNoise>
    void CheckRange2D(TNoise& noise)
    {
        for (int i = 0; i < 64; ++i)
        {
            const float x = static_cast<float>(i) * 0.37f - 8.0f;
            const float y = static_cast<float>(i) * -0.19f + 3.0f;
            const float v = noise.Sample2D(x, y);
            CHECK(v >= -1.0f - kRangeSlack);
            CHECK(v <= 1.0f + kRangeSlack);
        }
    }
}

TEST_CASE("noise: value determinism and seed [smoke]")
{
    ValueNoise a(42u);
    ValueNoise b(42u);
    ValueNoise c(43u);
    a.SetFrequency(1.0f);
    b.SetFrequency(1.0f);
    c.SetFrequency(1.0f);

    CheckDeterminism2D(a, b);
    CHECK_FALSE(NearlyEqual(a.Sample2D(2.0f, 3.0f), c.Sample2D(2.0f, 3.0f)));
    CheckRange2D(a);
}

TEST_CASE("noise: perlin determinism range continuity [full]")
{
    PerlinNoise a(7u);
    PerlinNoise b(7u);
    CheckDeterminism2D(a, b);
    CheckRange2D(a);

    const float p0 = a.Sample2D(1.0f, 2.0f);
    const float p1 = a.Sample2D(1.0f + 1.0e-3f, 2.0f);
    CHECK(std::fabs(p1 - p0) < 0.1f);
}

TEST_CASE("noise: cellular determinism range [full]")
{
    CellularNoise a(99u);
    CellularNoise b(99u);
    a.SetJitter(1.0f);
    b.SetJitter(1.0f);
    CheckDeterminism2D(a, b);
    CheckRange2D(a);
}

TEST_CASE("noise: fractal fbm edges and polymorphism [full]")
{
    auto source = std::make_shared<PerlinNoise>(11u);
    source->SetFrequency(1.0f);

    FractalNoise fractal;
    fractal.SetSource(source);
    fractal.SetFrequency(0.5f);
    fractal.SetOctaves(1);
    fractal.SetGain(0.5f);
    fractal.SetLacunarity(2.0f);

    const float single = source->Sample2D(0.5f * 1.25f, 0.5f * -0.75f);
    const float viaFractal = fractal.Sample2D(1.25f, -0.75f);
    CHECK(NearlyEqual(single, viaFractal));

    fractal.SetOctaves(0);
    CHECK(fractal.GetOctaves() == 1);
    fractal.SetOctaves(100);
    CHECK(fractal.GetOctaves() == 16);

    fractal.SetOctaves(5);
    fractal.SetGain(0.0f);
    const float gainZero = fractal.Sample2D(1.0f, 2.0f);
    CHECK(NearlyEqual(gainZero, source->Sample2D(0.5f, 1.0f)));

    FractalNoise empty;
    CHECK(NearlyEqual(empty.Sample3D(1.0f, 2.0f, 3.0f), 0.0f));

    const minEngine::Math::Noise::Noise& field = fractal;
    CHECK(std::isfinite(field.Sample2D(0.1f, 0.2f)));
    CheckRange2D(fractal);
}

TEST_CASE("noise: value 1d 3d smoke [full]")
{
    ValueNoise noise(5u);
    CHECK(std::isfinite(noise.Sample1D(-2.5f)));
    CHECK(std::isfinite(noise.Sample3D(1.0f, 2.0f, 3.0f)));
    CHECK(NearlyEqual(noise.Sample1D(1.0f), noise.Sample(1.0f)));
}