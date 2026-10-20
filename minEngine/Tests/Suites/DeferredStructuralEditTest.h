#pragma once

namespace minEngine
{
    bool RunDeferredStructuralEditTests();

    /** Scene retirement contract: a retired scene survives until the frame's submissions flush. */
    bool RunRetiredSceneOutlivesFrameTest();
}
