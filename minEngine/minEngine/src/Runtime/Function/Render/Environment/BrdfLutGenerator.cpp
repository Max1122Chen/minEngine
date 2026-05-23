#include "BrdfLutGenerator.h"

#include "../Texture.h"
#include "../Texture2DLoader.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Resource/ImageLoader.h"

#include <cmath>
#include <vector>

namespace minEngine
{
    namespace
    {
        constexpr float kPi = 3.14159265359f;

        float RadicalInverseVdC(uint32_t bits)
        {
            bits = (bits << 16u) | (bits >> 16u);
            bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
            bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
            bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
            bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
            return static_cast<float>(bits) * 2.3283064365386963e-10f;
        }

        Vector2 Hammersley(uint32_t index, uint32_t sampleCount)
        {
            return Vector2(
                static_cast<float>(index) / static_cast<float>(sampleCount),
                RadicalInverseVdC(index));
        }

        Vector3 ImportanceSampleGGX(Vector2 xi, const Vector3& N, float roughness)
        {
            const float a = roughness * roughness;
            const float phi = 2.0f * kPi * xi.x + 0.0f;
            const float cosTheta = std::sqrt((1.0f - xi.y) / (1.0f + (a * a - 1.0f) * xi.y));
            const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));

            const Vector3 H(
                std::cos(phi) * sinTheta,
                std::sin(phi) * sinTheta,
                cosTheta);

            const Vector3 up = std::abs(N.z) < 0.999f ? Vector3(0.0f, 0.0f, 1.0f) : Vector3(1.0f, 0.0f, 0.0f);
            const Vector3 tangent = glm::normalize(glm::cross(up, N));
            const Vector3 bitangent = glm::cross(N, tangent);

            return glm::normalize(tangent * H.x + bitangent * H.y + N * H.z);
        }

        float GeometrySchlickGGX(float NdotV, float roughness)
        {
            const float r = roughness + 1.0f;
            const float k = (r * r) / 8.0f;
            return NdotV / (NdotV * (1.0f - k) + k);
        }

        float GeometrySmith(const Vector3& N, const Vector3& V, const Vector3& L, float roughness)
        {
            const float NdotV = std::max(glm::dot(N, V), 0.0f);
            const float NdotL = std::max(glm::dot(N, L), 0.0f);
            const float ggxView = GeometrySchlickGGX(NdotV, roughness);
            const float ggxLight = GeometrySchlickGGX(NdotL, roughness);
            return ggxView * ggxLight;
        }

        Vector2 IntegrateBRDF(float NdotV, float roughness)
        {
            const Vector3 V(std::sqrt(std::max(0.0f, 1.0f - NdotV * NdotV)), 0.0f, NdotV);
            float A = 0.0f;
            float B = 0.0f;
            const Vector3 N(0.0f, 0.0f, 1.0f);
            constexpr uint32_t sampleCount = 64u;

            for (uint32_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
            {
                const Vector2 xi = Hammersley(sampleIndex, sampleCount);
                const Vector3 H = ImportanceSampleGGX(xi, N, roughness);
                const Vector3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);

                const float NdotL = std::max(L.z, 0.0f);
                const float NdotH = std::max(H.z, 0.0f);
                const float VdotH = std::max(glm::dot(V, H), 0.0f);

                if (NdotL > 0.0f)
                {
                    const float G = GeometrySmith(N, V, L, roughness);
                    const float G_Vis = (G * VdotH) / (NdotH * NdotV);
                    const float Fc = std::pow(1.0f - VdotH, 5.0f);
                    A += (1.0f - Fc) * G_Vis;
                    B += Fc * G_Vis;
                }
            }

            return Vector2(A, B) / static_cast<float>(sampleCount);
        }
    }

    std::shared_ptr<Texture2D> BrdfLutGenerator::CreateIntegratedBrdfLut(
        RHI& rhi,
        uint32_t size,
        std::string* outError)
    {
        if (size == 0)
        {
            if (outError)
            {
                *outError = "BRDF LUT size must be > 0.";
            }
            return nullptr;
        }

        std::vector<float> pixels(static_cast<size_t>(size) * size * 3, 0.0f);
        for (uint32_t y = 0; y < size; ++y)
        {
            for (uint32_t x = 0; x < size; ++x)
            {
                const float NdotV = (static_cast<float>(x) + 0.5f) / static_cast<float>(size);
                const float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(size);
                const Vector2 integrated = IntegrateBRDF(
                    std::min(std::max(NdotV, 0.0f), 1.0f),
                    std::min(std::max(roughness, 0.0f), 1.0f));

                const size_t pixelIndex = (static_cast<size_t>(y) * size + x) * 3;
                pixels[pixelIndex + 0] = integrated.x;
                pixels[pixelIndex + 1] = integrated.y;
                pixels[pixelIndex + 2] = 0.0f;
            }
        }

        ImagePixels image;
        image.Storage = ImageStorage::Float32;
        image.F32 = pixels.data();
        image.Width = static_cast<int>(size);
        image.Height = static_cast<int>(size);
        image.Channels = 3;

        std::shared_ptr<Texture2D> texture =
            Texture2DLoader::CreateFromHdrPixels(rhi, image, "EngineBrdfLUT");
        image.F32 = nullptr;

        if (!texture && outError)
        {
            *outError = "Failed to upload integrated BRDF LUT.";
        }

        if (texture)
        {
            ME_CORE_INFO("BrdfLutGenerator: created {}x{} integrated BRDF LUT.", size, size);
        }

        return texture;
    }
}
