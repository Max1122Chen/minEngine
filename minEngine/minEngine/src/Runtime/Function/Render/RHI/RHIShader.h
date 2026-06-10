#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"

#include <string>

namespace minEngine
{
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
