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

namespace detail {

// ------------------------------------------------------------
// Type lists
// ------------------------------------------------------------

template <typename... Ts>
struct type_list {};

template <typename List, typename T>
struct push_back;

template <typename... Ts, typename T>
struct push_back<type_list<Ts...>, T> {
    using type = type_list<Ts..., T>;
};

template <typename List, typename T>
using push_back_t = typename push_back<List, T>::type;

// ------------------------------------------------------------
// Permitted argument categories, in priority order
// ------------------------------------------------------------

template <typename T>
struct invocation_options {
   private:
    using U = std::remove_reference_t<T>;
    using C = std::add_const_t<U>;

   public:
    using type = std::conditional_t<
        std::is_const_v<U>,

        std::conditional_t<std::is_lvalue_reference_v<T>, std::tuple<U&>, std::tuple<U&&, U&>>,

        std::conditional_t<std::is_lvalue_reference_v<T>, std::tuple<U&, C&>,
                           std::tuple<U&&, C&&, U&, C&>>>;
};

template <typename T>
    requires std::is_const_v<std::remove_reference_t<T>>
struct invocation_options<T> {
   private:
    using U = std::remove_reference_t<T>;

   public:
    using type = std::conditional_t<std::is_lvalue_reference_v<T>, std::tuple<const U&>,
                                    std::tuple<const U&&, const U&>>;
};

template <typename T>
using invocation_options_t = typename invocation_options<T>::type;

// ------------------------------------------------------------
// Search machinery
// ------------------------------------------------------------

template <typename F, typename Remaining, typename Selected>
struct search;

// Try the next candidate when the current one isn't invocable.

template <typename Candidate, typename F, typename Remaining, typename Selected, typename Options,
          std::size_t I, bool = Candidate::kValid>
struct choose_candidate;

// Found a valid combination.
template <typename Candidate, typename F, typename Remaining, typename Selected, typename Options,
          std::size_t I>
struct choose_candidate<Candidate, F, Remaining, Selected, Options, I, true> : Candidate {};

// Forward declaration.
template <typename F, typename Remaining, typename Selected, typename Options, std::size_t I>
struct search_next;

// No more alternatives.
template <typename F, typename Remaining, typename Selected, typename Options, std::size_t I>
    requires(I == std::tuple_size_v<Options>)
struct search_next<F, Remaining, Selected, Options, I> {
    static constexpr bool kValid = false;
    static constexpr bool kNoThrow = false;
    using types = Selected;
};

// Try alternative I.
template <typename F, typename Remaining, typename Selected, typename Options, std::size_t I>
    requires(I < std::tuple_size_v<Options>)
struct search_next<F, Remaining, Selected, Options, I>
    : choose_candidate<
          search<F, Remaining, push_back_t<Selected, std::tuple_element_t<I, Options>>>, F,
          Remaining, Selected, Options, I> {};

// Try the next alternative.
template <typename Candidate, typename F, typename Remaining, typename Selected, typename Options,
          std::size_t I>
struct choose_candidate<Candidate, F, Remaining, Selected, Options, I, false>
    : search_next<F, Remaining, Selected, Options, I + 1> {};

// All arguments have been selected.
template <typename F, typename... Selected>
struct search<F, type_list<>, type_list<Selected...>> {
    static constexpr bool kValid = std::is_invocable_v<F, Selected...>;

    static constexpr bool kNoThrow = std::is_nothrow_invocable_v<F, Selected...>;

    using types = type_list<Selected...>;
};

// Select an alternative for the next argument.
template <typename F, typename Arg, typename... Rest, typename... Selected>
struct search<F, type_list<Arg, Rest...>, type_list<Selected...>>
    : search_next<F, type_list<Rest...>, type_list<Selected...>, invocation_options_t<Arg>, 0> {};

// ------------------------------------------------------------
// Actual invocation
// ------------------------------------------------------------

template <typename F, typename Tuple, typename... Selected, std::size_t... I>
constexpr decltype(auto) invoke_selected(
    F&& f, Tuple& args, type_list<Selected...> /*unused*/,
    std::index_sequence<I...> /*unused*/) noexcept(std::is_nothrow_invocable_v<F&&, Selected...>) {
    return std::invoke(std::forward<F>(f), static_cast<Selected>(std::get<I>(args))...);
}

template <auto F, typename Tuple, typename... Selected, std::size_t... I>
constexpr decltype(auto) invoke_selected(
    Tuple& args, type_list<Selected...> /*unused*/,
    std::index_sequence<I...> /*unused*/) noexcept(std::is_nothrow_invocable_v<decltype(F),
                                                                               Selected...>) {
    return std::invoke(F, static_cast<Selected>(std::get<I>(args))...);
}

}  // namespace detail

