#version 330 core
in vec2 fragUV;
out vec4 color;

uniform sampler2D textureSampler;

void main() {
    vec4 texColor = texture(textureSampler, fragUV);
    if (texColor.a < 0.1) discard; // Transparency
    color = texColor;
}
