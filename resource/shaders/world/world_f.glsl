#include "defines.gli"
#include "uniforms.gli"
#include "clustering.gli"
#include "math.gli"
#include "attr.gli"
#include "lighting.gli"
#include "decals.gli"
#include "occluders.gli"
#include "textures.gli"
#include "texgen.gli"
#include "framebuffer.gli"
#include "vm.gli"

float upsample_ssao(in vec2 texcoord, in float linearDepth)
{
	linearDepth *= 128.0;

	// 2x2 strided sampling, helps hide the dither pattern better
	vec4 values;
	values.x = texelFetch(ssaotex, ivec2(texcoord + vec2(0, 0)), 0).x;
	values.y = texelFetch(ssaotex, ivec2(texcoord + vec2(2, 0)), 0).x;
	values.z = texelFetch(ssaotex, ivec2(texcoord + vec2(0, 2)), 0).x;
	values.w = texelFetch(ssaotex, ivec2(texcoord + vec2(2, 2)), 0).x;
			
	// seem to get better results with a 1.5 offset here, too lazy to figure out why
	vec4 depths;
	depths.x = texelFetch(ztex, ivec2(texcoord + vec2(0.0, 0.0)), 0).x;
	depths.y = texelFetch(ztex, ivec2(texcoord + vec2(1.5, 0.0)), 0).x;
	depths.z = texelFetch(ztex, ivec2(texcoord + vec2(0.0, 1.5)), 0).x;
	depths.w = texelFetch(ztex, ivec2(texcoord + vec2(1.5, 1.5)), 0).x;

	// reject samples that have depth discontinuities
	vec4 diff = abs(depths / linearDepth - 1.0) * 32.0;
	vec4 weights = sat4(1.0 - diff);

	// total weight
	float totalWeight = weights.x + weights.y + weights.z + weights.w;

	// average when weight is bad
	if(totalWeight < 1e-4)
		return (values.x + values.y + values.z + values.w) * 0.25;

	return dot(weights / totalWeight, values);
}

uint get_cluster()
{
#ifdef FRAG_ATTR_FETCH
	float viewDist = fetch_vtx_pos().y;
#else
	float viewDist = unpackHalf4x16(vpos).y;
#endif
	return compute_cluster_index(gl_FragCoord.xy, viewDist) * CLUSTER_BUCKETS_PER_CLUSTER;
}

