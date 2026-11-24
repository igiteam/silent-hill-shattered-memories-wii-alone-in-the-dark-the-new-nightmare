$input v_texcoord0, v_color0

#include "bgfx_shader.sh"

// A part of the layer's image (or a plain white one) in a colour, blended
// over what is below by its alpha

SAMPLER2D(s_overlay, 0);

void main()
{
    gl_FragColor = texture2D(s_overlay, v_texcoord0) * v_color0;
}
