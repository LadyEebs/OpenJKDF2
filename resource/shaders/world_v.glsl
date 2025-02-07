#include "uniforms.gli"
#include "clustering.gli"
#include "math.gli"
#include "lighting.gli"

in vec3 coord3d;
in vec4 v_color[4];
in vec4 v_normal;
in vec4 v_uv[4];

in vec3 coordVS;

out vec4 f_color;
out float f_light;
out vec4 f_uv;
out vec4 f_uv_nooffset;
out vec3 f_coord;
out vec3 f_normal;
out float f_depth;

uniform mat4 projMatrix;
uniform mat4 modelMatrix;

noperspective out vec2 f_uv_affine;

uniform int  lightMode;

uniform vec3 ambientColor;
uniform vec4 ambientSH[3];
uniform vec3 ambientDominantDir;
uniform vec3 ambientSG[8];

vec3 CalculateAmbientDiffuse(vec3 normal)
{
	vec3 ambientDiffuse = vec3(0.0);
	for(int sg = 0; sg < 8; ++sg)
	{
		SG lightSG;
		lightSG.Amplitude = ambientSG[sg].xyz;
		lightSG.Axis = ambientSGBasis[sg].xyz;
		lightSG.Sharpness = ambientSGBasis[sg].w;
	
		vec3 diffuse = SGIrradiancePunctual(lightSG, normal);
		ambientDiffuse.xyz += diffuse;
	}
	return ambientDiffuse;
}

void main(void)
{
	vec4 viewPos = modelMatrix * vec4(coord3d, 1.0);
    vec4 pos = projMatrix * viewPos;

	mat3 normalMatrix = transpose(inverse(mat3(modelMatrix))); // if we ever need scaling
	f_normal = normalMatrix * (v_normal.xyz * 2.0 - 1.0);

	//if(lightMode < 3)
		//f_normal = face_normal;

    gl_Position = pos;
    f_color = clamp(v_color[0].bgra, vec4(0.0), vec4(1.0));

    f_uv = v_uv[0];
	f_uv_nooffset = v_uv[0];
	f_uv.xy += uv_offset.xy;
	f_uv_affine = v_uv[0].xy;

	f_coord = viewPos.xyz;

#ifdef UNLIT
	if (lightMode == 0)
		f_light = light_mult;
	else
		f_light = 0.0;
#else
    f_light = 0.0;
#endif

 	f_depth = pos.w / 128.0;

#ifdef UNLIT
	if(lightMode == 0) // full lit
		f_color.xyz = vec3(light_mult);
	else if(lightMode == 1) // not lit
		f_color.xyz = vec3(0.0);
#else
	// do ambient diffuse in vertex shader
	if (lightMode >= 2)
		f_color.xyz = max(f_color.xyz, ambientColor.xyz);
	
	if(lightMode >= 3)
		f_color.xyz += CalculateAmbientDiffuse(f_normal);
#endif
}
