uniform flexSampler2D tex;
uniform flexSampler2D tex2;
uniform vec2 iResolution;
uniform float param1;
uniform float param2;
uniform float param3;

in vec2 f_uv;

out flex4 fragColor;

void main(void)
{
	flex4 sampled = texture(tex, f_uv);
    flex4 sampled_color = flex4(1.0, 1.0, 1.0, 1.0);

	//float index = sampled.r;
	//if (param1 < 5)
	//{
	//	sampled_color = texture(tex2, vec2(index, 0.5));
	//}
	//else
	//{
	//	sampled_color = vec4(sampled.b, sampled.g, sampled.r, sampled.a);
	//}
	sampled_color = sampled;

    fragColor = sampled_color;
}
