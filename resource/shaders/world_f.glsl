#include "uniforms.gli"
#include "clustering.gli"
#include "math.gli"
#include "lighting.gli"
#include "decals.gli"
#include "occluders.gli"

#define LIGHT_DIVISOR (3.0)

#define TEX_MODE_TEST 0
#define TEX_MODE_WORLDPAL 1
#define TEX_MODE_BILINEAR 2
#define TEX_MODE_16BPP 5
#define TEX_MODE_BILINEAR_16BPP 6

#define D3DBLEND_ONE             (2)
#define D3DBLEND_SRCALPHA        (5)
#define D3DBLEND_INVSRCALPHA     (6)

uniform sampler2D tex;
uniform sampler2D texEmiss;
uniform sampler2D worldPalette;
uniform sampler2D worldPaletteLights;
uniform sampler2D displacement_map;


uniform sampler2D ztex;
uniform sampler2D ssaotex;
uniform sampler2D refrtex;
uniform sampler2D cliptex;

in vec4 f_color;
in float f_light;
in vec4 f_uv;
in vec4 f_uv_nooffset;
in vec3 f_coord;
in vec3 f_normal;
in float f_depth;

noperspective in vec2 f_uv_affine;

uniform mat4 modelMatrix;
uniform mat4 projMatrix;
uniform mat4 viewMatrix;

uniform int  lightMode;
uniform int  geoMode;
uniform int  ditherMode;

uniform int aoFlags;
uniform vec3 ambientColor;
uniform vec4 ambientSH[3];
uniform vec3 ambientDominantDir;
uniform vec3 ambientSG[8];



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

vec2 do_ceiling_uv(inout vec3 viewPos)
{
	mat4 invMat = inverse(modelMatrix); // fixme: expensive + only works when model component is identity
	vec3 cam_pos   = (invMat * vec4(0, 0, 0, 1)).xyz;

	mat4 invView = inverse(viewMatrix);
	vec3 world_pos = (invView * vec4(viewPos.xyz, 1.0)).xyz;

	vec3 ray_dir = normalize(world_pos.xyz - cam_pos.xyz);
	vec3 view_ceiling = texgen_params.xyz;
	vec3 view_norm = vec3(0,0,-1);

	float tmp = 0.0;
	if (!ceiling_intersect(cam_pos, ray_dir, view_norm, view_ceiling.xyz, tmp))
		tmp = 1000.0;

    vec3 sky_pos = tmp * ray_dir + cam_pos;
	
	viewPos.y = sky_pos.y;

	vec2 uv = sky_pos.xy * 16.0;

	vec4 proj_sky = projMatrix * modelMatrix * vec4(sky_pos.xyz, 1.0);
	
	return (uv + uv_offset.xy) / texsize.xy;
}

vec2 do_horizon_uv()
{
	vec2 projXY = vec2(0.5,-0.5) * (gl_FragCoord.xy / iResolution.xy);
	projXY = projXY.xy * iResolution.xy * (texgen_params.x);// / gl_FragCoord.w);

	vec2 uv;
	uv.x = projXY.x * texgen_params.y + (projXY.y * -texgen_params.z);
	uv.y = projXY.y * texgen_params.y + (projXY.x *  texgen_params.z);
	
	return (uv + uv_offset.xy) / texsize.xy;
}

void do_texgen(inout vec3 uv, inout vec3 viewPos)
{
	if(texgen == 1) // 1 = RD_TEXGEN_HORIZON
	{
		uv.xy = do_horizon_uv();
		uv.z = 0.0;
	}
	else if(texgen == 2) // 2 = RD_TEXGEN_CEILING
	{
		uv.xy = do_ceiling_uv(viewPos);
		uv.z = 0;
	}
}




