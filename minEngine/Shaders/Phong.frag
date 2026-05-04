#version 330 core

#define MAX_SPOT_SHADOW_MAPS 2
#define MAX_POINT_SHADOW_MAPS 2

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

vec3 CalcDirLight(DirectionalLightData light, vec3 normal, vec3 viewDir);   // We only support one directional light for simplicity, so no params needed.
vec3 CalcPointLight(PointLightData light, vec3 normal, vec3 fragPos, vec3 viewDir, int lightIndex);
vec3 CalcSpotLight(SpotLightData light, vec3 normal, vec3 fragPos, vec3 viewDir, int lightIndex);
float SampleDirShadowPCF(vec4 fragPosLightSpace, float shadowLayer, float bias);
float SampleSpotShadowPCF(vec4 fragPosLightSpace, int shadowIndex, float bias);
float SamplePointShadow(vec3 fragPos, vec3 lightPos, float farPlane, int shadowIndex, float bias);
vec3 GetCascadeDebugColor(int cascadeIndex);

// Vertex Attributes
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 FragPosLightSpace;
in vec4 FragPosViewSpace;

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

layout (std140) uniform DirLightViewProjs
{
    mat4 DirLightViewProj[4];
};

layout (std140) uniform SpotLightViewProjs
{
    mat4 SpotLightViewProj[16];
};

layout (std140) uniform CascadeFarPlanes
{
    float FarPlanes[4];
};

// Material info
uniform Material u_Material;

// Shadow maps
uniform sampler2DArray u_DirLightShadowMap;
uniform sampler2D u_SpotShadowMaps[MAX_SPOT_SHADOW_MAPS];
uniform samplerCube u_PointShadowMaps[MAX_POINT_SHADOW_MAPS];

out vec4 FragColor;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(CameraPos.xyz - FragPos);

    vec4 texColor = texture(u_Material.DiffuseMap, TexCoord);

    vec3 DirLightResult = CalcDirLight(DirectionalLight, norm, viewDir);

    vec3 PointLightResult = vec3(0.0);
    for(uint i = 0u; i < PointLightsCount && i < 16u; ++i)
    {
        PointLightResult += CalcPointLight(PointLights[i], norm, FragPos, viewDir, int(i));
    }

    vec3 SpotLightResult = vec3(0.0);
    for(uint i = 0u; i < SpotLightsCount && i < 16u; ++i)
    {
        SpotLightResult += CalcSpotLight(SpotLights[i], norm, FragPos, viewDir, int(i));
    }

    vec3 result = DirLightResult + PointLightResult + SpotLightResult;
    FragColor = vec4(result, texColor.a);
}

vec3 CalcDirLight(DirectionalLightData light, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(-light.Direction.xyz);
    vec3 lightColor = light.Color.rgb * light.Color.w;

    // Slope-scaled bias: reduce acne on grazing angles while avoiding large global offset.
    float ndotl = max(dot(normalize(normal), lightDir), 0.0);
    float bias = max(0.0005, 0.005 * (1.0 - ndotl));

    float shadow = 0.0;
    
    // Determine which cascade to sample based on the fragment's view space depth
    float viewDepth = -FragPosViewSpace.z;
    int cascadeIndex = 3; // Default to the last cascade if beyond all far planes

    for(int i = 0; i < 4; i++)
    {
        if(viewDepth < FarPlanes[i])
        {
            cascadeIndex = i;
            break;
        }
    }
    // lightColor = GetCascadeDebugColor(cascadeIndex); // Debug: visualize cascade splits with colors
    vec4 cascadeLightSpacePos = DirLightViewProj[cascadeIndex] * vec4(FragPos, 1.0);
    shadow = SampleDirShadowPCF(cascadeLightSpacePos, cascadeIndex, bias);
    

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

float SampleDirShadowPCF(vec4 fragPosLightSpace, float shadowLayer, float bias)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5; // Transform from NDC to [0,1] range

    // Outside light frustum means no valid shadow contribution in this pass.
    if(projCoords.x < 0.0 || projCoords.x > 1.0 ||
       projCoords.y < 0.0 || projCoords.y > 1.0 ||
       projCoords.z < 0.0 || projCoords.z > 1.0)
    {
        return 0.0;
    }

    vec2 texelSize = 1.0 / vec2(textureSize(u_DirLightShadowMap, 0).xy);
    float currentDepth = projCoords.z;

    float shadow = 0.0;
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            vec2 sampleUV = clamp(projCoords.xy + offset, 0.0, 1.0);
            float sampledDepth = texture(u_DirLightShadowMap, vec3(sampleUV, shadowLayer)).r;
            shadow += (currentDepth - bias > sampledDepth) ? 1.0 : 0.0;
        }
    }

    return shadow / 9.0;
}

