#version 420 core
layout (location = 0) in vec3 a_Position;

out vec3 v_TexCoords;

layout (std140, binding = 1) uniform SkyPassFrame
{
    mat4 u_Projection;
    mat4 u_View;
    float u_SkyIntensity;
    float _pad0;
    float _pad1;
    float _pad2;
};

void main()
{
    v_TexCoords = a_Position;
    mat4 view = u_View;
    view[3] = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 position = u_Projection * view * vec4(a_Position, 1.0);
    gl_Position = position.xyww;
}
