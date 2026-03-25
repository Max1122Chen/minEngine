#version 330 core

struct Material
{
    // TODO: support simple value input
    sampler2D DiffuseMap;
    sampler2D SpecularMap;
    float Shininess;
};

struct DirectionalLightData
{
    vec4 Direction;
    vec4 Color;     // w for intensity
};

struct PointLightData
{
    vec4 Position;  // w for radius
    vec4 Color;     // w for intensity
};

struct SpotLightData
{
    vec4 Direction;
    vec4 Position;
    vec4 Color;      // w for intensity
    vec4 ConeAngles; // x=inner, y=outer
};

vec3 CalcDirLight(DirectionalLightData light, vec3 normal, vec3 viewDir);
vec3 CalcPointLight(PointLightData light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 CalcSpotLight(SpotLightData light, vec3 normal, vec3 fragPos, vec3 viewDir);


out vec4 FragColor;

// Vertex Attributes
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

layout (std140) uniform PerFrameData
{
    mat4 View;
    mat4 Proj;
    mat4 ViewProj;
    vec4 CameraPos;
};

layout (std140) uniform LightsData
{
    DirectionalLightData DirectionalLight;
    PointLightData PointLights[16];
    SpotLightData SpotLights[16];
    uint PointLightsCount;
    uint SpotLightsCount;
};

// Material info
uniform Material u_Material;

// View info
// uniform vec3 u_ViewPosition;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(CameraPos.xyz - FragPos);

    vec4 texColor = texture(u_Material.DiffuseMap, TexCoord);

    vec3 DirLightResult = CalcDirLight(DirectionalLight, norm, viewDir);

    vec3 PointLightResult = vec3(0.0);
    for(uint i = 0u; i < PointLightsCount && i < 16u; ++i)
    {
        PointLightResult += CalcPointLight(PointLights[i], norm, FragPos, viewDir);
    }

    vec3 SpotLightResult = vec3(0.0);
    for(uint i = 0u; i < SpotLightsCount && i < 16u; ++i)
    {
        SpotLightResult += CalcSpotLight(SpotLights[i], norm, FragPos, viewDir);
    }

    vec3 result = DirLightResult + PointLightResult + SpotLightResult;
    FragColor = vec4(result, texColor.a);
}

vec3 CalcDirLight(DirectionalLightData light, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(-light.Direction.xyz);
    vec3 lightColor = light.Color.rgb * light.Color.w;

    // Ambient shading
    float ambientStrength = 0.1;    // TODO: make it configurable
    vec3 ambient = ambientStrength * lightColor * vec3(texture(u_Material.DiffuseMap, TexCoord));

    // Diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // Specular shading
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    // Combine results
    vec3 diffuse = diff * lightColor * vec3(texture(u_Material.DiffuseMap, TexCoord));
    vec3 specular = spec * lightColor * vec3(texture(u_Material.SpecularMap, TexCoord));     // Note: using diffuse map for specular for simplicity

    return (ambient + diffuse + specular);
}

vec3 CalcPointLight(PointLightData light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.Position.xyz - fragPos);
    vec3 lightColor = light.Color.rgb * light.Color.w;

    // Ambient shading
    float ambientStrength = 0.1;    // TODO: make it configurable
    vec3 ambient = ambientStrength * lightColor * vec3(texture(u_Material.DiffuseMap, TexCoord));

    // Diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // Specular shading
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    // Combine results
    vec3 diffuse = diff * lightColor * vec3(texture(u_Material.DiffuseMap, TexCoord));
    vec3 specular = spec * lightColor * vec3(texture(u_Material.SpecularMap, TexCoord));    // Note: using diffuse map for specular for simplicity
    return (ambient + diffuse + specular);
}

vec3 CalcSpotLight(SpotLightData light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.Position.xyz - fragPos);
    vec3 lightColor = light.Color.rgb * light.Color.w;
    
    float theta = dot(lightDir, normalize(-light.Direction.xyz));
    
    float innerCos = cos(radians(light.ConeAngles.x));
    float outerCos = cos(radians(light.ConeAngles.y));
    
    float epsilon = innerCos - outerCos;
    float intensity = clamp((theta - outerCos) / epsilon, 0.0, 1.0);

    // Diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // Specular shading
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    // Combine results
    vec3 diffuse = diff * lightColor * vec3(texture(u_Material.DiffuseMap, TexCoord));
    vec3 specular = spec * lightColor * vec3(texture(u_Material.SpecularMap, TexCoord));    // Note: using diffuse map for specular for simplicity
    diffuse *= intensity;
    specular *= intensity;
    
    return (diffuse + specular);
}