#version 450

layout(location = 0) in vec3 v_dir;

layout(set = 0, binding = 0) uniform CameraUBO
{
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    vec4 camera_pos; // xyz=position, w=exposure
} ubo;

// IBL textures (set=1)
layout(set = 1, binding = 0) uniform samplerCube irradiance_map;
layout(set = 1, binding = 1) uniform samplerCube prefiltered_env;
layout(set = 1, binding = 2) uniform sampler2D brdf_lut;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_normal; // G-buffer (unused for skybox)

void main()
{
    vec3 dir = normalize(v_dir);

    // Sample environment at mip 0 (sharpest)
    vec3 color = textureLod(prefiltered_env, dir, 0.0).rgb;

    // Output LINEAR HDR (tone mapping + gamma applied in post-processing blit pass)
    out_color = vec4(color, 1.0);

    // Skybox has no meaningful normal — write zero
    out_normal = vec4(0.0);
}