// ============================================================
// Public interface
// ============================================================

template <typename F, typename... Args>
constexpr decltype(auto) invoke_compatible(F&& f, Args&&... args) noexcept(
    detail::search<F&&, detail::type_list<Args&&...>, detail::type_list<>>::kNoThrow) {
    using Selection = detail::search<F&&, detail::type_list<Args&&...>, detail::type_list<>>;

    static_assert(Selection::kValid,
                  "Callable is not invocable with any compatible cv/ref combination");

    auto references = std::forward_as_tuple(std::forward<Args>(args)...);

    return detail::invoke_selected(std::forward<F>(f), references, typename Selection::types{},
                                   std::index_sequence_for<Args...>{});
}

template <auto F, typename... Args>
constexpr decltype(auto) invoke_compatible(Args&&... args) noexcept(
    detail::search<decltype(F), detail::type_list<Args&&...>, detail::type_list<>>::nothrow) {
    using Selection =
        detail::search<decltype(F), detail::type_list<Args&&...>, detail::type_list<>>;

    static_assert(Selection::kValid,
                  "Callable is not invocable with any compatible cv/ref combination");

    auto references = std::forward_as_tuple(std::forward<Args>(args)...);

    return detail::invoke_selected<F>(references, typename Selection::types{},
                                      std::index_sequence_for<Args...>{});
}

template <typename F, typename... Args>
inline constexpr bool is_compatible_invocable_v =
    detail::search<F&&, detail::type_list<Args&&...>, detail::type_list<>>::kValid;

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

