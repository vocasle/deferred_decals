#version 330 core

out vec4 color;

in vec2 TexCoords;

uniform sampler2D g_position;
uniform sampler2D g_normal;
uniform sampler2D g_albedoSpec;

//struct Light {
//    vec3 Position;
//    vec3 Color;
//    
//    float Linear;
//    float Quadratic;
//};

//const int NR_LIGHTS = 32;
//uniform Light lights[NR_LIGHTS];

uniform vec3 g_cameraPos;
uniform vec3 g_lightPos;

void main()
{             
    // retrieve data from gbuffer
    vec3 WorldPos = texture(g_position, TexCoords).rgb;
    vec3 Normal = texture(g_normal, TexCoords).rgb;
    vec3 albedo = texture(g_albedoSpec, TexCoords).rgb;
    float Specular = texture(g_albedoSpec, TexCoords).a;

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

    
    // then calculate lighting as usual
//    vec3 lighting  = Diffuse * 0.1; // hard-coded ambient component
//    vec3 viewDir  = normalize(viewPos - FragPos);
//    for(int i = 0; i < NR_LIGHTS; ++i)
//    {
//        // diffuse
//        vec3 lightDir = normalize(lights[i].Position - FragPos);
//        vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * lights[i].Color;
//        // specular
//        vec3 halfwayDir = normalize(lightDir + viewDir);  
//        float spec = pow(max(dot(Normal, halfwayDir), 0.0), 16.0);
//        vec3 specular = lights[i].Color * spec * Specular;
//        // attenuation
//        float distance = length(lights[i].Position - FragPos);
//        float attenuation = 1.0 / (1.0 + lights[i].Linear * distance + lights[i].Quadratic * distance * distance);
//        diffuse *= attenuation;
//        specular *= attenuation;
//        lighting += diffuse + specular;        
//    }
//    FragColor = vec4(lighting, 1.0);
}
