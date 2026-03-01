#ifndef FN_TASK_HPP
#define FN_TASK_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <new>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

namespace fn {

#if defined(__GNUC__)
#define FORCE_INLINE [[gnu::always_inline]] inline
#elif defined(__clang__)
#define FORCE_INLINE [[clang::always_inline]] inline
#elif defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline
#endif

template <typename... T>
struct AlwaysFalse : std::false_type {};

template <typename T>
struct RefToConstRef {
    using Type = T;
};

template <typename U>
struct RefToConstRef<std::reference_wrapper<U>> {
    using Type = std::reference_wrapper<const U>;
};

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
        return F(std::forward<ArgsSoFar>(args)...);
    }

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgs(ArgsSoFar&&... args) && {
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
        return std::invoke(std::forward<F>(f), std::forward<ArgsSoFar>(args)...);
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgs(F&& f, ArgsSoFar&&... args) && {
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
            return F(std::forward<ArgsSoFar>(args)..., std::forward_like<Self>(self.head));
        } else {
            return std::forward_like<Self>(self.tail).template apply<F>(
                std::forward<ArgsSoFar>(args)..., std::forward_like<Self>(self.head));
        }
    }

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgs(ArgsSoFar&&... args) && {
        if constexpr (sizeof...(Ts) == 0) {
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

enum class Op : std::uint8_t { kDestroy, kClone, kMove, kAssign, kMoveAssign };

template <std::size_t Alignment, std::size_t N, bool IsTrivial, bool Mutable, bool Extended = false,
          typename Signature = void()>
struct Storage;

template <typename ReturnType, typename... CallArgs>
using InvokerType = ReturnType (*)(void*, CallArgs...);

template <typename ReturnType, typename... CallArgs>
using InvokerTypeNoBuffer = ReturnType (*)(CallArgs...);

template <std::size_t N, bool Mutable, typename ReturnType, typename... CallArgs>
struct InvokerFactory {
    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(N > 0)
    FORCE_INLINE static constexpr InvokerType<ReturnType, CallArgs...> getInvoker() {  // NOLINT
        return [](void* s, CallArgs... cargs) -> ReturnType {                          // NOLINT
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<decltype(F), CallArgs...>,
                              "F is not callable with the provided arguments!");
                return F(std::forward<CallArgs>(cargs)...);
            } else if constexpr (Mutable) {
                static_assert(PayloadNoRvalueRef<PayloadType>::value);
                if constexpr (std::is_invocable_v<decltype(F), CallArgs...,
                                                  std::unwrap_ref_decay_t<BoundArgs>&...>) {
                    if constexpr (sizeof(PayloadType) <= N) {
                        return static_cast<PayloadType*>(s)->invoke(
                            std::forward<CallArgs>(cargs)...);
                    } else {
                        return (*static_cast<PayloadType**>(s))
                            ->invoke(std::forward<CallArgs>(cargs)...);
                    }
                } else if constexpr (std::is_invocable_v<decltype(F), CallArgs...,
                                                         std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                    if constexpr (sizeof(PayloadType) <= N) {
                        return std::move(*static_cast<PayloadType*>(s))
                            .invoke(std::forward<CallArgs>(cargs)...);
                    } else {
                        return std::move(**static_cast<PayloadType**>(s))
                            .invoke(std::forward<CallArgs>(cargs)...);
                    }
                } else if constexpr (std::is_invocable_v<decltype(F),
                                                         std::unwrap_ref_decay_t<BoundArgs>&...,
                                                         CallArgs...>) {
                    if constexpr (sizeof(PayloadType) <= N) {
                        return static_cast<PayloadType*>(s)->invokeFront(
                            std::forward<CallArgs>(cargs)...);
                    } else {
                        return (*static_cast<PayloadType**>(s))
                            ->invokeFront(std::forward<CallArgs>(cargs)...);
                    }
                } else if constexpr (std::is_invocable_v<decltype(F),
                                                         std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                         CallArgs...>) {
                    if constexpr (sizeof(PayloadType) <= N) {
                        return std::move(*static_cast<PayloadType*>(s))
                            .invokeFront(std::forward<CallArgs>(cargs)...);
                    } else {
                        return std::move(**static_cast<PayloadType**>(s))
                            .invokeFront(std::forward<CallArgs>(cargs)...);
                    }
                } else {
                    static_assert(AlwaysFalse<decltype(F)>::value,
                                  "F is not callable with the provided arguments!");
                }
            } else {
                static_assert(PayloadNoRvalueRef<PayloadType>::value);
                if constexpr (std::is_invocable_v<decltype(F), CallArgs...,
                                                  std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                    static_assert(
                        std::is_invocable_v<decltype(F), CallArgs...,
                                            const std::unwrap_ref_decay_t<
                                                typename RefToConstRef<BoundArgs>::Type>&...>,
                        "F is callable with the provided arguments with mutable specifier, but "
                        "this instance is marked immutable. Either change function signature to "
                        "not take captured arguments by non-const reference, or create this "
                        "instance with mutable activated (template boolean)");
                    if constexpr (sizeof(PayloadType) <= N) {
                        return static_cast<const PayloadType*>(s)->invoke(
                            std::forward<CallArgs>(cargs)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invoke(std::forward<CallArgs>(cargs)...);
                    }
                } else if constexpr (std::is_invocable_v<decltype(F),
                                                         std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                         CallArgs...>) {
                    static_assert(
                        std::is_invocable_v<decltype(F),
                                            const std::unwrap_ref_decay_t<
                                                typename RefToConstRef<BoundArgs>::Type>&...,
                                            CallArgs...>,
                        "F is callable with the provided arguments with mutable specifier, but "
                        "this instance is marked immutable. Either change function signature to "
                        "not take captured arguments by non-const reference, or create this "
                        "instance with mutable activated (template boolean)");
                    if constexpr (sizeof(PayloadType) <= N) {
                        return static_cast<const PayloadType*>(s)->invokeFront(
                            std::forward<CallArgs>(cargs)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invokeFront(std::forward<CallArgs>(cargs)...);
                    }
                } else if constexpr (std::is_invocable_v<
                                         decltype(F), CallArgs...,
                                         const std::unwrap_ref_decay_t<
                                             typename RefToConstRef<BoundArgs>::Type>&...>) {
                    if constexpr (sizeof(PayloadType) <= N) {
                        return static_cast<const PayloadType*>(s)->invoke(
                            std::forward<CallArgs>(cargs)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invoke(std::forward<CallArgs>(cargs)...);
                    }
                } else if constexpr (std::is_invocable_v<
                                         decltype(F),
                                         const std::unwrap_ref_decay_t<
                                             typename RefToConstRef<BoundArgs>::Type>&...,
                                         CallArgs...>) {
                    if constexpr (sizeof(PayloadType) <= N) {
                        return static_cast<const PayloadType*>(s)->invokeFront(
                            std::forward<CallArgs>(cargs)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invokeFront(std::forward<CallArgs>(cargs)...);
                    }
                } else {
                    static_assert(AlwaysFalse<decltype(F)>::value,
                                  "F is not callable with the provided arguments!");
                }
            }
        };
    }

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(N == 0)
    FORCE_INLINE static constexpr InvokerTypeNoBuffer<ReturnType, CallArgs...>
    getInvoker() {  // NOLINT
        return [](CallArgs... cargs) -> ReturnType {
            static_assert(sizeof...(BoundArgs) == 0);
            static_assert(
                sizeof(PayloadType) <= 1,
                "Payload is not empty, and therefore a buffer needs to be used to store it.");
            static_assert(std::is_invocable_v<decltype(F), CallArgs...>,
                          "F is not callable with the provided arguments!");
            return F(std::forward<CallArgs>(cargs)...);
        };
    }

    template <typename PayloadType, typename F, typename... BoundArgs>
        requires(N > 0)
    FORCE_INLINE static constexpr InvokerType<ReturnType, CallArgs...> getInvoker() {  // NOLINT
        return [](void* s, CallArgs... cargs) -> ReturnType {                          // NOLINT
            if constexpr (Mutable) {
                static_assert(PayloadNoRvalueRef<PayloadType>::value);
                if constexpr (std::is_invocable_v<F, CallArgs...,
                                                  std::unwrap_ref_decay_t<BoundArgs>&...>) {
                    if constexpr (sizeof(PayloadType) <= N) {
                        return static_cast<PayloadType*>(s)->invoke(
                            std::forward<CallArgs>(cargs)...);
                    } else {
                        return (*static_cast<PayloadType**>(s))
                            ->invoke(std::forward<CallArgs>(cargs)...);
                    }
                } else if constexpr (std::is_invocable_v<F, CallArgs...,
                                                         std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                    if constexpr (sizeof(PayloadType) <= N) {
                        return std::move(*static_cast<PayloadType*>(s))
                            .invoke(std::forward<CallArgs>(cargs)...);
                    } else {
                        return std::move(**static_cast<PayloadType**>(s))
                            .invoke(std::forward<CallArgs>(cargs)...);
                    }
                } else if constexpr (std::is_invocable_v<F, std::unwrap_ref_decay_t<BoundArgs>&...,
                                                         CallArgs...>) {
                    if constexpr (sizeof(PayloadType) <= N) {
                        return static_cast<PayloadType*>(s)->invokeFront(
                            std::forward<CallArgs>(cargs)...);
                    } else {
                        return (*static_cast<PayloadType**>(s))
                            ->invokeFront(std::forward<CallArgs>(cargs)...);
                    }
                } else if constexpr (std::is_invocable_v<F, std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                         CallArgs...>) {
                    if constexpr (sizeof(PayloadType) <= N) {
                        return std::move(*static_cast<PayloadType*>(s))
                            .invokeFront(std::forward<CallArgs>(cargs)...);
                    } else {
                        return std::move(**static_cast<PayloadType**>(s))
                            .invokeFront(std::forward<CallArgs>(cargs)...);
                    }
                } else {
                    static_assert(AlwaysFalse<F>::value,
                                  "F is not callable with the provided arguments!");
                }
            } else {
                static_assert(PayloadNoRvalueRef<PayloadType>::value);
                if constexpr (std::is_invocable_v<F, CallArgs...,
                                                  std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                    static_assert(
                        std::is_invocable_v<F, CallArgs...,
                                            const std::unwrap_ref_decay_t<
                                                typename RefToConstRef<BoundArgs>::Type>&...>,
                        "F is callable with the provided arguments with mutable specifier, but "
                        "this instance is marked immutable. Either change function signature to "
                        "not take captured arguments by non-const reference, or create this "
                        "instance with mutable activated (template boolean)");
                    if constexpr (sizeof(PayloadType) <= N) {
                        return static_cast<const PayloadType*>(s)->invoke(
                            std::forward<CallArgs>(cargs)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invoke(std::forward<CallArgs>(cargs)...);
                    }
                } else if constexpr (std::is_invocable_v<F, std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                         CallArgs...>) {
                    static_assert(
                        std::is_invocable_v<F,
                                            const std::unwrap_ref_decay_t<
                                                typename RefToConstRef<BoundArgs>::Type>&...,
                                            CallArgs...>,
                        "F is callable with the provided arguments with mutable specifier, but "
                        "this instance is marked immutable. Either change function signature to "
                        "not take captured arguments by non-const reference, or create this "
                        "instance with mutable activated (template boolean)");
                    if constexpr (sizeof(PayloadType) <= N) {
                        return static_cast<const PayloadType*>(s)->invokeFront(
                            std::forward<CallArgs>(cargs)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invokeFront(std::forward<CallArgs>(cargs)...);
                    }
                } else if constexpr (std::is_invocable_v<
                                         F, CallArgs...,
                                         const std::unwrap_ref_decay_t<
                                             typename RefToConstRef<BoundArgs>::Type>&...>) {
                    if constexpr (sizeof(PayloadType) <= N) {
                        return static_cast<const PayloadType*>(s)->invoke(
                            std::forward<CallArgs>(cargs)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invoke(std::forward<CallArgs>(cargs)...);
                    }
                } else if constexpr (std::is_invocable_v<
                                         F,
                                         const std::unwrap_ref_decay_t<
                                             typename RefToConstRef<BoundArgs>::Type>&...,
                                         CallArgs...>) {
                    if constexpr (sizeof(PayloadType) <= N) {
                        return static_cast<const PayloadType*>(s)->invokeFront(
                            std::forward<CallArgs>(cargs)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invokeFront(std::forward<CallArgs>(cargs)...);
                    }
                } else {
                    static_assert(AlwaysFalse<F>::value,
                                  "F is not callable with the provided arguments!");
                }
            }
        };
    }
};

template <std::size_t Alignment, std::size_t N, bool E, typename ReturnType, typename... CallArgs>
    requires(N > 0)
struct Storage<Alignment, N, true, true, E, ReturnType(CallArgs...)> {
    alignas(Alignment) mutable std::array<std::byte, N> data{};
    InvokerType<ReturnType, CallArgs...> invoker{nullptr};

    template <typename Payload, bool AllowHeap = false, bool Copyable = true>
    constexpr void doConstruct() noexcept {}

    [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept {
        return std::all_of(data.begin(), data.end(), [](auto v) { return v == 0; });
    }
};

template <std::size_t Alignment, bool E, typename ReturnType, typename... CallArgs>
struct Storage<Alignment, 0, true, true, E, ReturnType(CallArgs...)> {
    InvokerTypeNoBuffer<ReturnType, CallArgs...> invoker{nullptr};

    template <typename Payload, bool AllowHeap = false, bool Copyable = true>
    constexpr void doConstruct() noexcept {}

    [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept {
        return true;
    }
};

template <std::size_t Alignment, std::size_t N, bool E, typename ReturnType, typename... CallArgs>
    requires(N > 0)
struct Storage<Alignment, N, true, false, E, ReturnType(CallArgs...)> {
    alignas(Alignment) std::array<std::byte, N> data{};
    InvokerType<ReturnType, CallArgs...> invoker{nullptr};

    template <typename Payload, bool AllowHeap, bool Copyable = true>
    constexpr void doConstruct() noexcept {}

    [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept {
        return std::all_of(data.begin(), data.end(), [](auto v) { return v == 0; });
    }
};

template <std::size_t Alignment, bool E, typename ReturnType, typename... CallArgs>
struct Storage<Alignment, 0, true, false, E, ReturnType(CallArgs...)> {
    InvokerTypeNoBuffer<ReturnType, CallArgs...> invoker{nullptr};

    template <typename Payload, bool AllowHeap = false, bool Copyable = true>
    constexpr void doConstruct() noexcept {}

    [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept {
        return true;
    }
};

template <std::size_t Alignment, std::size_t N, typename ReturnType, typename... CallArgs>
    requires(N > 0)
struct Storage<Alignment, N, false, true, false, ReturnType(CallArgs...)> {
    alignas(Alignment) mutable std::array<std::byte, N> data{};
    InvokerType<ReturnType, CallArgs...> invoker{nullptr};
    void (*manager)(Op op, void* src, void* dst){nullptr};

    constexpr void doDelete() noexcept {
        if (manager != nullptr) {
            manager(Op::kDestroy, nullptr, data.data());
            manager = nullptr;
        }
    }

    template <Op op, std::size_t U, bool MutOther>
    constexpr void doConstructFromNonTrivial(
        Storage<Alignment, U, false, MutOther, false, ReturnType(CallArgs...)>&&
            other) noexcept {  // NOLINT
        static_assert(op == Op::kMove || op == Op::kMoveAssign);
        static_assert(N >= U);
        if constexpr (op == Op::kMoveAssign) {
            doDelete();
        }
        manager = other.manager;
        if (manager != nullptr) {
            manager(op, other.data.data(), data.data());
        }
        other.manager = nullptr;
    }

    template <Op op, std::size_t U, bool MutOther>
    constexpr void doConstructFromNonTrivial(
        const Storage<Alignment, U, false, MutOther, false, ReturnType(CallArgs...)>& other) {
        static_assert(op == Op::kClone || op == Op::kAssign);
        static_assert(N >= U);
        if constexpr (op == Op::kAssign) {
            doDelete();
        }
        manager = other.manager;
        if (manager != nullptr) {
            manager(op, static_cast<void*>(const_cast<std::byte*>(other.data.data())), data.data());
        }
    }

    template <std::size_t U, bool isAssign = false, bool AllowHeap = false, bool Copyable = true>
    constexpr void doConstructFromTrivial(void* dst, const void* src) noexcept(U <= N) {  // NOLINT
        static_assert(U <= N || AllowHeap);
        if constexpr (isAssign) {
            doDelete();
        }
        manager = [](Op o, void* src, void* dest) {
            if constexpr (U <= N) {
                if (o == Op::kDestroy) {
                    return;
                }
                std::memcpy(dest, src, U);
            } else {
                static_assert(AllowHeap);
                switch (o) {
                    case Op::kDestroy: {
                        auto* srcPtr = *static_cast<void**>(dest);
                        if constexpr (Alignment <= alignof(std::max_align_t)) {
                            ::operator delete(srcPtr);
                        } else {
                            ::operator delete(srcPtr, std::align_val_t{Alignment});
                        }
                        return;
                    }
                    case Op::kAssign:
                    case Op::kClone: {
                        if constexpr (Copyable) {
                            if constexpr (Alignment <= alignof(std::max_align_t)) {
                                auto* heapPtr = ::operator new(U);
                                void* oldData = *static_cast<void**>(src);
                                std::memcpy(heapPtr, oldData, U);
                                new (dest) void*(heapPtr);
                            } else {
                                auto* heapPtr = ::operator new(U, std::align_val_t{Alignment});
                                void* oldData = *static_cast<void**>(src);
                                std::memcpy(heapPtr, oldData, U);
                                new (dest) void*(heapPtr);
                            }
                        }
                        return;
                    }
                    case Op::kMoveAssign:
                    case Op::kMove: {
                        void* heapPtr = *static_cast<void**>(src);
                        new (dest) void*(heapPtr);
                        *static_cast<void**>(src) = nullptr;
                        return;
                    }
                }
                std::unreachable();
            }
        };
        if constexpr (U <= N) {
            std::memcpy(dst, src, U);
        } else {
            static_assert(AllowHeap);
            if constexpr (Alignment <= alignof(std::max_align_t)) {
                auto* heapPtr = ::operator new(U);
                std::memcpy(heapPtr, src, U);
                new (dst) void*(heapPtr);
            } else {
                auto* heapPtr = ::operator new(U, std::align_val_t{Alignment});
                std::memcpy(heapPtr, src, U);
                new (dst) void*(heapPtr);
            }
        }
    }

    template <typename Payload, bool onHeap = false, bool Copyable = true>
    constexpr void doConstruct() noexcept {  // NOLINT
        if constexpr (sizeof(Payload) <= N) {
            manager = [](Op op, void* src, void* dst) -> void {
                using T = Payload;
                switch (op) {
                    case Op::kDestroy:
                        if constexpr (!std::is_trivially_destructible_v<T>) {
                            static_cast<T*>(dst)->~T();
                        }
                        return;
                    case Op::kAssign:
                    case Op::kClone:
                        if constexpr (Copyable) {
                            if constexpr (std::is_trivially_copy_constructible_v<T>) {
                                std::memcpy(dst, src, sizeof(T));
                            } else {
                                new (dst) T(*static_cast<const T*>(src));
                            }
                        }
                        return;
                    case Op::kMoveAssign:
                    case Op::kMove:
                        if constexpr (std::is_trivially_move_constructible_v<T>) {
                            std::memcpy(dst, src, sizeof(T));
                        } else {
                            new (dst) T(std::move(*static_cast<T*>(src)));
                        }
                        return;
                }
                std::unreachable();
            };
        } else {
            static_assert(onHeap && N >= sizeof(void*));
            manager = [](Op op, void* src, void* dst) -> void {
                using T = Payload;
                switch (op) {
                    case Op::kDestroy: {
                        T* srcPtr = *static_cast<T**>(dst);
                        if (srcPtr == nullptr) {
                            return;
                        }
                        if constexpr (!std::is_trivially_destructible_v<T>) {
                            (srcPtr)->~T();
                        }
                        if constexpr (Alignment <= alignof(std::max_align_t)) {
                            ::operator delete(srcPtr);
                        } else {
                            ::operator delete(srcPtr, std::align_val_t{Alignment});
                        }
                        return;
                    }
                    case Op::kAssign:
                    case Op::kClone: {
                        if constexpr (Copyable) {
                            if constexpr (Alignment <= alignof(std::max_align_t)) {
                                auto* heapPtr = ::operator new(sizeof(T));
                                if constexpr (std::is_trivially_copy_constructible_v<T>) {
                                    std::memcpy(heapPtr, *static_cast<const T**>(src), sizeof(T));
                                } else {
                                    new (heapPtr) T(**static_cast<const T**>(src));
                                }
                                new (dst) T*(static_cast<T*>(heapPtr));
                            } else {
                                auto* heapPtr =
                                    ::operator new(sizeof(T), std::align_val_t{Alignment});
                                if constexpr (std::is_trivially_copy_constructible_v<T>) {
                                    std::memcpy(heapPtr, *static_cast<const T**>(src), sizeof(T));
                                } else {
                                    new (heapPtr) T(**static_cast<const T**>(src));
                                }
                                new (dst) T*(static_cast<T*>(heapPtr));
                            }
                        }
                        return;
                    }
                    case Op::kMoveAssign:
                    case Op::kMove: {
                        void* heapPtr = *static_cast<void**>(src);
                        new (dst) void*(heapPtr);
                        *static_cast<void**>(src) = nullptr;
                        return;
                    }
                }
                std::unreachable();
            };
        }
    }

    [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept {
        return manager == nullptr &&
               std::all_of(data.begin(), data.end(), [](auto v) { return v == 0; });
    }
};

template <std::size_t Alignment, typename ReturnType, typename... CallArgs>
struct Storage<Alignment, 0, false, true, false, ReturnType(CallArgs...)> {
    InvokerTypeNoBuffer<ReturnType, CallArgs...> invoker{nullptr};

    template <Op op, std::size_t U, bool MutOther>
    constexpr void doConstructFromNonTrivial(
        Storage<Alignment, U, false, MutOther, false, ReturnType(CallArgs...)>&&
            other) noexcept {  // NOLINT
        static_assert(op == Op::kMove || op == Op::kMoveAssign);
        static_assert(U == 0);
    }

    template <Op op, std::size_t U, bool MutOther>
    constexpr void doConstructFromNonTrivial(
        const Storage<Alignment, 0, false, MutOther, false, ReturnType(CallArgs...)>& /*other*/) {
        static_assert(op == Op::kClone || op == Op::kAssign);
    }

    template <std::size_t U, bool isAssign = false, bool AllowHeap = false, bool Copyable = true>
    constexpr void doConstructFromTrivial(void* /*dst*/, const void* /*src*/) noexcept {}

    template <typename Payload, bool AllowHeap = false, bool Copyable = true>
    constexpr void doConstruct() noexcept {}

    [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept {
        return true;
    }
};

template <std::size_t Alignment, std::size_t N, typename ReturnType, typename... CallArgs>
    requires(N > 0)
struct Storage<Alignment, N, false, true, true, ReturnType(CallArgs...)> {
    alignas(Alignment) mutable std::array<std::byte, N> data;
    InvokerType<ReturnType, CallArgs...> invoker{nullptr};
    void (*cloner)(void* dst, const void* src){nullptr};
    void (*mover)(void* dst, void* src){nullptr};
    void (*deleter)(void* data){nullptr};

    constexpr void doDelete() noexcept {
        if (deleter != nullptr) {
            deleter(data.data());
            deleter = nullptr;
        }
    }

    template <Op op, std::size_t U, bool MutOther>
    constexpr void doConstructFromNonTrivial(
        Storage<Alignment, U, false, MutOther, true, ReturnType(CallArgs...)>&&
            other) noexcept {  // NOLINT
        static_assert(op == Op::kMove || op == Op::kMoveAssign);
        static_assert(N >= U);
        if constexpr (op == Op::kMoveAssign) {
            doDelete();
        }
        cloner = other.cloner;
        mover = other.mover;
        deleter = other.deleter;
        if (mover != nullptr) {
            mover(data.data(), other.data.data());
        }
        other.cloner = nullptr;
        other.mover = nullptr;
        other.deleter = nullptr;
    }

    template <Op op, std::size_t U, bool MutOther>
    constexpr void doConstructFromNonTrivial(
        const Storage<Alignment, U, false, MutOther, true, ReturnType(CallArgs...)>& other) {
        static_assert(op == Op::kClone || op == Op::kAssign);
        static_assert(N >= U);
        if constexpr (op == Op::kAssign) {
            doDelete();
        }
        cloner = other.cloner;
        mover = other.mover;
        deleter = other.deleter;
        if (cloner != nullptr) {
            cloner(data.data(), other.data.data());
        }
    }

    template <std::size_t U, bool isAssign = false, bool AllowHeap = false, bool Copyable = true>
    constexpr void doConstructFromTrivial(void* dst, const void* src) noexcept {
        static_assert(Copyable);
        if constexpr (isAssign) {
            doDelete();
        }
        cloner = [](void* dst, const void* src) { std::memcpy(dst, src, U); };
        mover = [](void* dst, void* src) { std::memcpy(dst, src, U); };
        deleter = nullptr;
        std::memcpy(dst, src, U);
    }

    template <typename Payload, bool AllowHeap = false, bool Copyable = true>
    constexpr void doConstruct() noexcept {
        static_assert(Copyable);
        using T = Payload;
        cloner = [](void* dst, const void* src) {
            if constexpr (std::is_trivially_copy_constructible_v<T>) {
                std::memcpy(dst, src, sizeof(T));
            } else {
                new (dst) T(*static_cast<const T*>(src));
            };
        };
        mover = [](void* dst, void* src) {
            if constexpr (std::is_trivially_move_constructible_v<T>) {
                std::memcpy(dst, src, sizeof(T));
            } else {
                new (dst) T(std::move(*static_cast<T*>(src)));
            }
        };
        deleter = [](void* data) {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                static_cast<T*>(data)->~T();
            }
        };
    }

    [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept {
        return cloner == nullptr && mover == nullptr && deleter == nullptr;
        std::all_of(data.begin(), data.end(), [](auto v) { return v == 0; });
    }
};

template <std::size_t Alignment, typename ReturnType, typename... CallArgs>
struct Storage<Alignment, 0, false, true, true, ReturnType(CallArgs...)> {
    InvokerTypeNoBuffer<ReturnType, CallArgs...> invoker{nullptr};

    template <Op op, std::size_t U, bool MutOther>
    constexpr void doConstructFromNonTrivial(
        Storage<Alignment, 0, false, MutOther, true, ReturnType(CallArgs...)>&&
            other) noexcept {  // NOLINT
        static_assert(op == Op::kMove || op == Op::kMoveAssign);
    }

    template <Op op, std::size_t U, bool MutOther>
    constexpr void doConstructFromNonTrivial(
        const Storage<Alignment, 0, false, MutOther, true, ReturnType(CallArgs...)>& /*other*/) {
        static_assert(op == Op::kClone || op == Op::kAssign);
    }

    template <std::size_t U, bool isAssign = false, bool AllowHeap = false, bool Copyable = true>
    constexpr void doConstructFromTrivial(void* /*dst*/, const void* /*src*/) noexcept {}

    template <typename Payload, bool AllowHeap = false, bool Copyable = true>
    constexpr void doConstruct() noexcept {}

    [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept {
        return true;
    }
};

template <std::size_t Alignment, std::size_t N, typename ReturnType, typename... CallArgs>
    requires(N > 0)
struct Storage<Alignment, N, false, false, false, ReturnType(CallArgs...)> {
    alignas(Alignment) std::array<std::byte, N> data{};
    InvokerType<ReturnType, CallArgs...> invoker{nullptr};
    void (*manager)(Op op, void* src, void* dst){nullptr};

    constexpr void doDelete() noexcept {
        if (manager != nullptr) {
            manager(Op::kDestroy, nullptr, data.data());
            manager = nullptr;
        }
    }

    template <Op op, std::size_t U, bool MutOther>
    constexpr void doConstructFromNonTrivial(
        Storage<Alignment, U, false, MutOther, false, ReturnType(CallArgs...)>&&
            other) noexcept {  // NOLINT
        static_assert(op == Op::kMove || op == Op::kMoveAssign);
        static_assert(N >= U);
        if constexpr (op == Op::kMoveAssign) {
            doDelete();
        }
        manager = other.manager;
        if (manager != nullptr) {
            manager(op, other.data.data(), data.data());
        }
        other.manager = nullptr;
    }

    template <Op op, std::size_t U, bool MutOther>
    constexpr void doConstructFromNonTrivial(
        const Storage<Alignment, U, false, MutOther, false, ReturnType(CallArgs...)>& other) {
        static_assert(op == Op::kClone || op == Op::kAssign);
        static_assert(N >= U);
        if constexpr (op == Op::kAssign) {
            doDelete();
        }
        manager = other.manager;
        if (manager != nullptr) {
            manager(op, static_cast<void*>(const_cast<std::byte*>(other.data.data())), data.data());
        }
    }

    template <std::size_t U, bool isAssign = false, bool AllowHeap = false, bool Copyable = true>
    constexpr void doConstructFromTrivial(void* dst, const void* src) noexcept(U <= N) {  // NOLINT
        static_assert(U <= N || AllowHeap);
        if constexpr (isAssign) {
            doDelete();
        }
        manager = [](Op o, void* src, void* dest) {
            if constexpr (U <= N) {
                if (o == Op::kDestroy) {
                    return;
                }
                std::memcpy(dest, src, U);
            } else {
                static_assert(AllowHeap);
                switch (o) {
                    case Op::kDestroy: {
                        auto* srcPtr = *static_cast<void**>(dest);
                        if constexpr (Alignment <= alignof(std::max_align_t)) {
                            ::operator delete(srcPtr);
                        } else {
                            ::operator delete(srcPtr, std::align_val_t{Alignment});
                        }
                        return;
                    }
                    case Op::kAssign:
                    case Op::kClone: {
                        if constexpr (Copyable) {
                            if constexpr (Alignment <= alignof(std::max_align_t)) {
                                auto* heapPtr = ::operator new(U);
                                void* oldData = *static_cast<void**>(src);
                                std::memcpy(heapPtr, oldData, U);
                                new (dest) void*(heapPtr);
                            } else {
                                auto* heapPtr = ::operator new(U, std::align_val_t{Alignment});
                                void* oldData = *static_cast<void**>(src);
                                std::memcpy(heapPtr, oldData, U);
                                new (dest) void*(heapPtr);
                            }
                        }
                        return;
                    }
                    case Op::kMoveAssign:
                    case Op::kMove: {
                        void* heapPtr = *static_cast<void**>(src);
                        new (dest) void*(heapPtr);
                        *static_cast<void**>(src) = nullptr;
                        return;
                    }
                }
                std::unreachable();
            }
        };
        if constexpr (U <= N) {
            std::memcpy(dst, src, U);
        } else {
            static_assert(AllowHeap);
            if constexpr (Alignment <= alignof(std::max_align_t)) {
                auto* heapPtr = ::operator new(U);
                std::memcpy(heapPtr, src, U);
                new (dst) void*(heapPtr);
            } else {
                auto* heapPtr = ::operator new(U, std::align_val_t{Alignment});
                std::memcpy(heapPtr, src, U);
                new (dst) void*(heapPtr);
            }
        }
    }

    template <typename Payload, bool AllowHeap = false, bool Copyable = true>
    constexpr void doConstruct() noexcept {  // NOLINT
        if constexpr (sizeof(Payload) <= N) {
            manager = [](Op op, void* src, void* dst) -> void {
                using T = Payload;
                switch (op) {
                    case Op::kDestroy:
                        if constexpr (!std::is_trivially_destructible_v<T>) {
                            static_cast<T*>(dst)->~T();
                        }
                        return;

                    case Op::kAssign:
                    case Op::kClone:
                        if constexpr (Copyable) {
                            if constexpr (std::is_trivially_copy_constructible_v<T>) {
                                std::memcpy(dst, src, sizeof(T));
                            } else {
                                new (dst) T(*static_cast<const T*>(src));
                            }
                        }
                        return;

                    case Op::kMoveAssign:
                    case Op::kMove:
                        if constexpr (std::is_trivially_move_constructible_v<T>) {
                            std::memcpy(dst, src, sizeof(T));
                        } else {
                            new (dst) T(std::move(*static_cast<T*>(src)));
                        }
                        return;
                }
                std::unreachable();
            };
        } else {
            static_assert(AllowHeap && N >= sizeof(void*));
            manager = [](Op op, void* src, void* dst) -> void {
                using T = Payload;
                switch (op) {
                    case Op::kDestroy: {
                        T* srcPtr = *static_cast<T**>(dst);
                        if (srcPtr == nullptr) {
                            return;
                        }
                        if constexpr (!std::is_trivially_destructible_v<T>) {
                            (srcPtr)->~T();
                        }
                        if constexpr (Alignment <= alignof(std::max_align_t)) {
                            ::operator delete(srcPtr);
                        } else {
                            ::operator delete(srcPtr, std::align_val_t{Alignment});
                        }
                        return;
                    }
                    case Op::kAssign:
                    case Op::kClone: {
                        if constexpr (Copyable) {
                            if constexpr (Alignment <= alignof(std::max_align_t)) {
                                auto* heapPtr = ::operator new(sizeof(T));
                                if constexpr (std::is_trivially_copy_constructible_v<T>) {
                                    std::memcpy(heapPtr, *static_cast<const T**>(src), sizeof(T));
                                } else {
                                    new (heapPtr) T(**static_cast<const T**>(src));
                                }
                                new (dst) T*(static_cast<T*>(heapPtr));
                            } else {
                                auto* heapPtr =
                                    ::operator new(sizeof(T), std::align_val_t{Alignment});
                                if constexpr (std::is_trivially_copy_constructible_v<T>) {
                                    std::memcpy(heapPtr, *static_cast<const T**>(src), sizeof(T));
                                } else {
                                    new (heapPtr) T(**static_cast<const T**>(src));
                                }
                                new (dst) T*(static_cast<T*>(heapPtr));
                            }
                        }
                        return;
                    }
                    case Op::kMoveAssign:
                    case Op::kMove: {
                        void* heapPtr = *static_cast<void**>(src);
                        new (dst) void*(heapPtr);
                        *static_cast<void**>(src) = nullptr;
                        return;
                    }
                }
                std::unreachable();
            };
        }
    }

    [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept {
        return manager == nullptr &&
               std::all_of(data.begin(), data.end(), [](auto v) { return v == 0; });
    }
};

template <std::size_t Alignment, typename ReturnType, typename... CallArgs>
struct Storage<Alignment, 0, false, false, false, ReturnType(CallArgs...)> {
    InvokerTypeNoBuffer<ReturnType, CallArgs...> invoker{nullptr};

    template <Op op, std::size_t U, bool MutOther>
    constexpr void doConstructFromNonTrivial(
        Storage<Alignment, 0, false, MutOther, false, ReturnType(CallArgs...)>&&
            other) noexcept {  // NOLINT
        static_assert(op == Op::kMove || op == Op::kMoveAssign);
    }

    template <Op op, std::size_t U, bool MutOther>
    constexpr void doConstructFromNonTrivial(
        const Storage<Alignment, 0, false, MutOther, false, ReturnType(CallArgs...)>& /*other*/) {
        static_assert(op == Op::kClone || op == Op::kAssign);
    }

    template <std::size_t U, bool isAssign = false, bool AllowHeap = false, bool Copyable = true>
    constexpr void doConstructFromTrivial(void* /*dst*/, const void* /*src*/) noexcept {}

    template <typename Payload, bool AllowHeap = false, bool Copyable = true>
    constexpr void doConstruct() noexcept {}

    [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept {
        return true;
    }
};

template <std::size_t Alignment, std::size_t N, typename ReturnType, typename... CallArgs>
    requires(N > 0)
struct Storage<Alignment, N, false, false, true, ReturnType(CallArgs...)> {
    alignas(Alignment) std::array<std::byte, N> data{};
    InvokerType<ReturnType, CallArgs...> invoker{nullptr};
    void (*cloner)(void* dst, const void* src){nullptr};
    void (*mover)(void* dst, void* src){nullptr};
    void (*deleter)(void* data){nullptr};

    constexpr void doDelete() noexcept {
        if (deleter != nullptr) {
            deleter(data.data());
            deleter = nullptr;
        }
    }

    template <Op op, std::size_t U, bool MutOther>
    constexpr void doConstructFromNonTrivial(
        Storage<Alignment, U, false, MutOther, true, ReturnType(CallArgs...)>&&
            other) noexcept {  // NOLINT
        static_assert(op == Op::kMove || op == Op::kMoveAssign);
        static_assert(N >= U);
        if constexpr (op == Op::kMoveAssign) {
            doDelete();
        }
        cloner = other.cloner;
        mover = other.mover;
        deleter = other.deleter;
        if (mover != nullptr) {
            mover(data.data(), other.data.data());
        }
        other.cloner = nullptr;
        other.mover = nullptr;
        other.deleter = nullptr;
    }

    template <Op op, std::size_t U, bool MutOther>
    constexpr void doConstructFromNonTrivial(
        const Storage<Alignment, U, false, MutOther, true, ReturnType(CallArgs...)>& other) {
        static_assert(op == Op::kClone || op == Op::kAssign);
        static_assert(N >= U);
        if constexpr (op == Op::kAssign) {
            doDelete();
        }
        cloner = other.cloner;
        mover = other.mover;
        deleter = other.deleter;
        if (cloner != nullptr) {
            cloner(data.data(), other.data.data());
        }
    }

    template <std::size_t U, bool isAssign = false, bool AllowHeap = false, bool Copyable = true>
    constexpr void doConstructFromTrivial(void* dst, const void* src) noexcept {
        static_assert(N >= U);
        static_assert(Copyable);
        if constexpr (isAssign) {
            doDelete();
        }
        cloner = [](void* dst, const void* src) { std::memcpy(dst, src, U); };
        mover = [](void* dst, void* src) { std::memcpy(dst, src, U); };
        deleter = nullptr;
        std::memcpy(dst, src, U);
    }

    template <typename Payload, bool AllowHeap = false, bool Copyable = true>
    constexpr void doConstruct() noexcept {
        static_assert(Copyable);
        cloner = [](void* dst, const void* src) {
            if constexpr (std::is_trivially_copy_constructible_v<Payload>) {
                std::memcpy(dst, src, sizeof(Payload));
            } else {
                new (dst) Payload(*static_cast<const Payload*>(src));
            };
        };
        mover = [](void* dst, void* src) {
            if constexpr (std::is_trivially_move_constructible_v<Payload>) {
                std::memcpy(dst, src, sizeof(Payload));
            } else {
                new (dst) Payload(std::move(*static_cast<Payload*>(src)));
            }
        };
        deleter = [](void* data) {
            if constexpr (!std::is_trivially_destructible_v<Payload>) {
                static_cast<Payload*>(data)->~Payload();
            }
        };
    }

    [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept {
        return cloner == nullptr && mover == nullptr && deleter == nullptr;
        std::all_of(data.begin(), data.end(), [](auto v) { return v == 0; });
    }
};

template <std::size_t Alignment, typename ReturnType, typename... CallArgs>
struct Storage<Alignment, 0, false, false, true, ReturnType(CallArgs...)> {
    InvokerTypeNoBuffer<ReturnType, CallArgs...> invoker{nullptr};

    constexpr void doDelete() noexcept {}

    template <Op op, std::size_t U, bool MutOther>
    constexpr void doConstructFromNonTrivial(
        Storage<Alignment, 0, false, MutOther, true, ReturnType(CallArgs...)>&&
            other) noexcept {  // NOLINT
        static_assert(op == Op::kMove || op == Op::kMoveAssign);
        if constexpr (op == Op::kMoveAssign) {
            doDelete();
        }
    }

    template <Op op, std::size_t U, bool MutOther>
    constexpr void doConstructFromNonTrivial(
        const Storage<Alignment, 0, false, MutOther, true, ReturnType(CallArgs...)>& /*other*/) {
        static_assert(op == Op::kClone || op == Op::kAssign);
        if constexpr (op == Op::kAssign) {
            doDelete();
        }
    }

    template <std::size_t U, bool isAssign = false, bool AllowHeap = false, bool Copyable = true>
    constexpr void doConstructFromTrivial(void* /*dst*/, const void* /*src*/) noexcept {
        if constexpr (isAssign) {
            doDelete();
        }
    }

    template <typename Payload, bool AllowHeap = false, bool Copyable = true>
    constexpr void doConstruct() noexcept {}

    [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept {
        return true;
    }
};

template <typename Signature, bool Copyable = true, std::size_t N = 16, bool IsTrivial = false,
          bool Mutable = false, bool Extended = false, bool AllowHeap = false,
          std::size_t Align = alignof(std::max_align_t)>
struct Task;

template <typename T>
struct IsTask : std::false_type {};

template <typename R, typename... Args, bool Copyable, std::size_t N, bool IsTrivial, bool Mutable,
          bool Extended, bool AllowHeap, std::size_t Align>
struct IsTask<Task<R(Args...), Copyable, N, IsTrivial, Mutable, Extended, AllowHeap, Align>>
    : std::true_type {};

template <auto F>
struct FunctionTag {};

template <typename T>
struct IsFunctionTag : std::false_type {};

template <auto F>
struct IsFunctionTag<FunctionTag<F>> : std::true_type {};

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

struct RefTag {};

template <typename ReturnType, typename... CallArgs, bool Copyable, std::size_t N, bool IsTrivial,
          bool Mutable, bool Extended, bool AllowHeap, std::size_t Align>
struct Task<ReturnType(CallArgs...), Copyable, N, IsTrivial, Mutable, Extended, AllowHeap, Align> {
    template <typename, bool, std::size_t, bool, bool, bool, bool, std::size_t>
    friend struct Task;

    static_assert(!IsTrivial || !AllowHeap, "Trivial Task should not allow heap usage");
    static_assert(N > 0 || IsTrivial, "0 buffer size is only allowed on trivial types");
    static_assert(!Extended || !AllowHeap,
                  "Extended mode with heap under development. Switch one off");
    static_assert(Align > 0 && (Align & (Align - 1)) == 0, "Alignment must be a power of 2");
    static_assert(Copyable || !Extended, "Move only types under development for Extended option.");

    using R = ReturnType;
    using Args = PayloadTuple<CallArgs...>;
    using Signature = ReturnType(CallArgs...);

   private:
    Storage<Align, N, IsTrivial, Mutable, Extended, Signature> storage_;

    template <Op op, bool CopySrc, std::size_t N_src, bool MutSrc, bool IsTrivialSrc, bool ExtSrc,
              bool AllowHeapSrc>
    constexpr void doConvert(auto&& other) noexcept(  // NOLINT
        (op == Op::kMove || op == Op::kMoveAssign) &&
        (!IsTrivialSrc || IsTrivial || !AllowHeap || N_src <= N))
        requires std::same_as<std::remove_cvref_t<decltype(other)>,
                              Task<ReturnType(CallArgs...), CopySrc, N_src, IsTrivialSrc, MutSrc,
                                   ExtSrc, AllowHeapSrc>>
    {
        static_assert(std::is_const_v<std::remove_reference_t<decltype(other)>> ||
                          std::is_rvalue_reference_v<decltype(other)>,
                      "T must be const& or rvalue&&");
        constexpr bool kIsMovable = std::is_rvalue_reference_v<decltype(other)>;
        auto& dest = *this;
        static_assert(N >= N_src || (AllowHeap && IsTrivialSrc && !AllowHeapSrc),
                      "Payload too large to move into this storage");
        static_assert(
            !AllowHeapSrc || AllowHeap,
            "Cannot convert a task with heap storage option into one where it is disabled");
        static_assert(Mutable || !MutSrc);
        static_assert(!IsTrivial || IsTrivialSrc,
                      "Cannot copy/move non-trivial task into trivial one");
        static_assert(CopySrc || (op != Op::kClone && op != Op::kAssign));

        static_assert((Extended == ExtSrc) || (IsTrivial && IsTrivialSrc));
        static_assert(op != Op::kDestroy, "Called Task converter with invalid (destroy) flag");
        static_assert((kIsMovable && (op == Op::kMove || op == Op::kMoveAssign)) ||
                      (op == Op::kClone || op == Op::kAssign));
        dest.storage_.invoker = other.storage_.invoker;

        if constexpr (IsTrivialSrc) {
            if constexpr (IsTrivial) {
                if constexpr (N_src > 0) {
                    std::memcpy(dest.storage_.data.data(), other.storage_.data.data(), N_src);
                }
            } else {
                if constexpr (N_src == 0) {
                    dest.storage_.template doConstructFromTrivial<
                        N_src, op == Op::kAssign || op == Op::kMoveAssign, AllowHeap, Copyable>(
                        dest.storage_.data.data(), nullptr);
                } else {
                    dest.storage_.template doConstructFromTrivial<
                        N_src, op == Op::kAssign || op == Op::kMoveAssign, AllowHeap, Copyable>(
                        dest.storage_.data.data(), other.storage_.data.data());
                }
            }
        } else {
            if constexpr (kIsMovable) {
                dest.storage_.template doConstructFromNonTrivial<op, N_src, MutSrc>(
                    std::move(other.storage_));
            } else {
                dest.storage_.template doConstructFromNonTrivial<op, N_src, MutSrc>(other.storage_);
            }
        }

        if constexpr (kIsMovable) {
            other.storage_.invoker = nullptr;
        }
    }

    struct Internal {
        [[nodiscard]] constexpr const auto& getInvoker() const noexcept {
            return impl_.storage_.invoker;
        }

        [[nodiscard]] constexpr auto getBuffer() const noexcept {
            if constexpr (N == 0) {
                return std::span<const std::byte>{};
            } else {
                return std::span<const std::byte>{impl_.storage_.data};
            }
        }

        Internal(const Task& i) : impl_{i} {}
        Internal() = delete;
        Internal(const Internal&) = delete;
        Internal& operator=(const Internal&) = delete;
        Internal(Internal&&) = delete;
        Internal& operator=(Internal&&) = delete;
        ~Internal() = default;

       private:
        const Task& impl_;
    };

   public:
    [[nodiscard]] static constexpr auto getBufferSize() noexcept -> std::size_t {
        return N;
    }

    [[nodiscard]] static constexpr auto getSize() noexcept -> std::size_t {
        return sizeof(Task);
    }

    template <typename Sig>
    [[nodiscard]] static constexpr auto matchesSignature() noexcept -> bool {
        return std::is_same_v<Signature, Sig>;
    }

    template <typename R, typename... Args>
        requires(sizeof...(Args) > 0)
    [[nodiscard]] static constexpr auto matchesSignature() noexcept -> bool {
        return Task::matchesSignature<R(Args...)>();
    }

    [[nodiscard]] static constexpr auto isCopyable() noexcept -> bool {
        return Copyable;
    }

    [[nodiscard]] static constexpr auto isMoveOnly() noexcept -> bool {
        return !Copyable;
    }

    [[nodiscard]] static constexpr auto isTrivial() noexcept -> bool {
        return IsTrivial;
    }

    [[nodiscard]] static constexpr auto isMutable() noexcept -> bool {
        return Mutable;
    }

    [[nodiscard]] static constexpr auto isExtended() noexcept -> bool {
        return Extended;
    }

    [[nodiscard]] static constexpr auto allowsHeap() noexcept -> bool {
        return AllowHeap;
    }

    [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept {
        return storage_.invoker == nullptr;
    }

    constexpr explicit operator bool() const noexcept {
        return storage_.invoker != nullptr;
    }

    [[nodiscard]] constexpr bool operator==(const auto& other) const noexcept
        requires(IsTask<std::remove_cvref_t<decltype(other)>>::value)
    = delete;

    [[nodiscard]] constexpr auto internal() const noexcept {
        return Internal{*this};
    }

    template <std::size_t M, bool Mut, bool IsTrivial2, bool Ext2, bool AllowHeapSrc>
        requires((Mutable || !Mut) && (!IsTrivial || IsTrivial2) &&
                 (IsTrivial || IsTrivial2 || Extended == Ext2) && (AllowHeap || !AllowHeapSrc))
    Task& operator=(const Task<ReturnType(CallArgs...), true, M, IsTrivial2, Mut, Ext2,
                               AllowHeapSrc, Align>& other) {
        doConvert<Op::kAssign, true, M, Mut, IsTrivial2, Ext2, AllowHeapSrc>(other);
        return *this;
    };

    template <std::size_t M, bool Mut, bool IsTrivial2, bool Ext2, bool AllowHeapSrc>
        requires((Mutable || !Mut) && (!IsTrivial || IsTrivial2) &&
                 (IsTrivial || IsTrivial2 || Extended == Ext2) && (AllowHeap || !AllowHeapSrc))
    Task(const Task<ReturnType(CallArgs...), true, M, IsTrivial2, Mut, Ext2, AllowHeapSrc, Align>&
             other) {
        doConvert<Op::kClone, true, M, Mut, IsTrivial2, Ext2, AllowHeapSrc>(other);
    };

    Task(const Task& other)
        requires(IsTrivial && Copyable)
    = default;

    Task(const Task& other)
        requires(!IsTrivial && Copyable)
    {
        doConvert<Op::kClone, Copyable, N, Mutable, IsTrivial, Extended, AllowHeap>(other);
    }

    Task(const Task& other)
        requires(!Copyable)
    = delete;

    Task& operator=(const Task& other)
        requires(IsTrivial && Copyable)
    = default;

    Task& operator=(const Task& other)
        requires(!IsTrivial && Copyable)
    {
        if (this == &other) {
            return *this;
        }
        doConvert<Op::kAssign, Copyable, N, Mutable, IsTrivial, Extended, AllowHeap>(other);
        return *this;
    }

    Task& operator=(const Task& other)
        requires(!Copyable)
    = delete;

    template <bool CopyOther, std::size_t M, bool Mut, bool IsTrivial2, bool Ext2,
              bool AllowHeapSrc>
        requires((Mutable || !Mut) && (!IsTrivial || IsTrivial2) &&
                 (IsTrivial || IsTrivial2 || Extended == Ext2) && (AllowHeap || !AllowHeapSrc))
    Task(Task<ReturnType(CallArgs...), CopyOther, M, IsTrivial2, Mut, Ext2, AllowHeapSrc, Align>&&
             other) noexcept(!IsTrivial2 || IsTrivial || !AllowHeap || M <= N) {
        doConvert<Op::kMove, CopyOther, M, Mut, IsTrivial2, Ext2, AllowHeapSrc>(std::move(other));
    }

    template <bool CopyOther, std::size_t M, bool Mut, bool IsTrivial2, bool Ext2,
              bool AllowHeapSrc>
        requires((Mutable || !Mut) && (!IsTrivial || IsTrivial2) &&
                 (IsTrivial || IsTrivial2 || Extended == Ext2) && (AllowHeap || !AllowHeapSrc))
    Task& operator=(Task<ReturnType(CallArgs...), CopyOther, M, IsTrivial2, Mut, Ext2, AllowHeapSrc,
                         Align>&& other) noexcept(!IsTrivial2 || IsTrivial || !AllowHeap ||
                                                  M <= N) {
        doConvert<Op::kMoveAssign, CopyOther, M, Mut, IsTrivial2, Ext2, AllowHeapSrc>(
            std::move(other));
        return *this;
    }

    Task(Task&& other) noexcept
        requires(IsTrivial)
    = default;

    Task(Task&& other) noexcept
        requires(!IsTrivial)
    {
        doConvert<Op::kMove, Copyable, N, Mutable, IsTrivial, Extended, AllowHeap>(
            std::move(other));
    }

    Task& operator=(Task&& other) noexcept
        requires(IsTrivial)
    = default;

    Task& operator=(Task&& other) noexcept
        requires(!IsTrivial)
    {
        if (this == &other) {
            return *this;
        }
        doConvert<Op::kMoveAssign, Copyable, N, Mutable, IsTrivial, Extended, AllowHeap>(
            std::move(other));
        return *this;
    }

    Task& operator=(std::nullptr_t) {
        clear();
        return *this;
    }

    constexpr Task() = default;

    constexpr Task(std::nullptr_t) {};

    constexpr ~Task() noexcept
        requires(IsTrivial)
    = default;

    constexpr ~Task() noexcept
        requires(!IsTrivial)
    {
        storage_.doDelete();
    }

    enum class ArgsOrder : std::uint8_t { kBack, kFront };

    template <auto F, typename... BoundArgs>
    constexpr Task([[maybe_unused]] FunctionTag<F> /*unused*/, BoundArgs&&... args)  // NOLINT
        : storage_{.invoker =
                       InvokerFactory<N, Mutable, ReturnType, CallArgs...>::template getInvoker<
                           PayloadArgsT<F, BoundArgs...>, F, BoundArgs...>()} {
        using PayloadType = PayloadArgsT<F, BoundArgs...>;
        static_assert(PayloadNoRvalueRef<PayloadType>::value);
        static_assert(
            !Copyable || std::is_copy_constructible_v<PayloadType>,
            "Given payload is not copy constructible. Check types or create a unique task");
        static_assert(
            std::is_move_constructible_v<PayloadType> && std::is_destructible_v<PayloadType>,
            "Given payload is not constructible, move constructible or destructible");
        static_assert(sizeof(PayloadType) <= N || (AllowHeap && sizeof(PayloadType*) <= N),
                      "Size exceeds buffer. Increase Stack size or activate option to allow heap "
                      "storage (template option, or via FlexTask aliases)");
        static_assert(alignof(PayloadType) <= Align,
                      "Alignment requirement too high for buffer. Increase template alignment");
        if constexpr (IsTrivial) {
            static_assert(TrivialUse<PayloadType, Copyable>::value,
                          "Payload needs to be trivially move & copy constructible, and "
                          "trivially destructible to be allowed within a Trivial Task");
        }
        storage_.template doConstruct<PayloadType, AllowHeap, Copyable>();
        if constexpr (sizeof(PayloadType) <= N) {
            new (storage_.data.data()) PayloadType{{std::forward<BoundArgs>(args)...}};
        } else {
            static_assert(alignof(void*) <= Align,
                          "Alignment requirement too high to store heap pointer. Increase "
                          "template alignment");
            static_assert(AllowHeap);
            if constexpr (Align <= alignof(std::max_align_t)) {
                void* heapPtr = ::operator new(sizeof(PayloadType));
                auto* payload =  // NOLINT
                    new (heapPtr) PayloadType{{std::forward<BoundArgs>(args)...}};
                new (storage_.data.data()) PayloadType*(payload);
            } else {
                void* heapPtr = ::operator new(sizeof(PayloadType), std::align_val_t{Align});
                auto* payload =  // NOLINT
                    new (heapPtr) PayloadType{{std::forward<BoundArgs>(args)...}};
                new (storage_.data.data()) PayloadType*(payload);
            }
        }
    }

    template <typename... BoundArgs>
    constexpr Task(auto&& f, BoundArgs&&... args)  // NOLINT
        requires(!std::is_same_v<std::remove_cvref_t<decltype(f)>, RefTag> &&
                 !IsFunctionTag<std::remove_cvref_t<decltype(f)>>::value &&
                 (sizeof...(BoundArgs) > 0 || !IsTask<std::remove_cvref_t<decltype(f)>>::value))
        : storage_{.invoker =
                       InvokerFactory<N, Mutable, ReturnType, CallArgs...>::template getInvoker<
                           PayloadT<decltype(f), BoundArgs...>, std::decay_t<decltype(f)>,
                           std::decay_t<BoundArgs>...>()} {
        using F = decltype(f);
        using PayloadType = PayloadT<F, BoundArgs...>;
        static_assert(PayloadNoRvalueRef<PayloadType>::value);
        static_assert(sizeof(PayloadType) <= N || (AllowHeap && sizeof(PayloadType*) <= N),
                      "Size exceeds buffer. Increase Stack size or activate option to allow heap "
                      "storage (template option, or via FlexTask aliases)");
        static_assert(alignof(PayloadType) <= Align,
                      "Alignment requirement too high for buffer. Increase template alignment");
        static_assert(
            !Copyable || std::is_copy_constructible_v<PayloadType>,
            "Given payload is not copy constructible. Check types or create a unique task");
        static_assert(
            std::is_move_constructible_v<PayloadType> && std::is_destructible_v<PayloadType>,
            "Given payload is not move constructible or destructible");
        if constexpr (IsTrivial) {
            static_assert(TrivialUse<PayloadType, Copyable>::value,
                          "Payload needs to be trivially move & copy constructible, and "
                          "trivially destructible to be allowed within a Trivial Task");
        }
        storage_.template doConstruct<PayloadType, AllowHeap, Copyable>();
        if constexpr (sizeof(PayloadType) <= N) {
            new (storage_.data.data())
                PayloadType{std::forward<F>(f), {std::forward<BoundArgs>(args)...}};
        } else {
            static_assert(AllowHeap);
            static_assert(alignof(void*) <= Align,
                          "Alignment requirement too high to store heap pointer. Increase "
                          "template alignment");
            if constexpr (Align <= alignof(std::max_align_t)) {
                void* heapPtr = ::operator new(sizeof(PayloadType));
                auto* payload =  // NOLINT
                    new (heapPtr)
                        PayloadType{std::forward<F>(f), {std::forward<BoundArgs>(args)...}};
                new (storage_.data.data()) PayloadType*(payload);
            } else {
                void* heapPtr = ::operator new(sizeof(PayloadType), std::align_val_t{Align});
                auto* payload =  // NOLINT
                    new (heapPtr)
                        PayloadType{std::forward<F>(f), {std::forward<BoundArgs>(args)...}};
                new (storage_.data.data()) PayloadType*(payload);
            }
        }
    }

    template <typename F, typename... BoundArgs>
    constexpr Task([[maybe_unused]] RefTag /*unused*/, F&& f, BoundArgs&&... args)  // NOLINT
        requires(!IsFunctionTag<std::remove_cvref_t<decltype(f)>>::value &&
                 (sizeof...(BoundArgs) > 0 || !IsTask<std::remove_cvref_t<F>>::value))
        : storage_{.invoker =
                       InvokerFactory<N, Mutable, ReturnType, CallArgs...>::template getInvoker<
                           PayloadImpl<F, BoundArgs...>, F, BoundArgs...>()} {
        static_assert(
            (((std::is_lvalue_reference_v<decltype(args)> &&
               std::is_lvalue_reference_v<BoundArgs>) ||
              (!std::is_rvalue_reference_v<BoundArgs> && !std::is_lvalue_reference_v<BoundArgs> &&
               std::is_rvalue_reference_v<decltype(args)>)) &&
             ...));
        using PayloadType = PayloadImpl<F, BoundArgs...>;
        static_assert(PayloadNoRvalueRef<PayloadType>::value);
        static_assert(sizeof(PayloadType) <= N || (AllowHeap && sizeof(PayloadType*) <= N),
                      "Size exceeds buffer. Increase Stack size or activate option to allow heap "
                      "storage (template option, or via FlexTask aliases)");
        static_assert(alignof(PayloadType) <= Align,
                      "Alignment requirement too high for buffer. Increase template alignment");
        static_assert(
            !Copyable || std::is_copy_constructible_v<PayloadType>,
            "Given payload is not copy constructible. Check types or create a unique task");
        static_assert(
            std::is_move_constructible_v<PayloadType> && std::is_destructible_v<PayloadType>,
            "Given payload is not constructible, move constructible or destructible");
        if constexpr (IsTrivial) {
            static_assert(TrivialUse<PayloadType, Copyable>::value,
                          "Payload needs to be trivially move & copy constructible, and "
                          "trivially destructible to be allowed within a Trivial Task");
        }
        storage_.template doConstruct<PayloadType, AllowHeap, Copyable>();
        if constexpr (sizeof(PayloadType) <= N) {
            new (storage_.data.data())
                PayloadType{std::forward<F>(f), {std::forward<BoundArgs>(args)...}};
        } else {
            static_assert(AllowHeap);
            static_assert(alignof(void*) <= Align,
                          "Alignment requirement too high to store heap pointer. Increase "
                          "template alignment");
            if constexpr (Align <= alignof(std::max_align_t)) {
                void* heapPtr = ::operator new(sizeof(PayloadType));
                auto* payload =  // NOLINT
                    new (heapPtr)
                        PayloadType{std::forward<F>(f), {std::forward<BoundArgs>(args)...}};
                new (storage_.data.data()) PayloadType*(payload);
            } else {
                void* heapPtr = ::operator new(sizeof(PayloadType), std::align_val_t{Align});
                auto* payload =  // NOLINT
                    new (heapPtr)
                        PayloadType{std::forward<F>(f), {std::forward<BoundArgs>(args)...}};
                new (storage_.data.data()) PayloadType*(payload);
            }
        }
    }

    template <auto F, typename... BoundArgs>
    constexpr Task([[maybe_unused]] RefTag /*unused*/,                               // NOLINT
                   [[maybe_unused]] FunctionTag<F> /*unused*/, BoundArgs&&... args)  // NOLINT
        : storage_{.invoker =
                       InvokerFactory<N, Mutable, ReturnType, CallArgs...>::template getInvoker<
                           PayloadImplArgs<F, BoundArgs...>, F, BoundArgs...>()} {
        static_assert(
            (((std::is_lvalue_reference_v<decltype(args)> &&
               std::is_lvalue_reference_v<BoundArgs>) ||
              (!std::is_rvalue_reference_v<BoundArgs> && !std::is_lvalue_reference_v<BoundArgs> &&
               std::is_rvalue_reference_v<decltype(args)>)) &&
             ...));
        using PayloadType = PayloadImplArgs<F, BoundArgs...>;
        static_assert(PayloadNoRvalueRef<PayloadType>::value);
        static_assert(sizeof(PayloadType) <= N || (AllowHeap && sizeof(PayloadType*) <= N),
                      "Size exceeds buffer. Increase Stack size or activate option to allow heap "
                      "storage (template option, or via FlexTask aliases)");
        static_assert(alignof(PayloadType) <= Align,
                      "Alignment requirement too high for buffer. Increase template alignment");
        static_assert(
            !Copyable || std::is_copy_constructible_v<PayloadType>,
            "Given payload is not copy constructible. Check types or create a unique task");
        static_assert(
            std::is_move_constructible_v<PayloadType> && std::is_destructible_v<PayloadType>,
            "Given payload is not constructible, move constructible or destructible");
        if constexpr (IsTrivial) {
            static_assert(TrivialUse<PayloadType, Copyable>::value,
                          "Payload needs to be trivially move & copy constructible, and "
                          "trivially destructible to be allowed within a Trivial Task");
        }
        storage_.template doConstruct<PayloadType, AllowHeap, Copyable>();
        if constexpr (sizeof(PayloadType) <= N) {
            new (storage_.data.data()) PayloadType{{std::forward<BoundArgs>(args)...}};
        } else {
            static_assert(AllowHeap);
            static_assert(alignof(void*) <= Align,
                          "Alignment requirement too high to store heap pointer. Increase "
                          "template alignment");
            if constexpr (Align <= alignof(std::max_align_t)) {
                void* heapPtr = ::operator new(sizeof(PayloadType));
                auto* payload =  // NOLINT
                    new (heapPtr) PayloadType{{std::forward<BoundArgs>(args)...}};
                new (storage_.data.data()) PayloadType*(payload);
            } else {
                void* heapPtr = ::operator new(sizeof(PayloadType), std::align_val_t{Align});
                auto* payload =  // NOLINT
                    new (heapPtr) PayloadType{{std::forward<BoundArgs>(args)...}};
                new (storage_.data.data()) PayloadType*(payload);
            }
        }
    }

    FORCE_INLINE ReturnType operator()(CallArgs... args) {
        if constexpr (N == 0) {
            return storage_.invoker(std::forward<CallArgs>(args)...);
        } else {
            return storage_.invoker(static_cast<void*>(storage_.data.data()),
                                    std::forward<CallArgs>(args)...);
        }
    }

    FORCE_INLINE ReturnType invoke(CallArgs... args) {
        if (storage_.invoker == nullptr) [[unlikely]] {
            throw std::bad_function_call();
        }
        if constexpr (N == 0) {
            return storage_.invoker(std::forward<CallArgs>(args)...);
        } else {
            return storage_.invoker(static_cast<void*>(storage_.data.data()),
                                    std::forward<CallArgs>(args)...);
        }
    }

    FORCE_INLINE ReturnType operator()(CallArgs... args) const {
        if constexpr (N == 0) {
            return storage_.invoker(std::forward<CallArgs>(args)...);
        } else if constexpr (Mutable) {
            return storage_.invoker(static_cast<void*>(storage_.data.data()),
                                    std::forward<CallArgs>(args)...);
        } else {
            return storage_.invoker(
                static_cast<void*>(const_cast<std::byte*>(storage_.data.data())),
                std::forward<CallArgs>(args)...);
        }
    }

    FORCE_INLINE ReturnType invoke(CallArgs... args) const {
        if (storage_.invoker == nullptr) [[unlikely]] {
            throw std::bad_function_call();
        }
        if constexpr (N == 0) {
            return storage_.invoker(std::forward<CallArgs>(args)...);
        } else if constexpr (Mutable) {
            return storage_.invoker(static_cast<void*>(storage_.data.data()),
                                    std::forward<CallArgs>(args)...);
        } else {
            return storage_.invoker(
                static_cast<void*>(const_cast<std::byte*>(storage_.data.data())),
                std::forward<CallArgs>(args)...);
        }
    }

    constexpr void clear() noexcept {
        storage_.invoker = nullptr;
        if constexpr (!IsTrivial) {
            storage_.doDelete();
        }
    }
};

template <typename Signature, std::size_t N = 64, bool IsTrivial = false, bool Mutable = false,
          bool Extended = false, bool AllowHeap = false,
          std::size_t Alignment = alignof(std::max_align_t)>
using CopyableTask = Task<Signature, true, N, IsTrivial, Mutable, Extended, AllowHeap, Alignment>;

template <typename Signature, std::size_t N = 64, bool IsTrivial = false, bool Mutable = false,
          bool Extended = false, bool AllowHeap = false,
          std::size_t Alignment = alignof(std::max_align_t)>
using UniqueTask = Task<Signature, false, N, IsTrivial, Mutable, Extended, AllowHeap, Alignment>;

template <typename Signature, bool Copyable = true, bool IsTrivial = false, bool Mutable = false,
          bool Extended = false>
using Task16 = Task<Signature, Copyable, 16, IsTrivial, Mutable, Extended, false>;
template <typename Signature, bool Copyable = true, bool IsTrivial = false, bool Mutable = false,
          bool Extended = false>
using Task32 = Task<Signature, Copyable, 32, IsTrivial, Mutable, Extended, false>;
template <typename Signature, bool Copyable = true, bool IsTrivial = false, bool Mutable = false,
          bool Extended = false>
using Task48 = Task<Signature, Copyable, 48, IsTrivial, Mutable, Extended, false>;
template <typename Signature, bool Copyable = true, bool IsTrivial = false, bool Mutable = false,
          bool Extended = false>
using Task64 = Task<Signature, Copyable, 64, IsTrivial, Mutable, Extended, false>;
template <typename Signature, bool Copyable = true, bool IsTrivial = false, bool Mutable = false,
          bool Extended = false>
using Task128 = Task<Signature, Copyable, 128, IsTrivial, Mutable, Extended, false>;
template <typename Signature, bool Copyable = true, bool IsTrivial = false, bool Mutable = false,
          bool Extended = false>
using Task256 = Task<Signature, Copyable, 256, IsTrivial, Mutable, Extended, false>;

template <typename Signature, bool Copyable = true, bool Mutable = false>
using TrivialTask16 = Task16<Signature, Copyable, true, Mutable, false>;
template <typename Signature, bool Copyable = true, bool Mutable = false>
using TrivialTask32 = Task32<Signature, Copyable, true, Mutable, false>;
template <typename Signature, bool Copyable = true, bool Mutable = false>
using TrivialTask48 = Task48<Signature, Copyable, true, Mutable, false>;
template <typename Signature, bool Copyable = true, bool Mutable = false>
using TrivialTask64 = Task64<Signature, Copyable, true, Mutable, false>;
template <typename Signature, bool Copyable = true, bool Mutable = false>
using TrivialTask128 = Task128<Signature, Copyable, true, Mutable, false>;
template <typename Signature, bool Copyable = true, bool Mutable = false>
using TrivialTask256 = Task256<Signature, Copyable, true, Mutable, false>;

template <typename Signature, bool Copyable = true, bool Mutable = false, bool Extended = false>
using SmallTask = Task<Signature, Copyable, 16, false, Mutable, Extended, false>;

template <typename Signature, bool Copyable = true, std::size_t N = 64, bool Extended = false>
using TaskMut = Task<Signature, Copyable, N, false, true, Extended, false>;

template <typename Signature, bool Copyable = true, bool Extended = false>
using SmallTaskMut = Task<Signature, Copyable, 16, false, true, Extended, false>;

template <typename Signature, bool Copyable = true, bool M = false, std::size_t N = 64,
          bool AllowHeap = false>
using TrivialTask = Task<Signature, Copyable, N, true, M, false, AllowHeap>;

template <typename Signature, bool Copyable = true, std::size_t N = 64, bool AllowHeap = false>
using TrivialTaskMut = TrivialTask<Signature, Copyable, true, N, AllowHeap>;

template <typename Signature, bool Copyable = true, bool AllowHeap = false>
using TrivialSmallTask = TrivialTask<Signature, Copyable, false, 16, AllowHeap>;

template <typename Signature, bool Copyable = true, bool AllowHeap = false>
using TrivialSmallTaskMut = TrivialTask<Signature, Copyable, true, 16, AllowHeap>;

template <typename Signature, bool Copyable = true>
using TrivialTaskCaptureless = TrivialTask<Signature, Copyable, false, 1>;

template <typename Signature, bool Copyable = true>
using StaticTrivialTask = TrivialTask<Signature, Copyable, false, 0>;

template <typename Signature, bool Copyable = true, std::size_t N = 64, bool IsTrivial = false,
          bool Mutable = false>
using FlexTask = Task<Signature, Copyable, N, IsTrivial, Mutable, false, true>;

template <typename Signature, std::size_t N = 64, bool IsTrivial = false, bool Mutable = false>
using UniqueFlexTask = FlexTask<Signature, false, N, IsTrivial, Mutable>;

template <typename Signature, bool Copyable = true, bool IsTrivial = false, bool Mutable = false>
using FlexTask16 = FlexTask<Signature, Copyable, 16, IsTrivial, Mutable>;
template <typename Signature, bool Copyable = true, bool IsTrivial = false, bool Mutable = false>
using FlexTask32 = FlexTask<Signature, Copyable, 32, IsTrivial, Mutable>;
template <typename Signature, bool Copyable = true, bool IsTrivial = false, bool Mutable = false>
using FlexTask64 = FlexTask<Signature, Copyable, 64, IsTrivial, Mutable>;

template <typename Signature, bool Copyable = true, std::size_t N = 64, bool IsTrivial = false>
using FlexTaskMut = FlexTask<Signature, Copyable, N, IsTrivial, true>;

static_assert(TrivialTask<void(), true, false, 0>::getSize() == sizeof(void*));

template <typename Signature, bool Copyable = true>
using TrivialTaskCapturelessMut = TrivialTask<Signature, Copyable, true, 1>;

template <typename T>
struct FunctionTraits {
    static constexpr bool kValid{false};
};

template <typename T>
    requires(std::is_class_v<T> && requires { &T::operator(); })
struct FunctionTraits<T> : FunctionTraits<decltype(&T::operator())> {};

/*template <typename T>
struct FunctionTraits<T, std::void_t<decltype(&T::operator())>>
    : FunctionTraits<decltype(&T::operator())> {};*/

template <typename Ret, typename... Args>
struct FunctionTraits<Ret (*)(Args...)> {
    static constexpr bool kValid{true};
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
};

template <typename Ret, typename... Args>
struct FunctionTraits<Ret (*)(Args...) noexcept> {
    static constexpr bool kValid{true};
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
};

template <typename Class, typename Ret, typename... Args>
struct FunctionTraits<Ret (Class::*)(Args...)> {
    static constexpr bool kValid{true};
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
};

template <typename Class, typename Ret, typename... Args>
struct FunctionTraits<Ret (Class::*)(Args...) const> {
    static constexpr bool kValid{true};
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
};

template <size_t N, typename Tuple, typename Indices = std::make_index_sequence<N>>
struct slice_first;

template <size_t N, typename... Args, size_t... Is>
struct slice_first<N, std::tuple<Args...>, std::index_sequence<Is...>> {
    using type = std::tuple<std::tuple_element_t<Is, std::tuple<Args...>>...>;
};

template <size_t N, typename Tuple, typename Indices = std::make_index_sequence<N>>
struct slice_last;

template <size_t N, typename... Args, size_t... Is>
struct slice_last<N, std::tuple<Args...>, std::index_sequence<Is...>> {
    static constexpr size_t kTotal = sizeof...(Args);
    static_assert(N <= kTotal, "Slice size N cannot exceed total tuple size.");
    using type = std::tuple<std::tuple_element_t<Is + (kTotal - N), std::tuple<Args...>>...>;
};

template <typename T>
concept HasFunctionTraits = requires { FunctionTraits<T>::kValid; };

template <typename F, typename... BoundArgs>
    requires(HasFunctionTraits<std::decay_t<F>>)
struct TaskTypeDeductor {
    struct StripBoundArgs {
        using Traits = FunctionTraits<std::decay_t<F>>;
        using AllArgs = typename Traits::args_tuple;

        static constexpr size_t kRemainingCount = std::tuple_size_v<AllArgs> - sizeof...(BoundArgs);

        using SlicedTuple = typename slice_first<kRemainingCount, AllArgs>::type;
        using SlicedTupleReverse = typename slice_last<kRemainingCount, AllArgs>::type;

        template <typename Ret, typename Tuple, typename TupleReverse>
        struct rebuild;
        template <typename Ret, typename... Ts, typename... TsR>
        struct rebuild<Ret, std::tuple<Ts...>, std::tuple<TsR...>> {
            static constexpr bool kIsValid =
                std::is_invocable_v<std::decay_t<F>, Ts...,
                                    std::unwrap_ref_decay_t<BoundArgs>&&...> ||
                std::is_invocable_v<std::decay_t<F>, Ts...,
                                    std::unwrap_ref_decay_t<BoundArgs>&...> ||
                std::is_invocable_v<std::decay_t<F>, std::unwrap_ref_decay_t<BoundArgs>&&...,
                                    TsR...> ||
                std::is_invocable_v<std::decay_t<F>, std::unwrap_ref_decay_t<BoundArgs>&...,
                                    TsR...>;
            static constexpr bool kIsReverse =
                std::is_invocable_v<std::decay_t<F>, std::unwrap_ref_decay_t<BoundArgs>&&...,
                                    TsR...> ||
                std::is_invocable_v<std::decay_t<F>, std::unwrap_ref_decay_t<BoundArgs>&...,
                                    TsR...>;
            using R = Ret;
            using Args = std::conditional_t<kIsReverse, std::tuple<TsR...>, std::tuple<Ts...>>;
            using Signature = std::conditional_t<kIsReverse, Ret(TsR...), Ret(Ts...)>;
            static constexpr bool kMutable =
                (!kIsReverse &&
                 !std::is_invocable_v<const std::decay_t<F>&, Ts...,
                                      const std::unwrap_reference_t<typename RefToConstRef<
                                          std::decay_t<BoundArgs>>::Type>&...>) ||
                (kIsReverse &&
                 !std::is_invocable_v<const std::decay_t<F>&,
                                      const std::unwrap_reference_t<typename RefToConstRef<
                                          std::decay_t<BoundArgs>>::Type>&...,
                                      TsR...>);
        };

        using type = rebuild<typename Traits::return_type, SlicedTuple, SlicedTupleReverse>;
    };

    // To complete one needs case where F is static (no decay), but in that case it would be trivial
    // anyway, hence not urgent to change for now
    using PayloadType = PayloadT<F, BoundArgs...>;
    using PayloadRefType = PayloadImpl<F, BoundArgs...>;

    static constexpr bool kCopyable = std::is_copy_constructible_v<PayloadType>;
    static constexpr bool kRefCopyable = std::is_copy_constructible_v<PayloadRefType>;
    static constexpr bool kCanUseTrivial = TrivialUse<PayloadType, kCopyable>::value;
    static constexpr bool kRefCanUseTrivial = TrivialUse<PayloadRefType, kRefCopyable>::value;
    static constexpr std::size_t kAlignment =
        std::max(alignof(PayloadType), alignof(std::max_align_t));
    static constexpr std::size_t kRefAlignment =
        std::max(alignof(PayloadRefType), alignof(std::max_align_t));
    static constexpr auto kPayloadSize = sizeof(PayloadType);
    static constexpr auto kRefPayloadSize = sizeof(PayloadRefType);
    using TypeTraits = StripBoundArgs::type;
    using Signature = typename TypeTraits::Signature;
    static constexpr bool kIsValid = TypeTraits::kIsValid;

    template <bool Copyable = kCopyable, std::size_t N = kPayloadSize,
              bool IsTrivial = kCanUseTrivial, bool Mutable = TypeTraits::kMutable,
              std::size_t Align = kAlignment>
    using Type = Task<Signature, Copyable, N, IsTrivial, Mutable, false, false, Align>;

    template <std::size_t N>
    using FlexType = Task<Signature, kCopyable, N, kCanUseTrivial, TypeTraits::kMutable, false,
                          true, kAlignment>;

    using MutableType = Type<kCopyable, kPayloadSize, kCanUseTrivial, true, kAlignment>;
    using ImmutableType = Type<kCopyable, kPayloadSize, kCanUseTrivial, false, kAlignment>;

    template <bool Copyable = kRefCopyable, std::size_t N = kRefPayloadSize,
              bool IsTrivial = kRefCanUseTrivial, bool Mutable = TypeTraits::kMutable,
              std::size_t Align = kRefAlignment>
    using RefType = Task<Signature, Copyable, N, IsTrivial, Mutable, false, false, Align>;

    using MutableRefType =
        RefType<kRefCopyable, kRefPayloadSize, kRefCanUseTrivial, true, kRefAlignment>;
    using ImmutableRefType =
        RefType<kRefCopyable, kRefPayloadSize, kRefCanUseTrivial, false, kRefAlignment>;
};

template <auto F, typename... BoundArgs>
    requires(HasFunctionTraits<decltype(F)>)
struct TaskTypeDeductorAuto {
    using F_Type = decltype(F);
    struct StripBoundArgs {
        using Traits = FunctionTraits<std::decay_t<decltype(F)>>;
        using AllArgs = typename Traits::args_tuple;

        static constexpr size_t kRemainingCount = std::tuple_size_v<AllArgs> - sizeof...(BoundArgs);

        using SlicedTuple = typename slice_first<kRemainingCount, AllArgs>::type;
        using SlicedTupleReverse = typename slice_last<kRemainingCount, AllArgs>::type;

        template <typename Ret, typename Tuple, typename TupleReverse>
        struct rebuild;
        template <typename Ret, typename... Ts, typename... TsR>
        struct rebuild<Ret, std::tuple<Ts...>, std::tuple<TsR...>> {
            static constexpr bool kIsValid =
                std::is_invocable_v<F_Type, Ts..., std::unwrap_ref_decay_t<BoundArgs>&&...> ||
                std::is_invocable_v<F_Type, Ts..., std::unwrap_ref_decay_t<BoundArgs>&...> ||
                std::is_invocable_v<F_Type, std::unwrap_ref_decay_t<BoundArgs>&&..., TsR...> ||
                std::is_invocable_v<F_Type, std::unwrap_ref_decay_t<BoundArgs>&..., TsR...>;
            static constexpr bool kIsReverse =
                std::is_invocable_v<F_Type, std::unwrap_ref_decay_t<BoundArgs>&&..., TsR...> ||
                std::is_invocable_v<F_Type, std::unwrap_ref_decay_t<BoundArgs>&..., TsR...>;
            using R = Ret;
            using Args = std::conditional_t<kIsReverse, std::tuple<TsR...>, std::tuple<Ts...>>;
            using Signature = std::conditional_t<kIsReverse, Ret(TsR...), Ret(Ts...)>;
            static constexpr bool kMutable =
                (!kIsReverse &&
                 !std::is_invocable_v<
                     F_Type, Ts...,
                     const std::unwrap_ref_decay_t<typename RefToConstRef<BoundArgs>::Type>&...>) ||
                (kIsReverse &&
                 !std::is_invocable_v<
                     F_Type,
                     const std::unwrap_ref_decay_t<typename RefToConstRef<BoundArgs>::Type>&...,
                     TsR...>);
        };

        using type = rebuild<typename Traits::return_type, SlicedTuple, SlicedTupleReverse>;
    };

    // To complete one needs case where F is static (no decay), but in that case it would be trivial
    // anyway, hence not urgent to change for now
    using PayloadType = PayloadImplArgs<F, std::unwrap_ref_decay_t<BoundArgs>...>;
    using PayloadRefType = PayloadImplArgs<F, BoundArgs...>;

    static constexpr bool kCopyable = std::is_copy_constructible_v<PayloadType>;
    static constexpr bool kRefCopyable = std::is_copy_constructible_v<PayloadRefType>;
    static constexpr std::size_t kAlignment =
        std::max(alignof(PayloadType), alignof(std::max_align_t));
    static constexpr std::size_t kRefAlignment =
        std::max(alignof(PayloadRefType), alignof(std::max_align_t));
    static constexpr bool kCanUseTrivial = TrivialUse<PayloadType>::value;
    static constexpr bool kRefCanUseTrivial = TrivialUse<PayloadRefType>::value;
    static constexpr auto kPayloadSize = sizeof...(BoundArgs) > 0 ? sizeof(PayloadType) : 0;
    static constexpr auto kRefPayloadSize = sizeof...(BoundArgs) > 0
                                                ? sizeof(PayloadImplArgs<F, BoundArgs...>)
                                                : 0;
    using TypeTraits = StripBoundArgs::type;
    using Signature = typename TypeTraits::Signature;
    static constexpr bool kIsValid = TypeTraits::kIsValid;

    template <bool Copyable = kCopyable, std::size_t N = kPayloadSize,
              bool IsTrivial = kCanUseTrivial, bool Mutable = TypeTraits::kMutable,
              std::size_t Align = kAlignment>
    using Type = Task<Signature, Copyable, N, IsTrivial, Mutable, false, false, Align>;

    using MutableType = Type<kCopyable, kPayloadSize, kCanUseTrivial, true, kAlignment>;
    using ImmutableType = Type<kCopyable, kPayloadSize, kCanUseTrivial, false, kAlignment>;

    template <bool Copyable = kRefCopyable, std::size_t N = kRefPayloadSize,
              bool IsTrivial = kRefCanUseTrivial, bool Mutable = TypeTraits::kMutable,
              std::size_t Align = kRefAlignment>
    using RefType = Task<Signature, Copyable, N, IsTrivial, Mutable, false, false, Align>;

    using MutableRefType =
        RefType<kRefCopyable, kRefPayloadSize, kRefCanUseTrivial, true, kRefAlignment>;
    using ImmutableRefType =
        RefType<kRefCopyable, kRefPayloadSize, kRefCanUseTrivial, false, kRefAlignment>;
};

template <typename Sig, bool Cpy, std::size_t N, bool IsT, bool Mut, bool Ext, bool AllowHeap,
          std::size_t Align>
Task(Task<Sig, Cpy, N, IsT, Mut, Ext, AllowHeap, Align>)
    -> Task<Sig, Cpy, N, IsT, Mut, Ext, AllowHeap, Align>;

template <auto F, typename... BoundArgs>
    requires((sizeof...(BoundArgs) > 0 || !IsTask<std::remove_cvref_t<decltype(F)>>::value) &&
             FunctionTraits<decltype(F)>::kValid && TaskTypeDeductorAuto<F, BoundArgs...>::kIsValid)
Task(FunctionTag<F>, BoundArgs&&...)
    -> Task<typename TaskTypeDeductorAuto<F, BoundArgs...>::TypeTraits::Signature,
            TaskTypeDeductorAuto<F, BoundArgs...>::kCopyable,
            TaskTypeDeductorAuto<F, BoundArgs...>::kPayloadSize,
            TaskTypeDeductorAuto<F, BoundArgs...>::kCanUseTrivial,
            TaskTypeDeductorAuto<F, BoundArgs...>::TypeTraits::kMutable, false, false,
            TaskTypeDeductorAuto<F, BoundArgs...>::kAlignment>;

template <typename F, typename... BoundArgs>
    requires(!std::is_same_v<F, RefTag> && !IsFunctionTag<F>::value &&
             FunctionTraits<std::decay_t<F>>::kValid &&
             (sizeof...(BoundArgs) > 0 || !IsTask<std::remove_cvref_t<F>>::value) &&
             TaskTypeDeductor<F, BoundArgs...>::kIsValid)
Task(F&&, BoundArgs&&...) -> Task<typename TaskTypeDeductor<F, BoundArgs...>::TypeTraits::Signature,
                                  TaskTypeDeductor<F, BoundArgs...>::kCopyable,
                                  TaskTypeDeductor<F, BoundArgs...>::kPayloadSize,
                                  TaskTypeDeductor<F, BoundArgs...>::kCanUseTrivial,
                                  TaskTypeDeductor<F, BoundArgs...>::TypeTraits::kMutable, false,
                                  false, TaskTypeDeductor<F, BoundArgs...>::kAlignment>;

template <typename F, typename... BoundArgs>
    requires(!IsFunctionTag<F>::value &&
             (sizeof...(BoundArgs) > 0 || !IsTask<std::remove_cvref_t<F>>::value) &&
             FunctionTraits<std::decay_t<F>>::kValid && TaskTypeDeductor<F, BoundArgs...>::kIsValid)
Task(RefTag, F&&, BoundArgs&&...)
    -> Task<typename TaskTypeDeductor<F, BoundArgs...>::TypeTraits::Signature,
            TaskTypeDeductor<F, BoundArgs...>::kRefCopyable,
            TaskTypeDeductor<F, BoundArgs...>::kRefPayloadSize,
            TaskTypeDeductor<F, BoundArgs...>::kRefCanUseTrivial,
            TaskTypeDeductor<F, BoundArgs...>::TypeTraits::kMutable, false, false,
            TaskTypeDeductor<F, BoundArgs...>::kRefAlignment>;

template <auto F, typename... BoundArgs>
    requires((sizeof...(BoundArgs) > 0 || !IsTask<std::remove_cvref_t<decltype(F)>>::value) &&
             FunctionTraits<decltype(F)>::kValid && TaskTypeDeductorAuto<F, BoundArgs...>::kIsValid)
Task(RefTag, FunctionTag<F>, BoundArgs&&...)
    -> Task<typename TaskTypeDeductorAuto<F, BoundArgs...>::TypeTraits::Signature,
            TaskTypeDeductorAuto<F, BoundArgs...>::kRefCopyable,
            TaskTypeDeductorAuto<F, BoundArgs...>::kRefPayloadSize,
            TaskTypeDeductorAuto<F, BoundArgs...>::kRefCanUseTrivial,
            TaskTypeDeductorAuto<F, BoundArgs...>::TypeTraits::kMutable, false, false,
            TaskTypeDeductorAuto<F, BoundArgs...>::kRefAlignment>;

template <typename F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductor<F, Args...>::template Type<> makeTask(
    F&& f, Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    static_assert(TaskTypeDeductor<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    return Task(std::forward<F>(f), std::forward<Args>(args)...);
}

template <auto F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductorAuto<F, Args...>::template Type<> makeTask(
    Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<decltype(F)>>, "Given callable is invalid");
    static_assert(TaskTypeDeductorAuto<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    return Task(FunctionTag<F>{}, std::forward<Args>(args)...);
}

template <typename Sig, typename F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductor<F, Args...>::template Type<> makeTask(
    F&& f, Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    static_assert(std::is_same_v<typename TaskTypeDeductor<F, Args...>::TypeTraits::Signature, Sig>,
                  "Forwarded callable and arguments do not match requested signature");
    static_assert(TaskTypeDeductor<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    return Task(std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename Sig, auto F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductorAuto<F, Args...>::template Type<> makeTask(
    Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<decltype(F)>>, "Given callable is invalid");
    static_assert(
        std::is_same_v<typename TaskTypeDeductorAuto<F, Args...>::TypeTraits::Signature, Sig>,
        "Forwarded callable and arguments do not match requested signature");
    static_assert(TaskTypeDeductorAuto<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    return Task(FunctionTag<F>{}, std::forward<Args>(args)...);
}

template <std::size_t N = 16, typename F, typename... Args>
FORCE_INLINE constexpr auto makeFlexTask(F&& f, Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    using Deductor = TaskTypeDeductor<F, Args...>;
    static_assert(Deductor::kIsValid, "Forwarded callable is not callable with given arguments");
    return typename Deductor::template FlexType<N>{std::forward<F>(f), std::forward<Args>(args)...};
}

template <typename Sig, std::size_t N = 16, typename F, typename... Args>
FORCE_INLINE constexpr auto makeFlexTask(F&& f, Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    using Deductor = TaskTypeDeductor<F, Args...>;
    static_assert(Deductor::kIsValid, "Forwarded callable is not callable with given arguments");
    static_assert(std::is_same_v<typename Deductor::Signature, Sig>,
                  "Forwarded callable and arguments do not match requested signature");
    return typename Deductor::template FlexType<N>{std::forward<F>(f), std::forward<Args>(args)...};
}

template <std::size_t N = 16, auto F, typename... Args>
FORCE_INLINE constexpr auto makeFlexTask(Args&&... args) {
    static_assert(HasFunctionTraits<decltype(F)>, "Given callable is invalid");
    using Deductor = TaskTypeDeductorAuto<F, Args...>;
    static_assert(Deductor::kIsValid, "Forwarded callable is not callable with given arguments");
    return typename Deductor::template FlexType<N>{FunctionTag<F>{}, std::forward<Args>(args)...};
}

template <typename Sig, std::size_t N = 16, auto F, typename... Args>
FORCE_INLINE constexpr auto makeFlexTask(Args&&... args) {
    static_assert(HasFunctionTraits<decltype(F)>, "Given callable is invalid");
    using Deductor = TaskTypeDeductorAuto<F, Args...>;
    static_assert(Deductor::kIsValid, "Forwarded callable is not callable with given arguments");
    static_assert(std::is_same_v<typename Deductor::Signature, Sig>,
                  "Forwarded callable and arguments do not match requested signature");
    return typename Deductor::template FlexType<N>{FunctionTag<F>{}, std::forward<Args>(args)...};
}

template <typename F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductor<F, Args...>::template RefType<> makeRefTask(
    F&& f, Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    static_assert(TaskTypeDeductor<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    return Task(RefTag{}, std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename Sig, typename F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductor<F, Args...>::template RefType<> makeRefTask(
    F&& f, Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    static_assert(std::is_same_v<typename TaskTypeDeductor<F, Args...>::TypeTraits::Signature, Sig>,
                  "Forwarded callable and arguments do not match requested signature");
    static_assert(TaskTypeDeductor<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    return Task(RefTag{}, std::forward<F>(f), std::forward<Args>(args)...);
}

template <auto F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductorAuto<F, Args...>::template RefType<> makeRefTask(
    Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<decltype(F)>>, "Given callable is invalid");
    static_assert(TaskTypeDeductorAuto<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    return Task(RefTag{}, FunctionTag<F>{}, std::forward<Args>(args)...);
}

template <typename Sig, auto F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductorAuto<F, Args...>::template RefType<> makeRefTask(
    Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<decltype(F)>>, "Given callable is invalid");
    static_assert(TaskTypeDeductorAuto<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(
        std::is_same_v<typename TaskTypeDeductorAuto<F, Args...>::TypeTraits::Signature, Sig>,
        "Forwarded callable and arguments do not match requested signature");
    return Task(RefTag{}, FunctionTag<F>{}, std::forward<Args>(args)...);
}

template <typename F, typename... Args>
FORCE_INLINE constexpr TaskTypeDeductor<F, Args...>::MutableType makeMutTask(F&& f,
                                                                             Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    static_assert(TaskTypeDeductor<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    return Task(std::forward<F>(f), std::forward<Args>(args)...);
}

template <auto F, typename... Args>
FORCE_INLINE constexpr TaskTypeDeductorAuto<F, Args...>::MutableType makeMutTask(Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<decltype(F)>>, "Given callable is invalid");
    static_assert(TaskTypeDeductorAuto<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    return Task(FunctionTag<F>{}, std::forward<Args>(args)...);
}

template <typename Sig, typename F, typename... Args>
FORCE_INLINE constexpr TaskTypeDeductor<F, Args...>::MutableType makeMutTask(F&& f,
                                                                             Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    static_assert(std::is_same_v<typename TaskTypeDeductor<F, Args...>::TypeTraits::Signature, Sig>,
                  "Forwarded callable and arguments do not match requested signature");
    static_assert(TaskTypeDeductor<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    return Task(std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename Sig, auto F, typename... Args>
FORCE_INLINE constexpr TaskTypeDeductorAuto<F, Args...>::MutableType makeMutTask(Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<decltype(F)>>, "Given callable is invalid");
    static_assert(
        std::is_same_v<typename TaskTypeDeductorAuto<F, Args...>::TypeTraits::Signature, Sig>,
        "Forwarded callable and arguments do not match requested signature");
    static_assert(TaskTypeDeductorAuto<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    return Task(FunctionTag<F>{}, std::forward<Args>(args)...);
}

template <typename F, typename... Args>
FORCE_INLINE constexpr TaskTypeDeductor<F, Args...>::MutableRefType makeMutRefTask(F&& f,
                                                                                   Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    static_assert(TaskTypeDeductor<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    return Task(RefTag{}, std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename Sig, typename F, typename... Args>
FORCE_INLINE constexpr TaskTypeDeductor<F, Args...>::MutableRefType makeMutRefTask(F&& f,
                                                                                   Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    static_assert(std::is_same_v<typename TaskTypeDeductor<F, Args...>::TypeTraits::Signature, Sig>,
                  "Forwarded callable and arguments do not match requested signature");
    static_assert(TaskTypeDeductor<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    return Task(RefTag{}, std::forward<F>(f), std::forward<Args>(args)...);
}

template <auto F, typename... Args>
FORCE_INLINE constexpr TaskTypeDeductorAuto<F, Args...>::MutableRefType makeMutRefTask(
    Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<decltype(F)>>, "Given callable is invalid");
    static_assert(TaskTypeDeductorAuto<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    return Task(RefTag{}, FunctionTag<F>{}, std::forward<Args>(args)...);
}

template <typename Sig, auto F, typename... Args>
FORCE_INLINE constexpr TaskTypeDeductorAuto<F, Args...>::MutableRefType makeMutRefTask(
    Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<decltype(F)>>, "Given callable is invalid");
    static_assert(TaskTypeDeductorAuto<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(
        std::is_same_v<typename TaskTypeDeductorAuto<F, Args...>::TypeTraits::Signature, Sig>,
        "Forwarded callable and arguments do not match requested signature");
    return Task(RefTag{}, FunctionTag<F>{}, std::forward<Args>(args)...);
}

template <typename F, typename... Args>
FORCE_INLINE constexpr TaskTypeDeductor<F, Args...>::ImmutableType makeImmutTask(F&& f,
                                                                                 Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    static_assert(TaskTypeDeductor<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(
        !TaskTypeDeductor<F, Args...>::TypeTraits::kMutable,
        "Cannot make immutable task with given function signature. To be immutable, captured "
        "arguments cannot be taken by non-const reference in the function signature. If you're "
        "giving a lambda as callable, check that the lambda is not marked mutable.");
    return Task(std::forward<F>(f), std::forward<Args>(args)...);
}

template <auto F, typename... Args>
FORCE_INLINE constexpr TaskTypeDeductorAuto<F, Args...>::ImmutableType makeImmutTask(
    Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<decltype(F)>>, "Given callable is invalid");
    static_assert(TaskTypeDeductorAuto<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(
        !TaskTypeDeductorAuto<F, Args...>::TypeTraits::kMutable,
        "Cannot make immutable task with given function signature. To be immutable, captured "
        "arguments cannot be taken by non-const reference in the function signature.");
    return Task(FunctionTag<F>{}, std::forward<Args>(args)...);
}

template <typename Sig, typename F, typename... Args>
FORCE_INLINE constexpr TaskTypeDeductor<F, Args...>::ImmutableType makeImmutTask(F&& f,
                                                                                 Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    static_assert(TaskTypeDeductor<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(
        !TaskTypeDeductor<F, Args...>::TypeTraits::kMutable,
        "Cannot make immutable task with given function signature. To be immutable, captured "
        "arguments cannot be taken by non-const reference in the function signature. If you're "
        "giving a lambda as callable, check that the lambda is not marked mutable.");
    static_assert(std::is_same_v<typename TaskTypeDeductor<F, Args...>::TypeTraits::Signature, Sig>,
                  "Forwarded callable and arguments do not match requested signature");
    return Task(std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename Sig, auto F, typename... Args>
FORCE_INLINE constexpr TaskTypeDeductorAuto<F, Args...>::ImmutableType makeImmutTask(
    Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<decltype(F)>>, "Given callable is invalid");
    static_assert(TaskTypeDeductorAuto<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(
        !TaskTypeDeductorAuto<F, Args...>::TypeTraits::kMutable,
        "Cannot make immutable task with given function signature. To be immutable, captured "
        "arguments cannot be taken by non-const reference in the function signature.");
    static_assert(
        std::is_same_v<typename TaskTypeDeductorAuto<F, Args...>::TypeTraits::Signature, Sig>,
        "Forwarded callable and arguments do not match requested signature");
    return Task(FunctionTag<F>{}, std::forward<Args>(args)...);
}

template <typename F, typename... Args>
FORCE_INLINE constexpr TaskTypeDeductor<F, Args...>::ImmutableRefType makeImmutRefTask(
    F&& f, Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    static_assert(TaskTypeDeductor<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(
        !TaskTypeDeductor<F, Args...>::TypeTraits::kMutable,
        "Cannot make immutable task with given function signature. To be immutable, captured "
        "arguments cannot be taken by non-const reference in the function signature. If you're "
        "giving a lambda as callable, check that the lambda is not marked mutable.");
    return Task(RefTag{}, std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename Sig, typename F, typename... Args>
FORCE_INLINE constexpr TaskTypeDeductor<F, Args...>::ImmutableRefType makeImmutRefTask(
    F&& f, Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    static_assert(std::is_same_v<typename TaskTypeDeductor<F, Args...>::TypeTraits::Signature, Sig>,
                  "Forwarded callable and arguments do not match requested signature");
    static_assert(TaskTypeDeductor<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(
        !TaskTypeDeductor<F, Args...>::TypeTraits::kMutable,
        "Cannot make immutable task with given function signature. To be immutable, captured "
        "arguments cannot be taken by non-const reference in the function signature. If you're "
        "giving a lambda as callable, check that the lambda is not marked mutable.");
    return Task(RefTag{}, std::forward<F>(f), std::forward<Args>(args)...);
}

template <auto F, typename... Args>
FORCE_INLINE constexpr TaskTypeDeductorAuto<F, Args...>::ImmutableRefType makeImmutRefTask(
    Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<decltype(F)>>, "Given callable is invalid");
    static_assert(TaskTypeDeductorAuto<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(
        !TaskTypeDeductorAuto<F, Args...>::TypeTraits::kMutable,
        "Cannot make immutable task with given function signature. To be immutable, captured "
        "arguments cannot be taken by non-const reference in the function signature. If you're "
        "giving a lambda as callable, check that the lambda is not marked mutable.");
    return Task(RefTag{}, FunctionTag<F>{}, std::forward<Args>(args)...);
}

template <typename Sig, auto F, typename... Args>
FORCE_INLINE constexpr TaskTypeDeductorAuto<F, Args...>::ImmutableRefType makeImmutRefTask(
    Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<decltype(F)>>, "Given callable is invalid");
    static_assert(TaskTypeDeductorAuto<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(
        std::is_same_v<typename TaskTypeDeductorAuto<F, Args...>::TypeTraits::Signature, Sig>,
        "Forwarded callable and arguments do not match requested signature");
    static_assert(
        !TaskTypeDeductorAuto<F, Args...>::TypeTraits::kMutable,
        "Cannot make immutable task with given function signature. To be immutable, captured "
        "arguments cannot be taken by non-const reference in the function signature. If you're "
        "giving a lambda as callable, check that the lambda is not marked mutable.");
    return Task(RefTag{}, FunctionTag<F>{}, std::forward<Args>(args)...);
}

}  // namespace fn

#endif  // FN_TASK_HPP