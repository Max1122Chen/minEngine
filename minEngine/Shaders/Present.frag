#version 330 core

uniform sampler2D u_SceneColor;

// Vertex Attributes
in vec3 FragPos;
in vec2 TexCoord;

out vec4 FragColor;

void main()
{
    FragColor = texture(u_SceneColor, TexCoord);
}

