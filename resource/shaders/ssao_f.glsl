#ifdef RENDER_DROID2
import "uniforms.gli"
#endif

layout(binding = 0) uniform sampler2D tex;
uniform flexSampler2D tex2;
uniform flexSampler2D tex3;
uniform flexSampler2D tex4;
//uniform vec2 iResolution;
//uniform float param1;
uniform float param2;
uniform float param3;

uniform int param1;

layout(binding = 0) uniform sampler2DMS texMS;
layout(binding = 4) uniform flexSampler2DMS tex4MS;

in vec2 f_uv;

#ifdef RENDER_DROID2

out vec4 fragAO;

float depthFetch(ivec2 coord)
{
	float depth;
	if (param1 > 1)
		depth = texelFetch(texMS, coord, 0).x;
	else
		depth = texelFetch(tex, coord, 0).x;
	return linearize_depth(depth);
}

#ifdef SAMPLING

float depthFetchSingle(ivec2 coord)
{
	float depth;
	if (param1 > 1)
		depth = texelFetch(texMS, coord, gl_SampleID).x;
	else
		depth = texelFetch(tex, coord, 0).x;
	return linearize_depth(depth);
}

// depth-only crytek style AO similar to ShaderX7
// fastest approach I could get at full resolution and that will work with varying input matrices
void main(void)
{
	const flex secondPassScale =  flex(0.5); // scales the radius for the second pass to catch higher frequency details
	const flex radius          =  flex(1.0); // radius of the sampling kernel
	const flex minDist         =  flex(5.0); // radius shrinks below this distance to avoid extreme cache thrasing and visual artifacts
	const flex rangeScale      = flex(0.85); // gives the range scale check a little headroom
	const flex depthTestScale  = flex(64.0); // soft depth test factor

	// unrolled kernel building
	// hopefully constant folded by the compiler (faster than reading a constant buffer)
	const flex offsetStep  = flex(0.875);
	const flex sampleScale = flex(0.025);

	flex offsetScale = flex(0.0);
	flex3 sampleKernel[8] = flex3[8]
	(
		normalize(flex3( 1, 1, 1)) * sampleScale * (offsetScale += offsetStep),
		normalize(flex3(-1,-1,-1)) * sampleScale * (offsetScale += offsetStep),
		normalize(flex3(-1,-1, 1)) * sampleScale * (offsetScale += offsetStep),
		normalize(flex3(-1, 1,-1)) * sampleScale * (offsetScale += offsetStep),
		normalize(flex3(-1, 1 ,1)) * sampleScale * (offsetScale += offsetStep),
		normalize(flex3( 1,-1,-1)) * sampleScale * (offsetScale += offsetStep),
		normalize(flex3( 1,-1, 1)) * sampleScale * (offsetScale += offsetStep),
		normalize(flex3( 1, 1,-1)) * sampleScale * (offsetScale += offsetStep)
	);

	// sample the depth and rotation textures
	float depth = depthFetchSingle(ivec2(gl_FragCoord.xy));
	flex3 rotSample = textureLod(tex3, gl_FragCoord.xy * 0.25, 0).xyz;

	// shrink the radius closer to the camera
	flex3 sampleRadius = flex3(radius) * flex(saturate(depth / minDist));

	// compute factors for the depth comparison, scaled by the distance
	flex range             = rangeScale / sampleRadius.z;     // range around the pixel considered valid
	flex depthTestSoftness = depthTestScale / sampleRadius.z; // softness of the depth test

	// kernel is constant in screen space per ShaderX7 recommendation
	// ShaderX7 scales the Z by 2, not sure why but we do that here
	sampleRadius.xyz *= flex3(flex2(rcp(depth)), 2.0);

	// sample and accumulate
	flex ao = flex(0.0);
	for (int i = 0; i < 8; ++i)
	{
		// reflect the point around the random vector (instead of mul)
		flex3 samplePos = reflect(sampleKernel[i], rotSample) * sampleRadius;

		// sample 2 radii per sphere point to catch finer details
		for (int j = 0; j < 2; ++j)
		{
			// get the depth of the sphere sample
			flex sampleDepth = textureLod(tex2, f_uv.xy + samplePos.xy, 0).x * flex(128.0) + samplePos.z;  
		
			// soft depth test instead of binary test
			flex diff      = flex(depth) - sampleDepth;
			flex depthTest = saturate(-diff * depthTestSoftness);
			
			// slightly different range check than in ShaderX7, better control of the falloff
			flex rangeDiff      = range * diff;
			flex rangeIsInvalid = saturate(abs(rangeDiff)); // range around the sample
			rangeIsInvalid     += saturate(rangeDiff);      // double the invalid range in front of the sample
			
			// add up the contribution
			ao += mix(depthTest, flex(0.5), rangeIsInvalid * flex(0.5));

			// scale the sample for the next pass so we don't need to recompute it
			samplePos.xyz *= secondPassScale;
		}
	}

	// multiplied by 2 to account for mid-gray average of sphere samples
	fragAO = vec4(ao * flex(2.0 / 16.0));

	// give it some contrast
	fragAO *= fragAO;

	// use the alpha to attenuate SSAO because we're going to lazily slap it over the image
	flex mask = texelFetch(tex4MS, ivec2(gl_FragCoord.xy), 0).a;

	fragAO = (mask > flex(0.0)) ? fragAO : vec4(1.0);
}

