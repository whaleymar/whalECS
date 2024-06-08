#pragma once

// only works on gcc and clang i think
// credit: https://stackoverflow.com/a/68523970
// usage: `auto nameOfT = type_of<T>();`

#include <source_location>
#include <string_view>

template <typename T>
consteval auto func_name() {
    const auto& loc = std::source_location::current();
    return loc.function_name();
}

template <typename T>
consteval std::string_view type_of_impl_() {
    constexpr std::string_view functionName = func_name<T>();
    // func_name_ is 'consteval auto func_name() [with T = ...]'
    // (the "with" is sometimes not there)

    constexpr auto prefix = std::string_view{"T = "};
    constexpr auto suffix = std::string_view{"]"};
    constexpr auto start = functionName.find(prefix) + prefix.size();
    constexpr auto end = functionName.rfind(suffix);
    static_assert(start < end);

    return functionName.substr(start, (end - start));
}

template <typename T>
constexpr std::string_view type_of(T&& arg) {
    return type_of_impl_<decltype(arg)>();
}

template <typename T>
constexpr std::string_view type_of() {
    return type_of_impl_<T>();
}
