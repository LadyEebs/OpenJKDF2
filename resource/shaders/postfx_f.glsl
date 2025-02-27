#include "math.gli"

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

    vec4 sampled_color = texture(tex, uv);
	vec2 sourceSize = textureSize(tex, 0).xy;

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

	ivec2 crd = ivec2(uv.xy * sourceSize.xy);

	// edge-directed reconstruction:
	vec2 a0 = texture(tex, uv + vec2(1.0 / sourceSize.x, 0.0), 0).rg;
	vec2 a1 = texture(tex, uv - vec2(1.0 / sourceSize.x, 0.0), 0).rg;
	vec2 a2 = texture(tex, uv + vec2(0.0 ,1.0 / sourceSize.y), 0).rg;
	vec2 a3 = texture(tex, uv - vec2(0.0 ,1.0 / sourceSize.y), 0).rg;		

	float chroma = edge_filter(sampled_color.rg, a0, a1, a2, a3);

	bool pattern = ((crd.x & 1) == (crd.y & 1));

	sampled_color.b = chroma.x;
	//sampled_color.rgb = (pattern) ? sampled_color.rgb : sampled_color.rbg;
	//sampled_color.rgb = ycocg2rgb(sampled_color.rgb);

	sampled_color.rgb = (pattern) ? sampled_color.grb : sampled_color.brg;

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

    fragColor = sampled_color;
    fragColor.rgb = pow(fragColor.rgb, vec3(1.0/param3));
	fragColor.w = 1.0;
}