#endif

#ifdef COMPOSITE

// per-sample bilateral upscale
void main()
{
	float depth = depthFetch(ivec2(gl_FragCoord.xy));

	// 2x2 strided sampling, helps hide the dither pattern better
	flex4 values;
	values.x = texelFetch(tex2, ivec2(gl_FragCoord.xy + vec2(0, 0)), 0).x;
	values.y = texelFetch(tex2, ivec2(gl_FragCoord.xy + vec2(2, 0)), 0).x;
	values.z = texelFetch(tex2, ivec2(gl_FragCoord.xy + vec2(0, 2)), 0).x;
	values.w = texelFetch(tex2, ivec2(gl_FragCoord.xy + vec2(2, 2)), 0).x;
			
	// seem to get better results with a 1.5 offset here, too lazy to figure out why
	vec4 depths;
	depths.x = depthFetch(ivec2(gl_FragCoord.xy + vec2(0.0, 0.0)));
	depths.y = depthFetch(ivec2(gl_FragCoord.xy + vec2(1.5, 0.0)));
	depths.z = depthFetch(ivec2(gl_FragCoord.xy + vec2(0.0, 1.5)));
	depths.w = depthFetch(ivec2(gl_FragCoord.xy + vec2(1.5, 1.5)));

	// reject samples that have depth discontinuities
	vec4 diff = abs(depths / depth - 1.0) * 32.0;
	vec4 weights = saturate(1.0 - diff);

	// total weight
	float totalWeight = weights.x + weights.y + weights.z + weights.w;

	// average when weight is bad
	if(totalWeight < 1e-4)
		fragAO = vec4(float(values.x + values.y + values.z + values.w) * 0.25);
	else
		fragAO = vec4(dot(weights / totalWeight, vec4(values)));
}

#endif

#else

// legacy

out vec4 fragColor;

#define AOradius 1.0
#ifdef NEW_SSAO
#define Samples 8.0
#else
#define Samples 8.0
#endif

#ifdef VIEW_SPACE_GBUFFER
uniform vec3 cameraLB;
uniform vec3 cameraLT;
uniform vec3 cameraRB;
uniform vec3 cameraRT;

vec3 get_camera_frustum_ray(vec2 uv)
{
	//vec3 b = mix(cameraLB.xyz, cameraRB.xyz, uv.x);
	//vec3 t = mix(cameraLT.xyz, cameraRT.xyz, uv.x);
	//return mix(b, t, uv.y);
	
	// barycentric lerp
	return ((1.0 - uv.x - uv.y) * cameraLB.xyz + (uv.x * cameraRB.xyz + (uv.y * cameraLT.xyz)));
}

// Returns the world position from linear depth and a frustum ray
vec3 get_view_position_from_depth(vec3 cam_vec, float linear_depth)
{
	return cam_vec.xyz * linear_depth;
}

vec3 get_view_position(float linear_depth, vec2 uv)
{
	vec3 cam_vec = get_camera_frustum_ray(uv).xyz;
	return get_view_position_from_depth(cam_vec, linear_depth);
}


vec2 oct_wrap(vec2 v)
{
	vec2 signs;
	signs.x = v.x >= 0.0 ? 1.0 : -1.0;
	signs.y = v.y >= 0.0 ? 1.0 : -1.0;
    return (1.0 - abs(v.yx)) * (signs);
}

vec3 decode_octahedron(vec2 p)
{
	p = p * 2.0 - 1.0;

	vec3 n;
    n.z = 1.0 - abs(p.x) - abs(p.y);
    n.xy = n.z >= 0.0 ? p.xy : oct_wrap( p.xy );
    return normalize(n);
}
#endif

// --------------------------------------
// oldschool rand() from Visual Studio
// --------------------------------------
int   seed = 1;
void  srand(int s ) { seed = s; }
int   rand(void)  { seed=seed*0x343fd+0x269ec3; return (seed>>16)&32767; }
float frand(void) { return float(rand())/32767.0; }
// --------------------------------------
// hash by Hugo Elias
// --------------------------------------
int hash( int n ) { n=(n<<13)^n; return n*(n*n*15731+789221)+1376312589; }

