#include "uniforms.gli"

in vec3 coord3d;
in vec3 v_normal;
in vec4 v_color[4];
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

// https://therealmjp.github.io/posts/sg-series-part-1-a-brief-and-incomplete-history-of-baked-lighting-representations/
// SphericalGaussian(dir) := Amplitude * exp(Sharpness * (dot(Axis, dir) - 1.0f))
struct SG
{
    vec3 Amplitude;
    vec3 Axis;
    float Sharpness;
};

vec3 SGInnerProduct(SG x, SG y)
{
    float umLength = length(x.Sharpness * x.Axis + y.Sharpness * y.Axis);
    vec3 expo = exp(umLength - x.Sharpness - y.Sharpness) * x.Amplitude * y.Amplitude;
    float other = 1.0 - exp(-2.0 * umLength);
    return (2.0 * 3.141592 * expo * other) / umLength;
}

SG CosineLobeSG(vec3 direction)
{
    SG cosineLobe;
    cosineLobe.Axis = direction;
    cosineLobe.Sharpness = 2.133;
    cosineLobe.Amplitude = vec3(1.17);

    return cosineLobe;
}

vec3 SGIrradianceInnerProduct(SG lightingLobe, vec3 normal)
{
    SG cosineLobe = CosineLobeSG(normal);
    return max(SGInnerProduct(lightingLobe, cosineLobe), 0.0);
}

vec3 SGIrradiancePunctual(SG lightingLobe, vec3 normal)
{
    float cosineTerm = clamp(dot(lightingLobe.Axis, normal), 0.0, 1.0);
    return cosineTerm * 2.0 * 3.141592 * (lightingLobe.Amplitude) / lightingLobe.Sharpness;
}


vec3 ApproximateSGIntegral(in SG sg)
{
    return 2 * 3.141592 * (sg.Amplitude / sg.Sharpness);
}

vec3 SGIrradianceFitted(in SG lightingLobe, in vec3 normal)
{
    float muDotN = dot(lightingLobe.Axis, normal);
    float lambda = lightingLobe.Sharpness;

    const float c0 = 0.36f;
    const float c1 = 1.0f / (4.0f * c0);

    float eml  = exp(-lambda);
    float em2l = eml * eml;
    float rl   = 1.0/(lambda);

    float scale = 1.0f + 2.0f * em2l - rl;
    float bias  = (eml - em2l) * rl - em2l;

    float x  = sqrt(1.0f - scale);
    float x0 = c0 * muDotN;
    float x1 = c1 * x;

    float n = x0 + x1;

    float y = (abs(x0) <= x1) ? n * n / x : clamp(muDotN, 0.0, 1.0);

    float normalizedIrradiance = scale * y + bias;

    return normalizedIrradiance * ApproximateSGIntegral(lightingLobe);
}

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
	f_normal = normalMatrix * v_normal.xyz;

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
		f_light = 1.0;
	else
		f_light = 0.0;
#else
    f_light = 0.0;
#endif

 	f_depth = pos.w / 128.0;

#ifdef UNLIT
	if(lightMode == 0) // full lit
		f_color.xyz = vec3(1.0);
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
