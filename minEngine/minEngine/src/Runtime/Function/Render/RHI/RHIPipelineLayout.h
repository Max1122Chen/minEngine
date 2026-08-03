#pragma once

#include "Core.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace minEngine
{
    class RHIShaderBindingSetLayout;

    constexpr uint32_t kMaxShaderBindingSets = 4;

    class RHIPipelineLayout
    {
    public:
        virtual ~RHIPipelineLayout() = default;

        virtual uint32_t GetShaderBindingSetLayoutCount() const = 0;
        virtual RHIShaderBindingSetLayout* GetShaderBindingSetLayout(uint32_t setIndex) const = 0;
    };

    using RHIPipelineLayoutRef = std::shared_ptr<RHIPipelineLayout>;
}
