#version 330 core

layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;

uniform sampler2D g_albedoTex;
uniform sampler2D g_normalTex;
uniform sampler2D g_roughnessTex;

in vec3 WorldPos;
in vec2 TexCoords;
in mat3 TBN;

void main()
{
	gPosition = WorldPos;
	vec2 uv = TexCoords * 8.0;
    vec3 normalTS = texture(g_normalTex, uv).xyz * 2.0 - 1.0;
	vec3 normal = TBN * normalTS;
	gNormal = normalize(normal);
    vec3 albedo = texture(g_albedoTex, uv).rgb;
	float roughness = texture(g_roughnessTex, uv).a;
	gAlbedoSpec = vec4(albedo, roughness);
}
