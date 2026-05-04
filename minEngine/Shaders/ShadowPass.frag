#version 330 core

in vec3 WorldPos;

uniform vec3 u_LightPos;
uniform float u_FarPlane;
uniform int u_UseLinearDepth;

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