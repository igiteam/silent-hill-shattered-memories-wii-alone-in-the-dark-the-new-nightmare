$input v_texcoord0, v_position

#include "bgfx_shader.sh"
#include "flashlight.sh"

// How much the flashlight lights each point of the scene's mesh (red), into
// an image of its own that is blurred before it lights the picture (ours: the
// game lights the mesh's points and spreads the light between them; lit at
// every pixel, the light was cut sharply at the edges of the foreground's
// pieces). The room (u_highlight.y 1) everywhere; the objects of the
// foreground (z 1) only where the mask says they are

SAMPLER2D(s_mask, 2);

// y: 1 for the room, unless z: 1 (the objects: only where the mask says)
uniform vec4 u_highlight;

void main()
{
    if ((u_highlight.y < 0.5 || u_highlight.z > 0.5) && texture2D(s_mask, v_texcoord0).r < 0.5)
        discard;
    gl_FragColor = vec4(flashlightAt(v_position), 0.0, 0.0, 1.0);
}
