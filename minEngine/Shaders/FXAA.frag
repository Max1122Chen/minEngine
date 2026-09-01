#version 420 core

layout(location = 0) in vec2 TexCoord;
layout(location = 0) out vec4 FragColor;

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

    vec3 rgbNW = texture(u_SceneColor, TexCoord + vec2(-1.0, -1.0) * texel).rgb;
    vec3 rgbNE = texture(u_SceneColor, TexCoord + vec2( 1.0, -1.0) * texel).rgb;
    vec3 rgbSW = texture(u_SceneColor, TexCoord + vec2(-1.0,  1.0) * texel).rgb;
    vec3 rgbSE = texture(u_SceneColor, TexCoord + vec2( 1.0,  1.0) * texel).rgb;
    vec3 rgbM  = texture(u_SceneColor, TexCoord).rgb;

    vec3 lumaWeights = vec3(0.299, 0.587, 0.114);

    float lumaNW = dot(rgbNW, lumaWeights);
    float lumaNE = dot(rgbNE, lumaWeights);
    float lumaSW = dot(rgbSW, lumaWeights);
    float lumaSE = dot(rgbSE, lumaWeights);
    float lumaM  = dot(rgbM,  lumaWeights);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max(
        (lumaNW + lumaNE + lumaSW + lumaSE) *
        (0.25 * u_ReduceMul),
        u_ReduceMin
    );

    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = clamp(dir * rcpDirMin, vec2(-u_SpanMax), vec2(u_SpanMax)) * u_InvResolution;

    vec3 rgbA = 0.5 * (
        texture(u_SceneColor, TexCoord + dir * (1.0/3.0 - 0.5)).rgb +
        texture(u_SceneColor, TexCoord + dir * (2.0/3.0 - 0.5)).rgb
    );

    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(u_SceneColor, TexCoord + dir * -0.5).rgb +
        texture(u_SceneColor, TexCoord + dir *  0.5).rgb
    );

    float lumaB = dot(rgbB, lumaWeights);

    if ((lumaB < lumaMin) || (lumaB > lumaMax))
    {
        FragColor = vec4(rgbA, 1.0);
    }
    else
    {
        FragColor = vec4(rgbB, 1.0);
    }
}
