// this pass helps offset vgpr pressure from the main pass
// but the bandwidth overhead on older cards (a.k.a my rx480)
// makes going deferred slower

#define SPECULAR // force specular path

#include "defines.gli"
#include "uniforms.gli"
#include "clustering.gli"
#include "math.gli"
#include "lighting.gli"
#include "decals.gli"
#include "occluders.gli"

uniform sampler2D tex2;
uniform sampler2D tex3;
uniform sampler2D tex4;
uniform float param1;
uniform float param2;
uniform float param3;

in vec2 f_uv;

out vec4 fragOut;

void main(void)
{
	float depth = textureLod(tex, f_uv.xy, 0).x;
	float z = depth * 2.0 - 1.0;

	vec4 normalRoughness = textureLod(tex2, f_uv.xy, 0);
	vec3 normal = normalize(normalRoughness.xyz * 2.0 - 1.0);
	float roughness = normalRoughness.w;

    vec4 clipPos = vec4(f_uv.xy * 2.0 - 1.0, z, 1.0);
    vec4 viewPos = inverse(projMatrix) * clipPos;
    viewPos /= viewPos.w;

	vec3 view = normalize(-viewPos.xyz);
	vec3 reflected = reflect(-view, normal);

	light_result result;
	result.diffuse = vec3(0.0);
	result.specular = vec3(0.0);

	uint cluster = compute_cluster_index(gl_FragCoord.xy, viewPos.y);

	// loop light buckets
	uint first_item = firstLight;
	uint last_item = first_item + numLights - 1u;
	uint first_bucket = first_item >> 5u;
	uint last_bucket = min(last_item >> 5u, max(0u, CLUSTER_BUCKETS_PER_CLUSTER - 1u));
	for (uint bucket = first_bucket; bucket <= last_bucket; ++bucket)
	{
		uint bucket_bits = uint(texelFetch(clusterBuffer, int(cluster * CLUSTER_BUCKETS_PER_CLUSTER + bucket)).x);
		while(bucket_bits != 0u)
		{
			uint bucket_bit_index = findLSB_unsafe(bucket_bits);
			uint light_index = (bucket << 5u) + bucket_bit_index;
			bucket_bits ^= (1u << bucket_bit_index);

			if (light_index >= first_item && light_index <= last_item)
			{
				calc_point_light(result, light_index, roughness, viewPos.xyz, normal, reflected);
			}
			else if (light_index > last_item)
			{
				bucket = CLUSTER_BUCKETS_PER_CLUSTER;
				break;
			}
		}
	}

	fragOut = vec4(result.diffuse.rgb, luminance(result.specular.rgb));

	//fragOut.x = pack_argb2101010(vec4(result.diffuse.rgb, 1.0));
	//fragOut.y = pack_argb2101010(vec4(result.specular.rgb, 1.0));
}
