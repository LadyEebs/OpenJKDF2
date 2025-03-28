// this pass helps offset vgpr pressure from the main pass
// but the bandwidth overhead tends to make this slower overall

import "defines.gli"
import "uniforms.gli"
import "clustering.gli"
import "math.gli"
import "lighting.gli"
import "decals.gli"
import "occluders.gli"
import "reg.gli"

uniform sampler2D tex;
uniform sampler2D tex2;
uniform sampler2D tex3;
uniform sampler2D tex4;
uniform float param1;
uniform float param2;
uniform float param3;

//in vec2 f_uv;

out vec4 fragOut;

// interleaved chroma
vec2 encode_result(vec3 color)
{
	vec3 YCoCg = rgb2ycocg_unorm(color.rgb);
	ivec2 crd = ivec2(gl_FragCoord.xy);
	return ((crd.x & 1) == (crd.y & 1)) ? YCoCg.rg : YCoCg.rb;
}

void main(void)
{
	float depth = texelFetch(tex, ivec2(gl_FragCoord.xy), 0).x;
	fragOut = vec4(linearize_depth(depth)) / 128.0;

#if 0
	vec4 normalRoughness = texelFetch(tex2, ivec2(gl_FragCoord.xy), 0);
	vec3 normal = normalize(normalRoughness.xyz * 2.0 - 1.0);
	float roughness = normalRoughness.w;

	vec2 uv = gl_FragCoord.xy / iResolution.xy;
	float z = depth * 2.0 - 1.0;
    vec4 clipPos = vec4(uv.xy * 2.0 - 1.0, z, 1.0);
    vec4 viewPos = inverse(projMatrix) * clipPos;
    viewPos /= viewPos.w;

	vec3 view = normalize(-viewPos.xyz);
	vec3 reflected = reflect(-view, normal);

	light_result result;
	result.diffuse = packF2x11_1x10(vec3(0.0));
	result.specular = packF2x11_1x10(vec3(0.0));

	float shadow = 1.0;

	uint cluster = compute_cluster_index(gl_FragCoord.xy, viewPos.y);

	light_input params;
	params.pos       = packVPOS(vec4(viewPos.xyz, 0.0));
	params.normal    = encode_octahedron_uint(normal.xyz);
	params.view      = encode_octahedron_uint(view);
	params.reflected = encode_octahedron_uint(reflected.xyz);
	params.spec_c    = calc_spec_c(roughness);
	//params.tint    = f_color[0].rgb;
	params.tint      = 0xFFFFFFFF;

	// loop lights
	if (numLights > 0u)
	{
		uint first_item = firstLight;
		uint last_item = first_item + numLights - 1u;
		uint first_bucket = first_item >> 5u;
		uint last_bucket = min(last_item >> 5u, max(0u, CLUSTER_BUCKETS_PER_CLUSTER - 1u));
		for (uint bucket = first_bucket; bucket <= last_bucket; ++bucket)
		{
			uint bucket_bits = uint(texelFetch(clusterBuffer, int(cluster * CLUSTER_BUCKETS_PER_CLUSTER + bucket)).x);
		#if defined(GL_KHR_shader_subgroup_ballot) && defined(GL_KHR_shader_subgroup_arithmetic)
			bucket_bits = subgroupBroadcastFirst(subgroupOr(bucket_bits));
		#endif
			while(bucket_bits != 0u)
			{
				uint bucket_bit_index = findLSB(bucket_bits);
				uint light_index = (bucket << 5u) + bucket_bit_index;
				bucket_bits ^= (1u << bucket_bit_index);

				if (light_index >= first_item && light_index <= last_item)
				{
					calc_point_light(result, light_index, params);
				}
				else if (light_index > last_item)
				{
					bucket = CLUSTER_BUCKETS_PER_CLUSTER;
					break;
				}
			}
		}
	}

	// loop shadow occluders
	if (numOccluders > 0u)
	{
		uint first_item = firstOccluder;
		uint last_item = first_item + numOccluders - 1u;
		uint first_bucket = first_item >> 5u;
		uint last_bucket = min(last_item >> 5u, max(0u, CLUSTER_BUCKETS_PER_CLUSTER - 1u));
		for (uint bucket = first_bucket; bucket <= last_bucket; ++bucket)
		{
			uint bucket_bits = uint(texelFetch(clusterBuffer, int(cluster * CLUSTER_BUCKETS_PER_CLUSTER + bucket)).x);
		#if defined(GL_KHR_shader_subgroup_ballot) && defined(GL_KHR_shader_subgroup_arithmetic)
			bucket_bits = subgroupBroadcastFirst(subgroupOr(bucket_bits));
		#endif
			while (bucket_bits != 0u)
			{
				uint bucket_bit_index = uint(findLSB(bucket_bits));
				uint occluder_index = (bucket << 5u) + bucket_bit_index;
				bucket_bits ^= (1u << bucket_bit_index);
			
				if (occluder_index >= first_item && occluder_index <= last_item)
				{
					calc_shadow(shadow, occluder_index - first_item, params);
				}
				else if (occluder_index > last_item)
				{
					bucket = CLUSTER_BUCKETS_PER_CLUSTER;
					break;
				}
			}
		}
			
		result.diffuse = packF2x11_1x10(unpackF2x11_1x10(result.diffuse) * shadow);
		result.specular = packF2x11_1x10(unpackF2x11_1x10(result.specular) * shadow);
	}

	// we kinda throw out some color info here so we could probably do all the math in ycocg
	fragOut = vec4(encode_result(unpackF2x11_1x10(result.diffuse)), encode_result(unpackF2x11_1x10(result.specular)));

	//fragOut.x = pack_argb2101010(vec4(result.diffuse.rgb, 1.0));
	//fragOut.y = pack_argb2101010(vec4(result.specular.rgb, 1.0));

	// do decals
/*	if(numDecals > 0u)
	{
		vec4 color, glow;
		color = glow = vec4(0.0);

		uint first_item = firstDecal;
		uint last_item = first_item + numDecals - 1u;
		uint first_bucket = first_item >> 5u;
		uint last_bucket = min(last_item >> 5u, max(0u, CLUSTER_BUCKETS_PER_CLUSTER - 1u));
		for (uint bucket = first_bucket; bucket <= last_bucket; ++bucket)
		{
			uint bucket_bits = uint(texelFetch(clusterBuffer, int(cluster * CLUSTER_BUCKETS_PER_CLUSTER + bucket)).x);
		#if defined(GL_KHR_shader_subgroup_ballot) && defined(GL_KHR_shader_subgroup_arithmetic)
			bucket_bits = subgroupBroadcastFirst(subgroupOr(bucket_bits));
		#endif
			while(bucket_bits != 0u)
			{
				uint bucket_bit_index = findLSB(bucket_bits);
				uint decal_index = (bucket << 5u) + bucket_bit_index;
				bucket_bits ^= (1u << bucket_bit_index);
			
				if (decal_index >= first_item && decal_index <= last_item)
				{
					calc_decal(decal_index - first_item);
				}
				else if (decal_index > last_item)
				{
					bucket = CLUSTER_BUCKETS_PER_CLUSTER;
					break;
				}
			}
		}
	}*/
#endif
}
