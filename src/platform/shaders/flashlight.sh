// The light of the pictures and the models: the flashlight's beam (the game's
// spot light, type 1 of FUN_0041b100) and a light everywhere.

// w: light everywhere (0..1)
uniform vec4 u_light;
// The flashlight in camera space: xyz where it is, w 1 if it is on
uniform vec4 u_spotPlace;
// xyz where it points (a unit vector), w how far it reaches (625 cm)
uniform vec4 u_spotAim;

// How much the flashlight lights a point of camera space (0..1), as the game:
// only ahead of it (25 cm on), inside its cone (offset of 100 at its focal
// of 128, so about 38 degrees) and weaker to the edge and farther away:
// 2 x (1 - offset^2 / 100^2 - depth / reach)
float flashlightAt(vec3 at)
{
    if (u_spotPlace.w < 0.5)
        return 0.0;
    vec3 away = at - u_spotPlace.xyz;
    float depth = dot(away, u_spotAim.xyz);
    if (depth <= 25.0 || depth >= u_spotAim.w)
        return 0.0;
    float offset = length(away - depth * u_spotAim.xyz) * 128.0 / depth;
    return clamp(2.0 * (1.0 - offset * offset / 10000.0 - depth / u_spotAim.w), 0.0, 1.0);
}

// The light at a point of camera space: the flashlight's or the light
// everywhere, the stronger
float lightAt(vec3 at)
{
    return max(u_light.w, flashlightAt(at));
}

// Colour of a point of the picture with the light everywhere and the
// flashlight's beam (0..1): the brighter of
// the dark and the lit picture, so that light never darkens (in some scenes,
// like level 144, the dark picture has moonlight that the lit one lacks).
// Ours: where the lit picture is hardly brighter than the dark one (the
// ground of some cameras), the dark one made warmer and brighter, so that the
// beam is always seen
vec4 litColour(vec4 dark, vec4 lit, float everywhere, float beam)
{
    vec4 colour = mix(dark, max(dark, lit), max(everywhere, beam));
    vec4 warm = vec4(dark.rgb * vec3(1.55, 1.45, 1.25) + vec3(0.02, 0.018, 0.01), dark.a);
    return max(colour, mix(dark, warm, beam));
}
