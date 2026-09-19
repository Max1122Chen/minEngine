#include "MaterialInterface.h"

#include "Runtime/Function/Render/MaterialRenderProxy.h"

namespace minEngine
{
    bool MaterialInterface::IsTranslucent() const
    {
        const MaterialRenderProxy* proxy = GetRenderProxy();
        return proxy != nullptr && proxy->IsTranslucent();
    }

    bool MaterialInterface::IsMasked() const
    {
        const MaterialRenderProxy* proxy = GetRenderProxy();
        return proxy != nullptr && proxy->IsMasked();
    }

    bool MaterialInterface::IsCompiledForDraw() const
    {
        const MaterialRenderProxy* proxy = GetRenderProxy();
        return proxy != nullptr && proxy->IsCompiledForDraw();
    }
}
