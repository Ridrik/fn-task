#ifndef TASK_PAYLOAD_HPP
#define TASK_PAYLOAD_HPP

#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#if defined(__GNUC__)
#define FORCE_INLINE [[gnu::always_inline]] inline
#elif defined(__clang__)
#define FORCE_INLINE [[clang::always_inline]] inline
#elif defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline
#endif

namespace fn {

template <typename T>
    requires std::is_reference_v<T>
struct CallArgTag {
    T val;

    constexpr operator T() const noexcept {
        return std::forward<T>(val);
    }
};

template <typename T>
struct IsCallArg : std::false_type {};

template <typename T>
struct IsCallArg<CallArgTag<T>> : std::true_type {};

template <typename T>
static constexpr auto IsCallArgB = IsCallArg<T>::value;  // NOLINT

namespace detail {

template <typename... Ts>
struct TypeList {};

template <typename F, typename TupleOfTested, typename TupleOfRemaining>
struct invocable_combinator;

// Base case: No arguments remaining, test std::is_invocable with the accumulated tested types
template <typename F, typename... Tested>
struct invocable_combinator<F, TypeList<Tested...>, TypeList<>> {
    static constexpr bool kValid = std::is_invocable_v<F, Tested...>;
    static constexpr bool kNoThrow = std::is_nothrow_invocable_v<F, Tested...>;
    using Selected = TypeList<Tested...>;
};

// Recursive step: Take the next argument T, branch out into its common value/cv-ref variations
template <typename F, typename... Tested, typename T, typename... Rest>
    requires(!IsCallArgB<std::remove_cvref_t<T>>)
struct invocable_combinator<F, TypeList<Tested...>, TypeList<T, Rest...>> {
   private:
    using LRef = invocable_combinator<F, TypeList<Tested..., std::add_lvalue_reference_t<T>>,
                                      TypeList<Rest...>>;

    template <bool, bool, typename Dummy = void>
    struct Select {
        using RRef = invocable_combinator<
            F, TypeList<Tested..., std::add_rvalue_reference_t<std::remove_reference_t<T>>>,
            TypeList<Rest...>>;

        static constexpr bool kValid = RRef::kValid;
        static constexpr bool kNoThrow = RRef::kNoThrow;
        using Selected = typename RRef::Selected;
    };

    template <bool NoThrow, typename Dummy>
    struct Select<true, NoThrow, Dummy> {
        static constexpr bool kValid = true;
        static constexpr bool kNoThrow = NoThrow;
        using Selected = typename LRef::Selected;
    };

    using Result = Select<LRef::kValid, LRef::kNoThrow>;

