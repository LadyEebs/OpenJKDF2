#include "defines.gli"
#include "uniforms.gli"
#include "clustering.gli"
#include "math.gli"
#include "lighting.gli"
#include "decals.gli"
#include "occluders.gli"
#include "textures.gli"
#include "framebuffer.gli"

in vec4 f_color[2];

#ifdef REFRACTION
in vec3 f_uv[4];
#else
in vec3 f_uv[1];
#endif

in vec4 f_coord;
in vec3 f_normal;

flat in float f_lodbias;

const float f_light = 0.0;

//noperspective in vec2 f_uv_affine;

#include "vm.gli"

bool ceiling_intersect(vec3 pos, vec3 dir, vec3 normal, vec3 center, inout float t)
{
	float denom = dot(dir, normal);
	if (abs(denom) > 1e-6)
	{
		t = dot(center - pos, normal) / denom;
		if (t >= 0.0 && t < 1000.0)
		{
			return true;
		}
	}
	return false;
}

vec2 do_ceiling_uv()//inout vec3 viewPos)
{
	vec3 ray_dir = normalize(-f_coord.xyz);//viewPos);
	vec3 view_ceiling = (viewMatrix * vec4(texgen_params.xyz, 1.0)).xyz;
	vec3 view_norm = mat3(viewMatrix) * vec3(0,0,-1);

	float tmp = 0.0;
	if (!ceiling_intersect(vec3(0.0), ray_dir, view_norm, view_ceiling.xyz, tmp))
		tmp = 1000.0;

    vec3 sky_pos = tmp * ray_dir + vec3(0.0);
	
	//viewPos.y = sky_pos.y;

	vec2 uv = sky_pos.xy * 16.0;

	vec4 proj_sky = projMatrix * vec4(sky_pos.xyz, 1.0);
	
	return (uv + uv_offset[0].xy) / texsize.xy;
}

vec2 do_horizon_uv()
{
	vec2 projXY = vec2(0.5,-0.5) * (gl_FragCoord.xy / iResolution.xy);
	projXY = projXY.xy * iResolution.xy * (texgen_params.x / gl_FragCoord.w);

	vec2 uv;
	uv.x = projXY.x * texgen_params.y + (projXY.y * -texgen_params.z);
	uv.y = projXY.y * texgen_params.y + (projXY.x *  texgen_params.z);
	
	return (uv + uv_offset[0].xy) / texsize.xy;
}

// fixme
vec3 do_texgen(in vec3 uv)//, inout vec3 viewPos)
{
	if(texgen == 1) // 1 = RD_TEXGEN_HORIZON
	{
		uv.xy = do_horizon_uv();
		uv.z = 0.0;
	}
	else if(texgen == 2) // 2 = RD_TEXGEN_CEILING
	{
		uv.xy = do_ceiling_uv();//viewPos);
		uv.z = 0;
	}
	return uv.xyz;
}

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 fragGlow;

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
	vec4 weights = clamp(1.0 - diff, vec4(0.0), vec4(1.0));

	// total weight
	float totalWeight = weights.x + weights.y + weights.z + weights.w;

	// average when weight is bad
	if(totalWeight < 1e-4)
		return (values.x + values.y + values.z + values.w) * 0.25;

	return dot(weights / totalWeight, values);
}

float Schlick(float k, float costh)
{
    return (1.0 - k * k) / (4.0 * 3.141592 * pow(1.0 - k * costh, 2.0));
}

void calc_lod_bias()
{
	uint cluster = compute_cluster_index(gl_FragCoord.xy, f_coord.y) * CLUSTER_BUCKETS_PER_CLUSTER;
	c_l = (cluster & 0xFFFF) | packHalf2x16(vec2(0.0, f_lodbias));
}

