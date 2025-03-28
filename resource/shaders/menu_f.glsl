uniform sampler2D tex;
uniform sampler2D worldPalette;
uniform sampler2D displayPalette;
in vec4 f_color;
in vec2 f_uv;
out vec4 fragColor;

void main(void)
{
    vec4 sampled = texture(tex, f_uv);
    vec4 sampled_color = vec4(0.0, 0.0, 0.0, 0.0);
    vec4 vertex_color = f_color;
    float index = sampled.r;
    vec4 palval = texture(worldPalette, vec2(index, 0.5));
    vec4 palvald = texture(displayPalette, vec2(index, 0.5));
    vec4 blend = vec4(1.0, 1.0, 1.0, 1.0);

    float transparency = 1.0;
    if (index == 0.0)
        discard;
    sampled_color = vec4(palvald.r, palvald.g, palvald.b, transparency);

	//sampled_color.rgb = log2(1.0001 - sampled_color.rgb) / -2.0;
	float k = 4.0;
	sampled_color.rgb = k * sampled_color.rgb / (k - sampled_color.rgb);

	
	//float vignetteStrength = 15.0; // todo: expose
	//float vignettePower = 0.5; // todo: expose

	//vec2 oneOverUV = 1.0 - f_uv.xy;
	//float edge = f_uv.x * f_uv.y * oneOverUV.x * oneOverUV.y;
	//edge = clamp(vignetteStrength * edge, 0.0, 1.0);
	//sampled_color *= pow(edge, vignettePower) * 0.5 + 0.5;
    
	//vec2 uv = gl_FragCoord.xy / textureSize(tex, 0).xy;
	//float l = clamp(dot(uv.xy*2.0-1.0, uv.xy*2.0-1.0), 0.0, 1.0);
	//sampled_color.rgb *= pow(1.0 - l, 4.0);

    fragColor = sampled_color * vertex_color * blend;
}
