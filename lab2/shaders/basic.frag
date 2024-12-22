#version 330 core
in vec3 fragNormal;
uniform vec3 lightPosition;
uniform vec3 lightIntensity;
out vec4 FragColor;

void main()
{
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(lightPosition);

    float diff = max(dot(normal, lightDir), 0.0);
    
    vec3 diffuse = lightIntensity * diff;
    vec3 color = vec3(1.0, 1.0, 1.0); // Base white color
    FragColor = vec4(color * diffuse, 1.0);
}