//void calc_fog()
//{
//	fog = 0;
//#ifdef FOG
//#ifndef REFRACTION // tmp, fixme
//	if(fogEnabled > 0)
//	{
//		vec3 viewDir = normalize(-f_coord.xyz);
//	
//		float clipDepth = texelFetch(cliptex, ivec2(gl_FragCoord.xy), 0).r;
//
//		float distToCam = max(0.0, f_coord.w * 128.0 - clipDepth) * fastRcpNR1(-viewDir.y);
//		float fog_amount = clamp((distToCam - fogStart) * fastRcpNR1(fogEnd - fogStart), 0.0, 1.0);
//	
//		vec3 fog_color = fogColor.rgb;
//	
//		// fog light scatter
//		if(fogLightDir.w > 0.0)
//		{
//			float k = fogAnisotropy;
//			float c = 1.0 - k * dot(fogLightDir.xyz, viewDir.xyz);
//			fog_color *= (1.0 - k * k) * fastRcpNR0(c * c);
//			//fog_color *= fastRcpNR0(1.0 + fog_color);
//		}
//	
//		fog = packUnorm4x8(vec4(fog_color.rgb, fog_amount * fogColor.a));
//	}
//#endif
//#endif
//}

// packs light results into color0 and color1
void calc_light()
{
	v[0] = 0;
	v[1] = 0;

	vec3 viewPos = vec3(unpackHalf2x16(vpos.x), unpackHalf2x16(vpos.y).x); // f_coord.xyz;
	vec3 view = normalize(-viewPos.xyz);
	vec3 normal = normalize(f_normal.xyz);
	vec3 reflected = reflect(-view, normal);
	
	float fog = 0.0;
#ifdef FOG
	if(fogEnabled > 0)
	{
		float clipDepth = 0;//texelFetch(cliptex, ivec2(gl_FragCoord.xy), 0).r;

		float distToCam = max(0.0, unpackHalf2x16(vpos.y).y * 128.0 - clipDepth) * fastRcpNR1(-view.y);
		fog = clamp((distToCam - fogStart) * fastRcpNR1(fogEnd - fogStart), 0.0, 1.0);
	}
#endif

	float roughness = (roughnessFactor);
	
	v[0] = packColorRegister(f_color[0]);
	v[1] = packColorRegister(vec4(f_color[1].rgb, fog));

#ifdef UNLIT
	/*if (lightMode == 0) // fully lit
	{
		uint v0 = packColorRegister(vec4(light_mult)) & 0x00FFFFFF;

		v[0] = (v[0] & 0xFF000000) | v0;
		v[1] = (v[1] & 0xFF000000) | v0;
	}
	else if (lightMode == 1) // not lit
	{
		v[0] = (v[0] & 0xFF000000);
		v[1] = (v[1] & 0xFF000000);
	}
	else // "diffuse"/vertex lit
	{
		v[0] = packColorRegister(f_color[0]);
		v[1] = packColorRegister(vec4(f_color[1].rgb, fog));
	}*/

	if (lightMode == 0) // fully lit
	{
		v[0] = packColorRegister(vec4(vec3(light_mult), f_color[0].w));
		v[1] = packColorRegister(vec4(vec3(light_mult), fog));
	}
	else if (lightMode == 1) // not lit
	{
		v[0] = packColorRegister(vec4(0.0, 0.0, 0.0, f_color[0].w));
		v[1] = packColorRegister(vec4(0.0, 0.0, 0.0, fog));
	}
	else // "diffuse"/vertex lit
	{
		v[0] = packColorRegister(f_color[0]);
		v[1] = packColorRegister(vec4(f_color[1].rgb, fog));
	}
#else // UNLIT

	v[0] = packColorRegister(vec4(0.0, 0.0, 0.0, f_color[0].w));
	v[1] = packColorRegister(vec4(0.0, 0.0, 0.0, fog));

	light_result result;
	result.diffuse  = packF2x11_1x10(f_color[0].rgb);
	result.specular = packF2x11_1x10(f_color[1].rgb);

	light_input params;
	params.pos       = vpos;
	params.normal    = encode_octahedron_uint(normal.xyz);
	params.view      = encode_octahedron_uint(view);
	params.reflected = encode_octahedron_uint(reflected.xyz);
	params.spec_c    = calc_spec_c(roughness);
	params.tint      = packUnorm4x8(f_color[0]);

#ifndef VERTEX_LIT
	// light mode "gouraud" (a.k.a new per pixel)
	{
		//result.specular = packF2x11_1x10(calc_ambient_specular(roughness, normal, view, reflected));

		// loop lights
		//if (numLights > 0u)
		{
			uint s_first_item   = firstLight;
			uint s_last_item    = s_first_item + numLights - 1u;
			uint s_first_bucket = s_first_item >> 5u;
			uint s_last_bucket  = min(s_last_item >> 5u, max(0u, CLUSTER_BUCKETS_PER_CLUSTER - 1u));
			for (uint s_bucket  = s_first_bucket; s_bucket <= s_last_bucket; ++s_bucket)
			{
				uint bucket_bits = uint(texelFetch(clusterBuffer, int((c_l & 0xFFFF) + s_bucket)).x);
				while (bucket_bits != 0u)
				{
					uint bucket_bit_index = findLSB_unsafe(bucket_bits);
					uint light_index = (s_bucket << 5u) + bucket_bit_index;
					bucket_bits ^= (1u << bucket_bit_index);

					// eebs: unfortunately I don't have this extension but
					// in theory scalarizing this should speed things up
				#if defined(GL_ARB_shader_ballot) && defined(GL_KHR_shader_subgroup)
					bucket_bits = readFirstInvocationARB(subgroupOr(bucket_bits));
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
			uint bucket_bits = uint(texelFetch(clusterBuffer, int((c_l & 0xFFFF) + s_bucket)).x);
			while (bucket_bits != 0u)
			{
				uint bucket_bit_index = findLSB_unsafe(bucket_bits);
				uint occluder_index = (s_bucket << 5u) + bucket_bit_index;
				bucket_bits ^= (1u << bucket_bit_index);
			
				// eebs: unfortunately I don't have this extension but
				// in theory scalarizing this should speed things up
			#if defined(GL_ARB_shader_ballot) && defined(GL_KHR_shader_subgroup)
				bucket_bits = readFirstInvocationARB(subgroupOr(bucket_bits));
			#endif

				if (occluder_index >= s_first_item && occluder_index <= s_last_item)
				{
					vec3 viewPos = unpackHalf4x16(vpos).xyz; // f_coord.xyz;
					calc_shadow(shadow, occluder_index - s_first_item, viewPos.xyz, params.normal);
				}
				//else if (occluder_index > last_item)
				//{
				//	s_bucket = CLUSTER_BUCKETS_PER_CLUSTER;
				//	break;
				//}
			}
		}	
			
		//float ao   = shadow * (0.8) + (0.2); // remap so we don't overdarken
		result.diffuse = packF2x11_1x10(unpackF2x11_1x10(result.diffuse) * shadow);

		//float ndotv = dot(normal.xyz, view.xyz);
		//float specAO = mix(ao * ao, 1.0, clamp(-0.3 * ndotv * ndotv, 0.0, 1.0));
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

	uint v0 = packColorRegister(vec4(diffuse.rgb * diffuseScale.rgb, 0));
	uint v1 = packColorRegister(vec4(specular.rgb * specularScale.rgb, 0));

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
		uint s_first_item   = firstDecal;
		uint s_last_item    = s_first_item + numDecals - 1u;
		uint s_first_bucket = s_first_item >> 5u;
		uint s_last_bucket  = min(s_last_item >> 5u, max(0u, CLUSTER_BUCKETS_PER_CLUSTER - 1u));
		for (uint s_bucket  = s_first_bucket; s_bucket <= s_last_bucket; ++s_bucket)
		{
			uint bucket_bits = uint(texelFetch(clusterBuffer, int((c_l & 0xFFFF) + s_bucket)).x);
			while(bucket_bits != 0u)
			{
				uint bucket_bit_index = findLSB_unsafe(bucket_bits);
				uint decal_index      = (s_bucket << 5u) + bucket_bit_index;
				bucket_bits          ^= (1u << bucket_bit_index);
		
				// eebs: unfortunately I don't have this extension but
				// in theory scalarizing this should speed things up
			#if defined(GL_ARB_shader_ballot) && defined(GL_KHR_shader_subgroup)
				bucket_bits = readFirstInvocationARB(subgroupOr(bucket_bits));
			#endif

				if (decal_index >= s_first_item && decal_index <= s_last_item)
				{
					decal dec = decals[decal_index - s_first_item];

					vec3 viewPos = unpackHalf4x16(vpos).xyz; // f_coord.xyz;
					vec3 objectPosition = vec3(transform44(dec.invDecalMatrix, viewPos.xyz));				
					vec3 falloff        = (0.5) - abs(objectPosition.xyz);
					if( any(lessThanEqual(falloff, vec3(0.0))) )
						continue;
				
					vec2 decalTexCoord = objectPosition.xz + (0.5);
					decalTexCoord         = decalTexCoord.xy * dec.uvScaleBias.zw + dec.uvScaleBias.xy;
				
					vec4 decalColor = vec4(textureLod(decalAtlas, decalTexCoord, 0));
					if((dec.flags & 0x8u) == 0x8u) // rgb as alpha
						decalColor.a = max(decalColor.r, max(decalColor.g, decalColor.b));

					if(decalColor.a < (0.001))
						continue;
				
					decalColor.rgb *= vec3(dec.color.rgb);
				
					if((dec.flags & 0x2u) == 0x2u) // heat
					{
						decalColor.rgb = (4.0) * vec3(textureLod(blackbodyTex, decalColor.r, 0.0).rgb);
						//decalColor.rgb = blackbody(decalColor.r);
						
						vec4 glow = unpackRegister(r[1]);
						glow.rgb     = decalColor.rgb * (255.0) + glow.rgb;
						r[1]         = packRegister(glow);
					}

					//decalColor.rgb *= (255.0);
				
					// attenuate when facing away from the decal direction
					float edgeFactor = clamp(abs(objectPosition.z), (0.0), (1.0));
					edgeFactor      *= edgeFactor;
					edgeFactor      *= edgeFactor;
					edgeFactor      *= edgeFactor;
					edgeFactor       = (1.0) - edgeFactor;

					vec4 color = unpackRegister(r[0]);

					if((dec.flags & 0x4u) == 0x4u) // additive
					{
						color.rgb = edgeFactor * decalColor.rgb + color.rgb;
					}
					else
					{
						color.rgb = mix(color.rgb, decalColor.rgb, decalColor.w * edgeFactor);

						//color.rgb = color.rgb * (1.0 - decalColor.w) + decalColor.rgb * decalColor.w;
						//color.w *= 1.0 - decalColor.w;
					}

					r[0] = packRegister(color);
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

void main(void)
{
	mat3 tbn    = construct_tbn(f_uv[0].xy / f_uv[0].z, f_coord.xyz);
	vec3 viewTS = normalize(-f_coord.xyz) * tbn;
	vdir        = encodeHemiUnitVector(viewTS);
	vpos        = packHalf4x16(f_coord);

	// setup texcoord registers, from here out don't directly access f_uv
	//tr[0] = packTexcoordRegister(do_texgen(f_uv[0].xyz / f_uv[0].z).xy);

	tr[0] = packTexcoordRegister(f_uv[0].xy / f_uv[0].z);
#ifdef REFRACTION
	tr[1] = packTexcoordRegister(f_uv[1].xy / f_uv[1].z);
	tr[2] = packTexcoordRegister(f_uv[2].xy / f_uv[2].z);
	tr[3] = packTexcoordRegister(f_uv[3].xy / f_uv[3].z);
#endif

	// calculate anything that relies on interpolated data
	// up front so we can free vgprs for the vm stages
	// the results are compacted and stored for later
	calc_lod_bias();
	calc_light();

	// run the texture stage
	// todo: how to handle decals?
	run_tex_stage();

	// run the combiner stage
	run_combiner_stage();

	// blend in the decals
	calc_decals();

	// unpack output registers
	vec4 outColor = unpackRegister(r[0]); // unpack r0
	fragGlow = vec4(unpackRegister(r[1])); // unpack r1

	// apply glow multiplier
	fragGlow.rgb *= emissiveFactor.w;

	//uvec2 crd = uvec2(floor(gl_FragCoord.xy));
	//float dither = dither_value_float(crd);// texelFetch(dithertex, ivec2(gl_FragCoord.xy) & ivec2(3), 0).r;
	//float scale = 1.0 / float((1 << int(8)) - 1);
	//dither *= scale;

#ifdef FOG
	// apply fog to outputs
	//if(fogEnabled > 0)
	{
		//vec4 fog_color = unpackUnorm4x8(fog);
		float fog = unpackRegister(v[1]).w * fogColor.a;
		outColor.rgb = mix(outColor.rgb, fogColor.rgb, fog);
		fragGlow.rgb = fragGlow.rgb * (1.0 - fog);
	}
#endif

	// todo: make ditherMode a per-rt thing
	// dither the output if needed
	outColor.rgb = clamp(outColor.rgb * invlightMult, vec3(0.0), vec3(1.0));

#ifdef REFRACTION
	//vec3 localViewDir = normalize(-f_coord.xyz);
	vec2 screenUV = gl_FragCoord.xy / iResolution.xy;
	//float sceneDepth = textureLod(ztex, screenUV, 0).r;
	float softIntersect = 1.0;//clamp((sceneDepth - f_coord.w) / -localViewDir.y * 500.0, 0.0, 1.0);	
	
	vec2 disp = unpackRegister(r[5]).xy - 0.5;
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
	vec3 refr = sampleFramebuffer(refrtex, refrUV).rgb;
	
	//if ((aoFlags & 0x4) != 0x4)
	{
		float waterFogAmount = clamp(waterDepth / (2.0), 0.0, 1.0);

		vec3 half_color = texgen_params.rgb * 0.5;
		vec3 waterFogColor = texgen_params.rgb - (half_color.brr + half_color.ggb);

		//if(fogLightDir.w > 0.0)
			///waterFogColor *= 4.0 * 3.141592 * Schlick(0.35, dot(fogLightDir.xyz, localViewDir));
			//waterFogColor *= textureLod(dithertex, vec2(0.35, dot(fogLightDir.xyz, localViewDir)*0.5+0.5), 0.0).r;

		if ( any( greaterThan(waterFogColor, vec3(0.0)) ) )
			refr = mix(refr, waterFogColor, waterFogAmount);
	}
		
	vec3 tint = texgen_params.rgb * clamp(waterDepth, 0.0, 1.0);
	vec3 half_tint = tint.rgb * 0.5;
	vec3 tint_delta = tint.rgb - (half_tint.brr + half_tint.ggb);
	refr = clamp(tint_delta * refr + refr, vec3(0.0), vec3(1.0));

	float texalpha = 90.0 / 255.0;

	float alpha = outColor.w;// + (1.0-softIntersect);
	outColor.rgb = outColor.rgb * alpha + refr.rgb;// mix(outColor.rgb, refr.rgb, alpha);
	outColor.w = 1.0;
#endif

	fragColor.rg = subsample(vec4(outColor)).rg;
	fragColor.a = outColor.a;

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

	// note we subtract instead of add to avoid boosting blacks
	//fragGlow.rgb = min(fragGlow.rgb + (-dither), vec3(1.0));
	fragGlow.rgb = clamp(fragGlow.rgb, vec3(0.0), vec3(1.0));
}
