#pragma once

// yoinked from
// https://stackoverflow.com/questions/34672441/stdis-base-of-for-template-classes
#include <type_traits>
template <template <typename...> class base, typename derived>
struct IsBaseOfTemplateImpl {
    template <typename... Ts>
    static constexpr std::true_type test(const base<Ts...>*);
    static constexpr std::false_type test(...);
    using type = decltype(test(std::declval<derived*>()));
};

template <template <typename...> class base, typename derived>
using is_base_of_template = typename IsBaseOfTemplateImpl<base, derived>::type;