// packs light results into color0 and color1
void calc_light()
{
	v[0] = 0;
	v[1] = 0;

#ifdef FRAG_ATTR_FETCH
	vec3 viewPos   = fetch_vtx_pos();
	vec3 view      = normalize(-viewPos.xyz);
#else
	vec3 normal    = normalizeNear1(f_normal.xyz);
	vec3 viewPos   = unpackHalf4x16(vpos).xyz;
	vec3 view      = normalize(-viewPos.xyz);
	vec3 reflected = reflect(-view, normal);
#endif
	
	float fog = 0;//fetch_vtx_color(1).w;
#ifdef FOG
	if(fogEnabled > 0)
	{
		float clipDepth = 0;//texelFetch(cliptex, ivec2(gl_FragCoord.xy), 0).r;
	
		float distToCam = max(0.0, viewPos.y - clipDepth) * fastRcpNR1(-view.y);
		fog = sat1((distToCam - fogStart) * fastRcpNR1(fogEnd - fogStart));
	}
#endif

	float roughness = (roughnessFactor);
	
#if defined(UNLIT) && !defined(FRAG_ATTR_FETCH)

	v[0] = pack_vertex_reg(fetch_vtx_color(0));
	v[1] = pack_vertex_reg(vec4(fetch_vtx_color(1).rgb, fog));

#else // UNLIT

	v[0] = pack_vertex_reg(vec4(0.0, 0.0, 0.0, fetch_vtx_color(0).w));
	v[1] = pack_vertex_reg(vec4(0.0, 0.0, 0.0, fog));

	light_result result;
	result.diffuse  = packF2x11_1x10(fetch_vtx_color(0).rgb);
	result.specular = packF2x11_1x10(fetch_vtx_color(1).rgb);

	light_input params;
#ifndef FRAG_ATTR_FETCH
	params.pos       = vpos;
	params.normal    = encode_octahedron_uint(normal.xyz);
	params.view      = encode_octahedron_uint(view);
	params.reflected = encode_octahedron_uint(reflected.xyz);
#endif
	params.spec_c    = calc_spec_c(roughness);
	params.tint      = packUnorm4x8(fetch_vtx_color(0));

#ifndef VERTEX_LIT
	uint cluster = get_cluster();

	// light mode "gouraud" (a.k.a new per pixel)
	{
		// loop lights
		//if (numLights > 0u)
		{
			uint s_first_item   = firstLight;
			uint s_last_item    = s_first_item + numLights - 1u;
			uint s_first_bucket = s_first_item >> 5u;
			uint s_last_bucket  = min(s_last_item >> 5u, max(0u, CLUSTER_BUCKETS_PER_CLUSTER - 1u));
			for (uint s_bucket  = s_first_bucket; s_bucket <= s_last_bucket; ++s_bucket)
			{
				uint bucket_bits = uint(texelFetch(clusterBuffer, int(cluster + s_bucket)).x);
				while (bucket_bits != 0u)
				{
					uint bucket_bit_index = findLSB_unsafe(bucket_bits);
					uint light_index = (s_bucket << 5u) + bucket_bit_index;
					bucket_bits ^= (1u << bucket_bit_index);

					// eebs: unfortunately I don't have this extension but
					// in theory scalarizing this should speed things up
				#if defined(GL_ARB_shader_ballot) && defined(GL_KHR_shader_subgroup)
					//bucket_bits = readFirstInvocationARB(subgroupOr(bucket_bits));
				#endif

					if (light_index >= s_first_item && light_index <= s_last_item)
					{
						calc_point_light(result, light_index, params);
					}
					//else if (light_index > last_item)
					//{
					//	s_bucket = CLUSTER_BUCKETS_PER_CLUSTER;
					//	break;
					//}
				}
			}
		}
	}

	// loop shadow occluders
	if ((aoFlags & 0x1) == 0x1 && numOccluders > 0u)
	{		
		float shadow = (1.0);

		uint s_first_item   = firstOccluder;
		uint s_last_item    = s_first_item + numOccluders - 1u;
		uint s_first_bucket = s_first_item >> 5u;
		uint s_last_bucket  = min(s_last_item >> 5u, max(0u, CLUSTER_BUCKETS_PER_CLUSTER - 1u));
		for (uint s_bucket  = s_first_bucket; s_bucket <= s_last_bucket; ++s_bucket)
		{
			uint bucket_bits = uint(texelFetch(clusterBuffer, int(cluster + s_bucket)).x);
			while (bucket_bits != 0u)
			{
				uint bucket_bit_index = findLSB_unsafe(bucket_bits);
				uint occluder_index = (s_bucket << 5u) + bucket_bit_index;
				bucket_bits ^= (1u << bucket_bit_index);
			
				// eebs: unfortunately I don't have this extension but
				// in theory scalarizing this should speed things up
			#if defined(GL_ARB_shader_ballot) && defined(GL_KHR_shader_subgroup)
				//bucket_bits = readFirstInvocationARB(subgroupOr(bucket_bits));
			#endif

				if (occluder_index >= s_first_item && occluder_index <= s_last_item)
				{
					calc_shadow(shadow, occluder_index - s_first_item, params);
				}
				//else if (occluder_index > last_item)
				//{
				//	s_bucket = CLUSTER_BUCKETS_PER_CLUSTER;
				//	break;
				//}
			}
		}	
			
		result.diffuse = packF2x11_1x10(unpackF2x11_1x10(result.diffuse) * shadow);
		result.specular = packF2x11_1x10(unpackF2x11_1x10(result.specular) * shadow);
	}
#endif

	// gamma hack
	// Unity "Optimizing PBR"
	// https://community.arm.com/cfs-file/__key/communityserver-blogs-components-weblogfiles/00-00-00-20-66/siggraph2015_2D00_mmg_2D00_renaldas_2D00_slides.pdf
	//result.specular.rgb = sqrt(result.specular.rgb) * specularFactor.rgb;
	
	vec3 specularScale = specularFactor.rgb;
	vec3 diffuseScale  = 1.0 - specularFactor.rgb;

	vec3 diffuse  = unpackF2x11_1x10(result.diffuse);
	vec3 specular = unpackF2x11_1x10(result.specular);

	uint v0 = pack_vertex_reg(vec4(diffuse.rgb * diffuseScale.rgb, 0));
	uint v1 = pack_vertex_reg(vec4(specular.rgb * specularScale.rgb, 0));

	v[0] = (v[0] & 0xFF000000) | (v0 & 0x00FFFFFF);
	v[1] = (v[1] & 0xFF000000) | (v1 & 0x00FFFFFF);
#endif // UNLIT
}

