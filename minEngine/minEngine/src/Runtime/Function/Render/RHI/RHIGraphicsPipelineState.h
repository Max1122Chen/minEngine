#pragma once

namespace minEngine
{
    // Should be inherited by concrete RHI backends.
    class RHIGraphicsPipelineState
    {

    };

    class RHIGraphicsPSOCreateInfo
    {

    };

    // For RHIs that don't support graphics PSOs.
    class RHIGraphicsPSOStateFallback : public RHIGraphicsPipelineState
    {

    };
}