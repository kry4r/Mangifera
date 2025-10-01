#pragma once
#include <memory>
#include <vector>
#include <cstdint>

namespace mango::graphics
{
    class Buffer;
    class Texture;
    class Sampler;

    enum struct Descriptor_Type
    {
        uniform_buffer,
        storage_buffer,
        sampled_texture,
        storage_texture,
        sampler,
        combined_image_sampler,
    };

    struct Descriptor_Binding
    {
        uint32_t binding;                    // Binding index
        Descriptor_Type type;                // Descriptor type
        uint32_t count = 1;                  // Array size
        uint32_t shader_stages = 0;          // Shader stage flags
    };

    struct Descriptor_Set_Layout_Desc
    {
        std::vector<Descriptor_Binding> bindings;
    };

    class Descriptor_Set_Layout
    {
    public:
        virtual ~Descriptor_Set_Layout() = default;
        virtual const Descriptor_Set_Layout_Desc& get_desc() const = 0;
    };

    using Descriptor_Set_Layout_Handle = std::shared_ptr<Descriptor_Set_Layout>;

    struct Descriptor_Write
    {
        uint32_t binding;
        uint32_t array_element = 0;
        Descriptor_Type type;

        std::vector<std::shared_ptr<Buffer>> buffers;
        std::vector<std::shared_ptr<Texture>> textures;
        std::vector<std::shared_ptr<Sampler>> samplers;

        std::vector<uint64_t> buffer_offsets;
        std::vector<uint64_t> buffer_ranges;
    };

    struct Descriptor_Set
    {
    public:
        virtual ~Descriptor_Set() = default;

        virtual void update(const std::vector<Descriptor_Write>& writes) = 0;
    };

    using Descriptor_Set_Handle = std::shared_ptr<Descriptor_Set>;

} // namespace mango::graphics
