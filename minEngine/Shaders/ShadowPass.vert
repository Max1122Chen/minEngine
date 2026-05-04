#version 330 core
layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec2 a_TexCoord;

layout (std140) uniform LightViewProj
{
    mat4 ViewProj;
};
uniform mat4 u_Model;

out vec3 WorldPos;

void main()
{
    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    WorldPos = worldPos.xyz;
    gl_Position = ViewProj * worldPos;
}