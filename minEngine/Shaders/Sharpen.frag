#version 420 core

in vec2 TexCoord;
out vec4 FragColor;

layout (binding = 0) uniform sampler2D u_SceneColor;

layout (std140, binding = 1) uniform EnginePostParams
{
    vec2 u_InvResolution;
    float u_ReduceMin;
    float u_ReduceMul;
    float u_SpanMax;
    float u_Strength;
    float u_EdgeThreshold;
    vec2 _pad;
};

void main()
{
    vec2 texel = u_InvResolution;

    vec3 center = texture(u_SceneColor, TexCoord).rgb;

    vec3 north = texture(u_SceneColor, TexCoord + vec2(0.0,  texel.y)).rgb;
    vec3 south = texture(u_SceneColor, TexCoord + vec2(0.0, -texel.y)).rgb;
    vec3 east  = texture(u_SceneColor, TexCoord + vec2( texel.x, 0.0)).rgb;
    vec3 west  = texture(u_SceneColor, TexCoord + vec2(-texel.x, 0.0)).rgb;

    vec3 blur = (north + south + east + west) * 0.25;

    float lumaCenter = dot(center, vec3(0.299, 0.587, 0.114));
    float lumaBlur   = dot(blur,   vec3(0.299, 0.587, 0.114));

    float edge = abs(lumaCenter - lumaBlur);

    float sharpenAmount = u_Strength * (1.0 - smoothstep(0.0, u_EdgeThreshold, edge));

    vec3 result = center + (center - blur) * sharpenAmount;

    FragColor = vec4(result, 1.0);
}
