$input v_texcoord0

#include "bgfx_shader.sh"
#include "flashlight.sh"

// The picture of a camera: the dark version, or the lit one as much as the
// light says: the light everywhere, or the flashlight's beam (its blurred
// image, made from the scene's mesh: beam_ps, blur_ps)

SAMPLER2D(s_dark, 0);
SAMPLER2D(s_lit, 1);
SAMPLER2D(s_beam, 2);

void main()
{
    vec4 dark = texture2D(s_dark, v_texcoord0);
    vec4 lit = texture2D(s_lit, v_texcoord0);
    float beam = texture2D(s_beam, v_texcoord0).r;
    gl_FragColor = litColour(dark, lit, u_light.w, beam);
}
