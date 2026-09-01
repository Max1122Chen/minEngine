#version 420 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

layout(std140, binding = 0) uniform PerFrame
{
    mat4 View;
    mat4 Proj;
    mat4 ViewProj;
    vec4 CameraPos;
};

layout(location = 0) out vec4 v_Color;

void main()
{
    vec3 worldPos = a_Position;
    vec3 toCamera = CameraPos.xyz - worldPos;
    float distSq = dot(toCamera, toCamera);
    if (distSq > 1e-8)
    {
        // Small view-space bias reduces z-fighting when wireframes coincide with mesh surfaces.
        worldPos += toCamera * inversesqrt(distSq) * 0.003;
    }

    gl_Position = ViewProj * vec4(worldPos, 1.0);
    v_Color = a_Color;
}
