#include "defines.gli"
#include "uniforms.gli"
#include "clustering.gli"
#include "math.gli"
#include "lighting.gli"
#include "decals.gli"
#include "occluders.gli"
#include "textures.gli"
#include "framebuffer.gli"

#include "vm.gli"

in vec4 f_color[2];
//in float f_light;
in vec4 f_uv[4];
//in vec4 f_uv_nooffset;
in vec3 f_coord;
in vec3 f_normal;
in float f_depth;

const float f_light = 0.0;

noperspective in vec2 f_uv_affine;

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
	projXY = projXY.xy * iResolution.xy * (texgen_params.x);// / gl_FragCoord.w);

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




vec3 CalculateAmbientSpecular(vec3 normal, vec3 view, float roughness, vec3 f0)
{
	vec3 ambientSpecular = vec3(0.0);
	for(int sg = 0; sg < 8; ++sg)
	{
		SG ndf = DistributionTermSG(normal, roughness);
		SG warpedNDF = WarpDistributionSG(ndf, view);

		SG lightSG;
		lightSG.Amplitude = ambientSG[sg].xyz;// unpack_argb2101010(ambientSG[sg]).xyz / 255.0;
		lightSG.Axis = ambientSGBasis[sg].xyz;
		lightSG.Sharpness = ambientSGBasis[sg].w;

		float nDotL = clamp(dot(normal.xyz, warpedNDF.Axis.xyz), 0.0, 1.0);

		// NDF
		vec3 spec = SGInnerProduct(warpedNDF, lightSG);

		// no Geometry term

		// Fresnel
		vec3 h = normalize(warpedNDF.Axis + view);
		vec3 f = f0;// + (1.0 - f0) * exp2(-8.35 * max(0.0, dot(warpedNDF.Axis, h)));
		//f *= clamp(dot(f0, vec3(333.0)), 0.0, 1.0); // fade out when spec is less than 0.1% albedo
		
		ambientSpecular.xyz = (spec * nDotL) * f + ambientSpecular.xyz;
	}
	return ambientSpecular;// * 3.141592;
}

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

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 fragGlow;

vec3 normals(vec3 pos) {
    vec3 fdx = dFdx(pos);
    vec3 fdy = dFdy(pos);
    return normalize(cross(fdx, fdy));
}

// todo: binary search and lower sample count
vec2 parallax_mapping(vec2 tc, mat3 tbn)
{
    /*if (f_coord.x < 0.5) {
        return tc;
    }*/

	vec3 localViewDir = normalize(-f_coord.xyz);
    vec3 view_dir = normalize(transpose(tbn) * localViewDir);

    const float min_layers = 32.0;
    const float max_layers = 128.0;
    float num_layers = mix(max_layers, min_layers, abs(dot(vec3(0.0, 0.0, 1.0), view_dir)));

    float layer_depth = 1.0 / num_layers;
    float current_layer_depth = 0.0;
    vec2 shift_per_layer = (view_dir.xy / -view_dir.z) * displacement_factor;
    vec2 d_tc = shift_per_layer / num_layers;

    vec2 current_tc = tc;
    float current_sample = textureLod(displacement_map, current_tc, 0).r;

    while(current_layer_depth < current_sample) {
        current_tc -= d_tc;
        current_sample = textureLod(displacement_map, current_tc, 0).r;
        current_layer_depth += layer_depth;
    }

    vec2 prev_tc = current_tc + d_tc;

    float after_col_depth = current_sample - current_layer_depth;
    float before_col_depth = textureLod(displacement_map, prev_tc, 0).r - current_layer_depth + layer_depth;

    float a = after_col_depth / (after_col_depth - before_col_depth);
    vec2 adj_tc = mix(current_tc, prev_tc, a);

    return adj_tc;
}

// much thanks to Cary Knoop
// https://forum.blackmagicdesign.com/viewtopic.php?f=21&t=122108
float rolloff(float x, float cutoff, float soft)
{
    float low  = cutoff - soft;
	float high = cutoff + soft;
	
	if(x <= low)
		return x;
        
	if(x >= high)
		return cutoff;
	
	return -1.0f / (4.0f * soft) * (x * x - 2.0f * high * x + low * low);
}

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

// https://www.shadertoy.com/view/MdXyzX
// https://www.shadertoy.com/view/ctjXzd
vec2 wavedx(vec2 position, vec2 direction, float speed, float frequency, float timeshift)
{
    float x = dot(direction, position) * frequency + timeshift * speed;
    float wave = exp(sin(x) - 1.0);
    float dx = wave * cos(x);
    return vec2(wave, -dx);
}

float getwaves(vec2 position, int iterations,float t)
{
	float iter = 0.0;
    float phase = 6.0;
    float speed = 2.0;
    float weight = 1.0;
    float w = 0.0;
    float ws = 0.0;
#define DRAG_MULT 0.048
    for(int i=0;i<iterations;i++)
	{
        vec2 p = vec2(sin(iter), cos(iter));
        vec2 res = wavedx(position, p, speed, phase, t);
        position += p * res.y * weight * DRAG_MULT;
        w += res.x * weight;
        iter += 12.0;
        ws += weight;
        weight = mix(weight, 0.0, 0.2);
        phase *= 1.18;
        speed *= 1.07;
    }
    return w / ws;
}

vec3 normalPlane(vec2 uv, out vec3 normalMap)
{    
	int iter = 20;
	float T = texgen_params.w;
  
	const int textureOffset = 1;
    float p  = getwaves(uv,iter,T);
    float h1 = getwaves(uv +  vec2(textureOffset, 0) / 256.0, iter, T);
    float v1 = getwaves(uv +  vec2(0, textureOffset) / 256.0, iter, T);
   	
	normalMap.xy = (p - vec2(h1, v1)) * 100.0;
	normalMap.xy = -normalMap.xy;
	normalMap.z = 1.0;
    
    vec3 normal;
    mat3 TBN = construct_tbn(uv, f_coord.xyz);
    
    normal= TBN * normalMap;
    normal=normalize(normal);
    return normal;
}

vec3 getwaves_directions[] = vec3[](
  vec3(0.7900, 0.5533, -0.2640),
  vec3(-0.9075, 0.1099, 0.4055),
  vec3(0.7029, -0.5427, 0.4598),
  vec3(-0.1990, -0.7706, -0.6054),
  vec3(-0.8966, 0.2679, -0.3526),
  vec3(-0.1806, 0.4303, -0.8844),
  vec3(-0.0061, 0.8324, -0.5542),
  vec3(0.5143, -0.6805, 0.5219),
  vec3(-0.5450, 0.7928, 0.2727),
  vec3(0.5874, -0.7927, -0.1632),
  vec3(0.4356, -0.1174, 0.8925),
  vec3(-0.2174, 0.1649, -0.9621),
  vec3(-0.5134, -0.0137, -0.8581),
  vec3(-0.3361, -0.1214, 0.9340),
  vec3(0.6320, -0.4675, -0.6181),
  vec3(0.2561, 0.1685, -0.9519),
  vec3(0.7354, 0.6738, 0.0716),
  vec3(-0.0798, 0.9033, -0.4215),
  vec3(-0.1344, -0.6286, -0.7660),
  vec3(0.4724, 0.6836, 0.5564),
  vec3(-0.5242, -0.6188, 0.5851),
  vec3(0.0763, 0.0929, -0.9927),
  vec3(-0.9852, -0.1562, -0.0712),
  vec3(-0.2936, -0.7704, 0.5660),
  vec3(-0.4166, -0.7558, 0.5051),
  vec3(0.5641, -0.1422, 0.8134),
  vec3(-0.1560, 0.3815, -0.9111)
);

