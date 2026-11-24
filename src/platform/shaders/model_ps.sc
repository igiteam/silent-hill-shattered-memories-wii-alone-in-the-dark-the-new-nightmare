$input v_texcoord0, v_normal, v_position

#include "bgfx_shader.sh"
#include "flashlight.sh"

// A model: its texture lit as the game lights models (FUN_00426070): the
// level's ambient colour, the scene's light (the camera's, high up: the moon)
// on the sides facing it, and the flashlight's beam on the sides facing the
// flashlight, twice its strength (FUN_0041b100). With the light everywhere
// (the viewer's F3), a soft light from the camera instead.

SAMPLER2D(s_texture, 0);

// rgb: the level's ambient colour (0..1)
uniform vec4 u_ambient;
// xyz: the scene's light in camera space, w: its strength (0..1)
uniform vec4 u_sceneLight;

void main()
{
    vec4 colour = texture2D(s_texture, v_texcoord0);
    if (colour.a < 0.5)
        discard;
    vec3 normal = normalize(v_normal);
    // The camera looks along +y: a side facing it has its normal along -y
    float facing = abs(normal.y);
    vec3 toFlashlight = normalize(u_spotPlace.xyz - v_position);
    float beam = 2.0 * flashlightAt(v_position) * max(dot(normal, toFlashlight), 0.0);
    vec3 toScene = normalize(u_sceneLight.xyz - v_position);
    vec3 dark = u_ambient.rgb + vec3_splat(u_sceneLight.w * max(dot(normal, toScene), 0.0));
    float everywhere = 0.35 + 0.25 * facing + 0.6 * facing;
    vec3 light = mix(dark, vec3_splat(everywhere), u_light.w) + vec3_splat(0.9 * beam);
    gl_FragColor = vec4(colour.rgb * light, 1.0);
}