// blends directly to registers r0 and r1
// todo: do we want to have per-decal shaders? the vm cost could skyrocket..
void calc_decals()
{
	if(numDecals > 0u)
	{
		uint cluster = get_cluster();

		uint s_first_item   = firstDecal;
		uint s_last_item    = s_first_item + numDecals - 1u;
		uint s_first_bucket = s_first_item >> 5u;
		uint s_last_bucket  = min(s_last_item >> 5u, max(0u, CLUSTER_BUCKETS_PER_CLUSTER - 1u));
		for (uint s_bucket  = s_first_bucket; s_bucket <= s_last_bucket; ++s_bucket)
		{
			uint bucket_bits = uint(texelFetch(clusterBuffer, int(cluster + s_bucket)).x);
			while(bucket_bits != 0u)
			{
				uint bucket_bit_index = findLSB_unsafe(bucket_bits);
				uint decal_index      = (s_bucket << 5u) + bucket_bit_index;
				bucket_bits          ^= (1u << bucket_bit_index);
		
				// eebs: unfortunately I don't have this extension but
				// in theory scalarizing this should speed things up
			#if defined(GL_ARB_shader_ballot) && defined(GL_KHR_shader_subgroup)
				//bucket_bits = readFirstInvocationARB(subgroupOr(bucket_bits));
			#endif

				if (decal_index >= s_first_item && decal_index <= s_last_item)
				{
					calc_decal(decal_index - s_first_item);
				}
				//else if (decal_index > last_item)
				//{
				//	bucket = CLUSTER_BUCKETS_PER_CLUSTER;
				//	break;
				//}
			}
		}
	}
}

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 fragGlow;

