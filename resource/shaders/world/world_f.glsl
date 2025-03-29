import "defines.gli"
import "uniforms.gli"
import "clustering.gli"
import "math.gli"
import "attr.gli"
import "lighting.gli"
import "decals.gli"
import "occluders.gli"
import "textures.gli"
import "texgen.gli"
import "framebuffer.gli"
import "reg.gli"
import "vm.gli"

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
	float viewDist = readVPOS().y;
	return compute_cluster_index(gl_FragCoord.xy, viewDist) * CLUSTER_BUCKETS_PER_CLUSTER;
}

uint scalarize_buckets_bits(uint bucket_bits)
{
#if defined(GL_KHR_shader_subgroup_ballot) && defined(GL_KHR_shader_subgroup_arithmetic)
	bucket_bits = subgroupBroadcastFirst(subgroupOr(bucket_bits));
#endif
	return bucket_bits;
}

bool earlyShadowOut(float shadow)
{
#ifdef GL_KHR_shader_subgroup_vote
	return subgroupAll(shadow < 1.0/255.0);
#else
	return (shadow < 1.0/255.0);
#endif
}

// packs light results into color0 and color1
void calc_light()
{
	v[0] = 0;
	v[1] = 0;

	vec3 normal    = normalizeNear1(fetch_vtx_normal());
	vec3 viewPos   = readVPOS().xyz;
	vec3 view      = normalize(-viewPos.xyz);
	vec3 reflected = reflect(-view, normal);

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
	
#if 0//ndef ALPHA_DISCARD

	v[0] = pack_vertex_reg(vec4(fetch_vtx_color(0)));
	v[1] = pack_vertex_reg(vec4(fetch_vtx_color(1).rgb, fog));

	vec4 lighting = texelFetch(diffuseLightTex, ivec2(gl_FragCoord.xy), 0);
	vec4 a0 = texelFetch(diffuseLightTex, ivec2(gl_FragCoord.xy + vec2(1.0, 0.0)), 0);
	vec4 a1 = texelFetch(diffuseLightTex, ivec2(gl_FragCoord.xy - vec2(1.0, 0.0)), 0);
	vec4 a2 = texelFetch(diffuseLightTex, ivec2(gl_FragCoord.xy + vec2(0.0 ,1.0)), 0);
	vec4 a3 = texelFetch(diffuseLightTex, ivec2(gl_FragCoord.xy - vec2(0.0 ,1.0)), 0);		

	float chroma_d = edge_filter(lighting.rg, a0.rg, a1.rg, a2.rg, a3.rg);
	float chroma_s = edge_filter(lighting.ba, a0.ba, a1.ba, a2.ba, a3.ba);

	ivec2 crd = ivec2(gl_FragCoord.xy);
	bool pattern = ((crd.x & 1) == (crd.y & 1));

	vec3 diffuse = pattern ? vec3(lighting.rg, chroma_d) : vec3(lighting.r, chroma_d, lighting.g);
	diffuse.rgb = ycocg2rgb_unorm(diffuse.rgb);

	vec3 specular = pattern ? vec3(lighting.ba, chroma_s) : vec3(lighting.b, chroma_s, lighting.a);
	specular.rgb = ycocg2rgb_unorm(specular.rgb);
	
	vec3 specularScale = specularFactor.rgb;
	vec3 diffuseScale  = 1.0 - specularFactor.rgb;

	uint v0 = pack_vertex_reg(vec4(diffuse.rgb * diffuseScale.rgb, 0));
	uint v1 = pack_vertex_reg(vec4(specular.rgb * specularScale.rgb, 0));

	v[0] = (v[0] & 0xFF000000) | (v0 & 0x00FFFFFF);
	v[1] = (v[1] & 0xFF000000) | (v1 & 0x00FFFFFF);

#else

	if (lightMode < 2)
	{	
		v[0] = pack_vertex_reg( vec4(fetch_vtx_color(0)) );
		v[1] = pack_vertex_reg( vec4(fetch_vtx_color(1).rgb, fog) );
	}
	else
	{
		v[0] = pack_vertex_reg(vec4(0.0, 0.0, 0.0, fetch_vtx_color(0).w));
		v[1] = pack_vertex_reg(vec4(0.0, 0.0, 0.0, fog));

		light_result result;
		result.diffuse  = packF2x11_1x10(vec3(fetch_vtx_color(0).rgb));
		result.specular = packF2x11_1x10(vec3(fetch_vtx_color(1).rgb));

		if (lightMode > 2)
		{
			light_input params;
			params.pos       = vpos;
			params.normal    = encode_octahedron_uint(normal.xyz);
			params.view      = encode_octahedron_uint(view);
			params.reflected = encode_octahedron_uint(reflected.xyz);
			params.spec_c    = calc_spec_c(roughness);
			params.tint      = packUnorm4x8(vec4(fetch_vtx_color(0)));

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
						uint s_bucket_bits = scalarize_buckets_bits(bucket_bits);
						while (s_bucket_bits != 0u)
						{
							uint s_bucket_bit_index = findLSB(s_bucket_bits);
							uint s_light_index = (s_bucket << 5u) + s_bucket_bit_index;
							s_bucket_bits ^= (1u << s_bucket_bit_index);

							if (s_light_index >= s_first_item && s_light_index <= s_last_item)
							{
								calc_point_light(result, s_light_index, params);
							}
							else if (s_light_index > s_last_item)
							{
								s_bucket = CLUSTER_BUCKETS_PER_CLUSTER;
								break;
							}
						}
					}
				}
			}

			// loop shadow occluders
			if ((aoFlags & 0x1) == 0x1)// && numOccluders > 0u)
			{		
				float shadow = (1.0);

				uint s_first_item   = firstOccluder;
				uint s_last_item    = s_first_item + numOccluders - 1u;
				uint s_first_bucket = s_first_item >> 5u;
				uint s_last_bucket  = min(s_last_item >> 5u, max(0u, CLUSTER_BUCKETS_PER_CLUSTER - 1u));
				for (uint s_bucket  = s_first_bucket; s_bucket <= s_last_bucket; ++s_bucket)
				{
					uint bucket_bits = uint(texelFetch(clusterBuffer, int(cluster + s_bucket)).x);
					uint s_bucket_bits = scalarize_buckets_bits(bucket_bits);
					while (s_bucket_bits != 0u)
					{
						uint s_bucket_bit_index = findLSB(s_bucket_bits);
						uint s_occluder_index = (s_bucket << 5u) + s_bucket_bit_index;
						s_bucket_bits ^= (1u << s_bucket_bit_index);
				
						if (s_occluder_index >= s_first_item && s_occluder_index <= s_last_item)
						{
							calc_shadow(shadow, s_occluder_index - s_first_item, params);

							if (earlyShadowOut(shadow))
								break;
						}
						else if (s_occluder_index > s_last_item)
						{
							s_bucket = CLUSTER_BUCKETS_PER_CLUSTER;
							break;
						}
					}
					
					if (earlyShadowOut(shadow))
						break;
				}
			
			#ifndef ALPHA_DISCARD
				if ((aoFlags & 0x2) == 0x2)
				{
					float linearDepth = readVPOS().w;
					shadow *= upsample_ssao(gl_FragCoord.xy, linearDepth);
				}
			#endif
			
				result.diffuse = packF2x11_1x10(unpackF2x11_1x10(result.diffuse) * shadow);
				result.specular = packF2x11_1x10(unpackF2x11_1x10(result.specular) * shadow);
			} // aoFlags
		} // lightMode > 2

		// gamma hack
		// Unity "Optimizing PBR"
		// https://community.arm.com/cfs-file/__key/communityserver-blogs-components-weblogfiles/00-00-00-20-66/siggraph2015_2D00_mmg_2D00_renaldas_2D00_slides.pdf
		//result.specular.rgb = sqrt(result.specular.rgb) * specularFactor.rgb;
	
		const vec3 specularScale = specularFactor.rgb;
		const vec3 diffuseScale  = 1.0 - specularFactor.rgb;

		vec3 diffuse  = unpackF2x11_1x10(result.diffuse);
		vec3 specular = unpackF2x11_1x10(result.specular);

		uint v0 = pack_vertex_reg(vec4(diffuse.rgb * diffuseScale.rgb, 0));
		uint v1 = pack_vertex_reg(vec4(specular.rgb * specularScale.rgb, 0));

		v[0] = (v[0] & 0xFF000000) | (v0 & 0x00FFFFFF);
		v[1] = (v[1] & 0xFF000000) | (v1 & 0x00FFFFFF);
	} // lightMode < 2

