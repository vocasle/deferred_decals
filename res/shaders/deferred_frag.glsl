#version 330 core

out vec4 color;

in vec2 TexCoords;

uniform sampler2D g_position;
uniform sampler2D g_normal;
uniform sampler2D g_albedo;

uniform vec3 g_cameraPos;
uniform vec3 g_lightPos;
uniform int g_gbufferDebugMode;

void main()
{             
    vec4 albedo = texture(g_albedo, TexCoords).rgba;
	if (g_gbufferDebugMode != 0) {
		color = vec4(albedo.rgb, 1.0);
	}
	else {
		// retrieve data from gbuffer
		vec3 WorldPos = texture(g_position, TexCoords).rgb;
		vec3 Normal = texture(g_normal, TexCoords).rgb;
		float Specular = albedo.a;

		vec3 ambient = vec3(0.1);
		
		vec3 lightColor = vec3(1.0);
		vec3 l = normalize(g_lightPos - WorldPos);
		vec3 n = normalize(Normal);
		float NdotL = max(dot(n, l), 0.0);
		vec3 diffuse = NdotL * lightColor * albedo.rgb;

		vec3 v = normalize(g_cameraPos - WorldPos);
		vec3 h = normalize(l + v);
		float VdotR = max(dot(n, h), 0.0);
		float spec = pow(VdotR, 16.0);
		vec3 specular = Specular * spec * lightColor;

		color = vec4(1.0);
		color.rgb = ambient + diffuse + specular;
	}
}