float getwaves2(vec3 position)
{
    int iters = getwaves_directions.length();
    float result = 0.0;
    for(int i=0;i<iters;i++) {
        float x = dot(getwaves_directions[i], position);
      
        float wave = exp(sin(x) - 1.0);
        float dx = wave * cos(x);
      
        result += wave;
        position += getwaves_directions[i] * dx * -0.7;
    }
    return result / float(iters);
}

// 3D simplex noise adapted from https://www.shadertoy.com/view/Ws23RD
// * Removed gradient normalization

vec4 mod289(vec4 x)
{
    return x - floor(x / 289.0) * 289.0;
}

vec4 permute(vec4 x)
{
    return mod289((x * 34.0 + 1.0) * x);
}

vec4 snoise(vec3 v)
{
    const vec2 C = vec2(1.0 / 6.0, 1.0 / 3.0);

    // First corner
    vec3 i  = floor(v + dot(v, vec3(C.y)));
    vec3 x0 = v   - i + dot(i, vec3(C.x));

    // Other corners
    vec3 g = step(x0.yzx, x0.xyz);
    vec3 l = 1.0 - g;
    vec3 i1 = min(g.xyz, l.zxy);
    vec3 i2 = max(g.xyz, l.zxy);

    vec3 x1 = x0 - i1 + C.x;
    vec3 x2 = x0 - i2 + C.y;
    vec3 x3 = x0 - 0.5;

    // Permutations
    vec4 p =
      permute(permute(permute(i.z + vec4(0.0, i1.z, i2.z, 1.0))
                            + i.y + vec4(0.0, i1.y, i2.y, 1.0))
                            + i.x + vec4(0.0, i1.x, i2.x, 1.0));

    // Gradients: 7x7 points over a square, mapped onto an octahedron.
    // The ring size 17*17 = 289 is close to a multiple of 49 (49*6 = 294)
    vec4 j = p - 49.0 * floor(p / 49.0);  // mod(p,7*7)

    vec4 x_ = floor(j / 7.0);
    vec4 y_ = floor(j - 7.0 * x_); 

    vec4 x = (x_ * 2.0 + 0.5) / 7.0 - 1.0;
    vec4 y = (y_ * 2.0 + 0.5) / 7.0 - 1.0;

    vec4 h = 1.0 - abs(x) - abs(y);

    vec4 b0 = vec4(x.xy, y.xy);
    vec4 b1 = vec4(x.zw, y.zw);

    vec4 s0 = floor(b0) * 2.0 + 1.0;
    vec4 s1 = floor(b1) * 2.0 + 1.0;
    vec4 sh = -step(h, vec4(0.0));

    vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
    vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;

    vec3 g0 = vec3(a0.xy, h.x);
    vec3 g1 = vec3(a0.zw, h.y);
    vec3 g2 = vec3(a1.xy, h.z);
    vec3 g3 = vec3(a1.zw, h.w);

    // Compute noise and gradient at P
    vec4 m = max(0.6 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
    vec4 m2 = m * m;
    vec4 m3 = m2 * m;
    vec4 m4 = m2 * m2;
    vec3 grad =
      -6.0 * m3.x * x0 * dot(x0, g0) + m4.x * g0 +
      -6.0 * m3.y * x1 * dot(x1, g1) + m4.y * g1 +
      -6.0 * m3.z * x2 * dot(x2, g2) + m4.z * g2 +
      -6.0 * m3.w * x3 * dot(x3, g3) + m4.w * g3;
    vec4 px = vec4(dot(x0, g0), dot(x1, g1), dot(x2, g2), dot(x3, g3));
    return 42.0 * vec4(grad, dot(m4, px));
}

float water_caustics(vec3 pos)
{
    vec4 n = snoise( pos );

    pos -= 0.07*n.xyz;
    pos *= 1.62;
    n = snoise( pos );

    pos -= 0.07*n.xyz;
    n = snoise( pos );

    pos -= 0.07*n.xyz;
    n = snoise( pos );
    return n.w;
}

void sample_color(vec2 uv, float mipBias, inout vec4 sampled_color, inout vec4 emissive)
{
	if (tex_mode == TEX_MODE_TEST || geoMode <= 3)
	{
		sampled_color = fillColor;
		emissive = vec4(0.0);
    }
    else if (tex_mode == TEX_MODE_16BPP || tex_mode == TEX_MODE_BILINEAR_16BPP)
    {
		// todo: can probably do this swap on upload...
		vec4 sampled = texture(tex, uv.xy, mipBias);
        sampled_color = vec4(sampled.b, sampled.g, sampled.r, sampled.a);
		
		emissive.rgb += sampled_color.rgb * f_light;

		vec4 sampledEmiss = texture(texEmiss, uv.xy, mipBias);
		emissive.rgb += sampledEmiss.bgr * emissiveFactor.rgb;
    }
    else if (tex_mode == TEX_MODE_WORLDPAL
#ifndef CAN_BILINEAR_FILTER
    || tex_mode == TEX_MODE_BILINEAR
#endif
    )
    {
		float index = texture(tex, uv.xy, mipBias).r;
#ifdef ALPHA_DISCARD
        if (index == 0.0)
            discard;
#endif
		// Makes sure light is in a sane range
        float light = clamp(f_light, 0.0, 1.0);
        float light_worldpalidx = texture(worldPaletteLights, vec2(index, light)).r;
        vec4 lightPalval = texture(worldPalette, vec2(light_worldpalidx, 0.5));

		emissive = lightPalval;
        sampled_color = texture(worldPalette, vec2(index, 0.5));
    }
#ifdef CAN_BILINEAR_FILTER
    else if (tex_mode == TEX_MODE_BILINEAR)
    {
        bilinear_paletted(uv.xy, sampled_color, emissive);
	#ifdef ALPHA_DISCARD
        if (sampled_color.a < 0.01) {
            discard;
        }
	#endif
    }
#endif
}

float HenyeyGreenstein(float g, float costh)
{
    return (1.0 - g * g) / (4.0 * 3.141592 * pow(1.0 + g * g - 2.0 * g * costh, 3.0/2.0));
}

float Schlick(float k, float costh)
{
    return (1.0 - k * k) / (4.0 * 3.141592 * pow(1.0 - k * costh, 2.0));
}

float InScatterDir(vec3 q, vec3 dir, float d)
{
   float b = dot(dir, q);
   float c = dot(q, q) + 0.0005;
   float s = 1.0 / max(0.001,sqrt(c - b*b));
   float x = d*s;
   float y = b*s;
   float l = s * atan(x/max(0.001,1.+(x+y)*y));
   return l;
}

float InScatter(vec3 start, vec3 dir, vec3 lightPos, float d)
{
   vec3 q = start - lightPos;
   return InScatterDir(q, dir, d);
}


#define USE_VM

void calc_lod_bias()
{
	lodbias = min(compute_mip_bias(f_coord.y), float(numMips - 1));
}

void calc_fog()
{
	fog = 0;
#ifdef FOG
#ifndef REFRACTION // tmp, fixme
	if(fogEnabled > 0)
	{
		vec3 viewDir = normalize(-f_coord.xyz);
	
		float clipDepth = texelFetch(cliptex, ivec2(gl_FragCoord.xy), 0).r;

		float distToCam = max(0.0, f_depth * 128.0 - clipDepth) / -viewDir.y;
		float fog_amount = clamp((distToCam - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
	
		vec3 fog_color = fogColor.rgb;
	
		// fog light scatter
		if(fogLightDir.w > 0.0)
		{
			float k = fogAnisotropy;
			float c = 1.0 - k * dot(fogLightDir.xyz, viewDir.xyz);
			fog_color *= (1.0 - k * k) / (c * c);
			fog_color /= (1.0 + fog_color);
		}
	
		fog = pack_argb8888(vec4(fog_color.rgb, fog_amount * fogColor.a) * 255.0);
	}
#endif
#endif
}
float GGX_V1(in float m2, in float nDotX)
{
    return 1.0f / (nDotX + sqrt(m2 + (1 - m2) * nDotX * nDotX));
}
vec3 calc_ambient_specular(float roughness, vec3 normal, vec3 view, vec3 reflected)
{
	//return CalculateAmbientSpecular(normal, view, roughness, vec3(1.0));

	vec3 ambientSpec = vec3(0.0);
	
	float m = roughness * roughness;
	float m2 = max(m * m, 1e-5);
	float amplitude = 1.0 / (3.141592 * m2);
	float sharpness = (2.0 / m2) / (4.0 * max(dot(normal, view), 0.1));
	
	for(int sg = 0; sg < ambientNumSG; ++sg)
	{
		vec4 sgCol = ambientSG[sg];//unpack_argb2101010(ambientSG[sg]);
		vec3 ambientColor = mix(f_color[0].rgb, sgCol.xyz, sgCol.w); // use vertex color if no ambientSG data
	
		float umLength = length(sharpness * reflected + (ambientSGBasis[sg].w * ambientSGBasis[sg].xyz));
		float attenuation = 1.0 - exp(-2.0 * umLength);
		float nDotL = clamp(dot(normal.xyz, reflected), 0.0, 1.0);
	
		// todo: can we roll this into something like the direct light approx?
		float D = (2.0 * 3.141592) * nDotL * attenuation / umLength;
		//float D = (2.0) * nDotL * attenuation / umLength;
	
		float expo = (exp(umLength - sharpness - ambientSGBasis[sg].w) * amplitude);

		// fresnel approx as 1 / ldoth at center of the warped lobe
		//vec3 h = normalize(reflected + view);
		//D /= clamp(dot(reflected, h), 0.1, 1.0);

		ambientSpec = (D * expo) * ambientColor + ambientSpec;
	}

	return ambientSpec;
}



// packs light results into color0 and color1
void calc_light()
{
	vec3 diffuse = f_color[0].rgb;
	vec3 specular = vec3(0.0);//f_color[1].rgb;

	v[0]  = 0;//uvec2(0);
	v[1]  = 0;//uvec2(0);

	vec3 view = normalize(-f_coord.xyz);
	vec3 normal = normalize(f_normal.xyz);
	vec3 reflected = reflect(-view, normal);
	
	float roughness = roughnessFactor;
	//roughness = texture(textures[0], f_uv[0].xy, lodbias).r;

	//float blurred = tex_6x6(textures[0], f_uv[0].xy).r;
	//roughness = 1.-clamp((roughness - blurred * 0.85) * 4.0, 0.0, 1.0);
	//roughness *= roughness;
	//roughness *= roughness;
	//roughness = roughness * 0.9 + 0.1;
	//roughness = max(roughness, 0.05);

	//vec4 bump = bumpFromDepth(textures[0], f_uv[0].xy);
	//mat3 tbn = construct_tbn(f_uv[0].xy / f_uv[0].w, f_coord.xyz);
	//normal = tbn * bump.xyz;


#ifdef UNLIT
	if (lightMode == 0) // fully lit
	{
		v[0] = packColorRegister(vec4(light_mult*255.0));
	#ifdef SPECULAR
		v[1] = packColorRegister(vec4(light_mult*255.0));
	#endif // SPECULAR
	}
	else if (lightMode == 1) // not lit
	{
		v[0] = packColorRegister(vec4(0.0, 0.0, 0.0, f_color[0].w * 255.0));
	#ifdef SPECULAR
		v[1] = packColorRegister(vec4(0.0));
	#endif // SPECULAR
	}
	else // "diffuse"/vertex lit
	{
		v[0] = packColorRegister(f_color[0] * 255.0);
	#ifdef SPECULAR
		v[1] = packColorRegister(f_color[1] * 255.0);
		//v[1] = packColorRegister(vec4(calc_ambient_specular(roughness, normal, view, reflected) * 255.0, 255.0));
	#endif // SPECULAR
	}

#else // UNLIT

	light_result result;
	result.diffuse = f_color[0].rgb;// * f_color[0].rgb;
	
	//#ifdef SPECULAR
	//	//result.specular = calc_ambient_specular(roughness, normal, view, reflected);
	//	//result.specular = f_color[1].rgb;// * f_color[1].rgb;
	//	//result.specular = vec3(0.0);
	//#else
		result.specular = f_color[1].rgb;//vec3(0.0);
	//#endif

#if 0// !defined(ALPHA_BLEND) && !defined(ALPHA_DISCARD)
	
	// read opaque lighting data
	ivec2 crd = ivec2(gl_FragCoord.xy);

	vec4 light = texelFetch(diffuseLightTex, crd, 0);
	//result.diffuse.rgb += light.rgb;
#ifdef SPECULAR
	//result.specular.rgb += vec3(light.a);
#endif // SPECULAR

	// edge-directed reconstruction:
	vec4 a0 = texelFetch(diffuseLightTex, crd + ivec2(1,0), 0);
	vec4 a1 = texelFetch(diffuseLightTex, crd - ivec2(1,0), 0);
	vec4 a2 = texelFetch(diffuseLightTex, crd + ivec2(0,1), 0);
	vec4 a3 = texelFetch(diffuseLightTex, crd - ivec2(0,1), 0);		

	vec2 chroma;
	chroma.x = edge_filter(light.rg, a0.rg, a1.rg, a2.rg, a3.rg);
	chroma.y = edge_filter(light.ba, a0.ba, a1.ba, a2.ba, a3.ba);

	bool pattern = ((crd.x & 1) == (crd.y & 1));

	vec3 col = light.rgg;
	col.b = chroma.x;
	col.rgb = (pattern) ? col.rgb : col.rbg;
	col.rgb = ycocg2rgb(col.rgb);

	result.diffuse.rgb += col.rgb;
//#ifdef SPECULAR
	col = light.baa;
	col.b = chroma.y;
	col.rgb = (pattern) ? col.rgb : col.rbg;
	col.rgb = ycocg2rgb(col.rgb);

	//col.rgb = log2(col.rgb) / -1.0;

	result.specular.rgb += col.rgb;
//#endif // SPECULAR


#else
	uint cluster = compute_cluster_index(gl_FragCoord.xy, f_coord.y);

	float a = roughness;// * roughness;

	light_input params;
	params.pos = f_coord.xyz;
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
	params.tint = pack_argb8888(f_color[0] * 255.0);

	// light mode "gouraud" (a.k.a new per pixel)
	if (lightMode >= 3)
	{
//#ifdef SPECULAR
		result.specular = calc_ambient_specular(roughness, normal, view, reflected);
//#endif

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
				while (bucket_bits != 0u)
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
	}

	// loop shadow occluders
	if ((aoFlags & 0x1) == 0x1 && numOccluders > 0u)
	{		
//#if !defined(ALPHA_BLEND) && !defined(ALPHA_DISCARD)
		float shadow = 1.0;
		
		uint first_item = firstOccluder;
		uint last_item = first_item + numOccluders - 1u;
		uint first_bucket = first_item >> 5u;
		uint last_bucket = min(last_item >> 5u, max(0u, CLUSTER_BUCKETS_PER_CLUSTER - 1u));
		for (uint bucket = first_bucket; bucket <= last_bucket; ++bucket)
		{
			uint bucket_bits = uint(texelFetch(clusterBuffer, int(cluster * CLUSTER_BUCKETS_PER_CLUSTER + bucket)).x);
			while (bucket_bits != 0u)
			{
				uint bucket_bit_index = findLSB_unsafe(bucket_bits);
				uint occluder_index = (bucket << 5u) + bucket_bit_index;
				bucket_bits ^= (1u << bucket_bit_index);
			
				if (occluder_index >= first_item && occluder_index <= last_item)
				{
					calc_shadow(shadow, occluder_index - first_item, f_coord.xyz, params.normal);
				}
				else if (occluder_index > last_item)
				{
					bucket = CLUSTER_BUCKETS_PER_CLUSTER;
					break;
				}
			}
		}	
			
		float ao = shadow * 0.8 + 0.2; // remap so we don't overdarken
		result.diffuse.xyz *= ao;

		//float ndotv = dot(normal.xyz, view.xyz);
		//float specAO = mix(ao * ao, 1.0, clamp(-0.3 * ndotv * ndotv, 0.0, 1.0));
		result.specular.xyz *= ao;//specAO;
	#endif
	}
//#endif // !defined(ALPHA_BLEND) && !defined(ALPHA_DISCARD)

	//#ifndef SPECULAR
		// dielectric
		result.specular.rgb *= specularFactor.rgb;
		result.diffuse.rgb *= 1.0 - specularFactor.rgb;
		//vec3 fresnel = env_brdf( specularFactor.rgb, roughness, clamp(dot(normal.xyz, view.xyz),0.0,1.0) );
		//result.specular.rgb *= fresnel;
		//result.diffuse.rgb *= 1.0 - fresnel;
	//#endif
	
	// gamma hack
	// Unity "Optimizing PBR"
	// https://community.arm.com/cfs-file/__key/communityserver-blogs-components-weblogfiles/00-00-00-20-66/siggraph2015_2D00_mmg_2D00_renaldas_2D00_slides.pdf
	//result.specular.rgb = sqrt(result.specular.rgb) * specularFactor.rgb;
	
	//result.diffuse.rgb = sqrt(result.diffuse.rgb);

	// compress
	//result.specular.rgb = exp2(-1.0 * result.specular.rgb);

	//result.diffuse.rgb = result.diffuse.rgb / (1.0 + result.diffuse.rgb);
	//result.specular.rgb = result.specular.rgb / (1.0 + result.specular.rgb);
	result.specular.rgb = 1.0 - exp2(-result.specular.rgb);

	//result.diffuse.rgb = clamp(result.diffuse.rgb, vec3(0.0), vec3(1.0));
	//result.specular.rgb = clamp(result.specular.rgb, vec3(0.0), vec3(1.0));
	
	//result.specular.rgb = sqrt(result.specular.rgb);

	v[0] = packColorRegister(vec4(result.diffuse.rgb * 255.0, f_color[0].w * 255.0));
	//#ifdef SPECULAR
		v[1] = packColorRegister(vec4(result.specular.rgb * 255.0, f_color[1].w * 255.0));
	//#endif // SPECULAR

	// temp hack until material and texture assignments are moved to higher level
	#ifdef SPECULAR
		// metal
		//v[0] = v[1];
		//v[1] = 0;//uvec2(0);
	#endif

#endif // UNLIT
}

// todo: do we want to have per-decal shaders? the vm cost could skyrocket..
void calc_decals()
{
	vec4 color, glow;
	color = glow = vec4(0,0,0,1);
//#if !(defined(ALPHA_BLEND) || defined(ALPHA_DISCARD))

	uint cluster_index = compute_cluster_index(gl_FragCoord.xy, f_coord.y);
	uint bucket_index = cluster_index * CLUSTER_BUCKETS_PER_CLUSTER;
	if(numDecals > 0u)
	{
		uint first_item = firstDecal;
		uint last_item = first_item + numDecals - 1u;
		uint first_bucket = first_item >> 5u;
		uint last_bucket = min(last_item >> 5u, max(0u, CLUSTER_BUCKETS_PER_CLUSTER - 1u));
		for (uint bucket = first_bucket; bucket <= last_bucket; ++bucket)
		{
			uint bucket_bits = uint(texelFetch(clusterBuffer, int(bucket_index + bucket)).x);
			while(bucket_bits != 0u)
			{
				uint bucket_bit_index = findLSB_unsafe(bucket_bits);
				uint decal_index = (bucket << 5u) + bucket_bit_index;
				bucket_bits ^= (1u << bucket_bit_index);
			
				if (decal_index >= first_item && decal_index <= last_item)
				{
					decal dec = decals[decal_index - first_item];

					vec4 objectPosition = dec.invDecalMatrix * vec4(f_coord.xyz, 1.0);				
					vec3 falloff = 0.5f - abs(objectPosition.xyz);
					if( any(lessThanEqual(falloff, vec3(0.0))) )
						continue;
				
					vec2 decalTexCoord = objectPosition.xz + 0.5;
					decalTexCoord = decalTexCoord.xy * dec.uvScaleBias.zw + dec.uvScaleBias.xy;
				
					vec4 decalColor = textureLod(decalAtlas, decalTexCoord, 0);
					if((dec.flags & 0x8u) == 0x8u) // rgb as alpha
						decalColor.a = max(decalColor.r, max(decalColor.g, decalColor.b));

					if(decalColor.a < 0.001)
						continue;
				
					decalColor.rgb *= dec.color.rgb;
				
					if((dec.flags & 0x2u) == 0x2u) // heat
					{
						decalColor.rgb = 4.0 * textureLod(blackbodyTex, vec2(decalColor.r, 0.0), 0.0).rgb;
						//decalColor.rgb = blackbody(decalColor.r);
						glow.rgb += decalColor.rgb;
					}
				
					float edgeFactor = clamp(abs(objectPosition.z), 0.0, 1.0);
					edgeFactor *= edgeFactor;
					edgeFactor *= edgeFactor;
					edgeFactor *= edgeFactor;

					float edgeBlend = 1.0 - edgeFactor;
					if((dec.flags & 0x4u) == 0x4u) // additive
					{
						color.rgb += edgeBlend * decalColor.rgb;
					}
					else
					{
						color.rgb = color.rgb * (1.0 - decalColor.w) + decalColor.rgb * decalColor.w;
						color.w *= 1.0 - decalColor.w;
					}
				}
				else if (decal_index > last_item)
				{
					bucket = CLUSTER_BUCKETS_PER_CLUSTER;
					break;
				}
			}
		}
	}
//#endif
	d[0] = packRegister(color * 255.0);
	d[1] = packRegister(glow * 255.0);
}

void main(void)
{
#ifdef USE_VM
	mat3 tbn = construct_tbn(f_uv[0].xy / f_uv[0].w, f_coord.xyz);
	vec3 viewTS = transpose(tbn) * normalize(-f_coord.xyz);
	vdir = encodeHemiUnitVector(viewTS);
	
	// setup texcoord registers, from here out don't directly access f_uv
	// todo: fix texgen
	tr[0] = packTexcoordRegister(do_texgen(f_uv[0].xyz / f_uv[0].w).xy);
	tr[1] = packTexcoordRegister(f_uv[1].xy / f_uv[1].w);
	tr[2] = packTexcoordRegister(f_uv[2].xy / f_uv[2].w);
	tr[3] = packTexcoordRegister(f_uv[3].xy / f_uv[3].w);

	// calculate anything that relies on interpolated data
	// up front so we can free vgprs for the vm stages
	// the results are compacted and stored for later
	calc_lod_bias();
	calc_fog();
	calc_light();
	calc_decals();

	// run the texture stage
	// todo: how to handle decals?
	run_tex_stage();

	// sandwhich lighting between the texture stage and the combiner
	// this writes out the color0 and color1 registers
	// todo: there's interpolated data coming in here we can probably pack
	// todo: use the texture data in the lighting? adds overhead..
	//calc_light();

	// run the combiner stage
	run_combiner_stage();

	// unpack output registers
	fragColor = vec4(unpackRegister(r[0])); // unpack r0
	fragGlow = vec4(unpackRegister(r[1])); // unpack r1
	//fragGlow.rgb += unpackColorRegister(v[5]).rgb;

	vec4 decalDiffuse = unpackRegister(d[0]);
	float decalAlpha = decalDiffuse.w * (1.0/255.0);
	fragColor.rgb = fragColor.rgb * decalAlpha + decalDiffuse.rgb;
	//fragGlow.rgb += unpackRegister(d[1]).rgb;

	// apply overbright
	fragColor.rgb *= 1.0 / light_mult;

	// apply glow multiplier
	fragGlow.rgb *= emissiveFactor.w;

	uvec2 crd = uvec2(gl_FragCoord.xy);
	float dither = dither_value_float(crd);// texelFetch(dithertex, ivec2(gl_FragCoord.xy) & ivec2(3), 0).r;
	float scale = 1.0 / float((1 << int(8)) - 1);
	dither *= scale;

#ifdef FOG
	// apply fog to outputs
	if(fogEnabled > 0)
	{
		vec4 fog_color = unpack_argb8888(fog);
		fog_color.w *= (1.0/255.0);

		fragColor.rgb = mix(fragColor.rgb, fog_color.rgb, fog_color.w);
		fragGlow.rgb = fragGlow.rgb * (1.0 - fog_color.w);
	}
#endif

	// todo: make ditherMode a per-rt thing
	// dither the output if needed
	fragColor.rgb = min(fragColor.rgb * (1.0/255.0), vec3(1.0));
	fragColor.a *= (1.0 / 255.0);

#ifdef REFRACTION
	vec3 localViewDir = normalize(-f_coord.xyz);
	vec2 screenUV = gl_FragCoord.xy / iResolution.xy;
	//float sceneDepth = textureLod(ztex, screenUV, 0).r;
	float softIntersect = 1.0;//clamp((sceneDepth - f_depth) / -localViewDir.y * 500.0, 0.0, 1.0);	
	
	vec2 disp = unpackRegister(r[5]).xy - 0.5;
	disp *= 1.0/255.0 * min(0.05, 0.0001 / f_depth);// * softIntersect;

	vec2 refrUV = screenUV + disp;
		
	float refrDepth = textureLod(ztex, refrUV, 0).r;
	if (refrDepth < f_depth)
	{
		refrUV = screenUV;
		refrDepth = textureLod(ztex, screenUV, 0).r;
	}

	float waterStart = f_depth * 128.0;
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
	refr = clamp(tint_delta * refr + refr + dither, vec3(0.0), vec3(1.0));

	float texalpha = 90.0 / 255.0;

	float alpha = fragColor.w;// + (1.0-softIntersect);
	fragColor.rgb = fragColor.rgb * alpha + refr.rgb;// mix(fragColor.rgb, refr.rgb, alpha);
	fragColor.w = 1.0;


	//fragColor.rgb = vec3(alpha);
#endif

	fragColor.rg = subsample(fragColor).rg;

	// alpha testing
#ifdef ALPHA_DISCARD
    if (fragColor.a < 0.01) // todo: alpha test value
		discard;
#endif

//#if defined(ALPHA_BLEND) || defined(ALPHA_DISCARD)
	// todo: this is killing early-z for all the passes...
	float clipDepth = texelFetch(cliptex, ivec2(gl_FragCoord.xy), 0).r;
	if (clipDepth >= f_depth)
		discard;
//#endif

//#ifdef ALPHA_BLEND
	//if (fragColor.a - dither * 255.0/16.0 < 0.0)
	//	discard;
//#endif

	// note we subtract instead of add to avoid boosting blacks
	fragGlow.rgb = min(fragGlow.rgb * (1.0/255.0) + (-dither), vec3(1.0));

#else
	vec2 screenUV = gl_FragCoord.xy / iResolution.xy;

	const float DITHER_LUT[16] = float[16](
			0, 4, 1, 5,
			6, 2, 7, 3,
			1, 5, 0, 4,
			7, 3, 6, 2
	);	
	int wrap_x = int(gl_FragCoord.x) & 3;
	int wrap_y = int(gl_FragCoord.y) & 3;
	int wrap_index = wrap_x + wrap_y * 4;
	float dither = DITHER_LUT[wrap_index] / 255.0;
   
	vec3 adj_texcoords = f_uv[0].xyz / f_uv[0].w;
	vec3 viewPos = f_coord.xyz;

	// software actually uses the zmin of the entire face
	float mipBias = compute_mip_bias(viewPos.y);
	mipBias = min(mipBias, float(numMips - 1));

	mat3 tbn = construct_tbn(adj_texcoords.xy, f_coord.xyz);

    if(displacement_factor != 0.0)
	{
        adj_texcoords.xy = parallax_mapping(adj_texcoords.xy, tbn);
    }
	else if(uv_mode == 0)
	{
		adj_texcoords.xy = f_uv_affine;
	}
	
	adj_texcoords.xyz = do_texgen(adj_texcoords, viewPos);

	//if(texgen == 1) // 1 = RD_TEXGEN_HORIZON
	//{
	//	mat4 invMat = inverse(modelMatrix);
	//	vec3 worldPos = mat3(invMat) * viewPos;
	//	//worldPos = normalize(worldPos);// * vec3(1.0, 1.0, 2.0));
	//
	//	//adj_texcoords.xy = worldPos.xy / (worldPos.z + 1.0) * 0.5 + 0.5;
	//
	//	vec3 R = worldPos;
	//	float r = length(R);
    //    float c = -R.z / r;
    //    float theta = acos(c);
    //    float phi = atan(R.x, -R.y);
	//	  
	//	adj_texcoords.x = 0.5 + phi / 6.2831852;
	//	adj_texcoords.y = 1.0 - theta / 3.1415926;
	//	adj_texcoords.xy *= vec2(4.0, 2.0);// * vec2(texsize.x / texsize.y, 1.0);
	//	
  	//	float seamWidth = 1.0 / texsize.x;
    //    float seam = max (0.0, 1.0 - abs (R.x / r) / seamWidth) * clamp (1.0 + (R.z / r) / seamWidth, 0.0, 1.0);
	//	mipBias -= 2.0 * log2(1.0 + c * c) -12.3 * seam;
	//}

	vec3 surfaceNormals = normalize(f_normal.xyz);
	vec3 localViewDir = normalize(-viewPos.xyz);
	float ndotv = dot(surfaceNormals.xyz, localViewDir.xyz);
	
	//vec3 proc_texcoords = f_uv_nooffset.xyz / f_uv_nooffset.w;
#ifndef ALPHA_BLEND
	//if (texgen == 3)
	//{
	//	//vec3 normalMap;
	//	//surfaceNormals = normalPlane(adj_texcoords.xy, normalMap);
	//	//adj_texcoords.xy += normalMap.xy * 0.05;
	//
	//	float p = getwaves(proc_texcoords.xy, 20, texgen_params.w);
	//	adj_texcoords.xy += (p-0.5) * 0.1;
	//}
#endif

    vec4 vertex_color = f_color[0];
	//vertex_color.rgb += sqrt(PurkinjeShift(vec3(1.0 / 255.0), 1.0));
	vec4 sampled_color = vec4(1.0, 1.0, 1.0, 1.0);
    vec4 emissive = vec4(0.0);

#ifdef REFRACTION
	vec4 blend_col = vec4(0.0);
	float disp = 0.0;
	if (texgen == 3)
	{
		float ndotv = dot(surfaceNormals.xyz, localViewDir.xyz);	
	
		float fresnel = pow(1.0 - max(ndotv, 0.0), 5.0);
		float lo = 0.27;//mix(0.4, 0.7, fresnel);
		float hi = 0.77;//mix(1.4, 1.1, fresnel);
	
	   // float p = getwaves(proc_texcoords.xy, 20, texgen_params.w);
		
		vec4 s0, s1, s2, s3;
		vec4 e0, e1, e2, e3;
		//sample_color( adj_texcoords.xy * 1.0, mipBias, s0, e0);
		//sample_color( adj_texcoords.yx * 0.5, mipBias, s1, e1);
		//sample_color( adj_texcoords.xy * 0.5, mipBias, s2, e2);
		//sample_color( adj_texcoords.yx * 0.25, mipBias, s3, e3);
		
		sample_color( adj_texcoords.xy *  1.0 + 0.2 * texgen_params.w, mipBias, s0, e0);
		sample_color( adj_texcoords.xy *  0.5 - 0.1 * texgen_params.w, mipBias, s1, e1);
		sample_color( adj_texcoords.xy *  0.5 + 0.2 * texgen_params.w, mipBias, s2, e2);
		sample_color( adj_texcoords.xy * 0.25 - 0.1 * texgen_params.w, mipBias, s3, e3);

		//s0.rgb /= max(vec3(0.01), fillColor.rgb);
		//s1.rgb /= max(vec3(0.01), fillColor.rgb);
		//s2.rgb /= max(vec3(0.01), fillColor.rgb);
		//s3.rgb /= max(vec3(0.01), fillColor.rgb);

		float p = dot(s0.rgb + s1.rgb, vec3(0.333)) * 0.5;

		float alpha = 0.5;// dot(sampled_color.rgb, vec3(0.333));
		if(p > lo)
			alpha = 0.0;
        
		//float h = getwaves(proc_texcoords.xy * 0.6, 10, texgen_params.w);
		float h = dot(s2.rgb + s3.rgb, vec3(0.3333)) * 0.5;
		if (p + h > hi)
			alpha = 0.8;

		//blend_col.rgb = (s0.rgb + s1.rgb + s2.rgb + s3.rgb) * 0.25;

		disp = (s0.y - fillColor.y) * 2.0;		        
		adj_texcoords.xy += disp * 0.5 * f_depth * 128.0;

		//sampled_color.rgb = (s0.rgb + s1.rgb) * 0.5;
		sampled_color.rgb = vec3(p);
		//sample_color(adj_texcoords.xy, mipBias, sampled_color, emissive);
		//sampled_color.rgb = sampled_color.rgb * p + h * fillColor.rgb;
		vertex_color.w = alpha;

		blend_col = (s0 + s1) * 0.5;
		//sample_color(adj_texcoords.xy, mipBias, blend_col, e0);
	}
	else
#endif
	{
		sample_color(adj_texcoords.xy, mipBias, sampled_color, emissive);
	}

#ifdef UNLIT
	if (lightMode == 0)
	{
		// temp hack, mat16 doesn't output fill correctly
		if (tex_mode == TEX_MODE_16BPP || tex_mode == TEX_MODE_BILINEAR_16BPP)
			emissive.xyz *= 0.75;
		else
			emissive.xyz *= fillColor.xyz * 0.5;
	}
#endif

    vec4 albedoFactor_copy = albedoFactor;

	uint cluster_index = compute_cluster_index(gl_FragCoord.xy, viewPos.y);
	uint bucket_index = cluster_index * CLUSTER_BUCKETS_PER_CLUSTER;

	vec3 diffuseColor = sampled_color.xyz;
	vec3 specularColor = vec3(0.0);
	float roughness = 0.05;

	vec3 diffuseLight = vertex_color.xyz;
	vec3 specularLight = vec3(0.0);

#ifndef UNLIT
	#ifdef SPECULAR
		//roughness = 0.2;

		if (texgen == 3)
		{
			roughness = 0.01;
		}
		else
		{
			// fill color is effectively an anti-metalness control
			vec3 avgAlbedo = fillColor.xyz;

			float avgLum = dot(fillColor.xyz, vec3(0.2126, 0.7152, 0.0722));

			//roughness = mix(0.5, 0.01, avgLum);		
			//avgLum = avgLum * 0.7 + 0.3;

			// blend to metal when dark
			diffuseColor = sampled_color.xyz * (1.0 - avgLum);//(1.0 - fillColor.xyz);//diffuseColor * avgAlbedo;// -> 0 * (1-avgAlbedo) + diffuseColor * avgAlbedo -> mix(vec3(0.0), diffuseColor, avgAlbedo)
		
			// blend to dielectric white when bright
			specularColor = mix(fillColor.xyz, (sampled_color.xyz * 0.95 + 0.05), avgLum);//sampled_color.xyz * fillColor.xyz;//vec3(1.0);//mix(sampled_color.xyz, vec3(0.2), avgAlbedo);
		
			// try to estimate some roughness variation from the texture
		//	float smoothness = max(sampled_color.r, max(sampled_color.g, sampled_color.b));
		//	roughness = mix(0.25, 0.05, min(sqrt(smoothness * 2.0), 1.0)); // don't get too rough or there's no point in using specular here
		//	
		//	// blend out really dark stuff to fill color with high roughness (ex strifle scopes)
		//	float threshold = 1.0 / (15.0 / 255.0);
		//	roughness = mix(0.05, roughness, min(smoothness * threshold, 1.0));
			//specularColor = mix(min(avgAlbedo * 2.0, vec3(1.0)), specularColor, min(smoothness * threshold, 1.0));
		}
	#endif

		// for metals do some hacky stuff to make them look a bit more view-dependent
		//vec3 reflVec = reflect(-localViewDir, surfaceNormals);
		//diffuseColor.xyz *= pow(max(dot(reflVec, surfaceNormals), 0.0), 8.0) * 0.5 + 0.5;
#endif

	//if (texgen == 3)
	//{
	//	diffuseColor = fillColor.xyz * sampled_color.xyz;
	//}

	if(numDecals > 0u)
		BlendDecals(diffuseColor.xyz, emissive.xyz, bucket_index, viewPos.xyz, surfaceNormals);

	vec3 fogLight = vec3(1.0);

#ifndef UNLIT
	#ifdef SPECULAR
		// let's make it happen cap'n
		specularLight.xyz += CalculateAmbientSpecular(surfaceNormals, localViewDir, roughness, specularColor.xyz);
	#endif
		
	vec4 shadows = vec4(0.0, 0.0, 0.0, 1.0);
	if ((aoFlags & 0x1) == 0x1 && numOccluders > 0u)
		shadows = CalculateIndirectShadows(bucket_index, viewPos.xyz, surfaceNormals);

	vec3 ao = vec3(shadows.w * 0.8 + 0.2); // remap so we don't overdarken
	
#ifndef ALPHA_BLEND
	if((aoFlags & 0x2) == 0x2)
	{
		float ssao = upsample_ssao(gl_FragCoord.xy, f_depth);
		ao *= ssao;
	}
#endif

	// fake multi bounce
	vec3 albedo = vertex_color.xyz * diffuseColor.xyz;
	vec3 a =  2.0404 * albedo - 0.3324;
	vec3 b = -4.7951 * albedo + 0.6417;
	vec3 c =  2.7552 * albedo + 0.6903;
	vec3 x = ao;
	ao = max(x, ((x * a + b) * x + c) * x);	// ao = x * x * x * a - x * x * b + x * c;

	if ((aoFlags & 0x4) == 0x4)
	{
		mat4 invView = inverse(viewMatrix);
		vec3 pos = (invView * vec4(viewPos.xyz, 1.0)).xyz;
		vec3 wn = mat3(invView) * surfaceNormals;
		
		//pos *= 4.0; // tiling
		pos += wn.xyz * 0.25 * timeSeconds; // animate
		
		//float w = mix(water_caustics(pos), water_caustics(pos + 1.), 0.5);
		//ao *= exp(w * 4.0 - 1.0) * 0.5 + 0.5;

		float w = pow(getwaves2(vec3(pos.xyz * 64.0)), 3.0);
		ao *= w + 0.75;
	}

	diffuseLight.xyz *= ao;

	vec3 specAO = mix(ao * ao, vec3(1.0), clamp(-0.3 * ndotv * ndotv, 0.0, 1.0));
	specularLight.xyz *= specAO;

	//if (texgen == 3)
	//{
	//	float fresnel = 0.04 + (1.0-0.04) * pow(1.0 - max(ndotv, 0.0), 5.0);
	//	vertex_color.w *= fresnel;
	//}
	
	if (numLights > 0u)
		CalculatePointLighting(bucket_index, lightMode, f_coord.xyz, surfaceNormals, localViewDir, tbn, adj_texcoords.xy, shadows, diffuseColor.xyz, specularColor.xyz, roughness, diffuseLight, specularLight);
	
	//diffuseLight.xyz = clamp(diffuseLight.xyz, vec3(0.0), vec3(1.0));	
	//specularLight.xyz = clamp(specularLight.xyz, vec3(0.0), vec3(1.0));	

	// taper off
//	specularLight.r = rolloff(specularLight.r, 1.0, 0.2);
//	specularLight.g = rolloff(specularLight.g, 1.0, 0.2);
//	specularLight.b = rolloff(specularLight.b, 1.0, 0.2);
//
//	diffuseLight.r = rolloff(diffuseLight.r, 1.0, 0.2);
//	diffuseLight.g = rolloff(diffuseLight.g, 1.0, 0.2);
//	diffuseLight.b = rolloff(diffuseLight.b, 1.0, 0.2);

	//diffuseColor = ao;//1.0);
	//diffuseLight = vec3(1.0);
#endif
	//diffuseLight.xyz = clamp(diffuseLight.xyz, vec3(0.0), vec3(2.0));	
	
	//diffuseLight.xyz *= diffuseColor.xyz;
	//specularLight.xyz *= specularColor.xyz;

	diffuseLight *= 1.0 / light_mult;

	//float imax = max( diffuseLight.r, max( diffuseLight.g, diffuseLight.b ) );
	//if(imax > 1.0)
	//	diffuseLight.rgb *= 1.0 / imax;

	//if (tex_mode == TEX_MODE_WORLDPAL
    //|| tex_mode == TEX_MODE_BILINEAR
    //)
    //{
	//	float index = texture(tex, adj_texcoords.xy, mipBias).r;
	//
    //    float r = texture(worldPaletteLights, vec2(index, min(diffuseLight.r + dither, 1.0))).r;
	//	float g = texture(worldPaletteLights, vec2(index, min(diffuseLight.g + dither, 1.0))).r;
	//	float b = texture(worldPaletteLights, vec2(index, min(diffuseLight.b + dither, 1.0))).r;
	//
    //    diffuseLight.r = texture(worldPalette, vec2(r, 0.5)).r;		
    //    diffuseLight.g = texture(worldPalette, vec2(g, 0.5)).g;
    //    diffuseLight.b = texture(worldPalette, vec2(b, 0.5)).b;
    //}
	//else
	{
	#ifdef REFRACTION
		blend_col.rgb = fillColor.rgb;
		blend_col.rgb *= diffuseLight.xyz;
		//blend_col.rgb = diffuseLight.xyz;
	#endif
		diffuseLight.xyz *= diffuseColor.xyz;
		//diffuseLight.xyz = (PurkinjeShift(diffuseLight.xyz, 1.0));

		// paletted light levels desaturate at low intensity
		// mimic that behavior here
		//float lum0 = luminance(diffuseLight.xyz);
		//diffuseLight.xyz *= diffuseColor.xyz;
		//
		//float lum = luminance(diffuseLight.xyz);
		//diffuseLight.xyz = mix(vec3(lum), diffuseLight.xyz, min(1.0, lum0 * lum0 * 4.0));
	}

	//emissive.rgb += max(vec3(0.0), diffuseLight.rgb - 1.0);

	// everything over 1.0 gets mapped back down

	//diffuseLight.r = rolloff(diffuseLight.r, 1.0, 0.2);
	//diffuseLight.g = rolloff(diffuseLight.g, 1.0, 0.2);
	//diffuseLight.b = rolloff(diffuseLight.b, 1.0, 0.2);

	vec4 main_color = (vec4(diffuseLight.xyz, vertex_color.w) + vec4(specularLight.xyz, 0.0));// * vec4(sampled_color.xyz, 1.0);   

    main_color *= albedoFactor_copy;
	
	// add specular to emissive output
	//emissive.rgb += max(vec3(0.0), specularLight.rgb - 1.0 - dither);

	main_color.rgb = max(main_color.rgb, emissive.rgb);

    float orig_alpha = main_color.a;

#ifdef ALPHA_BLEND
    if (texgen != 3 && main_color.a < 0.01 && emissive.r == 0.0 && emissive.g == 0.0 && emissive.b == 0.0)
	{
        discard;
    }
#endif

	
#ifdef REFRACTION
	//if (texgen == 3)
	{
		//float sceneDepth = texture(ztex, screenUV).r;
		float softIntersect = 1.0;//clamp((sceneDepth - f_depth) / -localViewDir.y * 500.0, 0.0, 1.0);
		
		disp *= min(0.05, 0.0001 / f_depth);// * softIntersect;

		vec2 refrUV = screenUV + disp;
		
		float refrDepth = texture(ztex, refrUV).r;
		if (refrDepth < f_depth)
		{
			refrUV = screenUV;
			//refrDepth = sceneDepth;
			refrDepth = texture(ztex, screenUV).r;
		}

		float waterStart = f_depth * 128.0;
		float waterEnd = refrDepth * 128.0;
		float waterDepth = (waterEnd - waterStart);// / -localViewDir.y;			
	
		//vec3 refr = vec3(0.0);// textureLod(refrtex, refrUV, min(waterDepth * 4.0, 6.0)).rgb;
		//		
		//float distToCam = max(0.0, f_depth * 128.0) / -localViewDir.y;
		//float fog_amount = 1.0 - exp(-distToCam * 2.5);
		//
		//float s = 0.0;
		//float w;
		//float l = fog_amount;//(waterDepth * 1.0); 
		//for (float i = 0.; i < 9.0; i++)
		//{
		//	w = pow(l, i);
		//	refr += w * textureLod(refrtex, refrUV, i).rgb;
		//	s += w;
		//}
		//refr = refr / s;
		
		vec3 refr = textureLod(refrtex, refrUV, 0).rgb;

		//if(refrDepth < 0.9999)
		if ((aoFlags & 0x4) != 0x4)
		{
			float waterFogAmount = clamp(waterDepth / (2.0), 0.0, 1.0);
			//vec3 waterFogColor = exp(-(1.0 / (texgen_params.rgb * texgen_params.rgb)) * (3.0 * waterDepth + 0.5));
			//float waterFogAmount = 1.0 - exp(-waterDepth * 2.0);

			vec3 waterFogColor = texgen_params.rgb;
		
		//	float ndotv = -dot(localViewDir, -surfaceNormals);
		//
		//	float k = -0.85;
		//	//waterFogColor *= (1.0 - k * k) / (pow(1.0 - k * -ndotv, 2.0)); // modified schlick phase
		//	//waterFogColor *= 8.0 * 3.0 / (16.0 * 3.141592) * (1.0 + ndotv * ndotv); // modified rayleigh phase
		//	//waterFogColor *= abs(ndotv) * 0.8 + 0.2;
		//
		//	vec3 fogLight = vec3(0.0);
		//	for (int i = 0; i < 8; ++i)
		//	{			
		//		//fogLight += ambientSG[i].xyz * InScatterDir(ambientSGBasis[i].xyz, localViewDir, waterDepth);
		//		float ldotv = dot(ambientSGBasis[i].xyz, localViewDir);
		//		float phase = (1.0 - k * k) / (4.0 * 3.141592 * pow(1.0 - k * ldotv, 2.0));
		//		fogLight += vec3(phase);
		//	}
		//	waterFogColor *= fogLight * blend_col.rgb;// * 0.128;

			vec3 half_color = waterFogColor.rgb * 0.5;
			waterFogColor = waterFogColor.rgb - (half_color.brr + half_color.ggb);

			if(fogLightDir.w > 0.0)
				waterFogColor *= 4.0*3.141592*Schlick(0.35, dot(fogLightDir.xyz, localViewDir));

			if ( any( greaterThan(waterFogColor, vec3(0.0)) ) )
				refr.rgb = mix(refr.rgb, waterFogColor.rgb, waterFogAmount);

			//vec3 waterFogColor = texgen_params.rgb * 0.5;
			//vec3 absorption = (1.0 / texgen_params.rgb);
			//waterFogColor = exp(-absorption * waterDepth);

			//vec3 absorbance = (1.0/waterFogColor) * -waterDepth;
			//vec3 transmittance = exp(absorbance * 0.1);
			//
			//refr.rgb = refr.rgb * transmittance + waterFogColor;

			vec3 extColor = exp(-waterDepth * vec3(3.0));
			vec3 insColor = exp(-waterDepth * vec3(1.0));
			//refr = refr * extColor + waterFogColor * (1.0 - insColor);

			//float waterFogAmount = 1.0 - exp(-waterDepth * 10.0);
			//refr.rgb = mix(refr.rgb, waterFogColor.rgb, waterFogAmount);
		}
		
		vec3 tint = texgen_params.rgb;
		vec3 half_tint = tint.rgb * 0.5;
		vec3 tint_delta = tint.rgb - (half_tint.brr + half_tint.ggb);
		refr.rgb = clamp(tint_delta.rgb * refr.rgb + refr.rgb, vec3(0.0), vec3(1.0));

		float texalpha = 90.0 / 255.0;//mix(90.0, 180.0, min(1.0, waterDepth * 0.1)) / 255.0;
		//refr.rgb = mix(refr.rgb, blend_col.rgb, texalpha);

		//refr.rgb = mix(refr.rgb, blend_col.rgb, (1.0 - texalpha) * softIntersect);

		float alpha = main_color.w;// + (1.0-softIntersect);
		main_color.rgb = main_color.rgb * alpha + refr.rgb;
		main_color.w = 1.0;
	}
#endif
	
	float clipDepth = texture(cliptex, screenUV).r;

#ifdef FOG
	if(fogEnabled > 0)
	{
		float distToCam = max(0.0, f_depth * 128.0 - clipDepth * 128.0) / -localViewDir.y;
		//float fog_amount = 1.0 - exp(-distToCam * 2.5);
		float fog_amount = clamp((distToCam - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
		fog_amount *= fogColor.a;

		//float k = -0.75;
		//for (int i = 0; i < 8; ++i)
		//{			
		//	//fogLight += ambientSG[i].xyz * InScatterDir(ambientSGBasis[i].xyz, localViewDir, waterDepth);
		//	float ldotv = dot(ambientSGBasis[i].xyz, localViewDir);
		//	float phase = (1.0 - k * k) / ( pow(1.0 - k * ldotv, 2.0));
		//	fogLight += vec3(phase);
		//}
		//
		//for (int i = 0; i < numLights; ++i)
		//{
		//	light l = lights[i];
		//
		//	//float atten = 1.0 / (l.falloffMin * l.direction_intensity.w);	
		//	//vec3 c = atten * ( l.position.xyz );
		//	//vec3 d = atten * viewPos.xyz;
		//	//
		//	//float u = dot( c, c ) + 1;
		//	//float v = -2.0 * dot( c, d );
		//	//float w =  dot( d, d );
		//	//float div = 1.0 / sqrt( 4 * u * w - v * v );	
		//	//		
		//	//vec2 atan_res = atan( vec2( v + 2 * w, v ) * div );
		//	//float inscatter = sqrt( w ) * 2.0 * ( atan_res.x - atan_res.y ) * div; 
		//
		//	vec3 eye = -l.position.xyz;
		//
		//	float mid = -dot(eye, -localViewDir);
		//	float root = sqrt(max(0,mid * mid + l.falloffMin * l.falloffMin - dot(eye, eye)));
		//
		//	float d = max(0, min(mid + root, length(viewPos.xyz)) - max(mid - root, 0));
		//
		//	float inscatter = InScatterDir(-l.position.xyz, -localViewDir, d);
		//	fogLight += l.color.xyz * l.direction_intensity.w * inscatter;
		//}

		vec3 fog_color = fogColor.rgb;// * fogLight / (4.0 * 3.141592);
		//vec3 half_color = fog_color.rgb * 0.5;
		//fog_color = fog_color.rgb - (half_color.brr + half_color.ggb);

		if(fogLightDir.w > 0.0)
			fog_color *= 4.0*3.141592*Schlick(fogAnisotropy, dot(fogLightDir.xyz, localViewDir));

		main_color.rgb = mix(main_color.rgb, fog_color.rgb+dither, fog_amount);
		emissive.rgb = mix(emissive.rgb, emissive.rgb, fog_amount);
	}
#endif



#ifdef ALPHA_DISCARD
    if (fragColor.a < 0.01) // todo: alpha test value
        discard;
#endif

//#if defined(ALPHA_BLEND) || defined(ALPHA_DISCARD)
	// todo: this is killing early-z for all the passes...
	//if (clipDepth >= f_depth)
		//discard;
//#endif

	// dither the output in case we're using some lower precision output
	fragColor.rgb = min(fragColor.rgb + dither, vec3(1.0));

	uvec2 crd = uvec2(gl_FragCoord.xy);
	fragColor.rg = ((crd.x & 1u) == (crd.y & 1u)) ? fragColor.gr : fragColor.gb;

#ifndef ALPHA_BLEND
    fragGlow = emissive * vec4(vec3(emissiveFactor.w), f_color[0].w);

	// always dither emissive since it gets bloomed
	// note we subtract instead of add to avoid boosting blacks
	fragGlow.rgb = min(fragGlow.rgb - dither, vec3(1.0));
#else
    // Dont include any windows or transparent objects in emissivity output
	fragGlow = vec4(0.0, 0.0, 0.0, fragColor.w);
#endif



#endif

}
