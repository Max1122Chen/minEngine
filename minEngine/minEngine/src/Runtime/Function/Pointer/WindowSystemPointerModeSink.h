#pragma once

#include "Runtime/Function/Pointer/IPointerModeSink.h"

namespace minEngine
{
    class WindowSystem;

    /** Applies pointer window commands through WindowSystem (focus-scoped constraint path). */
    class WindowSystemPointerModeSink : public IPointerModeSink
    {
    public:
        explicit WindowSystemPointerModeSink(WindowSystem& window);
        void Apply(const PointerWindowCommand& command) override;

    private:
        WindowSystem& m_Window;
    };
}
