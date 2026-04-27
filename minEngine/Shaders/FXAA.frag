#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D u_SceneColor; // bound to unit 0
uniform vec2 u_InvResolution;   // (1/width, 1/height)

// FXAA parameters 
uniform float u_ReduceMin;      // default: 1.0/128.0
uniform float u_ReduceMul;      // default: 1.0/8.0
uniform float u_SpanMax;        // default: 8.0

void main()
{
    vec3 rgbNW = texture(u_SceneColor, TexCoord + vec2(-1.0, -1.0) * u_InvResolution).rgb;
    vec3 rgbNE = texture(u_SceneColor, TexCoord + vec2( 1.0, -1.0) * u_InvResolution).rgb;
    vec3 rgbSW = texture(u_SceneColor, TexCoord + vec2(-1.0,  1.0) * u_InvResolution).rgb;
    vec3 rgbSE = texture(u_SceneColor, TexCoord + vec2( 1.0,  1.0) * u_InvResolution).rgb;
    vec3 rgbM  = texture(u_SceneColor, TexCoord).rgb;

    // To luma
    vec3 lumaWeights = vec3(0.299, 0.587, 0.114);

    float lumaNW = dot(rgbNW, lumaWeights);
    float lumaNE = dot(rgbNE, lumaWeights);
    float lumaSW = dot(rgbSW, lumaWeights);
    float lumaSE = dot(rgbSE, lumaWeights);
    float lumaM  = dot(rgbM,  lumaWeights);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    // Edge detection
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

    // Sample along the edge
    vec3 rgbA = 0.5 * (
        texture(u_SceneColor, TexCoord + dir * (1.0/3.0 - 0.5)).rgb +
        texture(u_SceneColor, TexCoord + dir * (2.0/3.0 - 0.5)).rgb
    );

    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(u_SceneColor, TexCoord + dir * -0.5).rgb +
        texture(u_SceneColor, TexCoord + dir *  0.5).rgb
    );

    float lumaB = dot(rgbB, lumaWeights);

    // Choose the final color
    if ((lumaB < lumaMin) || (lumaB > lumaMax))
    {
        FragColor = vec4(rgbA, 1.0);
    }
    else
    {
        FragColor = vec4(rgbB, 1.0);
    }
}