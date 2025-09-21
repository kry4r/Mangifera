#pragma once
#include <string_view>
#include <bitset>
namespace mg::core
{
    struct Twig
    {
        virtual ~Twig() = default;
        virtual auto get_twig_type() -> std::string_view = 0;
    };

    template<typename T>
    struct Twig_Type_Id
    {
        static std::size_t value()
        {
            static std::size_t id = reinterpret_cast<std::size_t>(&id);
            return id;
        }
    };

    using Twig_Sign = std::bitset<64>;

    template<typename T>
    constexpr std::size_t get_component_index()
    {
        static_assert(std::is_base_of_v<Twig, T>, "T must derive from Twig");
        static std::size_t index = [] () {
            static std::size_t counter = 0;
            return counter++;
            }();
        assert(index < 64);
        return index;
    }
}
