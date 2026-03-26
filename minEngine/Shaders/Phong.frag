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
    vec4 Params;    // w for shadow map index
};

struct PointLightData
{
    vec4 Position;  // w for radius
    vec4 Color;     // w for intensity
    vec4 Params;    // w for shadow map index
};

struct SpotLightData
{
    vec4 Direction;
    vec4 Position;
    vec4 Color;      // w for intensity
    vec4 Params;     // x=inner, y=outer, w=shadow map index
};

vec3 CalcDirLight(DirectionalLightData light, vec3 normal, vec3 viewDir, vec4 fragPosLightSpace);
vec3 CalcPointLight(PointLightData light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 CalcSpotLight(SpotLightData light, vec3 normal, vec3 fragPos, vec3 viewDir);

// Vertex Attributes
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 FragPosLightSpace;

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

// Shadow maps
uniform sampler2D u_DirLightShadowMap;

out vec4 FragColor;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(CameraPos.xyz - FragPos);

    vec4 texColor = texture(u_Material.DiffuseMap, TexCoord);

    vec3 DirLightResult = CalcDirLight(DirectionalLight, norm, viewDir, FragPosLightSpace);

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

vec3 CalcDirLight(DirectionalLightData light, vec3 normal, vec3 viewDir, vec4 fragPosLightSpace)
{
    float shadow = 0.0;
    if(light.Params.w >= 0.0)
    {
        vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
        projCoords = projCoords * 0.5 + 0.5; // Transform from NDC to [0,1] range

        // Only sample shadow map when the fragment is inside light frustum.
        if(projCoords.x >= 0.0 && projCoords.x <= 1.0 &&
           projCoords.y >= 0.0 && projCoords.y <= 1.0 &&
           projCoords.z >= 0.0 && projCoords.z <= 1.0)
        {
            float closestDepth = texture(u_DirLightShadowMap, projCoords.xy).r; // Depth from shadow map
            float currentDepth = projCoords.z; // Depth of current fragment from light's perspective
            shadow = currentDepth - 0.005 > closestDepth ? 1.0 : 0.0; // Simple shadow factor with bias
        }
    }

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

    return (ambient + (diffuse + specular) * (1.0 - shadow));
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
    
    float innerCos = cos(radians(light.Params.x));
    float outerCos = cos(radians(light.Params.y));
    
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