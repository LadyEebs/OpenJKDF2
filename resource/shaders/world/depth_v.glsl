import "uniforms.gli"
import "math.gli"

in vec3 coord3d;
//in vec4 v_normal;	// vertex normal

out float f_depth;
//out vec3 f_normal;

void main(void)
{
	vec4 viewPos = modelMatrix * vec4(coord3d, 1.0);
    vec4 pos     = projMatrix * viewPos;
	f_depth = pos.w / 128.0;
	//f_normal.xyz = v_normal.xyz;

    gl_Position = pos;
}
