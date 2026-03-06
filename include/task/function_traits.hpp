#ifndef TASK_FUNCTION_TRAITS_HPP
#define TASK_FUNCTION_TRAITS_HPP

#include <tuple>

namespace fn {

template <typename T>
struct RefToConstRef {
    using Type = T;
};

template <typename U>
struct RefToConstRef<std::reference_wrapper<U>> {
    using Type = std::reference_wrapper<const U>;
};

template <typename, bool Copyable = true>
struct TrivialUse : std::false_type {};

// Captured arguments of type-erased callable are not copy-assigned or move-assigned, and instead
// are destroyed followed by the copy/move constructor. Therefore, to be safe to simply memcpy
// and not need manager pointers, one checks copy/move constructor and destructor triviallity
template <typename Pl>
    requires(std::is_trivially_copy_constructible_v<Pl> &&
             std::is_trivially_move_constructible_v<Pl> && std::is_trivially_destructible_v<Pl>)
struct TrivialUse<Pl, true> : std::true_type {};

template <typename Pl>
    requires(std::is_trivially_move_constructible_v<Pl> && std::is_trivially_destructible_v<Pl>)
struct TrivialUse<Pl, false> : std::true_type {};

template <std::size_t N, typename Tuple, typename Indices = std::make_index_sequence<N>>
struct slice_first;

template <std::size_t N, typename... Args, std::size_t... Is>
struct slice_first<N, std::tuple<Args...>, std::index_sequence<Is...>> {
    using type = std::tuple<std::tuple_element_t<Is, std::tuple<Args...>>...>;
};

template <std::size_t N, typename Tuple, typename Indices = std::make_index_sequence<N>>
struct slice_last;

template <std::size_t N, typename... Args, std::size_t... Is>
struct slice_last<N, std::tuple<Args...>, std::index_sequence<Is...>> {
    static constexpr std::size_t kTotal = sizeof...(Args);
    static_assert(N <= kTotal, "Slice size N cannot exceed total tuple size.");
    using type = std::tuple<std::tuple_element_t<Is + (kTotal - N), std::tuple<Args...>>...>;
};

template <typename T>
struct FunctionTraits {
    static constexpr bool kValid{false};
};

template <typename T>
    requires(std::is_class_v<T> && requires { &T::operator(); })
struct FunctionTraits<T> : FunctionTraits<decltype(&T::operator())> {};

template <typename Ret, typename... Args>
struct FunctionTraits<Ret (*)(Args...)> {
    static constexpr bool kValid{true};
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
    using func_ptr = Ret (*)(Args...);
};

template <typename Ret, typename... Args>
struct FunctionTraits<Ret(Args...)> {
    static constexpr bool kValid{true};
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
    using func_ptr = Ret (*)(Args...);
};

template <typename Ret, typename... Args>
struct FunctionTraits<Ret (*)(Args...) noexcept> {
    static constexpr bool kValid{true};
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
    using func_ptr = Ret (*)(Args...);
};

template <typename Class, typename Ret, typename... Args>
struct FunctionTraits<Ret (Class::*)(Args...)> {
    static constexpr bool kValid{true};
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
    using func_ptr = Ret (*)(Args...);
};

template <typename Class, typename Ret, typename... Args>
struct FunctionTraits<Ret (Class::*)(Args...) const> {
    static constexpr bool kValid{true};
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
    using func_ptr = Ret (*)(Args...);
};

template <typename T>
    requires requires { typename T::Signature; }
struct FunctionTraits<T> {
    static constexpr bool kValid{true};
    using return_type = typename FunctionTraits<typename T::Signature>::return_type;
    using args_tuple = typename FunctionTraits<typename T::Signature>::args_tuple;
    using func_ptr = typename FunctionTraits<typename T::Signature>::func_ptr;
};

template <typename T>
concept HasFunctionTraits = requires { FunctionTraits<T>::kValid; };

}  // namespace fn

#endif