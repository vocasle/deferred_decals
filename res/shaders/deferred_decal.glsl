#version 330 core

layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;

uniform sampler2D g_depth;
uniform mat4 g_invViewProj;
uniform mat4 g_decalInvWorld;

in vec3 WorldPos;
in vec2 TexCoords;
in mat3 TBN;
in vec4 GlPosition;

vec3 world_position_from_depth(vec2 screen_pos, float ndc_depth)
{
    // Remap depth to [-1.0, 1.0] range.
    float depth = ndc_depth * 2.0 - 1.0;

    // // Create NDC position.
    vec4 ndc_pos = vec4(screen_pos, depth, 1.0);

    // Transform back into world position.
    vec4 world_pos = g_invViewProj * ndc_pos;

    // Undo projection.
    world_pos = world_pos / world_pos.w;

    return world_pos.xyz;
}

void main()
{
	vec2 uv = TexCoords;
    vec2 screen_pos = GlPosition.xy / GlPosition.w;
    vec2 tex_coords = screen_pos * 0.5 + 0.5;

    float depth     = texture(g_depth, tex_coords).x;
    vec3  worldPos = world_position_from_depth(screen_pos, depth);
	vec3 localPos = (g_decalInvWorld * vec4(worldPos, 1.0)).xyz;
	vec3 ret = 0.5 - abs(localPos);
	if (ret.x < 0 || ret.y < 0 || ret.z < 0) {
		discard;
	}

	vec3 albedo = vec3(0.0, 1.0, 1.0);
	float roughness = 1.0;
	gAlbedoSpec = vec4(albedo, roughness);

}
