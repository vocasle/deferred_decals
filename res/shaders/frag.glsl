#version 330 core

out vec4 color;

uniform vec3 g_lightPos; 
uniform vec3 g_cameraPos;
uniform bool g_wireframe;

in vec3 WorldPos;
in vec3 Normal;

void main()
{
	if (g_wireframe) {
		color = vec4(1.0);
	}
	else {
		vec3 ambient = vec3(0.1);
		
		vec3 lightColor = vec3(1.0);
		vec3 l = normalize(g_lightPos - WorldPos);
		vec3 n = normalize(Normal);
		float NdotL = max(dot(n, l), 0.0);
		vec3 diffuse = NdotL * lightColor;

		vec3 v = normalize(g_cameraPos - WorldPos);
		vec3 r = reflect(-l, n);
		float VdotR = max(dot(v, r), 0.0);
		float spec = pow(VdotR, 32.0);
		float specularStrength = 1.0;
		vec3 specular = specularStrength * spec * lightColor;

		color = vec4(1.0);
		color.rgb = ambient + diffuse + specular;
	}
}
