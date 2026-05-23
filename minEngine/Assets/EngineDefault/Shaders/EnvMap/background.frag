#version 330 core
out vec4 FragColor;

in vec3 v_TexCoords;

uniform samplerCube u_Skybox;
uniform float u_SkyIntensity;

void main()
{
    FragColor = vec4(texture(u_Skybox, v_TexCoords).rgb * u_SkyIntensity, 1.0);
}
