#version 420 core

in vec3 WorldPos;

layout (std140, binding = 11) uniform ShadowPassParams
{
    int u_UseLinearDepth;
    float _pad0;
    float _pad1;
    float _pad2;
    vec3 u_LightPos;
    float u_FarPlane;
};

void main()
{
    if (u_UseLinearDepth == 1)
    {
        float lightDistance = length(WorldPos - u_LightPos);
        gl_FragDepth = lightDistance / u_FarPlane;
    }
    else
    {
        gl_FragDepth = gl_FragCoord.z;
    }
}
