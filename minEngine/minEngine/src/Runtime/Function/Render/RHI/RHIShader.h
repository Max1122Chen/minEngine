#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Render/RHI/RHIShaderBinding.h"

#include <cstdint>
#include <string>
#include <vector>

namespace minEngine
{
    struct RHIShaderStageBytecode
    {
        RHIGraphicsShaderStage Stage = RHIGraphicsShaderStage::Vertex;
        std::vector<uint32_t> SpirvWords;
    };

    struct RHIShaderCreateDesc
    {
        std::vector<RHIShaderStageBytecode> Stages;
        std::string DebugName;
    };

    // Modern RHI shader handle (no immediate-mode binding).
    class RHIShader
    {
    public:
        virtual ~RHIShader() = default;

        virtual bool IsValid() const = 0;
        virtual const std::string& GetCompileLog() const = 0;
    };

    using RHIShaderRef = std::shared_ptr<RHIShader>;
}