float SampleSpotShadowPCF(vec4 fragPosLightSpace, int shadowIndex, float bias)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if(projCoords.x < 0.0 || projCoords.x > 1.0 ||
       projCoords.y < 0.0 || projCoords.y > 1.0 ||
       projCoords.z < 0.0 || projCoords.z > 1.0)
    {
        return 0.0;
    }

    vec2 texelSize = 1.0 / vec2(textureSize(u_SpotShadowMaps[shadowIndex], 0));
    float currentDepth = projCoords.z;

    float shadow = 0.0;
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            vec2 sampleUV = clamp(projCoords.xy + offset, 0.0, 1.0);
            float sampledDepth = texture(u_SpotShadowMaps[shadowIndex], sampleUV).r;
            shadow += (currentDepth - bias > sampledDepth) ? 1.0 : 0.0;
        }
    }

    return shadow / 9.0;
}

float SamplePointShadow(vec3 fragPos, vec3 lightPos, float farPlane, int shadowIndex, float bias)
{
    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight) / farPlane;
    float sampledDepth = texture(u_PointShadowMaps[shadowIndex], fragToLight).r;
    return (currentDepth - bias > sampledDepth) ? 1.0 : 0.0;
}

vec3 CalcPointLight(PointLightData light, vec3 normal, vec3 fragPos, vec3 viewDir, int lightIndex)
{
    vec3 lightDir = normalize(light.Position.xyz - fragPos);
    vec3 lightColor = light.Color.rgb * light.Color.w;

    float shadow = 0.0;
    int shadowIndex = int(light.Params.w + 0.5);
    if (shadowIndex >= 0 && shadowIndex < MAX_POINT_SHADOW_MAPS)
    {
        float ndotl = max(dot(normalize(normal), lightDir), 0.0);
        float bias = max(0.002, 0.01 * (1.0 - ndotl));
        shadow = SamplePointShadow(fragPos, light.Position.xyz, light.Params.z, shadowIndex, bias);
    }

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
    return (ambient + (diffuse + specular) * (1.0 - shadow));
}

vec3 CalcSpotLight(SpotLightData light, vec3 normal, vec3 fragPos, vec3 viewDir, int lightIndex)
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

    float shadow = 0.0;
    int shadowIndex = int(light.Params.w + 0.5);
    if (shadowIndex >= 0 && shadowIndex < MAX_SPOT_SHADOW_MAPS)
    {
        float ndotl = max(dot(normalize(normal), lightDir), 0.0);
        float bias = max(0.0005, 0.005 * (1.0 - ndotl));
        vec4 fragPosLightSpace = SpotLightViewProj[lightIndex] * vec4(fragPos, 1.0);
        shadow = SampleSpotShadowPCF(fragPosLightSpace, shadowIndex, bias);
    }
    
    return (diffuse + specular) * (1.0 - shadow);
}

vec3 GetCascadeDebugColor(int cascadeIndex)
{
    if(cascadeIndex == 0) return vec3(1.0, 0.0, 0.0); // Red for cascade 0
    else if(cascadeIndex == 1) return vec3(0.0, 1.0, 0.0); // Green for cascade 1
    else if(cascadeIndex == 2) return vec3(0.0, 0.0, 1.0); // Blue for cascade 2
    else if(cascadeIndex == 3) return vec3(1.0, 1.0, 0.0); // Yellow for cascade 3
    else return vec3(1.0); // White for out of range
}