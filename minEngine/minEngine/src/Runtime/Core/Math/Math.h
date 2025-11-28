#pragma once
#include "glm/glm.hpp"

namespace minEngine
{

    typedef glm::vec2 Vector2;
    typedef glm::vec3 Vector3;
    typedef glm::vec4 Vector4;
    typedef glm::mat3 Matrix3;
    typedef glm::mat4 Matrix4;



    class Math
    {
        static float abs(float value) { return std::abs(value); }
        static float sqr(float value) { return value * value; }
        static float sqrt(float value) { return std::sqrt(value); }
        static float sin(float angle) { return std::sin(angle); }  
        static float cos(float angle) { return std::cos(angle); }
        static float tan(float angle) { return std::tan(angle); }

        
    };
}