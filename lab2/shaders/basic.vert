#version 330 core

layout (location = 0) in vec3 position;
layout (location = 2) in vec3 normal;

uniform mat4 MVP;
uniform mat4 model;

out vec3 fragNormal;
void main()
{
    gl_Position = MVP * vec4(position, 1.0);
	mat4 normalMatrix = transpose(inverse(model));
	fragNormal = vec3(normalMatrix * vec4(normal, 0.0));
}