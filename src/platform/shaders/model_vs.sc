$input a_position, a_normal, a_texcoord0
$output v_texcoord0, v_normal, v_position

#include "bgfx_shader.sh"
#include "camera.sh"

// A model (a character...) in camera space

void main()
{
    gl_Position = cameraToClip(a_position);
    v_texcoord0 = a_texcoord0;
    v_normal = a_normal;
    v_position = a_position;
}