// Color curve stuff idk
float gamma = 0.35;
float Cubic (float value) {
    
    // Possibly slightly faster calculation
    // when compared to Sigmoid
    
    if (value < 0.5)
    {
        return value * value * value * value * value * 16.0; 
    }
    
    value -= 1.0;
    
    return value * value * value * value * value * 16.0 + 1.0;
}

vec3 hemisphereVolumeRandPoint()
{
    vec3 p = vec3(frand() * 2.0 - 1.0,frand() * 2.0 - 1.0,frand());

    return normalize(p);
}

float depth(vec2 coord)
{
    vec2 uv = coord*vec2(iResolution.y/iResolution.x,1.0);
#ifdef VIEW_SPACE_GBUFFER
    return texture(tex, uv).x * 128.0 * 128.0;
#else
    return texture(tex, uv).z;
#endif
}

vec3 getpos(vec2 coord)
{
    vec2 uv = coord*vec2(iResolution.y/iResolution.x,1.0);
#ifdef VIEW_SPACE_GBUFFER
	float depth = texture(tex, uv).x;
	return get_view_position(depth, uv);
#else
    return texture(tex, uv).xyz;
#endif
}

float SSAO(vec2 coord)
{
    float cd = depth(coord);
    float screenRadius = 0.5 * (AOradius / cd) / 0.53135;
    float li = 0.0;
    float count = 0.0;
    vec2 fragCoord = f_uv*iResolution.xy;
	
#ifdef NEW_SSAO
	screenRadius *= 8.0;
	vec3 pos = getpos(coord);
	float radius = 0.003f;
#endif

#ifdef VIEW_SPACE_GBUFFER
	vec3 normal = decode_octahedron(texture(tex2, f_uv).xy);
#else
    vec3 normal = texture(tex2, f_uv).rgb;	
    normal *= vec3(1.0, 1.0, -1.0);
#endif
    normal = normalize(normal);

    vec3 rvec = texture(tex3, (f_uv*iResolution.xy) / vec2(4.0)).rgb;

    vec3 tangent = normalize(rvec - normal * dot(rvec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 tbn = mat3(tangent, bitangent, normal);

    vec3 kernels[128];
    for (int i = 0; i < int(Samples); i++)
    {
        kernels[i] = hemisphereVolumeRandPoint();// * frand();

        // Sample points should concentrate near the center of the hemisphere
        float scale = float(i) / Samples;
        scale = mix(0.1f, 1.0f, scale * scale);
        kernels[i] *= scale;
    }

    ivec2 q = ivec2(fragCoord);
    srand( hash(q.x+hash(q.y+hash(1117 * int(param1)))));

    for(float i=0.0; i<Samples; i++)
    {
        vec3 p = kernels[int(i)];

        p *= frand();

        // Rotate the hemisphere
        p = tbn * p;

        vec2 sp = vec2(coord.x + p.x * screenRadius, coord.y + p.y * screenRadius);
	#ifdef NEW_SSAO
		vec3 spos = getpos(sp);
		vec3 v = spos - pos;
		float dv = dot(v, v) + 1e-3;
		v /= sqrt(dv);

		float l = clamp(-(-2.0 * radius + dv) / radius + 1.0, 0.0, 1.0);
		l *= clamp((1.0 / 0.3) * dot(normal, v) - 0.3, 0.0, 1.0);
		li += l;
        count += 1.0;
	#else
        float d = depth(sp);
        float at = pow(length(p)-1.0, 2.0);
        li += step(cd + p.z * AOradius, d) * at;
        count += at;
	#endif
    }
#ifdef NEW_SSAO
    return 1.- li / count;
#else
    return li / count;
#endif
}

void main(void)
{
    vec2 fragCoord = f_uv*iResolution.xy;

    // init random seed
    //ivec2 q = ivec2(fragCoord);
    srand( hash(1117 * int(param1)));

    // coordinate
    vec2 uv = fragCoord/(iResolution.xy);
    vec2 coord = fragCoord/(iResolution.y);

    vec4 sampled_color = vec4(1.0, 1.0, 1.0, 1.0);
    float d = depth(coord);
#ifdef NEW_SSAO
    vec3 ao = vec3(0.9) * SSAO(coord);
#else
    vec3 ao = vec3(0.7) * SSAO(coord);
#endif
    vec3 color = mix(sampled_color.rgb, ao, 1.0 - smoothstep(0.0, 0.99, d*d/1e9));
    //color = mix(color, sampled_color.rgb, 1.0 - smoothstep(0.0, 0.1, d*d/15));
    
#ifndef NEW_SSAO
    // Color curve stuff, idk
    //color = pow(color,vec3(1.0/2.2)); // gamma
    color = vec3(Cubic(color.r),Cubic(color.g),Cubic(color.b));
#endif
    color = pow(color, vec3(gamma));

    //vec3 normal = texture(tex2, f_uv).rgb;
    //fragColor = vec4(normal, 1.0);
    fragColor = vec4(color, 1.0);
}

#endif
