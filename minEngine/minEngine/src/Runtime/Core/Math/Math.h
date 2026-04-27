#pragma once
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

namespace minEngine::Math
{
    typedef glm::vec2 Vector2;
    typedef glm::vec3 Vector3;
    typedef glm::vec4 Vector4;
    typedef glm::mat3 Matrix3;
    typedef glm::mat4 Matrix4;


    inline float abs(float value) { return std::abs(value); }
    inline float sqr(float value) { return value * value; }
    inline float sqrt(float value) { return std::sqrt(value); }
    inline float sin(float angle) { return std::sin(angle); }  
    inline float cos(float angle) { return std::cos(angle); }
    inline float tan(float angle) { return std::tan(angle); }

    inline float radians(float degrees) { return glm::radians(degrees); }

}

using namespace minEngine::Math;