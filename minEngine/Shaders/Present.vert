#version 420 core

layout (location = 0) in vec2 a_Position;
layout (location = 1) in vec2 a_TexCoord;

// Keep name TexCoord: FXAA/Sharpen still link via GLSL by name against this vert.
layout (location = 0) out vec2 TexCoord;

void main()
{
    gl_Position = vec4(a_Position.x, a_Position.y, 0.0, 1.0);
    TexCoord = a_TexCoord;
}
