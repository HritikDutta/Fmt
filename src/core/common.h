#pragma once

template <typename T>
struct Type {};

template <typename T, typename... Args>
inline T make(Type<T>, Args... args);

template <typename T, typename... Args>
inline T make(Args... args)
{
    return make(Type<T> {}, args...);
}

template <typename T>
inline T copy(const T& other)
{
    return other;
}

template <typename T>
inline void swap(T& a, T& b)
{
    T temp = a;
    a = b;
    b = temp;
}

template <typename Type>
struct Distinct
{
    using ValueType = Type;

    static_assert(is_type_integral(ValueType), "Only integral types allowed!");

    ValueType value;

    constexpr Distinct() = default;

    constexpr explicit Distinct(ValueType value)
    : value(value)
    {
    }

    constexpr Distinct(const Distinct& other) = default;
    constexpr Distinct(Distinct&& other) = default;

    constexpr Distinct operator=(const Distinct& other)
    {
        value = other.value;
        return *this;
    }
    
    constexpr Distinct operator=(Distinct&& other)
    {
        value = other.value;
        return *this;
    }

    constexpr Distinct operator==(const Distinct& other) const
    {
        return value == other.value;
    }

    constexpr Distinct operator!=(const Distinct& other) const
    {
        return value != other.value;
    }

    constexpr explicit operator ValueType() const
    {
        return value;
    }
};