   public:
    static constexpr bool kValid = Result::kValid;
    static constexpr bool kNoThrow = Result::kNoThrow;
    using Selected = typename Result::Selected;
};

template <typename F, typename... Tested, typename T, typename... Rest>
    requires(IsCallArgB<std::remove_cvref_t<T>>)
struct invocable_combinator<F, TypeList<Tested...>, TypeList<T, Rest...>>
    : invocable_combinator<F, TypeList<Tested..., T>, TypeList<Rest...>> {};

template <typename F, typename Tuple, typename... Selected, std::size_t... I>
constexpr decltype(auto) invoke_selected(
    F&& f, Tuple& args, TypeList<Selected...> /*unused*/,
    std::index_sequence<I...> /*unused*/) noexcept(std::is_nothrow_invocable_v<F&&, Selected...>) {
    return std::invoke(std::forward<F>(f), std::forward_like<Selected>(std::get<I>(args))...);
}

template <auto F, typename Tuple, typename... Selected, std::size_t... I>
constexpr decltype(auto) invoke_selected(
    Tuple& args, TypeList<Selected...> /*unused*/,
    std::index_sequence<I...> /*unused*/) noexcept(std::is_nothrow_invocable_v<decltype(F),
                                                                               Selected...>) {
    return std::invoke(F, std::forward_like<Selected>(std::get<I>(args))...);
}

template <typename... T>
struct AlwaysFalse : std::false_type {};

template <typename T>
struct is_valid : std::bool_constant<T::kValid> {};  // NOLINT

template <typename T>
struct is_no_throw : std::bool_constant<is_valid<T>::value && T::kNoThrow> {};  // NOLINT

}  // namespace detail

template <typename F, typename... Args>
struct is_compatible_invocable : std::false_type {};  // NOLINT

template <typename F, typename... Args>
    requires(std::disjunction_v<
             detail::is_valid<
                 detail::invocable_combinator<F, detail::TypeList<>, detail::TypeList<Args...>>>,
             detail::is_valid<detail::invocable_combinator<
                 std::add_lvalue_reference_t<F>, detail::TypeList<>, detail::TypeList<Args...>>>,
             detail::is_valid<detail::invocable_combinator<
                 std::add_rvalue_reference_t<std::remove_reference_t<F>>, detail::TypeList<>,
                 detail::TypeList<Args...>>>>)
struct is_compatible_invocable<F, Args...> : std::true_type {};

template <typename F, typename... Args>
struct is_compatible_nothrow_invocable : std::false_type {};  // NOLINT

template <typename F, typename... Args>
    requires(std::disjunction_v<
             detail::is_no_throw<
                 detail::invocable_combinator<F, detail::TypeList<>, detail::TypeList<Args...>>>,
             detail::is_no_throw<detail::invocable_combinator<
                 std::add_lvalue_reference_t<F>, detail::TypeList<>, detail::TypeList<Args...>>>,
             detail::is_no_throw<detail::invocable_combinator<
                 std::add_rvalue_reference_t<std::remove_reference_t<F>>, detail::TypeList<>,
                 detail::TypeList<Args...>>>>)
struct is_compatible_nothrow_invocable<F, Args...> : std::true_type {};

// Main trait: checks if there is any valid combination of cv/ref qualifiers
template <typename F, typename... Args>
constexpr bool is_compatible_invocable_b =  // NOLINT
    is_compatible_invocable<F, Args...>::value;

template <typename F, typename... Args>
constexpr bool is_compatible_nothrow_invocable_b =  // NOLINT
    is_compatible_nothrow_invocable<F, Args...>::value;

template <typename F, typename... Args>
    requires detail::invocable_combinator<F, detail::TypeList<>, detail::TypeList<Args...>>::kValid
constexpr decltype(auto) invoke_compatible(F&& f, Args&&... args) noexcept(
    detail::invocable_combinator<F, detail::TypeList<>, detail::TypeList<Args...>>::kNoThrow) {
    using Selection =
        detail::invocable_combinator<F, detail::TypeList<>, detail::TypeList<Args...>>;
    auto references = std::forward_as_tuple(std::forward<Args>(args)...);
    return detail::invoke_selected(std::forward<F>(f), references, typename Selection::Selected{},
                                   std::index_sequence_for<Args...>{});
}

template <typename F, typename... Args>
    requires(!detail::invocable_combinator<F, detail::TypeList<>,
                                           detail::TypeList<Args...>>::kValid) &&
            detail::invocable_combinator<std::add_lvalue_reference<F>, detail::TypeList<>,
                                         detail::TypeList<Args...>>::kValid
constexpr decltype(auto) invoke_compatible(
    F&& f, Args&&... args) noexcept(  // NOLINT(cppcoreguidelines-missing-std-forward)
    detail::invocable_combinator<std::add_lvalue_reference<F>, detail::TypeList<>,
                                 detail::TypeList<Args...>>::kNoThrow) {
    using Selection = detail::invocable_combinator<std::add_lvalue_reference<F>, detail::TypeList<>,
                                                   detail::TypeList<Args...>>;
    auto references = std::forward_as_tuple(std::forward<Args>(args)...);
    return detail::invoke_selected(f, references, typename Selection::Selected{},
                                   std::index_sequence_for<Args...>{});
}

template <typename F, typename... Args>
    requires(!detail::invocable_combinator<F, detail::TypeList<>,
                                           detail::TypeList<Args...>>::kValid) &&
            (!detail::invocable_combinator<std::add_lvalue_reference<F>, detail::TypeList<>,
                                           detail::TypeList<Args...>>::kValid) &&
            detail::invocable_combinator<std::add_rvalue_reference_t<std::remove_reference_t<F>>,
                                         detail::TypeList<>, detail::TypeList<Args...>>::kValid
constexpr decltype(auto) invoke_compatible(
    F&& f, Args&&... args) noexcept(  // NOLINT(cppcoreguidelines-missing-std-forward)
    detail::invocable_combinator<std::add_rvalue_reference_t<std::remove_reference_t<F>>,
                                 detail::TypeList<>, detail::TypeList<Args...>>::kNoThrow) {
    using Selection =
        detail::invocable_combinator<std::add_rvalue_reference_t<std::remove_reference_t<F>>,
                                     detail::TypeList<>, detail::TypeList<Args...>>;
    auto references = std::forward_as_tuple(std::forward<Args>(args)...);
    return detail::invoke_selected(std::move(f), references,  // NOLINT
                                   typename Selection::Selected{},
                                   std::index_sequence_for<Args...>{});
}

template <auto F, typename... Args>
constexpr decltype(auto) invoke_compatible(Args&&... args) {
    using Selection =
        detail::invocable_combinator<decltype(F), detail::TypeList<>, detail::TypeList<Args...>>;
    static_assert(Selection::kValid,
                  "Callable is not invocable with any compatible cv/ref combination");

    auto references = std::forward_as_tuple(std::forward<Args>(args)...);

    return detail::invoke_selected<F>(references, typename Selection::Selected{},
                                      std::index_sequence_for<Args...>{});
}

template <typename... Ts>
struct PayloadTuple;

template <typename>
struct IsPayloadTuple : std::false_type {};

template <typename... Ts>
struct IsPayloadTuple<PayloadTuple<Ts...>> : std::true_type {};

template <typename>
struct IsRefPayloadTuple : std::false_type {};

template <typename... Ts>
    requires((std::is_reference_v<Ts> && ...))
struct IsRefPayloadTuple<PayloadTuple<Ts...>> : std::true_type {};

template <typename T>
static constexpr auto kIsRefPayloadTuple = IsRefPayloadTuple<T>::value;

template <typename T>
struct unwrap_reference_qualifiers {  // NOLINT
    using type = T;
};

template <typename T>
struct unwrap_reference_qualifiers<std::reference_wrapper<T>> {
    using type = T;
};

template <typename T>
struct unwrap_reference_qualifiers<std::reference_wrapper<T>&&> {
    using type = T&&;
};

template <typename T>
struct unwrap_reference_qualifiers<std::reference_wrapper<T> const&&> {
    using type = T&&;
};

template <typename T>
struct unwrap_reference_qualifiers<const std::reference_wrapper<T>> {
    using type = T;
};

template <typename T>
struct unwrap_reference_qualifiers<std::reference_wrapper<T>&> {
    using type = T&;
};

template <typename T>
struct unwrap_reference_qualifiers<const std::reference_wrapper<T>&> {
    using type = T&;
};

template <typename T>
using unwrap_reference_qualifiers_t = typename unwrap_reference_qualifiers<T>::type;

template <bool Ignore = false>
constexpr decltype(auto) unwrap_arg_on_invocation(auto&& arg) noexcept {
    return std::forward<decltype(arg)>(arg);
}
template <bool Ignore = false, typename T>
    requires(!Ignore)
constexpr decltype(auto) unwrap_arg_on_invocation(
    std::reference_wrapper<T>&& arg) noexcept {  // NOLINT
    return std::move(arg.get());
}

template <>
struct PayloadTuple<> {
    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) apply([[maybe_unused]] this auto&& /*self*/,
                                                ArgsSoFar&&... args) {
        static_assert(std::is_invocable_v<decltype(F), decltype(args)...>,
                      "F is not callable with given arguments");
        return F(unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...);
    }

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyCompatible([[maybe_unused]] this auto&& /*self*/,
                                                          ArgsSoFar&&... args) {
        return fn::invoke_compatible<F>(unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...);
    }

