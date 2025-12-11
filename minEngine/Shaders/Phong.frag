#version 330 core

struct Material
{
    // TODO: support simple value input
    sampler2D DiffuseMap;
    sampler2D SpecularMap;
    float Shininess;
};

struct DirLight
{
    vec3 Direction;
    vec3 Color;
};

struct PointLight
{
    vec3 Position;
    vec3 Color;
};

struct SpotLight
{
    vec3 Position;
    vec3 Direction;
    vec3 Color;
    float InnerConeAngleCos;
    float OuterConeAngleCos;
};

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir);


out vec4 FragColor;

// Vertex Attributes
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

// Light infos
uniform int u_PointLightCount;
uniform int u_SpotLightCount;

uniform DirLight u_DirLight;
uniform PointLight u_PointLight;    // Only support one point light for now
uniform SpotLight u_SpotLight;      // Only support one spot light for now

// Material info
uniform Material u_Material;

// View info
uniform vec3 u_ViewPosition;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(u_ViewPosition - FragPos);

    vec3 DirLightResult = CalcDirLight(u_DirLight, norm, viewDir);

    // Only support one point light for now
    vec3 PointLightResult = CalcPointLight(u_PointLight, norm, FragPos, viewDir);

    // Only support one spot light for now
    vec3 SpotLightResult = CalcSpotLight(u_SpotLight, norm, FragPos, viewDir);

    vec3 result = DirLightResult + PointLightResult + SpotLightResult;
    FragColor = vec4(result, 1.0);
}

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(-light.Direction);

    // Ambient shading
    float ambientStrength = 0.1;    // TODO: make it configurable
    vec3 ambient = ambientStrength * light.Color * vec3(texture(u_Material.DiffuseMap, TexCoord));

    // Diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // Specular shading
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    // Combine results
    vec3 diffuse = diff * light.Color * vec3(texture(u_Material.DiffuseMap, TexCoord));
    vec3 specular = spec * light.Color * vec3(texture(u_Material.DiffuseMap, TexCoord));     // Note: using diffuse map for specular for simplicity

    return (ambient + diffuse + specular);
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.Position - fragPos);

    // Ambient shading
    float ambientStrength = 0.1;    // TODO: make it configurable
    vec3 ambient = ambientStrength * light.Color * vec3(texture(u_Material.DiffuseMap, TexCoord));

    // Diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // Specular shading
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    // Combine results
    vec3 diffuse = diff * light.Color * vec3(texture(u_Material.DiffuseMap, TexCoord));
    vec3 specular = spec * light.Color * vec3(texture(u_Material.DiffuseMap, TexCoord));    // Note: using diffuse map for specular for simplicity
    return (ambient + diffuse + specular);
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.Position - fragPos);
    
    float theta = dot(lightDir, normalize(-light.Direction));
    float epsilon = light.InnerConeAngleCos - light.OuterConeAngleCos;
    float intensity = clamp((theta - light.OuterConeAngleCos) / epsilon, 0.0, 1.0);

    // Diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // Specular shading
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    // Combine results
    vec3 diffuse = diff * light.Color * vec3(texture(u_Material.DiffuseMap, TexCoord));
    vec3 specular = spec * light.Color * vec3(texture(u_Material.DiffuseMap, TexCoord));    // Note: using diffuse map for specular for simplicity
    diffuse *= intensity;
    specular *= intensity;
    
    return (diffuse + specular);
}