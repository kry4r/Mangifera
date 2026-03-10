#include "graphics/render-resource/buffer.hpp"
#include "graphics/render-resource/texture.hpp"
#include "tests/test_macros.hpp"

int main()
{
    using namespace mango::graphics;

    Buffer_Desc buffer{};
    buffer.debug_name = "camera-ubo";
    buffer.transient = false;
    TEST_ASSERT(buffer.debug_name == "camera-ubo");
    TEST_ASSERT(!buffer.transient);

    Texture_Desc texture{};
    texture.debug_name = "gbuffer-normal";
    texture.aliasable = true;
    TEST_ASSERT(texture.debug_name == "gbuffer-normal");
    TEST_ASSERT(texture.aliasable);

    return 0;
}
