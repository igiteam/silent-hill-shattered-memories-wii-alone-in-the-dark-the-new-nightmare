$input a_position, a_texcoord0
$output v_texcoord0

#include "bgfx_shader.sh"

// The image of the beam over the whole of another one, to blur it

void main()
{
    gl_Position = vec4(a_position, 1.0);
    v_texcoord0 = a_texcoord0;
}
