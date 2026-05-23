#version 330 core
layout (location = 0) in vec3 a_Position;

out vec3 v_TexCoords;

uniform mat4 u_Projection;
uniform mat4 u_View;

void main()
{
    v_TexCoords = a_Position;
    mat4 view = u_View;
    view[3] = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 position = u_Projection * view * vec4(a_Position, 1.0);
    gl_Position = position.xyww;
}
