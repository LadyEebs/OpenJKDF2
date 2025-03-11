#include "uniforms.gli"
#include "clustering.gli"
#include "math.gli"
#include "lighting.gli"

in vec3 coord3d;
in vec4 v_normal;
in vec4 v_color[2];
in vec4 v_uv[4];
in vec3 coordVS;

out vec4 f_color[2];
//out float f_light;
out vec4 f_uv[4];
//out vec4 f_uv_nooffset;
out vec3 f_coord;
out vec3 f_normal;
out float f_depth;

noperspective out vec2 f_uv_affine;

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
	vec3 result = vec3(0.0);
//#ifdef SPECULAR
	float m = roughness * roughness;
	float m2 = max(m * m, 1e-4);
	float amplitude = 1.0 / (3.141592 * m2);
	float sharpness = (2.0 / m2) / (4.0 * max(dot(normal, view), 0.1));
	
	for(int sg = 0; sg < 8; ++sg)
	{
		vec4 sgCol = ambientSG[sg];//unpack_argb2101010(ambientSG[sg]);
		vec3 color = mix(f_color[0].rgb, sgCol.xyz, sgCol.w); // use vertex color if no ambientSG data
	
		float umLength = length(sharpness * reflected + (ambientSGBasis[sg].w * ambientSGBasis[sg].xyz));
		float attenuation = 1.0 - exp(-2.0 * umLength);
		float nDotL = clamp(dot(normal.xyz, reflected), 0.0, 1.0);
	
		//float D = (2.0 * 3.141592) * nDotL * attenuation / umLength;
		float D = (2.0 * 3.141592) * nDotL * attenuation / umLength;
		
		// fresnel approx as 1 / ldoth
		//vec3 h = normalize(reflected + view);
		//D /= clamp(dot(reflected, h), 0.1, 1.0);

		float expo = (exp(umLength - sharpness - ambientSGBasis[sg].w) * amplitude);
		result = (D * expo) * color + result;
	}
//#endif
	return result;// * 3.141592;
}

void main(void)
{
	vec4 viewPos = modelMatrix * vec4(coord3d, 1.0);
    vec4 pos = projMatrix * viewPos;

	mat3 normalMatrix = transpose(inverse(mat3(modelMatrix))); // if we ever need scaling
	f_normal = normalMatrix * (v_normal.xyz * 2.0 - 1.0);
	f_normal = normalize(f_normal);

	//if(lightMode < 3)
		//f_normal = face_normal;

    gl_Position = pos;
    f_color[0] = clamp(v_color[0].bgra, vec4(0.0), vec4(1.0));
	f_color[1] = clamp(v_color[1].bgra, vec4(0.0), vec4(1.0));

    f_uv[0] = v_uv[0];
    f_uv[1] = v_uv[1];
    f_uv[2] = v_uv[2];
    f_uv[3] = v_uv[3];
	//f_uv_nooffset = v_uv[0];
	f_uv[0].xy += uv_offset[0].xy;
	f_uv[1].xy += uv_offset[1].xy;
	f_uv[2].xy += uv_offset[2].xy;
	f_uv[3].xy += uv_offset[3].xy;
	f_uv_affine = v_uv[0].xy;

	f_coord = viewPos.xyz;
	vec3 view = normalize(-viewPos.xyz);

//#ifdef UNLIT
//	if (lightMode == 0)
//		f_light = light_mult;
//	else
//		f_light = 0.0;
//#else
//    f_light = 0.0;
//#endif

 	f_depth = pos.w / 128.0;

#ifdef UNLIT
	f_color[1] = vec4(0.0);
	if(lightMode == 0) // full lit
		f_color[0].xyz = vec3(light_mult);
	else if(lightMode == 1) // not lit
		f_color[0].xyz = vec3(0.0);
#else
	// do ambient diffuse in vertex shader
	if (lightMode >= 2)
		f_color[0].xyz = max(f_color[0].xyz, ambientColor.xyz);
	
	if(lightMode >= 2)
		f_color[0].xyz += CalculateAmbientDiffuse(f_normal);

	f_color[1] = vec4(0.0);
	//if (lightMode == 4)
		//f_color[1].xyz = CalculateAmbientSpecular(0.05, f_normal, view, reflect(-view, f_normal));

	#ifdef SPECULAR
		// metal
		//f_color[1].xyz = CalculateAmbientSpecular(0.25, f_normal, view, reflect(-view, f_normal));
	#endif

	// light mode "diffuse" (a.k.a new gouraud)
	if (lightMode == 2)
	{
		light_result result;
		result.diffuse = vec3(0.0);
		result.specular = vec3(0.0);

	//#ifdef SPECULAR
		result.specular = CalculateAmbientSpecular(roughnessFactor, f_normal, view, reflect(-view, f_normal));
	//#endif


		if(numLights > 0u)
		{
			float a = roughnessFactor;// * roughnessFactor;

			uint cluster = compute_cluster_index((pos.xy / pos.w * 0.5 + 0.5) * iResolution.xy, f_coord.y);
			light_input params;
			params.pos = f_coord.xyz;
			//params.normal = f_normal.xyz;
			params.normal = encode_octahedron_uint(f_normal.xyz);
			params.view = encode_octahedron_uint(view);
			params.reflected = encode_octahedron_uint(reflect(-view, f_normal));
			params.roughness = roughnessFactor;
			params.roughness2 = roughnessFactor * roughnessFactor;
			params.normalizationTerm = roughnessFactor * 4.0 + 2.0;

			params.a2 = a * a;
			params.rcp_a2 = 1.0 / params.a2;
			params.spec_c = get_spec_c(params.rcp_a2);
			params.rcp_a2 /= 3.141592; // todo: roll pi into specular light intensity?
			params.tint = pack_argb8888(f_color[0] * 255.0);

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
	
			f_color[0].xyz += result.diffuse.rgb;
			f_color[1].xyz += result.specular.rgb;
		}
	}


#endif
}