template <>
struct PayloadTuple<> {
    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) apply([[maybe_unused]] this auto&& /*self*/,
                                                ArgsSoFar&&... args) {
        return fn::invoke_compatible<F>(std::forward<ArgsSoFar>(args)...);
        /*static_assert(std::is_invocable_v<decltype(F), decltype(args)...>,
                      "F is not callable with given arguments");
        return F(std::forward<ArgsSoFar>(args)...);*/
    }

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgs(ArgsSoFar&&... args) && {
        return fn::invoke_compatible<F>(std::forward<ArgsSoFar>(args)...);
        /*static_assert(std::is_invocable_v<decltype(F), decltype(args)...>,
                      "F is not callable with given arguments");
        return F(std::forward<ArgsSoFar>(args)...);*/
    }

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFront([[maybe_unused]] this auto&& /*self*/,
                                                     auto&& callArgs, ArgsSoFar&&... args)
        requires(IsRefPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        return std::forward<decltype(callArgs)>(callArgs).template applyAsArgs<F>(
            std::forward<ArgsSoFar>(args)...);
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) apply([[maybe_unused]] this auto&& /*self*/, F&& f,
                                                ArgsSoFar&&... args) {
        return fn::invoke_compatible(std::forward<F>(f), std::forward<ArgsSoFar>(args)...);
        /*static_assert(std::is_invocable_v<F, decltype(args)...>,
                      "F is not callable with given arguments");
        return std::invoke(std::forward<F>(f), std::forward<ArgsSoFar>(args)...);*/
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgs(F&& f, ArgsSoFar&&... args) && {
        return fn::invoke_compatible(std::forward<F>(f), std::forward<ArgsSoFar>(args)...);
        /*static_assert(std::is_invocable_v<F, decltype(args)...>,
                      "F is not callable with given arguments");
        return std::invoke(std::forward<F>(f), std::forward<ArgsSoFar>(args)...);*/
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFront([[maybe_unused]] this auto&& /*self*/, F&& f,
                                                     auto&& callArgs, ArgsSoFar&&... args)
        requires(IsRefPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        return std::forward<decltype(callArgs)>(callArgs).template applyAsArgs<F>(
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
            return fn::invoke_compatible<F>(std::forward<ArgsSoFar>(args)...,
                                            std::forward_like<Self>(self.head));
            /*static_assert(std::is_invocable_v<decltype(F), decltype(args)...,
                                              decltype(std::forward_like<Self>(self.head))>,
                          "F is not callable with given arguments");
            return F(std::forward<ArgsSoFar>(args)..., std::forward_like<Self>(self.head));*/
        } else {
            return std::forward_like<Self>(self.tail).template apply<F>(
                std::forward<ArgsSoFar>(args)..., std::forward_like<Self>(self.head));
        }
    }

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgs(ArgsSoFar&&... args) && {
        if constexpr (sizeof...(Ts) == 0) {
            return fn::invoke_compatible<F>(std::forward<ArgsSoFar>(args)...,
                                            std::forward<T>(head));
            /*static_assert(std::is_invocable_v<decltype(F), decltype(args)..., decltype(head)>,
                          "F is not callable with given arguments");
            return F(std::forward<ArgsSoFar>(args)..., std::forward<T>(head));*/
        } else {
            return std::move(tail).template applyAsArgs<F>(std::forward<ArgsSoFar>(args)...,
                                                           std::forward<T>(head));
        }
    }

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFront(this auto&& self, auto&& callArgs,
                                                     ArgsSoFar&&... args)
        requires(IsRefPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
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

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) apply(this auto&& self, F&& f, ArgsSoFar&&... args) {
        using Self = decltype(self);
        if constexpr (sizeof...(Ts) == 0) {
            return fn::invoke_compatible(std::forward<F>(f), std::forward<ArgsSoFar>(args)...,
                                         std::forward_like<Self>(self.head));
            /*static_assert(std::is_invocable_v<decltype(f), decltype(args)...,
                                              decltype(std::forward_like<Self>(self.head))>,
                          "F is not callable with given arguments");
            return std::invoke(std::forward<F>(f), std::forward<ArgsSoFar>(args)...,
                               std::forward_like<Self>(self.head));*/
        } else {
            return std::forward_like<Self>(self.tail).apply(std::forward<F>(f),
                                                            std::forward<ArgsSoFar>(args)...,
                                                            std::forward_like<Self>(self.head));
        }
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgs(F&& f, ArgsSoFar&&... args) && {
        if constexpr (sizeof...(Ts) == 0) {
            return fn::invoke_compatible(std::forward<F>(f), std::forward<ArgsSoFar>(args)...,
                                         std::forward<T>(head));
            /*static_assert(std::is_invocable_v<decltype(f), decltype(args)..., decltype(head)>,
                          "F is not callable with given arguments");
            return std::invoke(std::forward<F>(f), std::forward<ArgsSoFar>(args)...,
                               std::forward<T>(head));*/
        } else {
            return std::move(tail).applyAsArgs(std::forward<F>(f), std::forward<ArgsSoFar>(args)...,
                                               std::forward<T>(head));
        }
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFront(this auto&& self, F&& f, auto&& callArgs,
                                                     ArgsSoFar&&... args)
        requires(IsRefPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        using Self = decltype(self);
        if constexpr (sizeof...(Ts) == 0) {
            return std::forward<decltype(callArgs)>(callArgs).template applyAsArgs<F>(
                std::forward<F>(f), std::forward<ArgsSoFar>(args)...,
                std::forward_like<Self>(self.head));
        } else {
            return std::forward_like<Self>(self.tail).template applyFront<F>(
                std::forward<F>(f), std::forward<decltype(callArgs)>(callArgs),
                std::forward<ArgsSoFar>(args)..., std::forward_like<Self>(self.head));
        }
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

    FORCE_INLINE constexpr auto invokeFront(this auto&& self, auto&&... t) -> decltype(auto) {
        using Self = decltype(self);
        return std::forward_like<Self>(self.args).applyFront(
            std::forward_like<Self>(self.f),
            PayloadTuple<decltype(t)...>{std::forward<decltype(t)>(t)...});
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

    FORCE_INLINE constexpr auto invokeFront(this auto&& self, auto&&... t) -> decltype(auto) {
        using Self = decltype(self);
        return std::forward_like<Self>(self.args).template applyFront<F>(
            PayloadTuple<decltype(t)...>{std::forward<decltype(t)>(t)...});
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

template <typename F, typename... Args>
using PayloadT = PayloadImpl<std::decay_t<F>, std::unwrap_ref_decay_t<Args>...>;

template <auto F, typename... Args>
using PayloadArgsT = PayloadImplArgs<F, std::unwrap_ref_decay_t<Args>...>;

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