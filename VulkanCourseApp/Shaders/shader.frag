#version 450

layout(location = 0) in vec3 fragColor;			// Interpolated color from vertex shader (location must match)
// These two have no connection 
layout(location = 0) out vec4 outColor;			// Final output color (must also have location, defines the attachment to output to)

void main() {
	outColor = vec4(fragColor, 1.0);
}