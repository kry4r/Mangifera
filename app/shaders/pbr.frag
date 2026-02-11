#version 450

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_uv;

layout(set = 0, binding = 0) uniform CameraUBO
{
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    vec4 camera_pos; // xyz=position, w=exposure
} ubo;

struct LightData
{
    vec4 position_type;    // xyz=position/direction, w=type (0=dir,1=point,2=spot)
    vec4 color_intensity;  // xyz=color, w=intensity
    vec4 params;           // xyz=spot_direction, w=range
    vec4 spot_params;      // x=inner_cos, y=outer_cos, zw=unused
};

layout(set = 0, binding = 1) uniform LightsUBO
{
    LightData lights[8];
    vec4 light_count;      // x=count, y=ibl_intensity, z=debug_mode
    mat4 shadow_view_proj;
} lights_ubo;

layout(push_constant) uniform PushConstants
{
    mat4 model;
    vec4 base_color;
    vec4 params;
} pc;

// IBL textures (set=1)
layout(set = 1, binding = 0) uniform samplerCube irradiance_map;
layout(set = 1, binding = 1) uniform samplerCube prefiltered_env;
layout(set = 1, binding = 2) uniform sampler2D brdf_lut;

// Shadow map (set=2)
layout(set = 2, binding = 0) uniform sampler2DShadow shadow_map;

layout(location = 0) out vec4 out_color;

const float PI = 3.14159265359;

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

vec3 fresnel_schlick(float cos_theta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

vec3 fresnel_schlick_roughness(float cos_theta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

float distribution_ggx(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float numerator = a2;
    float denominator = (NdotH2 * (a2 - 1.0) + 1.0);
    denominator = PI * denominator * denominator;
    return numerator / max(denominator, 0.0001);
}

float geometry_schlick_ggx(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float numerator = NdotV;
    float denominator = NdotV * (1.0 - k) + k;
    return numerator / denominator;
}

float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = geometry_schlick_ggx(NdotV, roughness);
    float ggx2 = geometry_schlick_ggx(NdotL, roughness);
    return ggx1 * ggx2;
}

// PCF shadow with 5x5 kernel + normal-based bias
float calc_shadow(vec3 world_pos, vec3 N)
{
    vec4 light_clip = lights_ubo.shadow_view_proj * vec4(world_pos, 1.0);
    vec3 proj = light_clip.xyz / light_clip.w;
    proj.xy = proj.xy * 0.5 + 0.5; // NDC [-1,1] to UV [0,1]

    // Outside shadow frustum = fully lit
    if (proj.z > 1.0 || proj.z < 0.0 ||
        proj.x < 0.0 || proj.x > 1.0 ||
        proj.y < 0.0 || proj.y > 1.0)
        return 1.0;

    // Normal-based bias: surfaces facing the light need less bias,
    // surfaces at grazing angles need more
    vec3 light_pos = lights_ubo.lights[0].position_type.xyz;
    vec3 L = normalize(light_pos - world_pos);
    float cos_theta = clamp(dot(N, L), 0.0, 1.0);
    float bias = mix(0.005, 0.0005, cos_theta);
    float compare_depth = proj.z - bias;

    float shadow = 0.0;
    vec2 texel_size = 1.0 / vec2(textureSize(shadow_map, 0));
    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            vec2 offset = vec2(float(x), float(y)) * texel_size;
            shadow += texture(shadow_map, vec3(proj.xy + offset, compare_depth));
        }
    }
    return shadow / 25.0;
}

