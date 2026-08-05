#version 420 core
layout (location = 0) in vec3 a_Position;

layout (location = 0) out vec3 v_WorldPos;

layout (std140, binding = 1) uniform EnvCaptureFrame
{
    mat4 u_Projection;
    mat4 u_View;
    float u_Roughness;
    float u_EnvironmentResolution;
    float _pad0;
    float _pad1;
};

void main()
{
    v_WorldPos = a_Position;
    gl_Position = u_Projection * u_View * vec4(a_Position, 1.0);
}
