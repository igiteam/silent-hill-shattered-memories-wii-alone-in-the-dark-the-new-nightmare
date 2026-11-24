$input a_position, a_texcoord0, a_color0
$output v_texcoord0, v_color0

#include "bgfx_shader.sh"

// The layer over everything (texts and their boxes): rectangles given in
// clip space (-1..1)

void main()
{
    gl_Position = vec4(a_position, 1.0);
    v_texcoord0 = a_texcoord0;
    v_color0 = a_color0;
}
