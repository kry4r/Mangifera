#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(set = 0, binding = 0) uniform CameraUBO
{
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    vec4 camera_pos;
} ubo;

layout(push_constant) uniform PushConstants
{
    mat4 model;
    vec4 base_color;
    vec4 params;
} pc;

layout(location = 0) out vec3 v_position;
layout(location = 1) out vec3 v_normal;
layout(location = 2) out vec2 v_uv;

void main()
{
    vec4 world_pos = pc.model * vec4(in_position, 1.0);
    v_position = world_pos.xyz;
    v_normal = transpose(inverse(mat3(pc.model))) * in_normal;
    v_uv = in_uv;
    gl_Position = ubo.view_proj * world_pos;
}
