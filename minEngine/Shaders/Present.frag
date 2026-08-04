#version 420 core

layout (binding = 0) uniform sampler2D u_SceneColor;

layout (location = 0) in vec2 TexCoord;

layout (location = 0) out vec4 FragColor;

void main()
{
    FragColor = texture(u_SceneColor, TexCoord);
}