vec3 CalculateAmbientSpecular(vec3 normal, vec3 view, float roughness, vec3 f0)
{
	vec3 ambientSpecular = vec3(0.0);
	for(int sg = 0; sg < 8; ++sg)
	{
		SG ndf = DistributionTermSG(normal, roughness);
		SG warpedNDF = WarpDistributionSG(ndf, view);

		SG lightSG;
		lightSG.Amplitude = ambientSG[sg].xyz;
		lightSG.Axis = ambientSGBasis[sg].xyz;
		lightSG.Sharpness = ambientSGBasis[sg].w;

		float nDotL = clamp(dot(normal.xyz, warpedNDF.Axis.xyz), 0.0, 1.0);

		// NDF
		vec3 spec = SGInnerProduct(warpedNDF, lightSG);

		// no Geometry term

		// Fresnel
		//vec3 h = normalize(warpedNDF.Axis + view);
		vec3 f = f0;// + (1.0 - f0) * exp2(-8.35 * max(0.0, dot(warpedNDF.Axis, h)));
		//f *= clamp(dot(f0, vec3(333.0)), 0.0, 1.0); // fade out when spec is less than 0.1% albedo
		
		ambientSpecular.xyz = (spec * nDotL) * f + ambientSpecular.xyz;
	}
	return ambientSpecular;// * 3.141592;
}

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 fragColorEmiss;

vec3 normals(vec3 pos) {
    vec3 fdx = dFdx(pos);
    vec3 fdy = dFdy(pos);
    return normalize(cross(fdx, fdy));
}

mat3 construct_tbn(vec2 uv)
{
	// use face normals for this
    //vec3 n = normals(f_coord.xyz);

    vec3 dp1 = dFdx(f_coord.xyz);
    vec3 dp2 = dFdy(f_coord.xyz);
    vec2 duv1 = dFdx(uv.xy);
    vec2 duv2 = dFdy(uv.xy);

	vec3 n = normalize(cross(dp1, dp2));

    vec3 dp2perp = cross(dp2, n);
    vec3 dp1perp = cross(n, dp1);

    vec3 t = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 b = dp2perp * duv1.y + dp1perp * duv2.y;

    float invmax = inversesqrt(max(dot(t, t), dot(b, b)));
    return mat3(t * invmax, b * invmax, n);
}

