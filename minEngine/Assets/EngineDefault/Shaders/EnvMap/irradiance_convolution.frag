#version 420 core
layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec3 v_WorldPos;

layout (binding = 0) uniform samplerCube u_EnvironmentMap;

const float kPi = 3.14159265359;

void main()
{
    vec3 normal = normalize(v_WorldPos);
    vec3 irradiance = vec3(0.0);

    vec3 up = abs(normal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    const float sampleDelta = 0.025;
    float sampleCount = 0.0;
    for (float phi = 0.0; phi < 2.0 * kPi; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * kPi; theta += sampleDelta)
        {
            vec3 tangentSample = vec3(
                sin(theta) * cos(phi),
                sin(theta) * sin(phi),
                cos(theta));
            vec3 sampleDir = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;
            irradiance += texture(u_EnvironmentMap, sampleDir).rgb * cos(theta) * sin(theta);
            sampleCount += 1.0;
        }
    }

    irradiance = kPi * irradiance * (1.0 / max(sampleCount, 1.0));
    FragColor = vec4(irradiance, 1.0);
}
