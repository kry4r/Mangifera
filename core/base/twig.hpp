#pragma once
#include <string_view>
#include <vector>
#include <cstdint>
#include <atomic>
#include <algorithm>

namespace mango::core
{
    using TwigID  = std::uint32_t;
    using TwigSet = std::vector<TwigID>;

    struct TwigBase {
        virtual ~TwigBase() = default;
        virtual std::string_view get_twig_type() const = 0;
        virtual TwigID get_twig_id() const = 0;
    };

    class TwigTypeRegistry {
    public:
        static TwigID register_type(std::string_view name) {
            TwigID id = next_id++;
            names.push_back(name);
            return id;
        }

        static std::string_view get_name(TwigID id) {
            return names.at(id);
        }

    private:
        inline static std::atomic<TwigID> next_id{0};
        inline static std::vector<std::string_view> names;
    };

    template<typename T>
    struct Twig : TwigBase {
        static TwigID type_id;

        static TwigID get_static_id() { return type_id; }

        std::string_view get_twig_type() const override {
            return TwigTypeRegistry::get_name(type_id);
        }

        TwigID get_twig_id() const override {
            return type_id;
        }
    };

    template<typename T>
    TwigID Twig<T>::type_id = TwigTypeRegistry::register_type(typeid(T).name());

    inline void insert_twig(TwigSet& set, TwigID id) {
        auto it = std::lower_bound(set.begin(), set.end(), id);
        if (it == set.end() || *it != id) {
            set.insert(it, id);
        }
    }

    inline void remove_twig(TwigSet& set, TwigID id) {
        auto it = std::lower_bound(set.begin(), set.end(), id);
        if (it != set.end() && *it == id) {
            set.erase(it);
        }
    }

    inline std::size_t hash_twigs(const TwigSet& twigs) {
        std::size_t h = 1469598103934665603ull;
        for (auto t : twigs) {
            h ^= t;
            h *= 1099511628211ull;
        }
        return h;
    }
}
