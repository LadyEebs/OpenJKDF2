layout(binding = 0) uniform flexSampler2D tex;

layout(location = 0) uniform vec2 iResolution;
layout(location = 1) uniform float param1;
layout(location = 2) uniform float param2;
layout(location = 3) uniform float param3;

layout(location = 0) in vec2 f_uv;

layout(location = 0) out flex4 fragColor;

void main(void)
{
	vec2 PixSize = param1 / iResolution.xy;
    
	// 6x6 downscale/upscale
    flex3 s0 = textureLod(tex, vec2( 0.000000000, 0.000000000) * PixSize.xy + f_uv.xy, 0.0).xyz;
	flex3 s1 = textureLod(tex, vec2( 0.604077935, 0.000000000) * PixSize.xy + f_uv.xy, 0.0).xyz;
	flex3 s2 = textureLod(tex, vec2( 0.000000000, 0.604077935) * PixSize.xy + f_uv.xy, 0.0).xyz;
	flex3 s3 = textureLod(tex, vec2(-0.604077935, 0.000000000) * PixSize.xy + f_uv.xy, 0.0).xyz;
	flex3 s4 = textureLod(tex, vec2( 0.000000000,-0.604077935) * PixSize.xy + f_uv.xy, 0.0).xyz;		
	flex3 s5 = textureLod(tex, vec2( 0.604077935, 0.604077935) * PixSize.xy + f_uv.xy, 0.0).xyz;
	flex3 s6 = textureLod(tex, vec2(-0.604077935, 0.604077935) * PixSize.xy + f_uv.xy, 0.0).xyz;
	flex3 s7 = textureLod(tex, vec2( 0.604077935,-0.604077935) * PixSize.xy + f_uv.xy, 0.0).xyz;
	flex3 s8 = textureLod(tex, vec2(-0.604077935,-0.604077935) * PixSize.xy + f_uv.xy, 0.0).xyz;
		
	flex3 Color = s0 * flex(0.145904019);
	Color += flex(0.11803490998) * (s1 + s2 + s3 + s4);
	Color += flex(0.09548908532) * (s5 + s6 + s7 + s8);

	// simple kawase
	//const vec2 s = vec2(-1,1);
	//const vec2 a = vec2(0,2);
	//flex4 Color = (texture(tex, f_uv + PixSize.xy * s.xx, 0.0) +
	// 			  texture(tex, f_uv + PixSize.xy * s.yx, 0.0) +
	// 			  texture(tex, f_uv + PixSize.xy * s.xy, 0.0) +
	// 			  texture(tex, f_uv + PixSize.xy * s.yy, 0.0)) / 6.0 +
	// 			 (texture(tex, f_uv + PixSize.xy * a.xy, 0.0) +
	// 			  texture(tex, f_uv - PixSize.xy * a.xy, 0.0) +
	// 			  texture(tex, f_uv + PixSize.xy * a.yx, 0.0) +
	// 			  texture(tex, f_uv - PixSize.xy * a.yx, 0.0)) / 12.0;
	
	fragColor = flex4(Color.rgb, param2);
}
