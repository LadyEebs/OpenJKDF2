#include "defines.gli"
#include "uniforms.gli"
#include "textures.gli"
#include "math.gli"

in vec4 f_color;
in vec4 f_uv;
in vec3 f_normal;
in float f_depth;

//layout(location = 0) out vec4 fragNormalRoughness;
layout(location = 0) out float fragDepth;

void main(void)
{
	//vec3 normal = normalize(f_normal.xyz);
	//fragNormalRoughness = vec4(normal * 0.5 + 0.5, roughnessFactor); // todo: better packing
	fragDepth = f_depth;
}