void main()
{
    vec3 N = normalize(v_normal);
    if (!gl_FrontFacing) N = -N; // flip normal for back faces (two-sided rendering)
    vec3 V = normalize(ubo.camera_pos.xyz - v_position);
    float exposure = ubo.camera_pos.w;

    // Debug visualization modes: 0=RGB, 1=Normals, 2=Depth
    int debug_mode = int(lights_ubo.light_count.z);
    if (debug_mode == 1) {
        out_color = vec4(N * 0.5 + 0.5, 1.0);
        return;
    } else if (debug_mode == 2) {
        float near_plane = 0.01;
        float far_plane = 10.0;
        float ndc_depth = gl_FragCoord.z;
        float linear_depth = (2.0 * near_plane) / (far_plane + near_plane - ndc_depth * (far_plane - near_plane));
        out_color = vec4(vec3(linear_depth), 1.0);
        return;
    }

    vec3 albedo = pc.base_color.rgb;
    float metallic = clamp(pc.params.x, 0.0, 1.0);
    float roughness = clamp(pc.params.y, 0.05, 1.0);
    float ao = clamp(pc.params.z, 0.0, 1.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Compute shadow factor (PCF with normal-based bias)
    float shadow = (lights_ubo.light_count.w > 0.5) ? calc_shadow(v_position, N) : 1.0;

    vec3 Lo = vec3(0.0);
    int num_lights = int(lights_ubo.light_count.x);

    for (int i = 0; i < num_lights; ++i)
    {
        vec3 L;
        float attenuation = 1.0;
        float light_type = lights_ubo.lights[i].position_type.w;
        vec3 light_color = lights_ubo.lights[i].color_intensity.xyz;
        float light_intensity = lights_ubo.lights[i].color_intensity.w;

        if (light_type < 0.5) // Directional
        {
            L = normalize(-lights_ubo.lights[i].position_type.xyz);
        }
        else if (light_type < 1.5) // Point
        {
            vec3 to_light = lights_ubo.lights[i].position_type.xyz - v_position;
            float dist = length(to_light);
            L = to_light / max(dist, 0.0001);
            float range = lights_ubo.lights[i].params.w;
            if (range > 0.0) {
                attenuation = clamp(1.0 - (dist / range), 0.0, 1.0);
                attenuation *= attenuation;
            } else {
                attenuation = 1.0 / (dist * dist + 1.0);
            }
        }
        else // Spot
        {
            vec3 to_light = lights_ubo.lights[i].position_type.xyz - v_position;
            float dist = length(to_light);
            L = to_light / max(dist, 0.0001);
            float range = lights_ubo.lights[i].params.w;
            if (range > 0.0) {
                attenuation = clamp(1.0 - (dist / range), 0.0, 1.0);
                attenuation *= attenuation;
            }

            vec3 spot_dir = normalize(lights_ubo.lights[i].params.xyz);
            float theta = dot(L, -spot_dir);
            float inner_cos = lights_ubo.lights[i].spot_params.x;
            float outer_cos = lights_ubo.lights[i].spot_params.y;
            float epsilon = inner_cos - outer_cos;
            float spot_factor = clamp((theta - outer_cos) / max(epsilon, 0.0001), 0.0, 1.0);
            attenuation *= spot_factor;
        }

        vec3 H = normalize(V + L);

        float NDF = distribution_ggx(N, H, roughness);
        float G = geometry_smith(N, V, L, roughness);
        vec3 F = fresnel_schlick(max(dot(H, V), 0.0), F0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        float NdotL = max(dot(N, L), 0.0);
        vec3 radiance = light_color * light_intensity * attenuation;

        // Apply shadow to the first light (main shadow caster)
        float light_shadow = (i == 0) ? shadow : 1.0;
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * light_shadow;
    }

    // IBL ambient lighting
    float ibl_intensity = lights_ubo.light_count.y; // passed from CPU
    vec3 F_ibl = fresnel_schlick_roughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kS_ibl = F_ibl;
    vec3 kD_ibl = (1.0 - kS_ibl) * (1.0 - metallic);

    vec3 irradiance = texture(irradiance_map, N).rgb;
    vec3 diffuse_ibl = irradiance * albedo;

    vec3 R = reflect(-V, N);
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefiltered_color = textureLod(prefiltered_env, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 env_brdf = texture(brdf_lut, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specular_ibl = prefiltered_color * (F_ibl * env_brdf.x + env_brdf.y);

    vec3 ambient = (kD_ibl * diffuse_ibl + specular_ibl) * ao * ibl_intensity;
    vec3 color = ambient + Lo;

    // Exposure + ACES filmic tone mapping
    color *= exposure;
    color = aces_tonemap(color);
    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    out_color = vec4(color, pc.base_color.a);
}
