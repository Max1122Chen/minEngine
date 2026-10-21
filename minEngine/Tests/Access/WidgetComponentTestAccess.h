#pragma once

#include "Runtime/Function/Framework/Components/WidgetComponent.h"

namespace minEngine::Testing
{
    template<>
    class TestAccess<WidgetComponent>
    {
    public:
        /** The proxy refill is pending; clears through the ordinary end-of-frame update. */
        static bool IsRenderStateDirty(const WidgetComponent& widget)
        {
            return widget.m_bRenderStateDirty;
        }

        static void ClearRenderStateDirty(WidgetComponent& widget)
        {
            widget.m_bRenderStateDirty = false;
        }
    };
}
