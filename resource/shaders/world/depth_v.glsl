#include "uniforms.gli"
#include "math.gli"

in vec3 coord3d;

out float f_depth;

void main(void)
{
	vec4 viewPos = modelMatrix * vec4(coord3d, 1.0);
    vec4 pos     = projMatrix * viewPos;
	f_depth = pos.w / 128.0;

    gl_Position = pos;
}
