#pragma once

namespace mango::core
{
    template <typename T>
    struct Singleton
    {
    private:
        static T* instance;

    protected:
        Singleton() = default;
        Singleton(const Singleton&) = delete;
        Singleton& operator=(const Singleton&) = delete;
        Singleton(Singleton&&) = delete;
        Singleton& operator=(Singleton&&) = delete;

    public:
        static T* current_instance()
        {
            if (instance == nullptr) {
                instance = new T();
            }
            return instance;
        }

        static void destroy_instance()
        {
            delete instance;
            instance = nullptr;
        }

        static bool has_instance()
        {
            return instance != nullptr;
        }

        virtual ~Singleton() = default;
    };

    // Static member definition
    template <typename T>
    T* Singleton<T>::instance = nullptr;
}
