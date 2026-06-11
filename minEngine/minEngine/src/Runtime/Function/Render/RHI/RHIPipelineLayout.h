#pragma once

#include "Core.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace minEngine
{
    class RHIBindingLayout;

    constexpr uint32_t kMaxPipelineDescriptorSets = 4;

    class RHIPipelineLayout
    {
    public:
        virtual ~RHIPipelineLayout() = default;

        virtual uint32_t GetSetLayoutCount() const = 0;
        virtual RHIBindingLayout* GetSetLayout(uint32_t setIndex) const = 0;
    };

    using RHIPipelineLayoutRef = std::shared_ptr<RHIPipelineLayout>;
}
