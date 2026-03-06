#ifndef TASK_PAYLOAD_HPP
#define TASK_PAYLOAD_HPP

#include <functional>
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
        static_assert(std::is_invocable_v<decltype(F), decltype(args)...>,
                      "F is not callable with given arguments");
        return F(std::forward<ArgsSoFar>(args)...);
    }

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgs(ArgsSoFar&&... args) && {
        static_assert(std::is_invocable_v<decltype(F), decltype(args)...>,
                      "F is not callable with given arguments");
        return F(std::forward<ArgsSoFar>(args)...);
    }

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFront([[maybe_unused]] this auto&& /*self*/,
                                                     auto&& callArgs, ArgsSoFar&&... args)
        requires(IsRefPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        return std::forward<decltype(callArgs)>(callArgs).template apply<F>(
            std::forward<ArgsSoFar>(args)...);
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) apply([[maybe_unused]] this auto&& /*self*/, F&& f,
                                                ArgsSoFar&&... args) {
        static_assert(std::is_invocable_v<F, decltype(args)...>,
                      "F is not callable with given arguments");
        return std::invoke(std::forward<F>(f), std::forward<ArgsSoFar>(args)...);
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgs(F&& f, ArgsSoFar&&... args) && {
        static_assert(std::is_invocable_v<F, decltype(args)...>,
                      "F is not callable with given arguments");
        return std::invoke(std::forward<F>(f), std::forward<ArgsSoFar>(args)...);
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFront([[maybe_unused]] this auto&& /*self*/, F&& f,
                                                     auto&& callArgs, ArgsSoFar&&... args)
        requires(IsRefPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        return std::forward<decltype(callArgs)>(callArgs).template apply<F>(
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
                                              decltype(std::forward_like<Self>(self.head))>,
                          "F is not callable with given arguments");
            return F(std::forward<ArgsSoFar>(args)..., std::forward_like<Self>(self.head));
        } else {
            return std::forward_like<Self>(self.tail).template apply<F>(
                std::forward<ArgsSoFar>(args)..., std::forward_like<Self>(self.head));
        }
    }

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgs(ArgsSoFar&&... args) && {
        if constexpr (sizeof...(Ts) == 0) {
            static_assert(std::is_invocable_v<decltype(F), decltype(args)..., decltype(head)>,
                          "F is not callable with given arguments");
            return F(std::forward<ArgsSoFar>(args)..., std::forward<T>(head));
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
            return std::forward<decltype(callArgs)>(callArgs).template apply<F>(
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
            static_assert(std::is_invocable_v<decltype(f), decltype(args)...,
                                              decltype(std::forward_like<Self>(self.head))>,
                          "F is not callable with given arguments");
            return std::invoke(std::forward<F>(f), std::forward<ArgsSoFar>(args)...,
                               std::forward_like<Self>(self.head));
        } else {
            return std::forward_like<Self>(self.tail).apply(std::forward<F>(f),
                                                            std::forward<ArgsSoFar>(args)...,
                                                            std::forward_like<Self>(self.head));
        }
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgs(F&& f, ArgsSoFar&&... args) && {
        if constexpr (sizeof...(Ts) == 0) {
            static_assert(std::is_invocable_v<decltype(f), decltype(args)..., decltype(head)>,
                          "F is not callable with given arguments");
            return std::invoke(std::forward<F>(f), std::forward<ArgsSoFar>(args)...,
                               std::forward<T>(head));
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
            return std::forward<decltype(callArgs)>(callArgs).template apply<F>(
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

/*template <typename F, typename... Args, typename Self, typename... CallArgs>
struct IsPayload<PayloadImpl<F, Args...>, Self, CallArgs...> : std::true_type {
    static constexpr bool kIsInvocable = requires(F f, CallArgs... callArgs) {
        std::is_invocable_v<decltype(std::forward_like<Self>(f)),
                            decltype(std::forward_like<Self>(callArgs))..., CallArgs...>;
    };
    static constexpr bool kIsInvocableFront = requires(F f, CallArgs... callArgs) {
        std::is_invocable_v<decltype(std::forward_like<Self>(f)), CallArgs...,
                            decltype(std::forward_like<Self>(callArgs))...>;
    };
};*/

template <typename From, typename To>
using ForwardLikeT = decltype(std::forward_like<From>(std::declval<To>()));

template <typename F, typename... BoundArgs, typename Self, typename... CallArgs>
struct IsPayload<PayloadImpl<F, BoundArgs...>, Self, CallArgs...> : std::true_type {
    using F_qual = ForwardLikeT<Self, F>;
    static constexpr bool kIsInvocable =
        std::is_invocable_v<F_qual, ForwardLikeT<Self, BoundArgs>..., CallArgs...>;

    static constexpr bool kIsInvocableFront =
        std::is_invocable_v<F_qual, CallArgs..., ForwardLikeT<Self, BoundArgs>...>;
};

}  // namespace fn

#endif  // TASK_PAYLOAD_HPP