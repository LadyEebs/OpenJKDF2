import "math.gli"
import "framebuffer.gli"

layout(binding = 0) uniform flexSampler2D tex;
layout(binding = 1) uniform flexSampler2D tex2;
layout(binding = 2) uniform flexSampler2D tex3;

layout(location = 0) uniform vec2 iResolution;
layout(location = 1) uniform float param1;
layout(location = 2) uniform float param2;
layout(location = 3) uniform float param3;

layout(location = 4) uniform vec3 colorEffects_tint;
layout(location = 5) uniform vec3 colorEffects_filter;
layout(location = 6) uniform vec3 colorEffects_add;
layout(location = 7) uniform float colorEffects_fade;

layout(location = 0) in vec4 f_color;
layout(location = 1) in vec2 f_uv;

layout(location = 0) out vec4 fragColor;

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

void main(void)
{
	// water distortion
	vec2 uv = f_uv;
	if(param1 > 0.0)
	{
		float ar = iResolution.x / iResolution.y;
		vec2 cycle = vec2(1.0, ar) * 3.141592 * 5.0;
		vec2 amp = vec2(1.0, ar) * 0.5 / 300.0;
		uv = uv.xy + (sin(uv.yx * cycle.xy + param1) * amp.xy) * (1.0 - amp.xy * 2.0) + amp.xy;
	}

	flex3 sampled_color = textureLod(tex, uv, 0).xyz;

	// when dithering, try to smooth it out with a classic voodoo style filter
	if(param2 > 0.0)
	{
		vec2 pixsize = vec2(1.0) / textureSize(tex, 0).xy;
		
		flex3 pixel00 = sampled_color;
		
		flex3 pixel01, pixel11, pixel10;	
		//if(param2 > 1.0) // 4x1
		//{
		//	pixel01 = textureLod(tex, uv - vec2(pixsize.x, 0.0), 0).xyz;
		//	pixel11 = textureLod(tex, uv + vec2(pixsize.x, 0.0), 0).xyz;
		//	pixel10 = textureLod(tex, uv + vec2(pixsize.x, 0.0), 0).xyz;
		//}
		//else // 2x2
		{
			pixel01 = textureLod(tex, uv + vec2(      0.0, -pixsize.y), 0).xyz;
			pixel11 = textureLod(tex, uv + vec2(pixsize.x, -pixsize.y), 0).xyz;
			pixel10 = textureLod(tex, uv + vec2(pixsize.x,        0.0), 0).xyz;
		}	
	
		flex3 diff0 = clamp(pixel01 - pixel00, flex3(-32.0/255.0), flex3(32.0/255.0));
		flex3 diff1 = clamp(pixel11 - pixel00, flex3(-32.0/255.0), flex3(32.0/255.0));
		flex3 diff2 = clamp(pixel10 - pixel00, flex3(-32.0/255.0), flex3(32.0/255.0));
		
		sampled_color = (pixel00 + (diff0 + diff1 + diff2) * flex3(1.0 / 3.0));
	}

	//vec2 invPixelSize = 1.0 / iResolution.xy;
	//
	//sampled_color = vec4(0.0);
	//sampled_color += 0.37487566 * texture(tex, uv + vec2(-0.75777,-0.75777)*invPixelSize);
	//sampled_color += 0.37487566 * texture(tex, uv + vec2(0.75777,-0.75777)*invPixelSize);
	//sampled_color += 0.37487566 * texture(tex, uv + vec2(0.75777,0.75777)*invPixelSize);
	//sampled_color += 0.37487566 * texture(tex, uv + vec2(-0.75777,0.75777)*invPixelSize);
 	//
	//sampled_color += -0.12487566 * texture(tex, uv + vec2(-2.907,0.0)*invPixelSize);
	//sampled_color += -0.12487566 * texture(tex, uv + vec2(2.907,0.0)*invPixelSize);
	//sampled_color += -0.12487566 * texture(tex, uv + vec2(0.0,-2.907)*invPixelSize);
	//sampled_color += -0.12487566 * texture(tex, uv + vec2(0.0,2.907)*invPixelSize);    

	//sampled_color.rgb = ycocg2rgb(sampled_color.yxz);

	flex vignetteStrength = flex(15.0); // todo: expose
	flex vignettePower = flex(0.2); // todo: expose

	flex2 oneOverUV = flex(1.0) - flex2(uv.xy);
	flex edge = flex(uv.x) * flex(uv.y) * oneOverUV.x * oneOverUV.y;
	edge = clamp(vignetteStrength * edge, flex(0.0), flex(1.0));
	sampled_color *= pow(edge, vignettePower) * flex(0.5) + flex(0.5);
    
	flex3 bloom = textureLod(tex2, uv.xy, 0).xyz;
	sampled_color.rgb = bloom.rgb + sampled_color.rgb * (flex3(1.0) - bloom.rgb);

#ifdef RENDER_DROID2
	sampled_color.rgb += flex3(colorEffects_add.rgb);

	vec3 half_tint = colorEffects_tint * 0.5;
	flex3 tint_delta = flex3( colorEffects_tint - (half_tint.brr + half_tint.ggb) );
	sampled_color.rgb = clamp(tint_delta.rgb * sampled_color.rgb + sampled_color.rgb, flex3(0.0), flex3(1.0));

	sampled_color.rgb *= flex(colorEffects_fade);
	sampled_color.rgb *= flex3(colorEffects_filter.rgb);
#endif

    fragColor.rgb = vec3(pow(sampled_color.rgb, flex3(fastRcpNR0(param3))));
	fragColor.w = 1.0;
}
