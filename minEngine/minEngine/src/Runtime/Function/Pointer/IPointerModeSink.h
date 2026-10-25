#pragma once

#include "Runtime/Function/Pointer/PointerPolicyTypes.h"

namespace minEngine
{
    /**
     * Window-layer apply seam for PointerPolicyService.
     * Real host: WindowSystemPointerModeSink. Tests: RecordingPointerModeSink.
     */
    class IPointerModeSink
    {
    public:
        virtual ~IPointerModeSink() = default;
        virtual void Apply(const PointerWindowCommand& command) = 0;
    };
}
