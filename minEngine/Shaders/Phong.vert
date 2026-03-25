#version 330 core
layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec2 a_TexCoord;
layout (location = 2) in vec3 a_Normal;

layout (std140) uniform PerFrameData
{
    mat4 View;
    mat4 Proj;
    mat4 ViewProj;
    vec4 CameraPos;
};


uniform mat4 u_Model;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

void main()
{
    gl_Position =  ViewProj * u_Model * vec4(a_Position, 1.0);
    FragPos = vec3(u_Model * vec4(a_Position, 1.0));
    Normal = mat3(transpose(inverse(u_Model))) * a_Normal;
    TexCoord = a_TexCoord;
}