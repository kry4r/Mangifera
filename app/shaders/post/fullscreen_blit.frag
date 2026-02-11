#version 450

layout(location = 0) in vec2 v_uv;

layout(set = 0, binding = 0) uniform sampler2D hdr_input;

layout(push_constant) uniform PushConstants
{
    float exposure;
    uint  tone_map_mode; // 0 = ACES, 1 = Reinhard, 2 = passthrough (compute post-process already applied)
} pc;

layout(location = 0) out vec4 out_color;

// ACES filmic tone mapping
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
    vec3 color = texture(hdr_input, v_uv).rgb;

    if (pc.tone_map_mode == 2) {
        // Passthrough: compute post-processing already did tone mapping + gamma
        out_color = vec4(color, 1.0);
        return;
    }

    // Exposure
    color *= pc.exposure;

    // Tone mapping
    if (pc.tone_map_mode == 0) {
        color = aces_tonemap(color);
    } else {
        // Reinhard
        color = color / (color + vec3(1.0));
    }

    // Gamma correction (explicit, matching current pbr.frag behavior)
    color = pow(color, vec3(1.0 / 2.2));

    out_color = vec4(color, 1.0);
}
