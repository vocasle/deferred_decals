#version 330 core

layout (location = 0) in vec3 inPos;

uniform mat4 g_view;
uniform mat4 g_proj;
uniform mat4 g_world;

void main()
{
	gl_Position = g_proj * g_view * g_world * vec4(inPos, 1.0);
}
