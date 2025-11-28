#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform sampler2D u_Texture1;
uniform sampler2D u_Texture2;
uniform vec3 u_LightColor;
uniform vec3 u_LigthPosition;
uniform vec3 u_ViewPosition;

void main()
{
    float ambientStrength = 0.1;
    float specularStrength = 0.5;
    float shininess = 32.0;

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(u_LigthPosition - FragPos);
    vec3 viewDir = normalize(u_ViewPosition - FragPos);

    vec3 ambient = ambientStrength * u_LightColor;


    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * u_LightColor;

    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * u_LightColor;

    vec3 result = (ambient + diffuse + specular) * mix(vec3(texture(u_Texture1, TexCoord)), vec3(texture(u_Texture2, TexCoord)), 0.5);

    FragColor = vec4(result, 1.0);
}