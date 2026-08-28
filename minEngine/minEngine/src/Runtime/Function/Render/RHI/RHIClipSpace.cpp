#include "RHIClipSpace.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace minEngine
{
    Matrix4 RHIClipSpace::MakePerspective(float fovYRadians, float aspect, float zNear, float zFar)
    {
        if (GetClipSpaceCapabilities().ClipDepthRange == RHIClipDepthRange::ZeroToOne)
        {
            return glm::perspectiveRH_ZO(fovYRadians, aspect, zNear, zFar);
        }
        return glm::perspective(fovYRadians, aspect, zNear, zFar);
    }

    Matrix4 RHIClipSpace::MakeOrthographic(
        float left,
        float right,
        float bottom,
        float top,
        float zNear,
        float zFar)
    {
        if (GetClipSpaceCapabilities().ClipDepthRange == RHIClipDepthRange::ZeroToOne)
        {
            return glm::orthoRH_ZO(left, right, bottom, top, zNear, zFar);
        }
        return glm::ortho(left, right, bottom, top, zNear, zFar);
    }

} // namespace minEngine
