$input v_texcoord0

#include "bgfx_shader.sh"

// Half of a blur of the image of the beam: nine samples along u_blur.xy (a
// step in texture units), weighted as a bell; done once across and once down

SAMPLER2D(s_beam, 0);

uniform vec4 u_blur;

void main()
{
    float weights[5];
    weights[0] = 0.2270270;
    weights[1] = 0.1945946;
    weights[2] = 0.1216216;
    weights[3] = 0.0540540;
    weights[4] = 0.0162162;
    float sum = texture2D(s_beam, v_texcoord0).r * weights[0];
    for (int k = 1; k < 5; k++) {
        vec2 step = u_blur.xy * float(k);
        sum += texture2D(s_beam, v_texcoord0 + step).r * weights[k];
        sum += texture2D(s_beam, v_texcoord0 - step).r * weights[k];
    }
    gl_FragColor = vec4(sum, 0.0, 0.0, 1.0);
}
