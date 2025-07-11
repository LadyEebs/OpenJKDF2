#define TEX_MODE_TEST 0
#define TEX_MODE_WORLDPAL 1
#define TEX_MODE_BILINEAR 2
#define TEX_MODE_16BPP 5
#define TEX_MODE_BILINEAR_16BPP 6

#define D3DBLEND_ONE             (2)
#define D3DBLEND_SRCALPHA        (5)
#define D3DBLEND_INVSRCALPHA     (6)

uniform sampler2D tex;

uniform sampler2D worldPalette;
uniform sampler2D worldPaletteLights;

uniform int tex_mode;
uniform int blend_mode;
uniform vec3 colorEffects_tint;
uniform vec3 colorEffects_filter;
uniform float colorEffects_fade;
uniform vec3 colorEffects_add;

uniform vec2 iResolution;
uniform int enableDither;

in vec4 f_color;
noperspective in float f_light;
in vec2 f_uv;
in vec3 f_coord;
in vec3 f_normal;
in float f_depth;

layout(location = 0) out vec4 fragColor;

void main(void)
{
    vec4 vertex_color = f_color;

	if (blend_mode == D3DBLEND_INVSRCALPHA)
    {
        if (vertex_color.a < 0.01) {
            discard;
        }
    }
	
    if (blend_mode != D3DBLEND_SRCALPHA && blend_mode != D3DBLEND_INVSRCALPHA && vertex_color.a != 0.0)
    {
        vertex_color.a = 1.0;
    }

    vec4 sampled = texture(tex, f_uv.xy);
    vec4 sampled_color = vec4(1.0, 1.0, 1.0, 1.0);

    float index = sampled.r;
    vec4 palval = texture(worldPalette, vec2(index, 0.5));

    if (tex_mode == TEX_MODE_TEST) {
        sampled_color = vec4(1.0, 1.0, 1.0, 1.0);
    }
    else if (tex_mode == TEX_MODE_16BPP
    || tex_mode == TEX_MODE_BILINEAR_16BPP
    )
    {
        sampled_color = vec4(sampled.b, sampled.g, sampled.r, sampled.a) * vertex_color;
    }
    else if (tex_mode == TEX_MODE_WORLDPAL
    || tex_mode == TEX_MODE_BILINEAR
    )
    {
        if (index == 0.0 && (blend_mode == D3DBLEND_SRCALPHA || blend_mode == D3DBLEND_INVSRCALPHA))
            discard;

        // Makes sure light is in a sane range
        float light = clamp(f_light, 0.0, 1.0);
        float light_worldpalidx = texture(worldPaletteLights, vec2(index, light)).r;

        vec4 lightPalval = texture(worldPalette, vec2(light_worldpalidx, 0.5));
        sampled_color = lightPalval;
		sampled_color.a *= vertex_color.a;
    }

    vec4 main_color = sampled_color;

    vec4 effectAdd_color = vec4(colorEffects_add.r, colorEffects_add.g, colorEffects_add.b, 0.0);
    
    if (main_color.a < 0.01) {
        discard;
    }
    
    if (blend_mode == D3DBLEND_INVSRCALPHA)
    {
        main_color.rgb *= (1.0 - main_color.a);
        main_color.a = (1.0 - main_color.a);
    }

    fragColor = main_color + effectAdd_color;

    vec3 tint = normalize(colorEffects_tint + 1.0) * sqrt(3.0);

    vec4 color_add = vec4(0.0, 0.0, 0.0, 1.0);
    color_add.r *= tint.r;
    color_add.g *= tint.g;
    color_add.b *= tint.b;

    color_add.r *= colorEffects_fade;
    color_add.g *= colorEffects_fade;
    color_add.b *= colorEffects_fade;

    color_add.r *= colorEffects_filter.r;
    color_add.g *= colorEffects_filter.g;
    color_add.b *= colorEffects_filter.b;
}
