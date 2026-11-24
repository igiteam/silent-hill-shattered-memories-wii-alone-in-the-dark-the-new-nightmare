// Camera space (x right, y depth, z up) to the window, as the game projects:
// x and z over the depth y. u_camera: x, y scale of x and z; z, w: depth
uniform vec4 u_camera;

vec4 cameraToClip(vec3 position)
{
    return vec4(u_camera.x * position.x, u_camera.y * position.z, u_camera.z * position.y + u_camera.w, position.y);
}
