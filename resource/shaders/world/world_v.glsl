#include "uniforms.gli"
#include "clustering.gli"
#include "math.gli"
#include "lighting.gli"
#include "attr.gli"

vec3 CalculateAmbientDiffuse(vec3 normal)
{
	vec3 ambientDiffuse = vec3(0.0);
	for(int sg = 0; sg < 8; ++sg)
	{
		SG lightSG;
		lightSG.Amplitude = ambientSG[sg].xyz;//unpack_argb2101010(ambientSG[sg]).xyz / 255.0;
		lightSG.Axis = ambientSGBasis[sg].xyz;
		lightSG.Sharpness = ambientSGBasis[sg].w;
	
		vec3 diffuse = SGIrradiancePunctual(lightSG, normal);
		ambientDiffuse.xyz += diffuse;
	}
	return ambientDiffuse;
}

vec3 CalculateAmbientSpecular(float roughness, vec3 normal, vec3 view, vec3 reflected)
{
	vec3 ambientSpec = vec3(0.0);

	float m = roughness * roughness;
	float m2 = max(m * m, 1e-4);
	float amplitude = 1.0 * fastRcpNR0(3.141592 * m2);
	float sharpness = (2.0 * fastRcpNR0(m2)) * fastRcpNR0(4.0 * max(dot(normal, view), 0.1));
	
	for(int sg = 0; sg < 8; ++sg)
	{
		vec4 sgCol = ambientSG[sg];//unpack_argb2101010(ambientSG[sg]);
		vec3 ambientColor = mix(v_color[0].bgr, sgCol.xyz, sgCol.w); // use vertex color if no ambientSG data
	
		float umLength = length(sharpness * reflected + (ambientSGBasis[sg].w * ambientSGBasis[sg].xyz));
		float attenuation = 1.0 - exp(-2.0 * umLength);
		float nDotL = clamp(dot(normal.xyz, reflected), 0.0, 1.0);
	
		// todo: can we roll this into something like the direct light approx?
		float D = (2.0 * 3.141592) * nDotL * attenuation * fastRcpNR1(umLength);
		//float D = (2.0 * nDotL) * attenuation * fastRcpNR1(umLength);
		
		float expo = (exp(umLength - sharpness - ambientSGBasis[sg].w) * amplitude);

		// fresnel approx as 1 / ldoth at center of the warped lobe
		//vec3 h = normalize(reflected + view);
		//D /= clamp(dot(reflected, h), 0.1, 1.0);

		ambientSpec = (D * expo) * ambientColor + ambientSpec;
	}
	return ambientSpec;
}


float compute_mip_bias(float z_min)
{
	float mipmap_level = float(0.0);
	mipmap_level        = z_min < mipDistances.x ? mipmap_level : float(1.0);
	mipmap_level        = z_min < mipDistances.y ? mipmap_level : float(2.0);
	mipmap_level        = z_min < mipDistances.z ? mipmap_level : float(3.0);
	return mipmap_level + float(mipDistances.w);
}

void main(void)
{
	vec4 viewPos = modelMatrix * vec4(coord3d, 1.0);
    vec4 pos = projMatrix * viewPos;

	mat3 normalMatrix = transpose(inverse(mat3(modelMatrix))); // if we ever need scaling
	vec3 normal = normalMatrix * (v_normal.xyz * 2.0 - 1.0);
	normal = normalize(normal);
	f_normal.xyz = vec3(normal);
	f_lodbias = min(compute_mip_bias(viewPos.y), (numMips - 1));

    gl_Position = pos;
    f_color[0] = vec4(clamp(v_color[0].bgra, vec4(0.0), vec4(1.0)));
	f_color[1] = vec4(clamp(v_color[1].bgra, vec4(0.0), vec4(1.0)));

    f_uv[0] = v_uv[0].xyw;
	f_uv[0].xy += uv_offset[0].xy;

#ifdef REFRACTION
    f_uv[1] = v_uv[1].xyw;
    f_uv[2] = v_uv[2].xyw;
    f_uv[3] = v_uv[3].xyw;

	f_uv[1].xy += uv_offset[1].xy;
	f_uv[2].xy += uv_offset[2].xy;
	f_uv[3].xy += uv_offset[3].xy;

#endif

 	f_coord.xyz = viewPos.xyz;
	f_coord.w = pos.w / 128.0;
	
	vec3 view = normalize(-viewPos.xyz);

	f_color[1].a = 0.0;
	if(fogEnabled > 0)
		f_color[1].a = clamp((pos.w - fogStart) / (fogEnd - fogStart), 0.0, 1.0);

#ifdef UNLIT
	f_color[1].rgb = vec3(0.0);

	if(lightMode == 0) // full lit
		f_color[0].xyz = vec3(light_mult);
	else if(lightMode == 1) // not lit
		f_color[0].xyz = vec3(0.0);

#else
	// do ambient diffuse in vertex shader
	if (lightMode >= 2)
		f_color[0].xyz = max(f_color[0].xyz, ambientColor.xyz);
	
	if(lightMode >= 2)
		f_color[0].xyz += CalculateAmbientDiffuse(normal);

	f_color[1].rgb = vec3(0.0);
	if(lightMode >= 2)
		f_color[1].xyz = CalculateAmbientSpecular(roughnessFactor, normal, view, reflect(-view, normal));

	//f_color[0].rgb *= (255.0);
	//f_color[1].rgb *= (255.0);


	// light mode "diffuse" (a.k.a new gouraud)
#ifdef VERTEX_LIT
	{
		light_result result;
		result.diffuse = packF2x11_1x10(f_color[0].rgb);
		result.specular = packF2x11_1x10(f_color[1].rgb);

		if(numLights > 0u)
		{
			float a = roughnessFactor;// * roughnessFactor;

			uint cluster = compute_cluster_index(clamp(pos.xy / pos.w * 0.5 + 0.5, vec2(0.0), vec2(1.0)) * iResolution.xy, f_coord.y);
			light_input params;
			params.pos       = uvec2(packHalf2x16(f_coord.xy), packHalf2x16(vec2(f_coord.z, f_coord.w)));
			params.normal    = encode_octahedron_uint(normal.xyz);
			params.view      = encode_octahedron_uint(view);
			params.reflected = encode_octahedron_uint(reflect(-view, normal));
			params.spec_c    = calc_spec_c(a);
			params.tint      = packUnorm4x8(f_color[0]);

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
						calc_point_light(result, light_index, params);
					}
					else if (light_index > last_item)
					{
						bucket = CLUSTER_BUCKETS_PER_CLUSTER;
						break;
					}
				}
			}
	
			f_color[0].rgb = unpackF2x11_1x10(result.diffuse);
			f_color[1].rgb = unpackF2x11_1x10(result.specular);
		}
	}
#endif

#endif
}