#endif // ALPHA_DISCARD
}

// blends directly to registers r0 and r1
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
			uint s_bucket_bits = scalarize_buckets_bits(bucket_bits);
			while(s_bucket_bits != 0u)
			{
				uint s_bucket_bit_index = findLSB(s_bucket_bits);
				uint s_decal_index      = (s_bucket << 5u) + s_bucket_bit_index;
				s_bucket_bits          ^= (1u << s_bucket_bit_index);

				if (s_decal_index >= s_first_item && s_decal_index <= s_last_item)
				{
					calc_decal(s_decal_index - s_first_item);
				}
				else if (s_decal_index > s_last_item)
				{
					s_bucket = CLUSTER_BUCKETS_PER_CLUSTER;
					break;
				}
			}
		}
	}
}

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 fragGlow;

void main(void)
{
#if defined(GL_KHR_shader_subgroup_ballot) && defined(GL_KHR_shader_subgroup_arithmetic)
	s_lodbias = subgroupBroadcastFirst(subgroupMax(uint( fetch_vtx_lodbias() )));
#else
	s_lodbias = uint( fetch_vtx_lodbias() );
#endif

	vec2    uv      = fetch_vtx_uv(0);
	vec3    viewPos = fetch_vtx_pos();
	flex3   viewDir = flex3(normalize(-viewPos.xyz));
	flex3   normal  = normalize(fetch_vtx_normal());
	flex3x3 tbn     = construct_tbn(uv, viewPos, normal);
	vdir            = encode_octahedron_uint(vec3(viewDir * tbn));

	// pack interpolated vertex data to reduce register pressure
	vpos  = packVPOS(fetch_vtx_coord());

	for (int i = 0; i < UV_SETS; ++i)
		tr[i] = packTexcoordRegister(fetch_vtx_uv(i));

	// calculate lighting
	calc_light();

	// run the combiner stage
	run_vm();

	// blend in the decals
	calc_decals();

	// unpack color and do fog and whatever else
	vec4 outColor = unpackUnorm4x8(r[0]); // unpack r0

	uvec2 crd = uvec2(floor(gl_FragCoord.xy));
	float dither = dither_value_float(crd);// texelFetch(dithertex, ivec2(gl_FragCoord.xy) & ivec2(3), 0).r;
	dither *= ditherScale;

#ifdef FOG
	// apply fog to outputs
	//if(fogEnabled > 0)
	{
		//vec4 fog_color = unpackUnorm4x8(fog);
		float fog = unpackUnorm4x8(v[1]).w * fogColor.a;
		outColor.rgb = mix(outColor.rgb, fogColor.rgb, sat1(fog + dither));
		fragGlow.rgb = fragGlow.rgb * (1.0 - fog);
	}
#endif

	// todo: make ditherMode a per-rt thing
	// dither the output if needed
	outColor.rgb = sat3(outColor.rgb * invlightMult + dither);

#ifdef REFRACTION
	//vec3 localViewDir = normalize(-coord.xyz);
	vec2 screenUV = gl_FragCoord.xy / iResolution.xy;
	//float sceneDepth = textureLod(ztex, screenUV, 0).r;
	float softIntersect = 1.0;//clamp((sceneDepth - f_coord.w) / -localViewDir.y * 500.0, 0.0, 1.0);	
	
	vec2 disp = unpackUnorm4x8(r[5]).xy - 0.5;
	disp *= min(0.05, 0.0001 / fetch_vtx_pos().y);// * softIntersect;

	vec2 refrUV = screenUV + disp;
		
	float refrDepth = textureLod(ztex, refrUV, 0).r;
	if (refrDepth < fetch_vtx_pos().y)
	{
		refrUV = screenUV;
		refrDepth = textureLod(ztex, screenUV, 0).r;
	}

	float waterStart = fetch_vtx_pos().y * 128.0;
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

	fragColor = subsample(outColor);

	// alpha testing
#ifdef ALPHA_DISCARD
    if (fragColor.a < 0.01) // todo: alpha test value
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
	fragGlow.rgb = sat3(fragGlow.rgb + -dither);
}
