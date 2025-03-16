#include "uniforms.gli"
#include "math.gli"

in vec3 coord3d;
in vec4 v_normal;
in vec4 v_color[2];
in vec4 v_uv[4];
in vec3 coordVS;

out vec4 f_color;
out vec4 f_uv;
out vec3 f_normal;
out float f_depth;

void main(void)
{
	vec4 viewPos = modelMatrix * vec4(coord3d, 1.0);
    vec4 pos = projMatrix * viewPos;

	mat3 normalMatrix = transpose(inverse(mat3(modelMatrix))); // if we ever need scaling
	f_normal = normalMatrix * (v_normal.xyz * 2.0 - 1.0);

    gl_Position = pos;
    f_color = v_color[0].bgra;

    f_uv = v_uv[0];

 	f_depth = pos.w / 128.0;
}
