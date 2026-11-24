$input a_position, a_texcoord0
$output v_texcoord0, v_position

#include "bgfx_shader.sh"
#include "camera.sh"

// The mesh of the scene of a camera, in camera space, into the image of the
// beam

void main()
{
    gl_Position = cameraToClip(a_position);
    v_texcoord0 = a_texcoord0;
    v_position = a_position;
}