vec2 parallax_mapping(vec2 tc)
{
    /*if (f_coord.x < 0.5) {
        return tc;
    }*/

	mat3 tbn = construct_tbn(tc);
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

float compute_mip_bias(float z_min)
{
	float mipmap_level = 0.0;
	mipmap_level = z_min < mipDistances.x ? mipmap_level : 1.0;
	mipmap_level = z_min < mipDistances.y ? mipmap_level : 2.0;
	mipmap_level = z_min < mipDistances.z ? mipmap_level : 3.0;

	// dither the mip level
	if(ditherMode == 1)
	{
		const mat4 bayerIndex = mat4(
			vec4(00.0/16.0, 12.0/16.0, 03.0/16.0, 15.0/16.0),
			vec4(08.0/16.0, 04.0/16.0, 11.0/16.0, 07.0/16.0),
			vec4(02.0/16.0, 14.0/16.0, 01.0/16.0, 13.0/16.0),
			vec4(10.0/16.0, 06.0/16.0, 09.0/16.0, 05.0/16.0)
		);
		ivec2 coord = ivec2(gl_FragCoord.xy);
		mipmap_level += bayerIndex[coord.x & 3][coord.y & 3];
	}

	return mipmap_level + mipDistances.w;
}

#ifdef CAN_BILINEAR_FILTER
void bilinear_paletted(vec2 uv, out vec4 color, out vec4 emissive)
{
	float mip = texQueryLod(tex, uv);
	mip += compute_mip_bias(f_coord.y);
	mip = min(mip, float(numMips - 1));

	ivec2 ires = textureSize( tex, int(mip) );
	vec2  fres = vec2( ires );

	vec2 st = uv*fres - 0.5;
    vec2 i = floor( st );
    vec2 w = fract( st );

	// textureGather doesn't handle mips, need to sample manually
	// use textureLod instead of texelFetch/manual textureGather to respect sampler states
	// this should be quite cache friendly so the overhead is minimal
    float a = textureLod( tex, (i + vec2(0.5,0.5)) / fres, mip ).x;
    float b = textureLod( tex, (i + vec2(1.5,0.5)) / fres, mip ).x;
    float c = textureLod( tex, (i + vec2(0.5,1.5)) / fres, mip ).x;
    float d = textureLod( tex, (i + vec2(1.5,1.5)) / fres, mip ).x;
	
	// read the palette
	vec4 ca = texture(worldPalette, vec2(a, 0.5));
    vec4 cb = texture(worldPalette, vec2(b, 0.5));
    vec4 cc = texture(worldPalette, vec2(c, 0.5));
    vec4 cd = texture(worldPalette, vec2(d, 0.5));

    if (a == 0.0) {
        ca.a = 0.0;
    }
    if (b == 0.0) {
        cb.a = 0.0;
    }
    if (c == 0.0) {
        cc.a = 0.0;
    }
    if (d == 0.0) {
        cd.a = 0.0;
    }

	color = mix(mix(ca, cb, w.x), mix(cc, cd, w.x), w.y);

	// Makes sure light is in a sane range
    float light = clamp(f_light, 0.0, 1.0);

	// read the light palette
	a = texture(worldPaletteLights, vec2(a, light)).x;
	b = texture(worldPaletteLights, vec2(b, light)).x;
	c = texture(worldPaletteLights, vec2(c, light)).x;
	d = texture(worldPaletteLights, vec2(d, light)).x;

	ca = texture(worldPalette, vec2(a, 0.5));
    cb = texture(worldPalette, vec2(b, 0.5));
    cc = texture(worldPalette, vec2(c, 0.5));
    cd = texture(worldPalette, vec2(d, 0.5));
	
	emissive = mix(mix(ca, cb, w.x), mix(cc, cd, w.x), w.y);
}
#endif

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
    mat3 TBN = construct_tbn(uv);
    
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
		emissive.rgb += sampledEmiss.rgb * emissiveFactor.rgb;
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

vec3 PurkinjeShift(vec3 light, float intensity)
{
	// constant 
    const vec3 m = vec3(0.63721, 0.39242, 1.6064); // maximal cone sensitivity
	const vec3 k = vec3(0.2, 0.2, 0.29);           // rod input strength long/medium/short
	const float K = 45.0;   // scaling constant
	const float S = 10.0;   // static saturation
	const float k3 = 0.6;   // surround strength of opponent signal
	const float rw = 0.139; // ratio of response for white light
	const float p = 0.6189; // relative weight of L cones
		
	// [jpatry21] slide 164
    // LMSR matrix using Smits method [smits00]
    // Mij = Integrate[ Ei(lambda) I(lambda) Rj(lambda) d(lambda) ]
	const mat4x3 M = mat4x3
    (	
        7.69684945, 18.4248204, 2.06809497,
		2.43113687, 18.6979422, 3.01246326,
		0.28911757, 1.40183293, 13.7922962,
		0.46638595, 15.5643680, 10.0599647
	);
    
    // [jpatry21] slide 165
    // M with gain control factored in
    // note: the result is slightly different, is this correct?
    // g = rsqrt(1 + (0.33 / m) * (q + k * q.w))
    const mat4x3 Mg = mat4x3
    (	
        vec3(0.33 / m.x, 1, 1) * (M[0] + k.x * M[3]),
        vec3(1, 0.33 / m.y, 1) * (M[1] + k.y * M[3]),
        vec3(1, 1, 0.33 / m.z) * (M[2] + k.z * M[3]),
        M[3]
	);
   
	// [jpatry21] slide 166
    const mat3x3 A = mat3x3
    (
        -1, -1, 1,
         1, -1, 1,
         0,  1, 0
    );
	
	// [jpatry21] slide 167
	// o = (K / S) * N * diag(k) * (diag(m)^-1)
    const mat3x3 N = mat3x3
    (
        -(k3 + rw),     p * k3,         p * S,
         1.0 + k3 * rw, (1.0 - p) * k3, (1.0 - p) * S,
         0, 1, 0
	);
	const mat3x3 diag_mi = inverse(mat3x3(m.x, 0, 0, 0, m.y, 0, 0, 0, m.z));
	const mat3x3 diag_k = mat3x3(k.x, 0, 0, 0, k.y, 0, 0, 0, k.z);
	const mat3x3 O =  (K / S) * N * diag_k * diag_mi;

	// [jpatry21] slide 168
	// c = M^-1 * A^-1 * o
    const mat3 Mi = inverse(mat3(M));
	const mat3x3 C = transpose(Mi) * inverse(A) * O;
    
    // map to some kind of mesopic range, this value is arbitrary, use your best approx
    const float scale = 1000.0;
    
    // reference version
    //vec4 lmsr = (light * scale) * M;
    //vec3 lmsGain = inversesqrt(1.0f + (0.33f / m) * (lmsr.rgb + k * lmsr.w));
    
    // matrix folded version, ever so slightly different but good enough
	vec4 lmsr = (light * scale) * Mg;
    vec3 lmsGain = inversesqrt(1.0f + lmsr.rgb);
    vec3 rgbGain = C * lmsGain * intensity / scale;    
    return rgbGain * lmsr.w + light;
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

float ChebyshevUpperBound(vec2 Moments, float t)
{
	// One-tailed inequality valid if t > Moments.x
	float p = (t <= Moments.x) ? 1.0 : 0.0;
	
	// Compute variance.
	float Variance = Moments.y - (Moments.x * Moments.x);
	Variance = max(Variance, 0.0001);

	// Compute probabilistic upper bound.
	float d = t - Moments.x;
	float p_max = Variance / (Variance + d * d);
	return max(p, p_max);
}

void main(void)
{
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
   
	vec3 adj_texcoords = f_uv.xyz / f_uv.w;
	vec3 viewPos = f_coord.xyz;

	// software actually uses the zmin of the entire face
	float mipBias = compute_mip_bias(viewPos.y);
	mipBias = min(mipBias, float(numMips - 1));

    if(displacement_factor != 0.0)
	{
        adj_texcoords.xy = parallax_mapping(adj_texcoords.xy);
    }
	else if(uv_mode == 0)
	{
		adj_texcoords.xy = f_uv_affine;
	}
	
	do_texgen(adj_texcoords, viewPos);

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

	vec3 surfaceNormals = normalize(f_normal);
	vec3 localViewDir = normalize(-viewPos.xyz);
	float ndotv = dot(surfaceNormals.xyz, localViewDir.xyz);
	
	vec3 proc_texcoords = f_uv_nooffset.xyz / f_uv_nooffset.w;
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

    vec4 vertex_color = f_color;
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
		pos += wn.xyz * 0.25 * colorEffects_fade; // animate
		
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
		CalculatePointLighting(bucket_index, lightMode, f_coord.xyz, surfaceNormals, localViewDir, shadows, diffuseColor.xyz, specularColor.xyz, roughness, diffuseLight, specularLight);
	
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

    fragColor = main_color;

//#if defined(ALPHA_BLEND) || defined(ALPHA_DISCARD)
	// todo: this is killing early-z for all the passes...
	if (clipDepth >= f_depth)
		discard;
//#endif

	// dither the output in case we're using some lower precision output
	if(ditherMode == 1)
		fragColor.rgb = min(fragColor.rgb + dither, vec3(1.0));

#ifndef ALPHA_BLEND
    fragColorEmiss = emissive * vec4(vec3(emissiveFactor.w), f_color.w);

	// always dither emissive since it gets bloomed
	// note we subtract instead of add to avoid boosting blacks
	fragColorEmiss.rgb = min(fragColorEmiss.rgb - dither, vec3(1.0));
#else
    // Dont include any windows or transparent objects in emissivity output
	fragColorEmiss = vec4(0.0, 0.0, 0.0, fragColor.w);
#endif
}
