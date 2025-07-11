uniform flexSampler2D tex;
uniform flexSampler2D worldPalette;
uniform flexSampler2D displayPalette;

in flex4 f_color;
in vec2 f_uv;

out flex4 fragColor;

flex4 sampleTex(flexSampler2D s, vec2 uv)
{
    flex4 sampled = textureLod(s, uv, 0);
    flex4 palvald = textureLod(displayPalette, vec2(sampled.r, 0.5), 0);

    flex transparency = (sampled.r == 0.0) ? flex(0.0) : flex(1.0);
    return flex4(palvald.r, palvald.g, palvald.b, transparency);
}

void main(void)
{
    flex4 sampled_color = sampleTex(tex, f_uv);
    flex4 vertex_color = f_color;
   
    if (sampled_color.a < flex(0.5))
        discard;
#ifdef RENDER_DROID2
	// fake untonemap to give it some brightness
	flex k = flex(10.0);
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
 #endif
    fragColor = sampled_color * vertex_color;
}
