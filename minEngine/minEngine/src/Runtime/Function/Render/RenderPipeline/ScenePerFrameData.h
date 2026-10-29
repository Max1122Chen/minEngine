#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"

namespace minEngine
{
    /**
     * The block shaders read from u_PerFrame (set 0, binding 0).
     *
     * One block belongs to one consumer role. A draw reads it when it is replayed, not when it is
     * recorded, so a role must never share its block with another role: the later recorder would
     * rewrite the matrices the earlier role's draws still have to read.
     */
    struct PerFrameData
    {
        Matrix4 View;
        Matrix4 Proj;
        Matrix4 ViewProj;

        Vector4 CameraPos;
    };
}
