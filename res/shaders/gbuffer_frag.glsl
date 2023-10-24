#version 330 core

layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec; 

in vec3 WorldPos;
in vec3 Normal;

void main()
{
	gPosition = WorldPos;
	gNormal = normalize(Normal);
	gAlbedoSpec = vec4(1.0, 1.0, 0.0, 0.0); 
}
