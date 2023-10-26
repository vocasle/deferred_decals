#version 330 core

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNorm;
layout (location = 2) in vec2 inTexCoords;

uniform mat4 g_view;
uniform mat4 g_proj;
uniform mat4 g_world;

out vec3 WorldPos;
out vec3 Normal;
out vec2 TexCoords;

void main()
{
	gl_Position = g_proj * g_view * g_world * vec4(inPos, 1.0);
	WorldPos = (g_world * vec4(inPos, 1.0)).xyz;
	Normal = (g_world * vec4(inNorm, 0.0)).xyz;
	TexCoords = inTexCoords;
}
