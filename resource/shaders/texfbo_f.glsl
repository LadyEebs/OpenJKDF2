uniform flexSampler2D tex;

uniform vec2 iResolution;
uniform float param1;
uniform float param2;
uniform float param3;

in vec2 f_uv;

out flex4 fragColor;

void main(void)
{
    flex4 sampled_color = texture(tex, f_uv);
    flex4 blend = flex4(flex3(param2), param1);

    fragColor = sampled_color * blend;
    fragColor.rgb = pow(fragColor.rgb, flex3(1.0 / param3));
    //fragColor.rgba = saturate(fragColor.rgba);
}
