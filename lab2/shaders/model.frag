#version 330 core

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;

uniform sampler2D textureSampler;

void main()
{
	vec3 lightDir = vec3(1.0f, 1.0f, 1.0f);
	float intensity = max(dot(normalize(Normal), normalize(lightDir)), 0.0f);

	vec3 diffuse = intensity * texture(textureSampler, TexCoord).rgb;
	vec3 ambient = 0.2f * texture(textureSampler, TexCoord).rgb;
	vec3 finalColor = ambient + diffuse;


    FragColor = vec4(finalColor, 1.0);
}