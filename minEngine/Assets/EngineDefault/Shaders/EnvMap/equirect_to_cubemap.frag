#version 330 core
out vec4 FragColor;

in vec3 v_WorldPos;

uniform sampler2D u_EquirectangularMap;

const vec2 kInvAtan = vec2(0.1591, 0.3183);

vec2 SampleSphericalMap(vec3 direction)
{
    vec2 uv = vec2(atan(direction.z, direction.x), asin(direction.y));
    uv *= kInvAtan;
    uv += 0.5;
    return uv;
}

void main()
{
    vec2 uv = SampleSphericalMap(normalize(v_WorldPos));
    vec3 color = texture(u_EquirectangularMap, uv).rgb;
    FragColor = vec4(color, 1.0);
}
