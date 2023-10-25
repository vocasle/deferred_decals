#version 330 core

out vec4 color;

in vec2 TexCoords;

uniform sampler2D g_position;
uniform sampler2D g_normal;
uniform sampler2D g_albedo;

uniform vec3 g_cameraPos;
uniform vec3 g_lightPos;

void main()
{             
    // retrieve data from gbuffer
    vec3 WorldPos = texture(g_position, TexCoords).rgb;
    vec3 Normal = texture(g_normal, TexCoords).rgb;
    vec3 albedo = texture(g_albedo, TexCoords).rgb;
    float Specular = texture(g_albedo, TexCoords).a;

	vec3 ambient = vec3(0.1);
	
	vec3 lightColor = vec3(1.0);
	vec3 l = normalize(g_lightPos - WorldPos);
	vec3 n = normalize(Normal);
	float NdotL = max(dot(n, l), 0.0);
	vec3 diffuse = NdotL * lightColor * albedo;

	vec3 v = normalize(g_cameraPos - WorldPos);
	vec3 r = reflect(-l, n);
	float VdotR = max(dot(v, r), 0.0);
	float spec = pow(VdotR, 32.0);
	float specularStrength = 1.0;
	vec3 specular = specularStrength * spec * lightColor;

	color = vec4(1.0);
	color.rgb = ambient + diffuse + specular;
}