void main(void)
{
	vec2 uv     = fetch_vtx_uv(0);
	mat3 tbn    = construct_tbn(uv, fetch_vtx_pos());
	vdir        = encodeHemiUnitVector(fetch_vtx_dir() * tbn);

	// pack interpolated vertex data to reduce register pressure
#ifndef FRAG_ATTR_FETCH
	vpos  = packHalf4x16(f_coord);

	tr[0] = packTexcoordRegister(fetch_vtx_uv(0));
#ifdef REFRACTION
	tr[1] = packTexcoordRegister(fetch_vtx_uv(1));
	tr[2] = packTexcoordRegister(fetch_vtx_uv(2));
	tr[3] = packTexcoordRegister(fetch_vtx_uv(3));
#endif
#endif

	// calculate lighting
	calc_light();

	// run the combiner stage
	run_vm();

	// blend in the decals
	calc_decals();

	// unpack color and do fog and whatever else
	vec4 outColor = unpackUnorm4x8(r[0]); // unpack r0

	//uvec2 crd = uvec2(floor(gl_FragCoord.xy));
	//float dither = dither_value_float(crd);// texelFetch(dithertex, ivec2(gl_FragCoord.xy) & ivec2(3), 0).r;
	//float scale = 1.0 / float((1 << int(8)) - 1);
	//dither *= scale;

#ifdef FOG
	// apply fog to outputs
	//if(fogEnabled > 0)
	{
		//vec4 fog_color = unpackUnorm4x8(fog);
		float fog = unpackUnorm4x8(v[1]).w * fogColor.a;
		outColor.rgb = mix(outColor.rgb, fogColor.rgb, fog);
		fragGlow.rgb = fragGlow.rgb * (1.0 - fog);
	}
#endif

	// todo: make ditherMode a per-rt thing
	// dither the output if needed
	outColor.rgb = sat3(outColor.rgb * invlightMult);

#ifdef REFRACTION
	//vec3 localViewDir = normalize(-f_coord.xyz);
	vec2 screenUV = gl_FragCoord.xy / iResolution.xy;
	//float sceneDepth = textureLod(ztex, screenUV, 0).r;
	float softIntersect = 1.0;//clamp((sceneDepth - f_coord.w) / -localViewDir.y * 500.0, 0.0, 1.0);	
	
	vec2 disp = unpackUnorm4x8(r[5]).xy - 0.5;
	disp *= min(0.05, 0.0001 / unpackHalf2x16(vpos.y).y);// * softIntersect;

	vec2 refrUV = screenUV + disp;
		
	float refrDepth = textureLod(ztex, refrUV, 0).r;
	if (refrDepth < unpackHalf2x16(vpos.y).y)
	{
		refrUV = screenUV;
		refrDepth = textureLod(ztex, screenUV, 0).r;
	}

	float waterStart = unpackHalf2x16(vpos.y).y * 128.0;
	float waterEnd = refrDepth * 128.0;
	float waterDepth = (waterEnd - waterStart);// / -localViewDir.y;			
	vec3 refr = sampleFramebuffer(refrtex, sat2(refrUV)).rgb;
	
	//if ((aoFlags & 0x4) != 0x4)
	{
		float waterFogAmount = sat1(waterDepth / (2.0));

		vec3 half_color = texgen_params.rgb * 0.5;
		vec3 waterFogColor = texgen_params.rgb - (half_color.brr + half_color.ggb);

		if ( any( greaterThan(waterFogColor, vec3(0.0)) ) )
			refr = mix(refr, waterFogColor, waterFogAmount);
	}
		
	vec3 tint = texgen_params.rgb * sat1(waterDepth);
	vec3 half_tint = tint.rgb * 0.5;
	vec3 tint_delta = tint.rgb - (half_tint.brr + half_tint.ggb);
	refr = sat3(tint_delta * refr + refr);

	float texalpha = 90.0 / 255.0;

	float alpha = outColor.w;// + (1.0-softIntersect);
	outColor.rgb = outColor.rgb * alpha + refr.rgb;// mix(outColor.rgb, refr.rgb, alpha);
	outColor.w = 1.0;
#endif

	fragColor.rg = subsample(vec4(outColor)).rg;
	fragColor.b = 0;
	fragColor.a = outColor.a;

	// alpha testing
#ifdef ALPHA_DISCARD
    if (fragColor.a < 0.5) // todo: alpha test value
		discard;
#endif

//#if defined(ALPHA_BLEND) || defined(ALPHA_DISCARD)
	// todo: this is killing early-z for all the passes...
	//float clipDepth = texelFetch(cliptex, ivec2(gl_FragCoord.xy), 0).r;
	//if (clipDepth >= f_coord.w)
	//	discard;
//#endif

//#ifdef ALPHA_BLEND
	//if (fragColor.a - dither * 255.0/16.0 < 0.0)
	//	discard;
//#endif

	// unpack glow and output
	fragGlow = vec4(unpackUnorm4x8(r[1])); // unpack r1

	// apply glow multiplier
	fragGlow.rgb *= emissiveFactor.w;

	// note we subtract instead of add to avoid boosting blacks
	//fragGlow.rgb = min(fragGlow.rgb + (-dither), vec3(1.0));
	fragGlow.rgb = sat3(fragGlow.rgb);
}
