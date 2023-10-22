#version 330 core

out vec4 color;

vec3 lightPos = vec3(0.0, 20.0, -20.0);

in vec3 WorldPos;
in vec3 Normal;

void main()
{
	vec3 l = normalize(lightPos - WorldPos);
	vec3 n = normalize(Normal);
	float NdotL = dot(n, l);
	color = vec4(0.0, 1.0, 1.0, 1.0);
	color.rgb *= NdotL;
}
