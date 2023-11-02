#version 330 core

layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;

//uniform sampler2D g_albedoTex;
//uniform sampler2D g_normalTex;
//uniform sampler2D g_roughnessTex;

//uniform int g_gbufferDebugMode;

//#define GDM_VERTEX_NORMAL 1
//#define GDM_TANGENT 2
//#define GDM_BITANGENT 3
//#define GDM_NORMAL_MAP 4
//#define GDM_ALBEDO 5
//#define GDM_POSITION 6

in vec3 WorldPos;
in vec2 TexCoords;
in mat3 TBN;

void main()
{
	gPosition = WorldPos;
	vec2 uv = TexCoords;
//    vec3 normalTS = texture(g_normalTex, uv).xyz * 2.0 - 1.0;
//	vec3 normal = TBN * normalTS;
	vec3 normal = TBN[2];
	gNormal = normalize(normal);
//    vec3 albedo = texture(g_albedoTex, uv).rgb;
//	float roughness = texture(g_roughnessTex, uv).a;
	vec3 albedo = vec3(0.0, 1.0, 0.0);
	float roughness = 1.0;
	gAlbedoSpec = vec4(albedo, roughness);


//	if (g_gbufferDebugMode == GDM_VERTEX_NORMAL) {
//		albedo = normalize(TBN[2]);
//	}
//	else if (g_gbufferDebugMode == GDM_TANGENT) {
//		albedo = normalize(TBN[0]);
//	}
//	else if (g_gbufferDebugMode == GDM_BITANGENT) {
//		albedo = normalize(TBN[1]);
//	}
//	else if (g_gbufferDebugMode == GDM_NORMAL_MAP) {
//		albedo = normalTS;
//	}
//	else if (g_gbufferDebugMode == GDM_POSITION) {
//		albedo = gPosition;
//	}

}