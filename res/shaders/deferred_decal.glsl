#version 330 core

layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;
layout (location = 3) out vec4 gDebugDepth;

uniform mat4 g_invViewProj;
uniform mat4 g_decalInvWorld;
uniform vec4 g_rtSize;
uniform vec3 g_bboxMin;
uniform vec3 g_bboxMax;

uniform sampler2D g_depth;
uniform sampler2D g_albedo;
uniform sampler2D g_normal;

in vec3 WorldPos;
in vec2 TexCoords;
in mat3 TBN;
in vec4 ClipPos;

bool InBBox(vec3 pos)
{
	return pos.x < g_bboxMax.x && pos.y < g_bboxMax.y && pos.z < g_bboxMax.z;	
}

vec3 WorldPosFromDepth(vec2 screenPos, float ndcDepth)
{
    // Remap depth to [-1.0, 1.0] range.
    float depth = ndcDepth * 2.0 - 1.0;

    // // Create NDC position.
    vec4 ndcPos = vec4(screenPos, depth, 1.0);

    // Transform back into world position.
    vec4 worldPos = g_invViewProj * ndcPos;

    // Undo projection.
    worldPos = worldPos / worldPos.w;

    return worldPos.xyz;
}

void main()
{
	vec2 screenPos = ClipPos.xy / ClipPos.w;
	vec2 uv = screenPos * 0.5 + 0.5;
    float depth = texture(g_depth, uv).x;
    vec3  worldPos = WorldPosFromDepth(screenPos, depth);
	vec3 localPos = (g_decalInvWorld * vec4(worldPos, 1.0)).xyz;
	vec2 decalUV = localPos.xz * 0.5 + 0.5;
	if (!InBBox(abs(localPos))) {
//		gAlbedoSpec = vec4(1.0, 0.0, 0.0, 1.0);
		discard;
	}
	else {
		vec3 albedo = texture(g_albedo, decalUV).rgb;
		float roughness = 1.0;
		gAlbedoSpec = vec4(albedo, roughness);
		gDebugDepth = vec4(worldPos, depth);
	}
}
