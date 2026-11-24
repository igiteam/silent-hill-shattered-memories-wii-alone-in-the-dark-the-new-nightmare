$input v_texcoord0, v_position

#include "bgfx_shader.sh"
#include "flashlight.sh"

// The foreground drawn again over the models: the picture at the same place
// of the window, only where an object of the foreground (mask 1) is nearer
// than what was drawn before (the mask comes from the mesh's sheet), lit as
// the picture under it (the blurred image of the beam), red when highlighted
// (u_highlight.x)

SAMPLER2D(s_dark, 0);
SAMPLER2D(s_lit, 1);
SAMPLER2D(s_mask, 2);
SAMPLER2D(s_beam, 3);

// Where the picture is in the window: left, top, width, height in pixels
uniform vec4 u_frame;
// x: 1 to tint the foreground in red
uniform vec4 u_highlight;

void main()
{
    if (texture2D(s_mask, v_texcoord0).r < 0.5)
        discard;
    vec2 picture = (gl_FragCoord.xy - u_frame.xy) / u_frame.zw;
    vec4 dark = texture2D(s_dark, picture);
    vec4 lit = texture2D(s_lit, picture);
    float beam = texture2D(s_beam, picture).r;
    vec4 colour = litColour(dark, lit, u_light.w, beam);
    colour.rgb = mix(colour.rgb, vec3(1.0, 0.1, 0.1), 0.5 * u_highlight.x);
    gl_FragColor = vec4(colour.rgb, 1.0);
}
