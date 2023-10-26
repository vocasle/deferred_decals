#version 330 core

layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;

uniform sampler2D g_albedoTex;

in vec3 WorldPos;
in vec3 Normal;
in vec2 TexCoords;

void main()
{
	gPosition = WorldPos;
	gNormal = normalize(Normal);
    vec3 albedo = texture(g_albedoTex, TexCoords).rgb;
	gAlbedoSpec = vec4(albedo, 0.0); 
}
