#version 330 core
layout (location = 0) in vec3 a_Position;

layout (std140) uniform PerFrameData
{
    mat4 View;
    mat4 Proj;
    mat4 ViewProj;
    vec4 CameraPos;
};

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;


void main()
{
    gl_Position =  u_Projection * u_View * u_Model * vec4(a_Position, 1.0);
}