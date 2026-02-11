#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(set = 0, binding = 0) uniform ShadowUBO
{
    mat4 light_vp;
} ubo;

layout(push_constant) uniform PushConstants
{
    mat4 model;
    vec4 base_color;
    vec4 params;
} pc;

void main()
{
    gl_Position = ubo.light_vp * pc.model * vec4(in_position, 1.0);
}
