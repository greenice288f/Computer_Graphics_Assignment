#version 330 core

in vec3 color;
in vec2 uv; 
uniform sampler2D textureSampler; 

// TODO: To add UV input to this fragment shader 

// TODO: To add the texture sampler

out vec3 finalColor;

void main()
{
	//finalColor = color;
	finalColor = texture(textureSampler, uv).rgb; 

	// TODO: texture lookup. 
}