    template <auto F, bool Ignore = false, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgs(ArgsSoFar&&... args) && {
        static_assert(std::is_invocable_v<decltype(F), decltype(args)...>,
                      "F is not callable with given arguments");
        return F(unwrap_arg_on_invocation<Ignore>(std::forward<ArgsSoFar>(args))...);
    }

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgsCompatible(ArgsSoFar&&... args) && {
        return fn::invoke_compatible<F>(unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...);
    }

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFront([[maybe_unused]] this auto&& /*self*/,
                                                     auto&& callArgs, ArgsSoFar&&... args)
        requires(IsPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        return std::forward<decltype(callArgs)>(callArgs).template applyAsArgs<F>(
            std::forward<ArgsSoFar>(args)...);
    }

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFrontCompatible(
        [[maybe_unused]] this auto&& /*self*/, auto&& callArgs, ArgsSoFar&&... args)
        requires(IsPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        return std::forward<decltype(callArgs)>(callArgs).template applyAsArgsCompatible<F>(
            std::forward<ArgsSoFar>(args)...);
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) apply([[maybe_unused]] this auto&& /*self*/, F&& f,
                                                ArgsSoFar&&... args) {
        static_assert(std::is_invocable_v<unwrap_reference_qualifiers_t<F>,
                                          decltype(unwrap_arg_on_invocation(
                                              std::declval<decltype(args)>()))...>,
                      "F is not callable with given arguments");
        return std::invoke(std::forward<unwrap_reference_qualifiers_t<F>>(f),
                           unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...);
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyCompatible([[maybe_unused]] this auto&& /*self*/,
                                                          F&& f, ArgsSoFar&&... args) {
        return fn::invoke_compatible(std::forward<unwrap_reference_qualifiers_t<F>>(f),
                                     unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...);
    }

    template <bool Ignore = false, typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgs(F&& f, ArgsSoFar&&... args) && {
        static_assert(std::is_invocable_v<unwrap_reference_qualifiers_t<F>,
                                          decltype(unwrap_arg_on_invocation(
                                              std::declval<decltype(args)>()))...>,
                      "F is not callable with given arguments");
        return std::invoke(std::forward<unwrap_reference_qualifiers_t<F>>(f),
                           unwrap_arg_on_invocation<Ignore>(std::forward<ArgsSoFar>(args))...);
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgsCompatible(F&& f, ArgsSoFar&&... args) && {
        return fn::invoke_compatible(std::forward<unwrap_reference_qualifiers_t<F>>(f),
                                     unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...);
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFront([[maybe_unused]] this auto&& /*self*/, F&& f,
                                                     auto&& callArgs, ArgsSoFar&&... args)
        requires(IsPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        return std::forward<decltype(callArgs)>(callArgs).template applyAsArgs<F>(
            std::forward<F>(f), std::forward<ArgsSoFar>(args)...);
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFrontCompatible(
        [[maybe_unused]] this auto&& /*self*/, F&& f, auto&& callArgs, ArgsSoFar&&... args)
        requires(IsPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        return std::forward<decltype(callArgs)>(callArgs).template applyAsArgsCompatible<F>(
            std::forward<F>(f), std::forward<ArgsSoFar>(args)...);
    }
};

template <typename T, typename... Ts>
struct PayloadTuple<T, Ts...> {
    T head;
    [[no_unique_address]] PayloadTuple<Ts...> tail{};

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) apply(this auto&& self, ArgsSoFar&&... args) {
        using Self = decltype(self);
        if constexpr (sizeof...(Ts) == 0) {
            static_assert(std::is_invocable_v<decltype(F), decltype(args)...,
                                              decltype(unwrap_arg_on_invocation(
                                                  std::forward_like<Self>(self.head)))>,
                          "F is not callable with given arguments");
            return F(unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...,
                     unwrap_arg_on_invocation(std::forward_like<Self>(self.head)));
        } else {
            return std::forward_like<Self>(self.tail).template apply<F>(
                std::forward<ArgsSoFar>(args)..., std::forward_like<Self>(self.head));
        }
    }

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyCompatible(this auto&& self, ArgsSoFar&&... args) {
        using Self = decltype(self);
        if constexpr (sizeof...(Ts) == 0) {
            return fn::invoke_compatible<F>(
                unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...,
                unwrap_arg_on_invocation(std::forward_like<Self>(self.head)));
        } else {
            return std::forward_like<Self>(self.tail).template applyCompatible<F>(
                std::forward<ArgsSoFar>(args)..., std::forward_like<Self>(self.head));
        }
    }

    template <auto F, bool Ignore = false, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgs(ArgsSoFar&&... args) && {
        if constexpr (sizeof...(Ts) == 0) {
            static_assert(
                std::is_invocable_v<
                    decltype(F),
                    decltype(unwrap_arg_on_invocation<Ignore>(std::forward<ArgsSoFar>(args)))...,
                    decltype(unwrap_arg_on_invocation<Ignore>(std::forward<T>(head)))>,
                "F is not callable with given arguments");
            return F(unwrap_arg_on_invocation<Ignore>(std::forward<ArgsSoFar>(args))...,
                     unwrap_arg_on_invocation<Ignore>(std::forward<T>(head)));
        } else {
            return std::move(tail).template applyAsArgs<F, Ignore>(std::forward<ArgsSoFar>(args)...,
                                                                   std::forward<T>(head));
        }
    }

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgsCompatible(ArgsSoFar&&... args) && {
        if constexpr (sizeof...(Ts) == 0) {
            return fn::invoke_compatible<F>(
                unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...,
                unwrap_arg_on_invocation(std::forward<T>(head)));
        } else {
            return std::move(tail).template applyAsArgsCompatible<F>(
                std::forward<ArgsSoFar>(args)..., std::forward<T>(head));
        }
    }

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFront(this auto&& self, auto&& callArgs,
                                                     ArgsSoFar&&... args)
        requires(IsPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        using Self = decltype(self);
        if constexpr (sizeof...(Ts) == 0) {
            return std::forward<decltype(callArgs)>(callArgs).template applyAsArgs<F>(
                std::forward<ArgsSoFar>(args)..., std::forward_like<Self>(self.head));
        } else {
            return std::forward_like<Self>(self.tail).template applyFront<F>(
                std::forward<decltype(callArgs)>(callArgs), std::forward<ArgsSoFar>(args)...,
                std::forward_like<Self>(self.head));
        }
    }

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFrontCompatible(this auto&& self, auto&& callArgs,
                                                               ArgsSoFar&&... args)
        requires(IsPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        using Self = decltype(self);
        if constexpr (sizeof...(Ts) == 0) {
            return std::forward<decltype(callArgs)>(callArgs).template applyAsArgsCompatible<F>(
                std::forward<ArgsSoFar>(args)..., std::forward_like<Self>(self.head));
        } else {
            return std::forward_like<Self>(self.tail).template applyFrontCompatible<F>(
                std::forward<decltype(callArgs)>(callArgs), std::forward<ArgsSoFar>(args)...,
                std::forward_like<Self>(self.head));
        }
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) apply(this auto&& self, F&& f, ArgsSoFar&&... args) {
        using Self = decltype(self);
        if constexpr (sizeof...(Ts) == 0) {
            static_assert(
                std::is_invocable_v<
                    unwrap_reference_qualifiers_t<F>,
                    decltype(unwrap_arg_on_invocation(std::declval<decltype(args)>()))...,
                    decltype(unwrap_arg_on_invocation(std::forward_like<Self>(self.head)))>,
                "F is not callable with given arguments");
            return std::invoke(std::forward<unwrap_reference_qualifiers_t<F>>(f),
                               unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...,
                               unwrap_arg_on_invocation(std::forward_like<Self>(self.head)));
        } else {
            return std::forward_like<Self>(self.tail).apply(std::forward<F>(f),
                                                            std::forward<ArgsSoFar>(args)...,
                                                            std::forward_like<Self>(self.head));
        }
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyCompatible(this auto&& self, F&& f,
                                                          ArgsSoFar&&... args) {
        using Self = decltype(self);
        if constexpr (sizeof...(Ts) == 0) {
            return fn::invoke_compatible(
                std::forward<unwrap_reference_qualifiers_t<F>>(f),
                unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...,
                unwrap_arg_on_invocation(std::forward_like<Self>(self.head)));
        } else {
            return std::forward_like<Self>(self.tail).applyCompatible(
                std::forward<F>(f), std::forward<ArgsSoFar>(args)...,
                std::forward_like<Self>(self.head));
        }
    }

    template <bool Ignore = false, typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgs(F&& f, ArgsSoFar&&... args) && {
        if constexpr (sizeof...(Ts) == 0) {
            static_assert(
                std::is_invocable_v<
                    unwrap_reference_qualifiers_t<F>,
                    decltype(unwrap_arg_on_invocation<Ignore>(std::declval<decltype(args)>()))...,
                    decltype(unwrap_arg_on_invocation<Ignore>(std::forward<T>(head)))>,
                "F is not callable with given arguments");
            return std::invoke(std::forward<unwrap_reference_qualifiers_t<F>>(f),
                               unwrap_arg_on_invocation<Ignore>(std::forward<ArgsSoFar>(args))...,
                               unwrap_arg_on_invocation<Ignore>(std::forward<T>(head)));
        } else {
            return std::move(tail).template applyAsArgs<Ignore>(
                std::forward<F>(f), std::forward<ArgsSoFar>(args)..., std::forward<T>(head));
        }
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgsCompatible(F&& f, ArgsSoFar&&... args) && {
        if constexpr (sizeof...(Ts) == 0) {
            return fn::invoke_compatible(std::forward<unwrap_reference_qualifiers_t<F>>(f),
                                         unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...,
                                         unwrap_arg_on_invocation(std::forward<T>(head)));
        } else {
            return std::move(tail).applyAsArgsCompatible(
                std::forward<F>(f), std::forward<ArgsSoFar>(args)..., std::forward<T>(head));
        }
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFront(this auto&& self, F&& f, auto&& callArgs,
                                                     ArgsSoFar&&... args)
        requires(IsPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        using Self = decltype(self);
        if constexpr (sizeof...(Ts) == 0) {
            return std::forward<decltype(callArgs)>(callArgs).applyAsArgs(
                std::forward<F>(f), std::forward<ArgsSoFar>(args)...,
                std::forward_like<Self>(self.head));
        } else {
            return std::forward_like<Self>(self.tail).template applyFront<F>(
                std::forward<F>(f), std::forward<decltype(callArgs)>(callArgs),
                std::forward<ArgsSoFar>(args)..., std::forward_like<Self>(self.head));
        }
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFrontCompatible(this auto&& self, F&& f,
                                                               auto&& callArgs, ArgsSoFar&&... args)
        requires(IsPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        using Self = decltype(self);
        if constexpr (sizeof...(Ts) == 0) {
            return std::forward<decltype(callArgs)>(callArgs).template applyAsArgsCompatible<F>(
                std::forward<F>(f), std::forward<ArgsSoFar>(args)...,
                std::forward_like<Self>(self.head));
        } else {
            return std::forward_like<Self>(self.tail).template applyFrontCompatible<F>(
                std::forward<F>(f), std::forward<decltype(callArgs)>(callArgs),
                std::forward<ArgsSoFar>(args)..., std::forward_like<Self>(self.head));
        }
    }
};

template <std::size_t I, typename... Ts>
struct PayloadTupleGetter;

template <typename T>
struct RefWrapToRef {
    using Type = T;
};

template <typename T>
struct RefWrapToRef<std::reference_wrapper<T>> {
    using Type = T&;
};

template <typename T, typename... Ts>
struct PayloadTupleGetter<0, T, Ts...> {
    static constexpr T& get(PayloadTuple<T, Ts...>& tuple) {
        return tuple.head;
    }

    static constexpr const T& get(const PayloadTuple<T, Ts...>& tuple) {
        return tuple.head;
    }

    static constexpr RefWrapToRef<T>::Type& getWrapToRef(PayloadTuple<T, Ts...>& tuple) {
        return tuple.head;
    }

    static constexpr const RefWrapToRef<T>::Type& getWrapToRef(
        const PayloadTuple<T, Ts...>& tuple) {
        return tuple.head;
    }
};

template <std::size_t I, typename T, typename... Ts>
    requires(I > 0)
struct PayloadTupleGetter<I, T, Ts...> {
    static constexpr decltype(auto) get(PayloadTuple<T, Ts...>& tuple) {
        return PayloadTupleGetter<I - 1, Ts...>::get(tuple.tail);
    }

    static constexpr decltype(auto) get(const PayloadTuple<T, Ts...>& tuple) {
        return PayloadTupleGetter<I - 1, Ts...>::get(tuple.tail);
    }

    static constexpr decltype(auto) getWrapToRef(PayloadTuple<T, Ts...>& tuple) {
        return PayloadTupleGetter<I - 1, Ts...>::getWrapToRef(tuple.tail);
    }

    static constexpr decltype(auto) getWrapToRef(const PayloadTuple<T, Ts...>& tuple) {
        return PayloadTupleGetter<I - 1, Ts...>::getWrapToRef(tuple.tail);
    }
};

template <typename F, typename... Args>
struct PayloadImpl {
    F f;
    [[no_unique_address]] PayloadTuple<Args...> args;

    FORCE_INLINE constexpr auto invoke(this auto&& self, auto&&... t) -> decltype(auto) {
        using Self = decltype(self);
        return std::forward_like<Self>(self.args).apply(std::forward_like<Self>(self.f),
                                                        std::forward<decltype(t)>(t)...);
    }

    FORCE_INLINE constexpr auto invokeCompatible(this auto&& self, auto&&... t) -> decltype(auto) {
        using Self = decltype(self);
        return std::forward_like<Self>(self.args).applyCompatible(std::forward_like<Self>(self.f),
                                                                  std::forward<decltype(t)>(t)...);
    }

    FORCE_INLINE constexpr auto invokeFront(this auto&& self, auto&&... t) -> decltype(auto) {
        static_assert((std::is_reference_v<decltype(t)> && ...));
        using Self = decltype(self);
        return std::forward_like<Self>(self.args).applyFront(
            std::forward_like<Self>(self.f),
            PayloadTuple<CallArgTag<decltype(t)>...>{std::forward<decltype(t)>(t)...});
    }

    FORCE_INLINE constexpr auto invokeFrontCompatible(this auto&& self, auto&&... t)
        -> decltype(auto) {
        static_assert((std::is_reference_v<decltype(t)> && ...));
        using Self = decltype(self);
        return std::forward_like<Self>(self.args).applyFrontCompatible(
            std::forward_like<Self>(self.f),
            PayloadTuple<CallArgTag<decltype(t)>...>{std::forward<decltype(t)>(t)...});
    }
};

template <auto F, typename... Args>
struct PayloadImplArgs {
    using F_Type = decltype(F);
    [[no_unique_address]] PayloadTuple<Args...> args;

    FORCE_INLINE constexpr auto invoke(this auto&& self, auto&&... t) -> decltype(auto) {
        using Self = decltype(self);
        return std::forward_like<Self>(self.args).template apply<F>(
            std::forward<decltype(t)>(t)...);
    }

    FORCE_INLINE constexpr auto invokeCompatible(this auto&& self, auto&&... t) -> decltype(auto) {
        using Self = decltype(self);
        return std::forward_like<Self>(self.args).template applyCompatible<F>(
            std::forward<decltype(t)>(t)...);
    }

    FORCE_INLINE constexpr auto invokeFront(this auto&& self, auto&&... t) -> decltype(auto) {
        static_assert((std::is_reference_v<decltype(t)> && ...));
        using Self = decltype(self);
        return std::forward_like<Self>(self.args).template applyFront<F>(
            PayloadTuple<CallArgTag<decltype(t)>...>{std::forward<decltype(t)>(t)...});
    }

    FORCE_INLINE constexpr auto invokeFrontCompatible(this auto&& self, auto&&... t)
        -> decltype(auto) {
        static_assert((std::is_reference_v<decltype(t)> && ...));
        using Self = decltype(self);
        return std::forward_like<Self>(self.args).template applyFrontCompatible<F>(
            PayloadTuple<CallArgTag<decltype(t)>...>{std::forward<decltype(t)>(t)...});
    }
};

template <typename... Args>
struct PayloadNoRvalueRef : std::false_type {};

template <typename... Args>
    requires((!std::is_rvalue_reference_v<Args>) && ...)
struct PayloadNoRvalueRef<PayloadTuple<Args...>> : std::true_type {};

template <typename F, typename... Args>
    requires(!std::is_rvalue_reference_v<F> && PayloadNoRvalueRef<PayloadTuple<Args...>>::value)
struct PayloadNoRvalueRef<PayloadImpl<F, Args...>> : std::true_type {};

template <auto F, typename... Args>
    requires(PayloadNoRvalueRef<PayloadTuple<Args...>>::value)
struct PayloadNoRvalueRef<PayloadImplArgs<F, Args...>> : std::true_type {};

template <typename T>
struct AddRefWrapperToRef {
    using Type = T;
};

template <typename T>
struct AddRefWrapperToRef<T&> {
    using Type = std::reference_wrapper<T>;
};

template <typename T>
using AddRefWrapperToRef_t = typename AddRefWrapperToRef<T>::Type;

template <typename F, typename... Args>
using PayloadT = PayloadImpl<std::decay_t<F>, std::decay_t<Args>...>;

template <typename F, typename... Args>
using PayloadNamedRef = PayloadImpl<AddRefWrapperToRef_t<F>, AddRefWrapperToRef_t<Args>...>;

template <auto F, typename... Args>
using PayloadArgsT = PayloadImplArgs<F, std::decay_t<Args>...>;

template <auto F, typename... Args>
using PayloadStaticNamedRef = PayloadImplArgs<F, AddRefWrapperToRef_t<Args>...>;

template <typename, typename, typename...>
struct IsPayload : std::false_type {};

template <typename From, typename To>
using ForwardLikeT = decltype(std::forward_like<From>(std::declval<To>()));

template <typename F, typename... BoundArgs, typename Self, typename... CallArgs>
struct IsPayload<PayloadImpl<F, BoundArgs...>, Self, CallArgs...> : std::true_type {
    using F_qual = ForwardLikeT<Self, F>;
    static constexpr bool kIsInvocable =
        std::is_invocable_v<F_qual, CallArgs..., ForwardLikeT<Self, BoundArgs>...>;

    static constexpr bool kIsInvocableFront =
        std::is_invocable_v<F_qual, ForwardLikeT<Self, BoundArgs>..., CallArgs...>;
};

template <auto F, typename... BoundArgs, typename Self, typename... CallArgs>
struct IsPayload<PayloadImplArgs<F, BoundArgs...>, Self, CallArgs...> : std::true_type {
    using F_qual = ForwardLikeT<Self, decltype(F)>;
    static constexpr bool kIsInvocable =
        std::is_invocable_v<F_qual, CallArgs..., ForwardLikeT<Self, BoundArgs>...>;

    static constexpr bool kIsInvocableFront =
        std::is_invocable_v<F_qual, ForwardLikeT<Self, BoundArgs>..., CallArgs...>;
};

}  // namespace fn

#endif  // TASK_PAYLOAD_HPP