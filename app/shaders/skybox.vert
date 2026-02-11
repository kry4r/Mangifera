#version 450

layout(set = 0, binding = 0) uniform CameraUBO
{
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    vec4 camera_pos; // xyz=position, w=exposure
} ubo;

layout(location = 0) out vec3 v_dir;

// Fullscreen triangle: 3 vertices cover the entire screen
void main()
{
    // Generate fullscreen triangle vertices
    vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vec4 clip_pos = vec4(pos * 2.0 - 1.0, 1.0, 1.0);

    // Remove translation from view matrix (rotation only)
    mat4 inv_proj = inverse(ubo.proj);
    mat3 inv_view_rot = transpose(mat3(ubo.view)); // inverse of rotation = transpose

    // Unproject from clip space to world-space direction
    vec4 view_dir = inv_proj * clip_pos;
    v_dir = inv_view_rot * view_dir.xyz;

    gl_Position = vec4(pos * 2.0 - 1.0, 1.0, 1.0); // depth = 1.0 (far plane)
}
