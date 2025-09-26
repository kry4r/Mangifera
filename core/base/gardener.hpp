#pragma once
#include "base/entity.hpp"
#include "twig.hpp"
#include <functional>
#include <vector>
#include <memory>
#include <type_traits>

namespace mango::core
{
    // Forward declaration
    class Scene_Manager;

    // Gardener lifecycle hooks
    enum class Gardener_Event
    {
        PRE_GRAFT,    // Before attaching twig
        POST_GRAFT,   // After attaching twig
        PRE_LOPPER,   // Before detaching twig
        POST_LOPPER   // After detaching twig
    };

    // Base gardener interface
    class Gardener_Base
    {
    public:
        virtual ~Gardener_Base() = default;

        virtual void on_pre_graft(Entity entity, std::size_t twig_type_id) {}
        virtual void on_post_graft(Entity entity, std::size_t twig_type_id) {}
        virtual void on_pre_lopper(Entity entity, std::size_t twig_type_id) {}
        virtual void on_post_lopper(Entity entity, std::size_t twig_type_id) {}

        virtual void update(float delta_time) {}
        virtual void fixed_update(float fixed_delta_time) {}

        virtual void set_enabled(bool enabled) { enabled_ = enabled; }
        virtual bool is_enabled() const { return enabled_; }

    protected:
        bool enabled_ = true;
    };

    // Template gardener service - like Java service pattern
    template<typename Derived>
    class Gardener : public Gardener_Base
    {
    private:
        static std::unique_ptr<Derived> instance_;
        static bool initialized_;

    protected:
        Gardener() = default;

    public:
        // Singleton-like access but with dependency injection capability
        static Derived* get_instance()
        {
            if (!instance_) {
                instance_ = std::make_unique<Derived>();
                initialized_ = true;
            }
            return instance_.get();
        }

        // Allow custom instance injection (useful for testing)
        static void set_instance(std::unique_ptr<Derived> custom_instance)
        {
            instance_ = std::move(custom_instance);
            initialized_ = true;
        }

        static bool is_initialized() { return initialized_; }

        static void shutdown()
        {
            instance_.reset();
            initialized_ = false;
        }

        // CRTP access to derived implementation
        Derived* as_derived() { return static_cast<Derived*>(this); }
        const Derived* as_derived() const { return static_cast<const Derived*>(this); }
    };

    // Static member definitions
    template<typename Derived>
    std::unique_ptr<Derived> Gardener<Derived>::instance_ = nullptr;

    template<typename Derived>
    bool Gardener<Derived>::initialized_ = false;

    // Gardener registry for managing all gardeners
    class Gardener_Registry
    {
    private:
        std::vector<std::unique_ptr<Gardener_Base>> gardeners_;
        std::vector<Gardener_Base*> active_gardeners_;

    public:
        template<typename T, typename... Args>
        T* register_gardener(Args&&... args)
        {
            static_assert(std::is_base_of_v<Gardener_Base, T>, "T must derive from Gardener_Base");

            auto gardener = std::make_unique<T>(std::forward<Args>(args)...);
            T* raw_ptr = gardener.get();
            gardeners_.push_back(std::move(gardener));
            active_gardeners_.push_back(raw_ptr);
            return raw_ptr;
        }

        template<typename T>
        T* get_gardener()
        {
            for (auto* gardener : active_gardeners_) {
                if (auto* typed_gardener = dynamic_cast<T*>(gardener)) {
                    return typed_gardener;
                }
            }
            return nullptr;
        }

        void update_all(float delta_time)
        {
            for (auto* gardener : active_gardeners_) {
                if (gardener->is_enabled()) {
                    gardener->update(delta_time);
                }
            }
        }

        void fixed_update_all(float fixed_delta_time)
        {
            for (auto* gardener : active_gardeners_) {
                if (gardener->is_enabled()) {
                    gardener->fixed_update(fixed_delta_time);
                }
            }
        }

        // Trigger lifecycle events
        void trigger_event(Gardener_Event event, Entity entity, std::size_t twig_type_id)
        {
            for (auto* gardener : active_gardeners_) {
                if (!gardener->is_enabled()) continue;

                switch (event) {
                    case Gardener_Event::PRE_GRAFT:
                        gardener->on_pre_graft(entity, twig_type_id);
                        break;
                    case Gardener_Event::POST_GRAFT:
                        gardener->on_post_graft(entity, twig_type_id);
                        break;
                    case Gardener_Event::PRE_LOPPER:
                        gardener->on_pre_lopper(entity, twig_type_id);
                        break;
                    case Gardener_Event::POST_LOPPER:
                        gardener->on_post_lopper(entity, twig_type_id);
                        break;
                }
            }
        }

        void clear()
        {
            active_gardeners_.clear();
            gardeners_.clear();
        }
    };
}
