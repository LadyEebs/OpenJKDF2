in vec3 coord3d;
in vec4 v_color;
in vec2 v_uv;

uniform mat4 mvp;

out flex4 f_color;
out vec2 f_uv;

void main(void)
{
    vec4 pos = mvp * vec4(coord3d, 1.0);
    pos.w = 1.0/(1.0-coord3d.z);
    pos.xyz *= pos.w;
    gl_Position = pos;
    f_color = flex4(v_color);
    f_uv = v_uv;
}
