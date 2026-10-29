#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Render/RHI/RHIDeferredCreate.h"
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

    /**
     * Modern RHI shader handle (no immediate-mode binding).
     *
     * Specializing the bytecode and linking the program drive the graphics API, so the program is
     * created by the graphics role whatever thread asked for the shader. The handle therefore
     * answers three different things, and a caller has to be able to tell them apart:
     *  - IsReady() false, HasNativeCreateFailed() false: the program is queued. Nothing has run
     *    yet, so there is no result to report; the caller keeps the handle and waits.
     *  - IsReady() true, IsValid() true: the program exists and can be bound.
     *  - HasNativeCreateFailed() true: the driver refused it. GetCompileLog() carries the message
     *    and no later frame changes the answer, so the caller reports it once.
     */
    class RHIShader : public RHIDeferredCreateTarget
    {
    public:
        virtual ~RHIShader() = default;

        /** The program compiled and linked. False while it is queued as well as after a failure. */
        virtual bool IsValid() const = 0;
        /** The driver's message: empty until the program has been created, then why it failed. */
        virtual const std::string& GetCompileLog() const = 0;
    };

    using RHIShaderRef = std::shared_ptr<RHIShader>;
}
