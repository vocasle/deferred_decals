#version 330 core

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNorm;
layout (location = 2) in vec2 inTexCoords;
layout (location = 3) in vec4 inTangent;

uniform mat4 g_view;
uniform mat4 g_proj;
uniform mat4 g_world;

out vec3 WorldPos;
out vec2 TexCoords;
out mat3 TBN;

void main()
{
	gl_Position = g_proj * g_view * g_world * vec4(inPos, 1.0);
	WorldPos = (g_world * vec4(inPos, 1.0)).xyz;
	vec3 N = normalize((g_world * vec4(inNorm, 0.0)).xyz);
	TexCoords = inTexCoords;
	vec3 T = normalize((g_world * vec4(inTangent.xyz, 0.0)).xyz);
	T = normalize(T - dot(T, N) * N);
	vec3 B = cross(N, T) * inTangent.w;
	TBN = mat3(T, B, N); 
}
