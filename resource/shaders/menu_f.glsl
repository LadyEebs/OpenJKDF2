uniform sampler2D tex;
uniform sampler2D worldPalette;
uniform sampler2D displayPalette;
in vec4 f_color;
in vec2 f_uv;
out vec4 fragColor;

vec4 sampleTex(sampler2D s, vec2 uv)
{
    vec4 sampled = textureLod(s, uv, 0);
    vec4 palvald = textureLod(displayPalette, vec2(sampled.r, 0.5), 0);

    float transparency = (sampled.r == 0.0) ? 0.0 : 1.0;
    return vec4(palvald.r, palvald.g, palvald.b, transparency);
}

void main(void)
{
    vec4 sampled_color = sampleTex(tex, f_uv);
    vec4 vertex_color = f_color;
   
    if (sampled_color.a < 0.5)
        discard;

	// fake untonemap to give it some brightness
	float k = 6.0;
	//sampled_color.rgb = k * sampled_color.rgb / (k - sampled_color.rgb);
	sampled_color.rgb = k * sampled_color.rgb / (k - max3(sampled_color.r, sampled_color.g, sampled_color.b));
	
//	float vignetteStrength = 7.0;
//	float vignettePower = 0.85;
//
//	vec2 menuUV = f_uv.xy * textureSize(tex, 0).xy / vec2(640.0, 480.0);
//
//	vec2 oneOverUV = 1.0 - menuUV.xy;
//	float edge = menuUV.x * oneOverUV.x;// menuUV.x * menuUV.y * oneOverUV.x * oneOverUV.y;
//	edge = clamp(vignetteStrength * edge, 0.0, 1.0);
//	sampled_color *= pow(edge, vignettePower);
 
    fragColor = sampled_color * vertex_color;
}
