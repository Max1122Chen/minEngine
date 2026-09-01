#version 420 core
layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec3 v_TexCoords;

layout (binding = 0) uniform samplerCube u_Skybox;

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
    FragColor = vec4(texture(u_Skybox, v_TexCoords).rgb * u_SkyIntensity, 1.0);
}
