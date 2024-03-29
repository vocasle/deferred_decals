#version 330 core

layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;

uniform mat4 g_invViewProj;
uniform mat4 g_decalInvWorld;
uniform vec4 g_rtSize;
uniform mat4 g_world;

uniform sampler2D g_depth;
uniform sampler2D g_albedo;
uniform sampler2D g_normal;
uniform sampler2D g_gbufferNormal;

in vec3 WorldPos;
in vec2 TexCoords;
in vec4 ClipPos;

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
    vec3 gbufferNormal = texture(g_gbufferNormal, uv).xyz;
	vec3 T = vec3(1.0, 0.0, 0.0);
    vec3 B = vec3(0.0, 0.0, 1.0);
    vec3 N = vec3(0.0, 1.0, 0.0);

    vec3 projectionDirectionWS = mat3(g_world) * N; 
    // discard pixels in case angle between axis of projection and
    // normal from GBuffer is greater than 0
    if (dot(projectionDirectionWS, normalize(gbufferNormal)) < 0.9f) {
        discard;
    }
	// discard pixels that lie outside of projection box
	if (abs(localPos).x > 1.0 || abs(localPos).z > 1.0 ||
        abs(localPos).y > 1.0) {
		discard;
	}

    mat3 localTBN = mat3(T, B, N);
    vec3 normalTS = texture(g_normal, decalUV).xyz * 2.0 - 1.0;
    vec3 normal = localTBN * normalTS;
    normal = mat3(g_world) * normalize(normal);
    vec3 albedo = texture(g_albedo, decalUV).rgb;
    float roughness = 1.0;
    gAlbedoSpec = vec4(albedo, roughness);
    gNormal = normalize(normal + gbufferNormal);
}
