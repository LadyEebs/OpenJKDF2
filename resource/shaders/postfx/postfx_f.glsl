#include "math.gli"
#include "framebuffer.gli"

uniform sampler2D tex;
uniform sampler2D tex2;
uniform vec2 iResolution;
uniform float param1;
uniform float param2;
uniform float param3;

uniform vec3 colorEffects_tint;
uniform vec3 colorEffects_filter;
uniform vec3 colorEffects_add;
uniform float colorEffects_fade;

in vec4 f_color;
in vec2 f_uv;
in vec3 f_coord;
out vec4 fragColor;

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

   // vec4 sampled_color = texture(tex, uv);
	//vec2 sourceSize = textureSize(tex, 0).xy;
   //
	// when dithering, try to smooth it out with a classic voodoo style filter
	//if(param2 > 0.0)
	//{
	//	vec2 sourceSize = textureSize(tex, 0).xy;
	//	
	//	vec4 pixel00 = sampled_color;
	//	vec4 pixel01, pixel11, pixel10;
	//
	//	if(param2 > 1.0) // 4x1
	//	{
	//		vec4 pixel01 = texture(tex, uv - vec2(1.0 / sourceSize.x, 0.0));
	//		vec4 pixel11 = texture(tex, uv + vec2(1.0 / sourceSize.x, 0.0));
	//		vec4 pixel10 = texture(tex, uv + vec2(2.0 / sourceSize.x, 0.0));
	//	}
	//	else // 2x2
	//	{
	//		pixel01 = texture(tex, uv + vec2(0.0,                -1.0 / sourceSize.y));
	//		pixel11 = texture(tex, uv + vec2(1.0 / sourceSize.x, -1.0 / sourceSize.y));
	//		pixel10 = texture(tex, uv + vec2(1.0 / sourceSize.x,  0.0));
	//	}	
	//
	//	vec4 diff0 = clamp(pixel01 - pixel00, -32.0/255.0, 32.0/255.0);
	//	vec4 diff1 = clamp(pixel11 - pixel00, -32.0/255.0, 32.0/255.0);
	//	vec4 diff2 = clamp(pixel10 - pixel00, -32.0/255.0, 32.0/255.0);
	//	
	//	sampled_color = (pixel00 + (diff0 + diff1 + diff2) / 3.0);
	//}

	vec3 sampled_color = sampleFramebuffer(tex, uv);

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


	float vignetteStrength = 15.0; // todo: expose
	float vignettePower = 0.2; // todo: expose

	vec2 oneOverUV = 1.0 - uv.xy;
	float edge = uv.x * uv.y * oneOverUV.x * oneOverUV.y;
	edge = clamp(vignetteStrength * edge, 0.0, 1.0);
	sampled_color *= pow(edge, vignettePower) * 0.5 + 0.5;
    
	vec4 bloom = texture(tex2, uv.xy);
	sampled_color.rgb = bloom.rgb + sampled_color.rgb * (1.0 - bloom.rgb);

#ifdef RENDER_DROID2
	sampled_color.rgb += colorEffects_add.rgb;

	vec3 half_tint = colorEffects_tint * 0.5;
	vec3 tint_delta = colorEffects_tint - (half_tint.brr + half_tint.ggb);
	sampled_color.rgb = clamp(tint_delta.rgb * sampled_color.rgb + sampled_color.rgb, vec3(0.0), vec3(1.0));

	sampled_color.rgb *= colorEffects_fade;
	sampled_color.rgb *= colorEffects_filter.rgb;
#endif

    fragColor.rgb = sampled_color.rgb;
    fragColor.rgb = pow(fragColor.rgb, vec3(1.0/param3));
	fragColor.w = 1.0;
}
