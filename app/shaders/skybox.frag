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

// ACES filmic tone mapping (same as pbr.frag)
vec3 aces_tonemap(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 dir = normalize(v_dir);
    float exposure = ubo.camera_pos.w;

    // Sample environment at mip 0 (sharpest)
    vec3 color = textureLod(prefiltered_env, dir, 0.0).rgb;

    // Exposure + tone mapping + gamma
    color *= exposure;
    color = aces_tonemap(color);
    color = pow(color, vec3(1.0 / 2.2));

    out_color = vec4(color, 1.0);
}
