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

// interleaved chroma
vec2 encode_result(vec3 color)
{
	vec3 YCoCg = rgb2ycocg(color.rgb);
	ivec2 crd = ivec2(gl_FragCoord.xy);
	return ((crd.x & 1) == (crd.y & 1)) ? YCoCg.rg : YCoCg.rb;
}

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

	float shadow = 1.0;

	uint cluster = compute_cluster_index(gl_FragCoord.xy, viewPos.y);

	float a = roughness * roughness;

	light_input params;
	params.pos = viewPos.xyz;
	params.normal = encode_octahedron_uint(normal.xyz);
	params.view = encode_octahedron_uint(view);
	params.reflected = encode_octahedron_uint(reflected.xyz);

	params.roughness = roughness;
	params.roughness2 = roughness * roughness;
	params.normalizationTerm = roughness * 4.0 + 2.0;

	params.a2 = a * a;
	params.rcp_a2 = 1.0 / params.a2;
	params.spec_c = get_spec_c(params.rcp_a2);
	params.rcp_a2 /= 3.141592; // todo: roll pi into specular light intensity?
	//params.tint = f_color[0].rgb;
	params.tint = 0xFFFFFFFF;

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
			while(bucket_bits != 0u)
			{
				uint bucket_bit_index = findLSB_unsafe(bucket_bits);
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
			while (bucket_bits != 0u)
			{
				uint bucket_bit_index = uint(findLSB(bucket_bits));
				uint occluder_index = (bucket << 5u) + bucket_bit_index;
				bucket_bits ^= (1u << bucket_bit_index);
			
				if (occluder_index >= first_item && occluder_index <= last_item)
				{
					calc_shadow(shadow, occluder_index - first_item, viewPos.xyz, params.normal);
				}
				else if (occluder_index > last_item)
				{
					bucket = CLUSTER_BUCKETS_PER_CLUSTER;
					break;
				}
			}
		}
			
		vec3 ao = vec3(shadow * 0.8 + 0.2); // remap so we don't overdarken
		result.diffuse.xyz *= ao;

		float ndotv = dot(normal.xyz, view.xyz);
		vec3 specAO = mix(ao * ao, vec3(1.0), clamp(-0.3 * ndotv * ndotv, 0.0, 1.0));
		result.specular.xyz *= specAO;
	}

	result.specular.rgb = exp2(-result.specular.rgb);

	// we kinda throw out some color info here so we could probably do all the math in ycocg
	fragOut = vec4(encode_result(result.diffuse.rgb), encode_result(result.specular.rgb));

	//fragOut.x = pack_argb2101010(vec4(result.diffuse.rgb, 1.0));
	//fragOut.y = pack_argb2101010(vec4(result.specular.rgb, 1.0));

	// do decals

	if(numDecals > 0u)
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
			while(bucket_bits != 0u)
			{
				uint bucket_bit_index = findLSB_unsafe(bucket_bits);
				uint decal_index = (bucket << 5u) + bucket_bit_index;
				bucket_bits ^= (1u << bucket_bit_index);
			
				if (decal_index >= first_item && decal_index <= last_item)
				{
					decal dec = decals[decal_index - first_item];

					vec4 objectPosition = inverse(dec.decalMatrix) * vec4(viewPos.xyz, 1.0);				
					vec3 falloff = 0.5f - abs(objectPosition.xyz);
					if( any(lessThanEqual(falloff, vec3(0.0))) )
						continue;
				
					vec2 decalTexCoord = objectPosition.xz + 0.5;
					decalTexCoord = decalTexCoord.xy * dec.uvScaleBias.zw + dec.uvScaleBias.xy;
				
					vec4 decalColor = textureLod(decalAtlas, decalTexCoord, 0);
				
					bool isHeat = (dec.flags & 0x2u) == 0x2u;
					bool isAdditive = (dec.flags & 0x4u) == 0x4u;
					bool isRgbAlpha = (dec.flags & 0x8u) == 0x8u;
					if(isRgbAlpha)
						decalColor.a = max(decalColor.r, max(decalColor.g, decalColor.b));

					if(decalColor.a < 0.001)
						continue;
				
					decalColor.rgb *= dec.color.rgb;
				
					if(isHeat)
					{
						decalColor.rgb = blackbody(decalColor.r);
						glow.rgb += decalColor.rgb;
					}
				
					float edgeBlend = 1.0 - pow(clamp(abs(objectPosition.z), 0.0, 1.0), 8);
					if(isAdditive)
						color.rgb += edgeBlend * decalColor.rgb;
					else
						color = mix(color, vec4(decalColor.rgb, 1.0), decalColor.w * edgeBlend);
				}
				else if (decal_index > last_item)
				{
					bucket = CLUSTER_BUCKETS_PER_CLUSTER;
					break;
				}
			}
		}
	}
}
