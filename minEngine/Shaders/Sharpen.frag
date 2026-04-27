#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D u_SceneColor; 
uniform vec2 u_InvResolution;   // (1/width, 1/height)

// Parameters for edge protection
uniform float u_Strength;       
uniform float u_EdgeThreshold;  

void main()
{
    vec2 texel = u_InvResolution;


    vec3 center = texture(u_SceneColor, TexCoord).rgb;

    // 4 neighboring samples
    vec3 north = texture(u_SceneColor, TexCoord + vec2(0.0,  texel.y)).rgb;
    vec3 south = texture(u_SceneColor, TexCoord + vec2(0.0, -texel.y)).rgb;
    vec3 east  = texture(u_SceneColor, TexCoord + vec2( texel.x, 0.0)).rgb;
    vec3 west  = texture(u_SceneColor, TexCoord + vec2(-texel.x, 0.0)).rgb;

    // box blur
    vec3 blur = (north + south + east + west) * 0.25;

    // Calculate luminance for edge detection
    float lumaCenter = dot(center, vec3(0.299, 0.587, 0.114));
    float lumaBlur   = dot(blur,   vec3(0.299, 0.587, 0.114));

    float edge = abs(lumaCenter - lumaBlur);

    // Calculate sharpen amount based on edge strength
    float sharpenAmount = u_Strength * (1.0 - smoothstep(0.0, u_EdgeThreshold, edge));

    // Unsharp Mask
    vec3 result = center + (center - blur) * sharpenAmount;

    FragColor = vec4(result, 1.0);
}