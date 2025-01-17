in vec3 coord3d;
in vec4 v_color;
in float v_light;
in vec4 v_uv;
in vec3 v_normal;

in vec3 coordVS;

uniform mat4 projMatrix;
out vec4 f_color;
out float f_light;
out vec4 f_uv;
out vec3 f_coord;
out vec3 f_normal;
out float f_depth;

uniform mat4 modelMatrix;

layout(std140) uniform sharedBlock
{
	vec4  ambientSGBasis[8];

	vec4  colorEffects_tint;
	vec4  colorEffects_filter;
	vec4  colorEffects_add;
	
	vec4  mipDistances;

	float colorEffects_fade;
	float light_mult;
	vec2  iResolution;

	vec2  clusterTileSizes;
	vec2  clusterScaleBias;
};

layout(std140) uniform textureBlock
{
	int   tex_mode;
	int   uv_mode;
	int   texgen;
	int   numMips;

	vec2 texsize;
	vec2 uv_offset;

	vec4 texgen_params;

	vec4 padding;
};

noperspective out vec2 f_uv_affine;

void main(void)
{
	vec4 viewPos = modelMatrix * vec4(coord3d, 1.0);
    vec4 pos = projMatrix * viewPos;
	f_normal = normalize(mat3(modelMatrix) * v_normal.xyz);

    gl_Position = pos;
    f_color = v_color.bgra;

    f_uv = v_uv;
	f_uv.xy += uv_offset.xy;
	f_uv_affine = v_uv.xy;
	f_coord = viewPos.xyz;

    f_light = v_light;
 	f_depth = pos.w / 128.0;
}
