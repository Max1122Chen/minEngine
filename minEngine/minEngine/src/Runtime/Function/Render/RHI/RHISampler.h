#pragma once

#include "Core.h"

#include <cstdint>
#include <memory>

namespace minEngine
{
    /** MVP filters for RHISamplerDesc (mip variants applied at backend bind/create). */
    ME_ENUM()
    enum class RHISamplerFilter : uint8_t
    {
        Nearest = 0,
        Linear = 1,
    };

    ME_ENUM()
    enum class RHISamplerAddress : uint8_t
    {
        Repeat = 0,
        MirroredRepeat = 1,
        ClampToEdge = 2,
        ClampToBorder = 3,
    };

    struct RHISamplerDesc
    {
        RHISamplerFilter MinFilter = RHISamplerFilter::Linear;
        RHISamplerFilter MagFilter = RHISamplerFilter::Linear;
        RHISamplerAddress AddressU = RHISamplerAddress::Repeat;
        RHISamplerAddress AddressV = RHISamplerAddress::Repeat;
        RHISamplerAddress AddressW = RHISamplerAddress::Repeat;

        bool operator==(const RHISamplerDesc& other) const
        {
            return MinFilter == other.MinFilter && MagFilter == other.MagFilter
                && AddressU == other.AddressU && AddressV == other.AddressV
                && AddressW == other.AddressW;
        }

        bool operator!=(const RHISamplerDesc& other) const { return !(*this == other); }
    };

    inline RHISamplerDesc MakeLinearRepeatSamplerDesc()
    {
        return RHISamplerDesc{};
    }

    inline RHISamplerDesc MakeNearestRepeatSamplerDesc()
    {
        RHISamplerDesc desc;
        desc.MinFilter = RHISamplerFilter::Nearest;
        desc.MagFilter = RHISamplerFilter::Nearest;
        return desc;
    }

    inline RHISamplerDesc MakeNearestClampSamplerDesc()
    {
        RHISamplerDesc desc = MakeNearestRepeatSamplerDesc();
        desc.AddressU = RHISamplerAddress::ClampToEdge;
        desc.AddressV = RHISamplerAddress::ClampToEdge;
        desc.AddressW = RHISamplerAddress::ClampToEdge;
        return desc;
    }

    inline RHISamplerDesc MakeLinearClampSamplerDesc()
    {
        RHISamplerDesc desc;
        desc.AddressU = RHISamplerAddress::ClampToEdge;
        desc.AddressV = RHISamplerAddress::ClampToEdge;
        desc.AddressW = RHISamplerAddress::ClampToEdge;
        return desc;
    }

    /** Depth/shadow sampling: nearest + border (matches prior GL depth path). */
    inline RHISamplerDesc MakeShadowCompareSamplerDesc()
    {
        RHISamplerDesc desc = MakeNearestClampSamplerDesc();
        desc.AddressU = RHISamplerAddress::ClampToBorder;
        desc.AddressV = RHISamplerAddress::ClampToBorder;
        desc.AddressW = RHISamplerAddress::ClampToBorder;
        return desc;
    }

    class RHISampler
    {
    public:
        virtual ~RHISampler() = default;

        virtual const RHISamplerDesc& GetDesc() const = 0;
    };

    using RHISamplerRef = std::shared_ptr<RHISampler>;

    struct RHISamplerDescHash
    {
        size_t operator()(const RHISamplerDesc& desc) const noexcept
        {
            size_t h = static_cast<size_t>(desc.MinFilter);
            h = (h * 131u) + static_cast<size_t>(desc.MagFilter);
            h = (h * 131u) + static_cast<size_t>(desc.AddressU);
            h = (h * 131u) + static_cast<size_t>(desc.AddressV);
            h = (h * 131u) + static_cast<size_t>(desc.AddressW);
            return h;
        }
    };
}

#include "Generated/Reflection/RHISampler.gen.h"
