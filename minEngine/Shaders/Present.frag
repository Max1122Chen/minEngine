#version 420 core

layout (binding = 0) uniform sampler2D u_SceneColor;

in vec3 FragPos;
in vec2 TexCoord;

out vec4 FragColor;

void main()
{
    FragColor = texture(u_SceneColor, TexCoord);
}
