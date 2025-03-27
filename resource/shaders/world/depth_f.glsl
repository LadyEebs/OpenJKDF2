import "defines.gli"
import "uniforms.gli"
import "textures.gli"
import "math.gli"

in float f_depth;
//in vec3 f_normal;

//layout(location = 0) out vec4 fragNormalRoughness;
layout(location = 0) out float fragDepth;

void main(void)
{
	//vec3 normal = normalize(f_normal.xyz);
	//fragNormalRoughness = vec4(normal * 0.5 + 0.5, roughnessFactor); // todo: better packing
	fragDepth = f_depth;
}
