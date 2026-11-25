#ifndef FN_TASK_HPP
#define FN_TASK_HPP

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <new>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

#include <task/deductors.hpp>
#include <task/function_traits.hpp>
#include <task/payload.hpp>

namespace fn {

enum class Op : std::uint8_t { kDestroy, kClone, kMove, kAssign, kMoveAssign };

template <Alignment alignment, InlineSize size, Triviality t, bool Extended = false,
          typename Signature = void()>
struct Storage;

template <typename Signature, typename Task = int, InlineSize size = 0, bool IsConst = false,
          bool LVRef = false, bool RVRef = false, bool IsNoExcept = false>
struct InvokerBuilder;

template <typename Task, InlineSize size, typename Ret, typename... Args, bool IsConst, bool LVRef,
          bool RVRef, bool IsNoExcept>
struct InvokerBuilder<Ret(Args...), Task, size, IsConst, LVRef, RVRef, IsNoExcept> {
    using Type =
        std::conditional_t<IsNoExcept, Ret (*)(void*, Args...) noexcept, Ret (*)(void*, Args...)>;
    using TypeNoBuffer =
        std::conditional_t<IsNoExcept, Ret (*)(Args...) noexcept, Ret (*)(Args...)>;

    template <typename T>
    using ApplyQualifiers = std::conditional_t<
        LVRef, std::conditional_t<IsConst, const T, T>&,
        std::conditional_t<RVRef,
                           std::remove_reference_t<std::conditional_t<IsConst, const T, T>>&&,
                           std::conditional_t<IsConst, const T, T>>>;

    template <typename T>
    using Convert = ApplyQualifiers<T>;

    static constexpr bool kRefLess = !LVRef && !RVRef;

    template <typename Ft, typename... Argst>
    static constexpr bool kInvocableVerifierV =
        kRefLess
            ? (IsNoExcept  // NOLINT
                   ? is_any_compatible_nothrow_invocable_b<IsCallArg, Ft, Argst...>
                   : is_any_compatible_invocable_b<IsCallArg, Ft, Argst...>)
            : (IsNoExcept
                   ? is_any_compatible_nothrow_invocable_b<IgnoreExpansionPolicyOnRefInvocation, Ft,
                                                           Argst...>
                   : is_any_compatible_invocable_b<IgnoreExpansionPolicyOnRefInvocation, Ft,
                                                   Argst...>);

    template <typename, typename>
    struct UnwrapTester;

    template <typename F, typename... TestArgs>
    struct UnwrapTester<F, std::tuple<TestArgs...>> {
        static constexpr auto kValid = kInvocableVerifierV<F, TestArgs...>;
    };

    template <typename PayloadType, auto binder, bool Compatible, bool Indirection = false>
    struct InvokeBinder;

    template <typename PayloadType, std::size_t... Is, Binder<Is...> binder, bool Compatible,
              bool Indirection>
    struct InvokeBinder<PayloadType, binder, Compatible, Indirection> {
        static constexpr Ret call(void* s, auto&&... args) noexcept(IsNoExcept) {
            if constexpr (Indirection) {
                if constexpr (Compatible) {
                    return std::forward_like<ApplyQualifiers<PayloadType>>(
                               **static_cast<PayloadType**>(s))
                        .template invokeCompatible<IsCallArg, Is...>(std::forward<Args>(args)...);
                } else {
                    return std::forward_like<ApplyQualifiers<PayloadType>>(
                               **static_cast<PayloadType**>(s))
                        .template invokeCompatible<IgnoreExpansionPolicyOnRefInvocation, Is...>(
                            std::forward<Args>(args)...);
                }
            } else {
                if constexpr (Compatible) {
                    return std::forward_like<ApplyQualifiers<PayloadType>>(
                               *static_cast<PayloadType*>(s))
                        .template invokeCompatible<IsCallArg, Is...>(std::forward<Args>(args)...);
                } else {
                    return std::forward_like<ApplyQualifiers<PayloadType>>(
                               *static_cast<PayloadType*>(s))
                        .template invokeCompatible<IgnoreExpansionPolicyOnRefInvocation, Is...>(
                            std::forward<Args>(args)...);
                }
            }
        }
    };

    template <auto /*binding*/, typename PayloadType, auto F, typename... BoundArgs>
        requires(size == 0)
    FORCE_INLINE static constexpr TypeNoBuffer getInvoker() noexcept {
        static_assert(sizeof...(BoundArgs) == 0);
        if constexpr (std::is_void_v<Ret>) {
            return [](Args... args) noexcept(IsNoExcept) {
                std::invoke(F, std::forward<Args>(args)...);
            };
        } else {
            return [](Args... args) noexcept(IsNoExcept) -> Ret {
                return std::invoke(F, std::forward<Args>(args)...);
            };
        }
    }

    template <auto binder, typename PayloadType, typename F, typename... BoundArgs>
        requires(size > 0 && kIsBinder<binder>)
    FORCE_INLINE static constexpr Type
    getInvoker() noexcept {  // NOLINT(readability-function-cognitive-complexity)
        return [](void* s,   // NOLINT(readability-function-cognitive-complexity)
                  Args... args) noexcept(IsNoExcept) -> Ret {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(
                    kInvocableVerifierV<Convert<F>,
                                        CallArgTag<decltype(std::forward<Args>(args))>...>,
                    "F is not callable with the provided arguments!");
                return (*static_cast<PayloadType*>(s)).invoke(std::forward<Args>(args)...);
            } else if constexpr (!kIsIndicelessBinder<binder>) {
                using ArgsOrderTuple =
                    rebind_call_and_bound<std::tuple<CallArgTag<Args&&>...>,
                                          std::tuple<Convert<BoundArgs>...>, binder>::type;
                static_assert(UnwrapTester<Convert<F>, ArgsOrderTuple>::kValid,
                              "F is not callable with the provided arguments.");
                static constexpr auto kIndirection = sizeof(PayloadType) > size;
                static constexpr auto kInvokeCompatible = kRefLess;
                return InvokeBinder<PayloadType, binder, kInvokeCompatible, kIndirection>::call(
                    s, std::forward<Args>(args)...);
            } else if constexpr (binder != Binding::kFront &&
                                 kInvocableVerifierV<
                                     Convert<F>, CallArgTag<decltype(std::forward<Args>(args))>...,
                                     Convert<BoundArgs>...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    if constexpr (kRefLess) {
                        return std::move(*static_cast<PayloadType*>(s))
                            .invokeCompatible(std::forward<Args>(args)...);
                    } else {
                        return std::forward_like<ApplyQualifiers<PayloadType>>(
                                   *static_cast<PayloadType*>(s))
                            .template invokeCompatible<IgnoreExpansionPolicyOnRefInvocation>(
                                std::forward<Args>(args)...);
                    }
                } else {
                    if constexpr (kRefLess) {
                        return std::move(**static_cast<PayloadType**>(s))
                            .invokeCompatible(std::forward<Args>(args)...);
                    } else {
                        return std::forward_like<ApplyQualifiers<PayloadType>>(
                                   **static_cast<PayloadType**>(s))
                            .template invokeCompatible<IgnoreExpansionPolicyOnRefInvocation>(
                                std::forward<Args>(args)...);
                    }
                }
            } else if constexpr (binder != Binding::kBack &&
                                 kInvocableVerifierV<
                                     Convert<F>, Convert<BoundArgs>...,
                                     CallArgTag<decltype(std::forward<Args>(args))>...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    if constexpr (kRefLess) {
                        return std::move(*static_cast<PayloadType*>(s))
                            .invokeFrontCompatible(std::forward<Args>(args)...);
                    } else {
                        return std::forward_like<ApplyQualifiers<PayloadType>>(
                                   *static_cast<PayloadType*>(s))
                            .template invokeFrontCompatible<IgnoreExpansionPolicyOnRefInvocation>(
                                std::forward<Args>(args)...);
                    }
                } else {
                    if constexpr (kRefLess) {
                        return std::move(**static_cast<PayloadType**>(s))
                            .invokeFrontCompatible(std::forward<Args>(args)...);
                    } else {
                        return std::forward_like<ApplyQualifiers<PayloadType>>(
                                   **static_cast<PayloadType**>(s))
                            .template invokeFrontCompatible<IgnoreExpansionPolicyOnRefInvocation>(
                                std::forward<Args>(args)...);
                    }
                }
            } else if constexpr (is_any_compatible_invocable_b<
                                     IsCallArg, F, BoundArgs...,
                                     CallArgTag<decltype(std::forward<Args>(args))>...> ||
                                 is_any_compatible_invocable_b<
                                     IsCallArg, F,
                                     CallArgTag<decltype(std::forward<Args>(args))>...,
                                     BoundArgs...>) {
                static_assert(AlwaysFalse<F>::value,
                              "F is not callable with the provided arguments since it wouldn't "
                              "preserve signature qualifier, noexcept, or is not rightly ordered");
            } else {
                static_assert(AlwaysFalse<F>::value,
                              "F is not callable with the provided arguments.");
            }
        };
    }

    template <auto binder, typename PayloadType, auto F, typename... BoundArgs>
        requires(size > 0 && kIsBinder<binder>)
    FORCE_INLINE static constexpr Type
    getInvoker() noexcept {  // NOLINT(readability-function-cognitive-complexity)
        return [](void* s,   // NOLINT(readability-function-cognitive-complexity)
                  Args... args) noexcept(IsNoExcept) {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<decltype(F), Args...>,
                              "F is not callable with the provided arguments!");
                return F(std::forward<Args>(args)...);
            } else if constexpr (!kIsIndicelessBinder<binder>) {
                using ArgsOrderTuple =
                    rebind_call_and_bound<std::tuple<CallArgTag<Args&&>...>,
                                          std::tuple<Convert<BoundArgs>...>, binder>::type;
                static_assert(UnwrapTester<Convert<decltype(F)>, ArgsOrderTuple>::kValid,
                              "F is not callable with the provided arguments.");
                static constexpr auto kIndirection = sizeof(PayloadType) > size;
                static constexpr auto kInvokeCompatible = kRefLess;
                return InvokeBinder<PayloadType, binder, kInvokeCompatible, kIndirection>::call(
                    s, std::forward<Args>(args)...);
            } else if constexpr (binder != Binding::kFront &&
                                 kInvocableVerifierV<Convert<decltype(F)>, Args...,
                                                     Convert<BoundArgs>...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    if constexpr (kRefLess) {
                        return std::move(*static_cast<PayloadType*>(s))
                            .invokeCompatible(std::forward<Args>(args)...);
                    } else {
                        return std::forward_like<ApplyQualifiers<PayloadType>>(
                                   *static_cast<PayloadType*>(s))
                            .template invokeCompatible<IgnoreExpansionPolicyOnRefInvocation>(
                                std::forward<Args>(args)...);
                    }
                } else {
                    if constexpr (kRefLess) {
                        return std::move(**static_cast<PayloadType**>(s))
                            .invokeCompatible(std::forward<Args>(args)...);
                    } else {
                        return std::forward_like<ApplyQualifiers<PayloadType>>(
                                   **static_cast<PayloadType**>(s))
                            .template invokeCompatible<IgnoreExpansionPolicyOnRefInvocation>(
                                std::forward<Args>(args)...);
                    }
                }
            } else if constexpr (binder != Binding::kBack &&
                                 kInvocableVerifierV<Convert<decltype(F)>, Convert<BoundArgs>...,
                                                     Args...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    if constexpr (kRefLess) {
                        return std::move(*static_cast<PayloadType*>(s))
                            .invokeFrontCompatible(std::forward<Args>(args)...);
                    } else {
                        return std::forward_like<ApplyQualifiers<PayloadType>>(
                                   *static_cast<PayloadType*>(s))
                            .template invokeFrontCompatible<IgnoreExpansionPolicyOnRefInvocation>(
                                std::forward<Args>(args)...);
                    }
                } else {
                    if constexpr (kRefLess) {
                        return std::move(**static_cast<PayloadType**>(s))
                            .invokeFrontCompatible(std::forward<Args>(args)...);
                    } else {
                        return std::forward_like<ApplyQualifiers<PayloadType>>(
                                   **static_cast<PayloadType**>(s))
                            .template invokeFrontCompatible<IgnoreExpansionPolicyOnRefInvocation>(
                                std::forward<Args>(args)...);
                    }
                }
            } else if constexpr (is_any_compatible_invocable_b<
                                     IsCallArg, std::unwrap_reference_t<decltype(F)>,
                                     std::unwrap_reference_t<BoundArgs>..., Args...> ||
                                 is_any_compatible_invocable_b<
                                     IsCallArg, std::unwrap_reference_t<decltype(F)>, Args...,
                                     std::unwrap_reference_t<BoundArgs>...>) {
                static_assert(AlwaysFalse<decltype(F)>::value,
                              "F is not callable with the provided arguments since it wouldn't "
                              "preserve signature qualifier or noexcept");
            } else {
                static_assert(AlwaysFalse<decltype(F)>::value,
                              "F is not callable with the provided arguments.");
            }
        };
    }

    FORCE_INLINE Ret operator()(this auto&& self, Args... args) {
        if constexpr (size == 0) {
            return (const_cast<Task&>(self)).storage_.invoker(std::forward<Args>(args)...);
        } else {
            return (const_cast<Task&>(self))
                .storage_.invoker(
                    static_cast<void*>((const_cast<Task&>(self)).storage_.data.data()),
                    std::forward<Args>(args)...);
        }
    }

    FORCE_INLINE Ret invoke(this auto&& self, Args... args) {
        if ((static_cast<Task>(self)).storage_ == nullptr) {
            throw std::bad_function_call();
        }
        if constexpr (size == 0) {
            return (static_cast<Task>(self)).storage_.invoker(std::forward<Args>(args)...);
        } else {
            return (static_cast<Task>(self))
                .storage_.invoker(
                    static_cast<void*>((static_cast<Task>(self)).storage_.data.data()),
                    std::forward<Args>(args)...);
        }
    }
};

template <typename Task, InlineSize size, typename Signature>
    requires(!IsBaseSignature<Signature>)
struct InvokerBuilder<Signature, Task, size>
    : InvokerBuilder<typename FunctionTraits<Signature>::base_signature, Task, size,
                     FunctionTraits<Signature>::kIsConst, FunctionTraits<Signature>::kIsLValueRef,
                     FunctionTraits<Signature>::kIsRValueRef,
                     FunctionTraits<Signature>::kIsNoExcept> {};

template <Alignment alignment, InlineSize size, typename Signature>
    requires(size > 0)
struct Storage<alignment, size, Triviality::kTrivial, true, Signature> {
    alignas(alignment.bytes) mutable std::array<std::byte, size> data{};
    InvokerBuilder<Signature>::Type invoker{nullptr};

    template <typename Payload, HeapPolicy /*hp*/ = HeapPolicy::kDisallow,
              Copyability /*cp*/ = Copyability::kCopyable>
    constexpr void doConstruct() noexcept {}

    [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept {
        return std::all_of(data.begin(), data.end(), [](auto v) { return v == std::byte{0}; });
    }
};

template <Alignment alignment, typename Signature>
struct Storage<alignment, 0, Triviality::kTrivial, true, Signature> {
    InvokerBuilder<Signature>::TypeNoBuffer invoker{nullptr};

    template <typename Payload, HeapPolicy /*hp*/ = HeapPolicy::kDisallow,
              Copyability /*cp*/ = Copyability::kCopyable>
    constexpr void doConstruct() noexcept {}

    [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept {
        return true;
    }
};

template <Alignment alignment, InlineSize size, typename Signature>
    requires(size > 0)
struct Storage<alignment, size, Triviality::kTrivial, false, Signature> {
    alignas(alignment.bytes) mutable std::array<std::byte, size> data{};
    InvokerBuilder<Signature>::Type invoker{nullptr};

    template <typename Payload, HeapPolicy /*hp*/, Copyability /*cp*/ = Copyability::kCopyable>
    constexpr void doConstruct() noexcept {}

    [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept {
        return std::all_of(data.begin(), data.end(), [](auto v) { return v == std::byte{0}; });
    }
};

template <Alignment alignment, typename Signature>
struct Storage<alignment, 0, Triviality::kTrivial, false, Signature> {
    InvokerBuilder<Signature>::TypeNoBuffer invoker{nullptr};

    template <typename Payload, HeapPolicy hp = HeapPolicy::kDisallow,
              Copyability cp = Copyability::kCopyable>
    constexpr void doConstruct() noexcept {}

    [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept {
        return true;
    }
};

template <Alignment alignment, InlineSize size, typename Signature>
    requires(size > 0)
struct Storage<alignment, size, Triviality::kNonTrivial, false, Signature> {
    alignas(alignment.bytes) mutable std::array<std::byte, size> data{};
    InvokerBuilder<Signature>::Type invoker{nullptr};
    void (*manager)(Op op, void* src, void* dst){nullptr};

    constexpr void doDelete() noexcept {
        if (manager != nullptr) {
            manager(Op::kDestroy, nullptr, data.data());
            manager = nullptr;
        }
    }

    template <Op op, InlineSize sizeSrc, typename SigOther>
    constexpr void doConstructFromNonTrivial(
        Storage<alignment, sizeSrc, Triviality::kNonTrivial, false,
                SigOther>&& other) noexcept {  // NOLINT
        static_assert(op == Op::kMove || op == Op::kMoveAssign);
        static_assert(size >= sizeSrc);
        if constexpr (op == Op::kMoveAssign) {
            doDelete();
        }
        manager = other.manager;
        if (manager != nullptr) {
            manager(op, other.data.data(), data.data());
        }
        other.manager = nullptr;
    }

    template <Op op, InlineSize sizeSrc, typename SigOther>
    constexpr void doConstructFromNonTrivial(
        const Storage<alignment, sizeSrc, Triviality::kNonTrivial, false, SigOther>& other) {
        static_assert(op == Op::kClone || op == Op::kAssign);
        static_assert(size >= sizeSrc);
        if constexpr (op == Op::kAssign) {
            doDelete();
        }
        manager = other.manager;
        if (manager != nullptr) {
            manager(op, static_cast<void*>(const_cast<std::byte*>(other.data.data())), data.data());
        }
    }

    template <std::size_t U, bool isAssign = false, HeapPolicy hp = HeapPolicy::kDisallow,
              Copyability cp = Copyability::kCopyable>
    constexpr void doConstructFromTrivial(void* dst,                              // NOLINT
                                          const void* src) noexcept(U <= size) {  // NOLINT
        static_assert(U <= size || hp == HeapPolicy::kAllow);
        if constexpr (isAssign) {
            doDelete();
        }
        manager = [](Op o, void* src, void* dest) {
            if constexpr (U <= size) {
                if (o == Op::kDestroy) {
                    return;
                }
                std::memcpy(dest, src, U);
            } else {
                static_assert(hp == HeapPolicy::kAllow);
                switch (o) {
                    case Op::kDestroy: {
                        auto* srcPtr = *static_cast<void**>(dest);
                        if constexpr (alignment <= alignof(std::max_align_t)) {
                            ::operator delete(srcPtr);
                        } else {
                            ::operator delete(srcPtr, std::align_val_t{std::size_t{alignment}});
                        }
                        return;
                    }
                    case Op::kAssign:
                    case Op::kClone: {
                        if constexpr (cp == Copyability::kCopyable) {
                            if constexpr (alignment <= alignof(std::max_align_t)) {
                                auto* heapPtr = ::operator new(U);
                                void* oldData = *static_cast<void**>(src);
                                std::memcpy(heapPtr, oldData, U);
                                new (dest) void*(heapPtr);
                            } else {
                                auto* heapPtr =
                                    ::operator new(U, std::align_val_t{std::size_t{alignment}});
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
        if constexpr (U <= size) {
            std::memcpy(dst, src, U);
        } else {
            static_assert(hp == HeapPolicy::kAllow);
            if constexpr (alignment <= alignof(std::max_align_t)) {
                auto* heapPtr = ::operator new(U);
                std::memcpy(heapPtr, src, U);
                new (dst) void*(heapPtr);
            } else {
                auto* heapPtr = ::operator new(U, std::align_val_t{std::size_t{alignment}});
                std::memcpy(heapPtr, src, U);
                new (dst) void*(heapPtr);
            }
        }
    }

    template <typename Payload, HeapPolicy hp = HeapPolicy::kDisallow,
              Copyability cp = Copyability::kCopyable>
    constexpr void doConstruct() noexcept {  // NOLINT
        if constexpr (sizeof(Payload) <= size) {
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
                        if constexpr (cp == Copyability::kCopyable) {
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
            static_assert(hp == HeapPolicy::kAllow && size >= sizeof(void*));
            manager = [](Op op, void* src, void* dst) -> void {
                using T = Payload;
                switch (op) {
                    case Op::kDestroy: {
                        T* srcPtr = *static_cast<T**>(dst);
                        if (srcPtr == nullptr) {
                            return;
                        }
                        if constexpr (!std::is_trivially_destructible_v<T>) {
                            srcPtr->~T();
                        }
                        if constexpr (alignment <= alignof(std::max_align_t)) {
                            ::operator delete(srcPtr);
                        } else {
                            ::operator delete(srcPtr, std::align_val_t{std::size_t{alignment}});
                        }
                        return;
                    }
                    case Op::kAssign:
                    case Op::kClone: {
                        if constexpr (cp == Copyability::kCopyable) {
                            if constexpr (alignment <= alignof(std::max_align_t)) {
                                auto* heapPtr = ::operator new(sizeof(T));
                                if constexpr (std::is_trivially_copy_constructible_v<T>) {
                                    std::memcpy(heapPtr, *static_cast<const T**>(src), sizeof(T));
                                } else {
                                    new (heapPtr) T(**static_cast<const T**>(src));
                                }
                                new (dst) T*(static_cast<T*>(heapPtr));
                            } else {
                                auto* heapPtr = ::operator new(
                                    sizeof(T), std::align_val_t{std::size_t{alignment}});
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
               std::all_of(data.begin(), data.end(), [](auto v) { return v == std::byte{0}; });
    }
};

template <Alignment alignment, typename Signature>
struct Storage<alignment, 0, Triviality::kNonTrivial, false, Signature> {
    InvokerBuilder<Signature>::TypeNoBuffer invoker{nullptr};

    template <Op op, InlineSize sizeSrc, typename SigOther>
    constexpr void doConstructFromNonTrivial(
        Storage<alignment, sizeSrc, Triviality::kNonTrivial, false, SigOther>&&
        /*other*/) noexcept {  // NOLINT
        static_assert(op == Op::kMove || op == Op::kMoveAssign);
        static_assert(sizeSrc == 0);
    }

    template <Op op, InlineSize sizeSrc, typename SigOther>
    constexpr void doConstructFromNonTrivial(
        const Storage<alignment, 0, Triviality::kNonTrivial, false, SigOther>& /*other*/) {
        static_assert(op == Op::kClone || op == Op::kAssign);
    }

    template <std::size_t U, bool isAssign = false, HeapPolicy hp = HeapPolicy::kDisallow,
              Copyability cp = Copyability::kCopyable>
    constexpr void doConstructFromTrivial(void* /*dst*/, const void* /*src*/) noexcept {}

    template <typename Payload, HeapPolicy hp = HeapPolicy::kDisallow,
              Copyability cp = Copyability::kCopyable>
    constexpr void doConstruct() noexcept {}

    [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept {
        return true;
    }
};

template <Alignment alignment, InlineSize size, typename Signature>
    requires(size > 0)
struct Storage<alignment, size, Triviality::kNonTrivial, true, Signature> {
    alignas(alignment.bytes) mutable std::array<std::byte, size> data;
    InvokerBuilder<Signature>::Type invoker{nullptr};
    void (*cloner)(void* dst, const void* src){nullptr};
    void (*mover)(void* dst, void* src){nullptr};
    void (*deleter)(void* data){nullptr};

    constexpr void doDelete() noexcept {
        if (deleter != nullptr) {
            deleter(data.data());
            deleter = nullptr;
        }
    }

    template <Op op, InlineSize sizeSrc, typename SigOther>
    constexpr void doConstructFromNonTrivial(
        Storage<alignment, sizeSrc, Triviality::kNonTrivial, true,
                SigOther>&& other) noexcept {  // NOLINT
        static_assert(op == Op::kMove || op == Op::kMoveAssign);
        static_assert(size >= sizeSrc);
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

    template <Op op, InlineSize sizeSrc, typename SigOther>
    constexpr void doConstructFromNonTrivial(
        const Storage<alignment, sizeSrc, Triviality::kNonTrivial, true, SigOther>& other) {
        static_assert(op == Op::kClone || op == Op::kAssign);
        static_assert(size >= sizeSrc);
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

    template <std::size_t U, bool isAssign = false, HeapPolicy hp = HeapPolicy::kDisallow,
              Copyability cp = Copyability::kCopyable>
    constexpr void doConstructFromTrivial(void* dst, const void* src) noexcept {
        static_assert(cp == Copyability::kCopyable);
        if constexpr (isAssign) {
            doDelete();
        }
        cloner = [](void* dst, const void* src) { std::memcpy(dst, src, U); };
        mover = [](void* dst, void* src) { std::memcpy(dst, src, U); };
        deleter = nullptr;
        std::memcpy(dst, src, U);
    }

    template <typename Payload, HeapPolicy hp = HeapPolicy::kDisallow,
              Copyability cp = Copyability::kCopyable>
    constexpr void doConstruct() noexcept {
        static_assert(cp == Copyability::kCopyable);
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
        std::all_of(data.begin(), data.end(), [](auto v) { return v == std::byte{0}; });
    }
};

template <Alignment alignment, typename Signature>
struct Storage<alignment, 0, Triviality::kNonTrivial, true, Signature> {
    InvokerBuilder<Signature>::TypeNoBuffer invoker{nullptr};

    template <Op op, InlineSize sizeSrc, typename SigOther>
    constexpr void doConstructFromNonTrivial(
        Storage<alignment, 0, Triviality::kNonTrivial, true, SigOther>&&
        /*other*/) noexcept {  // NOLINT
        static_assert(op == Op::kMove || op == Op::kMoveAssign);
    }

    template <Op op, InlineSize sizeSrc, typename SigOther>
    constexpr void doConstructFromNonTrivial(
        const Storage<alignment, 0, Triviality::kNonTrivial, true, SigOther>& /*other*/) {
        static_assert(op == Op::kClone || op == Op::kAssign);
    }

    template <std::size_t U, bool isAssign = false, HeapPolicy hp = HeapPolicy::kDisallow,
              Copyability cp = Copyability::kCopyable>
    constexpr void doConstructFromTrivial(void* /*dst*/, const void* /*src*/) noexcept {}

    template <typename Payload, HeapPolicy hp = HeapPolicy::kDisallow,
              Copyability cp = Copyability::kCopyable>
    constexpr void doConstruct() noexcept {}

    [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept {
        return true;
    }
};

template <typename Signature, Copyability cb = Copyability::kCopyable, InlineSize size = 16,
          Triviality t = Triviality::kTrivial, bool Extended = false,
          HeapPolicy hp = HeapPolicy::kDisallow, Alignment align = alignof(std::max_align_t)>
struct Task;

template <typename S, Copyability cb, InlineSize size, Triviality t, bool Extended, HeapPolicy hp,
          Alignment align>
struct Task : public InvokerBuilder<S, Task<S, cb, size, t, Extended, hp, align>, size> {
    template <typename, Copyability, InlineSize, Triviality, bool, HeapPolicy, Alignment>
    friend struct Task;

    template <typename, typename, InlineSize, bool, bool, bool, bool>
    friend struct InvokerBuilder;

    // NOLINTBEGIN
    static constexpr std::size_t kSize = size;
    static constexpr auto kTriviality = t;
    static constexpr auto kHeapPolicy = hp;
    static constexpr auto kCopyability = cb;
    static constexpr bool kIsTrivial = t == Triviality::kTrivial;
    static constexpr bool kAllowHeap = hp == HeapPolicy::kAllow;
    static constexpr bool kCopyable = cb == Copyability::kCopyable;
    static constexpr bool kIsExtended = Extended;
    static constexpr std::size_t kAlignment = align;
    // NOLINTEND

    static_assert(!kIsTrivial || !kAllowHeap, "Trivial Task should not allow heap usage");
    static_assert(size > 0 || kIsTrivial, "0 buffer size is only allowed on trivial types");
    static_assert(!Extended || !kAllowHeap,
                  "Extended mode with heap under development. Switch one off");
    static_assert(align > 0 && (align & (align - 1)) == 0, "Alignment must be a power of 2");
    static_assert(kCopyable || !Extended, "Move only types under development for Extended option.");

    using Signature = S;
    using traits = FunctionTraits<Signature>;
    static constexpr bool kMutable = !traits::kIsConst;
    using R = typename traits::return_type;

   private:
    Storage<align, size, t, Extended, Signature> storage_;

    template <Op op, typename SigOther, Copyability cbSrc, InlineSize sizeSrc, bool MutSrc,
              Triviality tSrc, bool ExtSrc,
              HeapPolicy hpSrc>
    constexpr void doConvert(auto&& other) noexcept(  // NOLINT
        (op == Op::kMove || op == Op::kMoveAssign) &&
        (tSrc == Triviality::kNonTrivial || kIsTrivial || !kAllowHeap || sizeSrc <= size))
        requires std::same_as<std::remove_cvref_t<decltype(other)>,
                              Task<SigOther, cbSrc, sizeSrc, tSrc, ExtSrc, hpSrc, align>>
    {
        using SrcType = std::remove_cvref_t<decltype(other)>;
        static_assert(std::is_const_v<std::remove_reference_t<decltype(other)>> ||
                          std::is_rvalue_reference_v<decltype(other)>,
                      "T must be const& or rvalue&&");
        constexpr bool kIsMovable = std::is_rvalue_reference_v<decltype(other)>;
        auto& dest = *this;
        static_assert(
            size >= sizeSrc || (kAllowHeap && SrcType::kIsTrivial && !SrcType::kAllowHeap),
            "Payload too large to move into this storage");
        static_assert(
            !SrcType::kAllowHeap || kAllowHeap,
            "Cannot convert a task with heap storage option into one where it is disabled");
        static_assert(kMutable || !MutSrc);
        static_assert(!kIsTrivial || SrcType::kIsTrivial,
                      "Cannot copy/move non-trivial task into trivial one");
        static_assert(SrcType::kCopyable || (op != Op::kClone && op != Op::kAssign));

        static_assert((Extended == ExtSrc) || (kIsTrivial && SrcType::kIsTrivial));
        static_assert(op != Op::kDestroy, "Called Task converter with invalid (destroy) flag");
        static_assert((kIsMovable && (op == Op::kMove || op == Op::kMoveAssign)) ||
                      (op == Op::kClone || op == Op::kAssign));
        dest.storage_.invoker = other.storage_.invoker;

        if constexpr (SrcType::kIsTrivial) {
            if constexpr (kIsTrivial) {
                if constexpr (sizeSrc > 0) {
                    std::memcpy(dest.storage_.data.data(), other.storage_.data.data(), sizeSrc);
                }
            } else {
                if constexpr (sizeSrc == 0) {
                    dest.storage_.template doConstructFromTrivial<
                        sizeSrc, op == Op::kAssign || op == Op::kMoveAssign, hp, cb>(
                        dest.storage_.data.data(), nullptr);
                } else {
                    dest.storage_.template doConstructFromTrivial<
                        sizeSrc, op == Op::kAssign || op == Op::kMoveAssign, hp, cb>(
                        dest.storage_.data.data(), other.storage_.data.data());
                }
            }
        } else {
            if constexpr (kIsMovable) {
                dest.storage_.template doConstructFromNonTrivial<op, sizeSrc, SigOther>(
                    std::move(other.storage_));
            } else {
                dest.storage_.template doConstructFromNonTrivial<op, sizeSrc, SigOther>(
                    other.storage_);
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
            if constexpr (size == 0) {
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
        return size;
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
        return kCopyable;
    }

    [[nodiscard]] static constexpr auto isMoveOnly() noexcept -> bool {
        return !kCopyable;
    }

    [[nodiscard]] static constexpr auto isTrivial() noexcept -> bool {
        return kIsTrivial;
    }

    [[nodiscard]] static constexpr auto isMutable() noexcept -> bool {
        return kMutable;
    }

    [[nodiscard]] static constexpr auto isExtended() noexcept -> bool {
        return kIsExtended;
    }

    [[nodiscard]] static constexpr auto allowsHeap() noexcept -> bool {
        return kAllowHeap;
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

    template <typename SigOther, InlineSize sizeSrc, Triviality tSrc, bool Ext2, HeapPolicy hpSrc>
    Task& operator=(
        const Task<SigOther, Copyability::kCopyable, sizeSrc, tSrc, Ext2, hpSrc, align>& other)
        requires((kMutable || !Task<SigOther, Copyability::kCopyable, sizeSrc, tSrc, Ext2, hpSrc,
                                    align>::kMutable) &&
                 (!kIsTrivial || tSrc == Triviality::kTrivial) &&
                 (kIsTrivial || tSrc == Triviality::kTrivial || Extended == Ext2) &&
                 (kAllowHeap || hpSrc == HeapPolicy::kDisallow) &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    {
        doConvert<Op::kAssign, SigOther, true, sizeSrc,
                  Task<SigOther, cb, sizeSrc, tSrc, Ext2, hpSrc, align>::kMutable, tSrc, Ext2,
                  hpSrc>(other);
        return *this;
    };

    template <typename SigOther, InlineSize sizeSrc, Triviality tSrc, bool Ext2, HeapPolicy hpSrc>
    Task(const Task<SigOther, Copyability::kCopyable, sizeSrc, tSrc, Ext2, hpSrc, align>& other)
        requires((kMutable || !Task<SigOther, Copyability::kCopyable, sizeSrc, tSrc, Ext2, hpSrc,
                                    align>::kMutable) &&
                 (!kIsTrivial || tSrc == Triviality::kTrivial) &&
                 (kIsTrivial || tSrc == Triviality::kTrivial || Extended == Ext2) &&
                 (kAllowHeap || hpSrc == HeapPolicy::kDisallow) &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    {
        doConvert<
            Op::kClone, SigOther, Copyability::kCopyable, sizeSrc,
            Task<SigOther, Copyability::kCopyable, sizeSrc, tSrc, Ext2, hpSrc, align>::kMutable,
            tSrc, Ext2, hpSrc>(other);
    };

    Task(const Task& /*other*/)
        requires(kIsTrivial && kCopyable)
    = default;

    Task(const Task& other)
        requires(!kIsTrivial && kCopyable)
    {
        doConvert<Op::kClone, Signature, cb, size, kMutable, t, Extended, hp>(other);
    }

    Task(const Task& /*other*/)
        requires(!kCopyable)
    = delete;

    Task& operator=(const Task& /*other*/)
        requires(kIsTrivial && kCopyable)
    = default;

    Task& operator=(const Task& other)
        requires(!kIsTrivial && kCopyable)
    {
        if (this == &other) {
            return *this;
        }
        doConvert<Op::kAssign, Signature, cb, size, kMutable, t, Extended, hp>(other);
        return *this;
    }

    Task& operator=(const Task& /*other*/)
        requires(!kCopyable)
    = delete;

    template <typename SigOther, Copyability cbSrc, InlineSize sizeSrc, Triviality tSrc, bool Ext2,
              HeapPolicy hpSrc>
    Task(Task<SigOther, cbSrc, sizeSrc, tSrc, Ext2, hpSrc, align>&& other) noexcept(
        tSrc == Triviality::kNonTrivial || kIsTrivial || !kAllowHeap || sizeSrc <= size)
        requires((kMutable ||
                  !Task<SigOther, cbSrc, sizeSrc, tSrc, Ext2, hpSrc, align>::kMutable) &&
                 (!kIsTrivial || tSrc == Triviality::kTrivial) &&
                 (kIsTrivial || tSrc == Triviality::kTrivial || Extended == Ext2) &&
                 (kAllowHeap || hpSrc == HeapPolicy::kDisallow) &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    {
        doConvert<Op::kMove, SigOther, cbSrc, sizeSrc,
                  Task<SigOther, cbSrc, sizeSrc, tSrc, Ext2, hpSrc, align>::kMutable, tSrc, Ext2,
                  hpSrc>(std::move(other));
    }

    template <typename SigOther, Copyability cbSrc, InlineSize sizeSrc, Triviality tSrc, bool Ext2,
              HeapPolicy hpSrc>
    Task& operator=(Task<SigOther, cbSrc, sizeSrc, tSrc, Ext2, hpSrc, align>&& other) noexcept(
        tSrc == Triviality::kNonTrivial || kIsTrivial || !kAllowHeap || sizeSrc <= size)
        requires((kMutable ||
                  !Task<SigOther, cbSrc, sizeSrc, tSrc, Ext2, hpSrc, align>::kMutable) &&
                 (!kIsTrivial || tSrc == Triviality::kTrivial) &&
                 (kIsTrivial || tSrc == Triviality::kTrivial || Extended == Ext2) &&
                 (kAllowHeap || hpSrc == HeapPolicy::kDisallow) &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    {
        doConvert<Op::kMoveAssign, SigOther, cbSrc, sizeSrc,
                  Task<SigOther, cbSrc, sizeSrc, tSrc, Ext2, hpSrc, align>::kMutable, tSrc, Ext2,
                  hpSrc>(std::move(other));
        return *this;
    }

    Task(Task&& /*other*/) noexcept
        requires kIsTrivial
    = default;

    Task(Task&& other) noexcept
        requires(!kIsTrivial)
    {
        doConvert<Op::kMove, Signature, cb, size, kMutable, t, Extended, hp>(std::move(other));
    }

    Task& operator=(Task&& /*other*/) noexcept
        requires kIsTrivial
    = default;

    Task& operator=(Task&& other) noexcept
        requires(!kIsTrivial)
    {
        if (this == &other) {
            return *this;
        }
        doConvert<Op::kMoveAssign, Signature, cb, size, kMutable, t, Extended, hp>(
            std::move(other));
        return *this;
    }

    void swap(Task& other) noexcept {
        auto tmp = Task{std::move(other)};
        other = std::move(*this);
        (*this) = std::move(tmp);
    }

    Task& operator=(std::nullptr_t) {
        clear();
        return *this;
    }

    constexpr Task() = default;

    constexpr Task(std::nullptr_t) {};

    constexpr ~Task() noexcept
        requires kIsTrivial
    = default;

    constexpr ~Task() noexcept
        requires(!kIsTrivial)
    {
        storage_.doDelete();
    }

    enum class ArgsOrder : std::uint8_t { kBack, kFront };

    template <auto F, typename... BoundArgs>
    constexpr Task([[maybe_unused]] FunctionTag<F> /*unused*/,
                   BoundArgs&&... args)  // NOLINT
        : storage_{.invoker = InvokerBuilder<Signature, std::remove_cv_t<decltype(*this)>, size>::
                       template getInvoker<Binder<>{}, PayloadArgsT<F, BoundArgs...>, F,
                                           std::decay_t<BoundArgs>...>()} {
        using PayloadType = PayloadArgsT<F, BoundArgs...>;
        if constexpr (!std::is_empty_v<PayloadType>) {
            static_assert(PayloadNoRvalueRef<PayloadType>::value);
            static_assert(
                !kCopyable || std::is_copy_constructible_v<PayloadType>,
                "Given payload is not copy constructible. Check types or create a unique task");
            static_assert(
                std::is_move_constructible_v<PayloadType> && std::is_destructible_v<PayloadType>,
                "Given payload is not constructible, move constructible or destructible");
            static_assert(
                sizeof(PayloadType) <= size || (kAllowHeap && sizeof(PayloadType*) <= size),
                "Size exceeds buffer. Increase Stack size or activate option to allow heap "
                "storage (template option, or via FlexTask aliases)");
            static_assert(alignof(PayloadType) <= align,
                          "Alignment requirement too high for buffer. Increase template alignment");
            if constexpr (kIsTrivial) {
                static_assert(TrivialUse<PayloadType, cb>::value,
                              "Payload needs to be trivially move & copy constructible, and "
                              "trivially destructible to be allowed within a Trivial Task");
            }
            storage_.template doConstruct<PayloadType, hp, cb>();
            if constexpr (sizeof(PayloadType) <= size) {
                new (storage_.data.data()) PayloadType{{std::forward<BoundArgs>(args)...}};
            } else {
                static_assert(alignof(void*) <= align,
                              "Alignment requirement too high to store heap pointer. Increase "
                              "template alignment");
                static_assert(kAllowHeap);
                if constexpr (align <= alignof(std::max_align_t)) {
                    void* heapPtr = ::operator new(sizeof(PayloadType));
                    auto* payload =  // NOLINT
                        new (heapPtr) PayloadType{{std::forward<BoundArgs>(args)...}};
                    new (storage_.data.data()) PayloadType*(payload);
                } else {
                    void* heapPtr =
                        ::operator new(sizeof(PayloadType), std::align_val_t{std::size_t{align}});
                    auto* payload =  // NOLINT
                        new (heapPtr) PayloadType{{std::forward<BoundArgs>(args)...}};
                    new (storage_.data.data()) PayloadType*(payload);
                }
            }
        }
    }

    template <auto F, Binder binder, typename... BoundArgs>
    constexpr Task([[maybe_unused]] FunctionTag<F> /*unused*/, BinderTag<binder> /*unused*/,
                   BoundArgs&&... args)  // NOLINT
        : storage_{
              .invoker =
                  InvokerBuilder<Signature, std::remove_cv_t<decltype(*this)>,
                                 size>::template getInvoker<binder, PayloadArgsT<F, BoundArgs...>,
                                                            F, std::decay_t<BoundArgs>...>()} {
        using PayloadType = PayloadArgsT<F, BoundArgs...>;
        if constexpr (!std::is_empty_v<PayloadType>) {
            static_assert(PayloadNoRvalueRef<PayloadType>::value);
            static_assert(
                !kCopyable || std::is_copy_constructible_v<PayloadType>,
                "Given payload is not copy constructible. Check types or create a unique task");
            static_assert(
                std::is_move_constructible_v<PayloadType> && std::is_destructible_v<PayloadType>,
                "Given payload is not constructible, move constructible or destructible");
            static_assert(
                sizeof(PayloadType) <= size || (kAllowHeap && sizeof(PayloadType*) <= size),
                "Size exceeds buffer. Increase Stack size or activate option to allow heap "
                "storage (template option, or via FlexTask aliases)");
            static_assert(alignof(PayloadType) <= align,
                          "Alignment requirement too high for buffer. Increase template alignment");
            if constexpr (kIsTrivial) {
                static_assert(TrivialUse<PayloadType, cb>::value,
                              "Payload needs to be trivially move & copy constructible, and "
                              "trivially destructible to be allowed within a Trivial Task");
            }
            storage_.template doConstruct<PayloadType, hp, cb>();
            if constexpr (sizeof(PayloadType) <= size) {
                new (storage_.data.data()) PayloadType{{std::forward<BoundArgs>(args)...}};
            } else {
                static_assert(alignof(void*) <= align,
                              "Alignment requirement too high to store heap pointer. Increase "
                              "template alignment");
                static_assert(kAllowHeap);
                if constexpr (align <= alignof(std::max_align_t)) {
                    void* heapPtr = ::operator new(sizeof(PayloadType));
                    auto* payload =  // NOLINT
                        new (heapPtr) PayloadType{{std::forward<BoundArgs>(args)...}};
                    new (storage_.data.data()) PayloadType*(payload);
                } else {
                    void* heapPtr =
                        ::operator new(sizeof(PayloadType), std::align_val_t{std::size_t{align}});
                    auto* payload =  // NOLINT
                        new (heapPtr) PayloadType{{std::forward<BoundArgs>(args)...}};
                    new (storage_.data.data()) PayloadType*(payload);
                }
            }
        }
    }

    template <typename... BoundArgs>
    constexpr Task(auto&& f, BoundArgs&&... args)  // NOLINT
        requires(!std::is_same_v<std::remove_cvref_t<decltype(f)>, RefTag> &&
                 !IsBinderTagB<std::remove_cvref_t<decltype(f)>> &&
                 !IsFunctionTag<std::remove_cvref_t<decltype(f)>>::value &&
                 (sizeof...(BoundArgs) > 0 || !IsTask<std::remove_cvref_t<decltype(f)>>::value))
        : storage_{
              .invoker = InvokerBuilder<Signature, std::remove_cv_t<decltype(*this)>, size>::
                  template getInvoker<Binder<>{}, PayloadDecayed<decltype(f), BoundArgs...>,
                                      std::decay_t<decltype(f)>, std::decay_t<BoundArgs>...>()} {
        using F = decltype(f);
        using PayloadType = PayloadDecayed<F, BoundArgs...>;
        static_assert(PayloadNoRvalueRef<PayloadType>::value);
        static_assert(sizeof(PayloadType) <= size || (kAllowHeap && sizeof(PayloadType*) <= size),
                      "Size exceeds buffer. Increase Stack size or activate option to allow heap "
                      "storage (template option, or via FlexTask aliases)");
        static_assert(alignof(PayloadType) <= align,
                      "Alignment requirement too high for buffer. Increase template alignment");
        static_assert(
            !kCopyable || std::is_copy_constructible_v<PayloadType>,
            "Given payload is not copy constructible. Check types or create a unique task");
        static_assert(
            std::is_move_constructible_v<PayloadType> && std::is_destructible_v<PayloadType>,
            "Given payload is not move constructible or destructible");
        if constexpr (kIsTrivial) {
            static_assert(TrivialUse<PayloadType, cb>::value,
                          "Payload needs to be trivially move & copy constructible, and "
                          "trivially destructible to be allowed within a Trivial Task");
        }
        storage_.template doConstruct<PayloadType, hp, cb>();
        if constexpr (sizeof(PayloadType) <= size) {
            new (storage_.data.data())
                PayloadType{std::forward<F>(f), {std::forward<BoundArgs>(args)...}};
        } else {
            static_assert(kAllowHeap);
            static_assert(alignof(void*) <= align,
                          "Alignment requirement too high to store heap pointer. Increase "
                          "template alignment");
            if constexpr (align <= alignof(std::max_align_t)) {
                void* heapPtr = ::operator new(sizeof(PayloadType));
                auto* payload =  // NOLINT
                    new (heapPtr)
                        PayloadType{std::forward<F>(f), {std::forward<BoundArgs>(args)...}};
                new (storage_.data.data()) PayloadType*(payload);
            } else {
                void* heapPtr =
                    ::operator new(sizeof(PayloadType), std::align_val_t{std::size_t{align}});
                auto* payload =  // NOLINT
                    new (heapPtr)
                        PayloadType{std::forward<F>(f), {std::forward<BoundArgs>(args)...}};
                new (storage_.data.data()) PayloadType*(payload);
            }
        }
    }

    template <Binder binder, typename... BoundArgs>
    constexpr Task(BinderTag<binder> /*unused*/, auto&& f, BoundArgs&&... args)  // NOLINT
        requires(!std::is_same_v<std::remove_cvref_t<decltype(f)>, RefTag> &&
                 !IsFunctionTag<std::remove_cvref_t<decltype(f)>>::value &&
                 (sizeof...(BoundArgs) > 0 || !IsTask<std::remove_cvref_t<decltype(f)>>::value))
        : storage_{
              .invoker = InvokerBuilder<Signature, std::remove_cv_t<decltype(*this)>, size>::
                  template getInvoker<binder, PayloadDecayed<decltype(f), BoundArgs...>,
                                      std::decay_t<decltype(f)>, std::decay_t<BoundArgs>...>()} {
        using F = decltype(f);
        using PayloadType = PayloadDecayed<F, BoundArgs...>;
        static_assert(PayloadNoRvalueRef<PayloadType>::value);
        static_assert(sizeof(PayloadType) <= size || (kAllowHeap && sizeof(PayloadType*) <= size),
                      "Size exceeds buffer. Increase Stack size or activate option to allow heap "
                      "storage (template option, or via FlexTask aliases)");
        static_assert(alignof(PayloadType) <= align,
                      "Alignment requirement too high for buffer. Increase template alignment");
        static_assert(
            !kCopyable || std::is_copy_constructible_v<PayloadType>,
            "Given payload is not copy constructible. Check types or create a unique task");
        static_assert(
            std::is_move_constructible_v<PayloadType> && std::is_destructible_v<PayloadType>,
            "Given payload is not move constructible or destructible");
        if constexpr (kIsTrivial) {
            static_assert(TrivialUse<PayloadType, cb>::value,
                          "Payload needs to be trivially move & copy constructible, and "
                          "trivially destructible to be allowed within a Trivial Task");
        }
        storage_.template doConstruct<PayloadType, hp, cb>();
        if constexpr (sizeof(PayloadType) <= size) {
            new (storage_.data.data())
                PayloadType{std::forward<F>(f), {std::forward<BoundArgs>(args)...}};
        } else {
            static_assert(kAllowHeap);
            static_assert(alignof(void*) <= align,
                          "Alignment requirement too high to store heap pointer. Increase "
                          "template alignment");
            if constexpr (align <= alignof(std::max_align_t)) {
                void* heapPtr = ::operator new(sizeof(PayloadType));
                auto* payload =  // NOLINT
                    new (heapPtr)
                        PayloadType{std::forward<F>(f), {std::forward<BoundArgs>(args)...}};
                new (storage_.data.data()) PayloadType*(payload);
            } else {
                void* heapPtr =
                    ::operator new(sizeof(PayloadType), std::align_val_t{std::size_t{align}});
                auto* payload =  // NOLINT
                    new (heapPtr)
                        PayloadType{std::forward<F>(f), {std::forward<BoundArgs>(args)...}};
                new (storage_.data.data()) PayloadType*(payload);
            }
        }
    }

    template <typename F, typename... BoundArgs>
    constexpr Task([[maybe_unused]] RefTag /*unused*/, F&& f,
                   BoundArgs&&... args)  // NOLINT
        requires(!IsFunctionTag<std::remove_cvref_t<decltype(f)>>::value &&
                 !IsBinderTagB<std::remove_cvref_t<decltype(f)>>)
        : storage_{.invoker = InvokerBuilder<Signature, std::remove_cv_t<decltype(*this)>, size>::
                       template getInvoker<Binder<>{}, PayloadImpl<F, BoundArgs...>, F,
                                           BoundArgs...>()} {
        static_assert(
            (((std::is_lvalue_reference_v<decltype(args)> &&
               std::is_lvalue_reference_v<BoundArgs>) ||
              (!std::is_rvalue_reference_v<BoundArgs> && !std::is_lvalue_reference_v<BoundArgs> &&
               std::is_rvalue_reference_v<decltype(args)>)) &&
             ...));
        using PayloadType = PayloadImpl<F, BoundArgs...>;
        static_assert(PayloadNoRvalueRef<PayloadType>::value);
        static_assert(sizeof(PayloadType) <= size || (kAllowHeap && sizeof(PayloadType*) <= size),
                      "Size exceeds buffer. Increase Stack size or activate option to allow heap "
                      "storage (template option, or via FlexTask aliases)");
        static_assert(alignof(PayloadType) <= align,
                      "Alignment requirement too high for buffer. Increase template alignment");
        static_assert(
            !kCopyable || std::is_copy_constructible_v<PayloadType>,
            "Given payload is not copy constructible. Check types or create a unique task");
        static_assert(
            std::is_move_constructible_v<PayloadType> && std::is_destructible_v<PayloadType>,
            "Given payload is not constructible, move constructible or destructible");
        if constexpr (kIsTrivial) {
            static_assert(TrivialUse<PayloadType, cb>::value,
                          "Payload needs to be trivially move & copy constructible, and "
                          "trivially destructible to be allowed within a Trivial Task");
        }
        storage_.template doConstruct<PayloadType, hp, cb>();
        if constexpr (sizeof(PayloadType) <= size) {
            new (storage_.data.data())
                PayloadType{std::forward<F>(f), {std::forward<BoundArgs>(args)...}};
        } else {
            static_assert(kAllowHeap);
            static_assert(alignof(void*) <= align,
                          "Alignment requirement too high to store heap pointer. Increase "
                          "template alignment");
            if constexpr (align <= alignof(std::max_align_t)) {
                void* heapPtr = ::operator new(sizeof(PayloadType));
                auto* payload =  // NOLINT
                    new (heapPtr)
                        PayloadType{std::forward<F>(f), {std::forward<BoundArgs>(args)...}};
                new (storage_.data.data()) PayloadType*(payload);
            } else {
                void* heapPtr =
                    ::operator new(sizeof(PayloadType), std::align_val_t{std::size_t{align}});
                auto* payload =  // NOLINT
                    new (heapPtr)
                        PayloadType{std::forward<F>(f), {std::forward<BoundArgs>(args)...}};
                new (storage_.data.data()) PayloadType*(payload);
            }
        }
    }

    template <Binder binder, typename F, typename... BoundArgs>
    constexpr Task([[maybe_unused]] RefTag /*unused*/, BinderTag<binder> /*unused*/, F&& f,
                   BoundArgs&&... args)  // NOLINT
        requires(!IsFunctionTag<std::remove_cvref_t<decltype(f)>>::value)
        : storage_{
              .invoker = InvokerBuilder<Signature, std::remove_cv_t<decltype(*this)>, size>::
                  template getInvoker<binder, PayloadImpl<F, BoundArgs...>, F, BoundArgs...>()} {
        static_assert(
            (((std::is_lvalue_reference_v<decltype(args)> &&
               std::is_lvalue_reference_v<BoundArgs>) ||
              (!std::is_rvalue_reference_v<BoundArgs> && !std::is_lvalue_reference_v<BoundArgs> &&
               std::is_rvalue_reference_v<decltype(args)>)) &&
             ...));
        using PayloadType = PayloadImpl<F, BoundArgs...>;
        static_assert(PayloadNoRvalueRef<PayloadType>::value);
        static_assert(sizeof(PayloadType) <= size || (kAllowHeap && sizeof(PayloadType*) <= size),
                      "Size exceeds buffer. Increase Stack size or activate option to allow heap "
                      "storage (template option, or via FlexTask aliases)");
        static_assert(alignof(PayloadType) <= align,
                      "Alignment requirement too high for buffer. Increase template alignment");
        static_assert(
            !kCopyable || std::is_copy_constructible_v<PayloadType>,
            "Given payload is not copy constructible. Check types or create a unique task");
        static_assert(
            std::is_move_constructible_v<PayloadType> && std::is_destructible_v<PayloadType>,
            "Given payload is not constructible, move constructible or destructible");
        if constexpr (kIsTrivial) {
            static_assert(TrivialUse<PayloadType, cb>::value,
                          "Payload needs to be trivially move & copy constructible, and "
                          "trivially destructible to be allowed within a Trivial Task");
        }
        storage_.template doConstruct<PayloadType, hp, cb>();
        if constexpr (sizeof(PayloadType) <= size) {
            new (storage_.data.data())
                PayloadType{std::forward<F>(f), {std::forward<BoundArgs>(args)...}};
        } else {
            static_assert(kAllowHeap);
            static_assert(alignof(void*) <= align,
                          "Alignment requirement too high to store heap pointer. Increase "
                          "template alignment");
            if constexpr (align <= alignof(std::max_align_t)) {
                void* heapPtr = ::operator new(sizeof(PayloadType));
                auto* payload =  // NOLINT
                    new (heapPtr)
                        PayloadType{std::forward<F>(f), {std::forward<BoundArgs>(args)...}};
                new (storage_.data.data()) PayloadType*(payload);
            } else {
                void* heapPtr =
                    ::operator new(sizeof(PayloadType), std::align_val_t{std::size_t{align}});
                auto* payload =  // NOLINT
                    new (heapPtr)
                        PayloadType{std::forward<F>(f), {std::forward<BoundArgs>(args)...}};
                new (storage_.data.data()) PayloadType*(payload);
            }
        }
    }

    template <auto F, typename... BoundArgs>
    constexpr Task([[maybe_unused]] RefTag /*unused*/,  // NOLINT
                   [[maybe_unused]] FunctionTag<F> /*unused*/,
                   BoundArgs&&... args)  // NOLINT
        : storage_{.invoker = InvokerBuilder<Signature, std::remove_cv_t<decltype(*this)>, size>::
                       template getInvoker<Binder<>{}, PayloadImplArgs<F, BoundArgs...>, F,
                                           BoundArgs...>()} {
        static_assert(
            (((std::is_lvalue_reference_v<decltype(args)> &&
               std::is_lvalue_reference_v<BoundArgs>) ||
              (!std::is_rvalue_reference_v<BoundArgs> && !std::is_lvalue_reference_v<BoundArgs> &&
               std::is_rvalue_reference_v<decltype(args)>)) &&
             ...));
        using PayloadType = PayloadImplArgs<F, BoundArgs...>;
        if constexpr (!std::is_empty_v<PayloadType>) {
            static_assert(PayloadNoRvalueRef<PayloadType>::value);
            static_assert(
                sizeof(PayloadType) <= size || (kAllowHeap && sizeof(PayloadType*) <= size),
                "Size exceeds buffer. Increase Stack size or activate option to allow heap "
                "storage (template option, or via FlexTask aliases)");
            static_assert(alignof(PayloadType) <= align,
                          "Alignment requirement too high for buffer. Increase template alignment");
            static_assert(
                !kCopyable || std::is_copy_constructible_v<PayloadType>,
                "Given payload is not copy constructible. Check types or create a unique task");
            static_assert(
                std::is_move_constructible_v<PayloadType> && std::is_destructible_v<PayloadType>,
                "Given payload is not constructible, move constructible or destructible");
            if constexpr (kIsTrivial) {
                static_assert(TrivialUse<PayloadType, cb>::value,
                              "Payload needs to be trivially move & copy constructible, and "
                              "trivially destructible to be allowed within a Trivial Task");
            }
            storage_.template doConstruct<PayloadType, hp, cb>();
            if constexpr (sizeof(PayloadType) <= size) {
                new (storage_.data.data()) PayloadType{{std::forward<BoundArgs>(args)...}};
            } else {
                static_assert(kAllowHeap);
                static_assert(alignof(void*) <= align,
                              "Alignment requirement too high to store heap pointer. Increase "
                              "template alignment");
                if constexpr (align <= alignof(std::max_align_t)) {
                    void* heapPtr = ::operator new(sizeof(PayloadType));
                    auto* payload =  // NOLINT
                        new (heapPtr) PayloadType{{std::forward<BoundArgs>(args)...}};
                    new (storage_.data.data()) PayloadType*(payload);
                } else {
                    void* heapPtr =
                        ::operator new(sizeof(PayloadType), std::align_val_t{std::size_t{align}});
                    auto* payload =  // NOLINT
                        new (heapPtr) PayloadType{{std::forward<BoundArgs>(args)...}};
                    new (storage_.data.data()) PayloadType*(payload);
                }
            }
        }
    }

    template <auto F, Binder binder, typename... BoundArgs>
    constexpr Task([[maybe_unused]] RefTag /*unused*/,  // NOLINT
                   [[maybe_unused]] FunctionTag<F> /*unused*/, BinderTag<binder> /*unused*/,
                   BoundArgs&&... args)  // NOLINT
        : storage_{.invoker = InvokerBuilder<Signature, std::remove_cv_t<decltype(*this)>, size>::
                       template getInvoker<binder, PayloadImplArgs<F, BoundArgs...>, F,
                                           BoundArgs...>()} {
        static_assert(
            (((std::is_lvalue_reference_v<decltype(args)> &&
               std::is_lvalue_reference_v<BoundArgs>) ||
              (!std::is_rvalue_reference_v<BoundArgs> && !std::is_lvalue_reference_v<BoundArgs> &&
               std::is_rvalue_reference_v<decltype(args)>)) &&
             ...));
        using PayloadType = PayloadImplArgs<F, BoundArgs...>;
        if constexpr (!std::is_empty_v<PayloadType>) {
            static_assert(PayloadNoRvalueRef<PayloadType>::value);
            static_assert(
                sizeof(PayloadType) <= size || (kAllowHeap && sizeof(PayloadType*) <= size),
                "Size exceeds buffer. Increase Stack size or activate option to allow heap "
                "storage (template option, or via FlexTask aliases)");
            static_assert(alignof(PayloadType) <= align,
                          "Alignment requirement too high for buffer. Increase template alignment");
            static_assert(
                !kCopyable || std::is_copy_constructible_v<PayloadType>,
                "Given payload is not copy constructible. Check types or create a unique task");
            static_assert(
                std::is_move_constructible_v<PayloadType> && std::is_destructible_v<PayloadType>,
                "Given payload is not constructible, move constructible or destructible");
            if constexpr (kIsTrivial) {
                static_assert(TrivialUse<PayloadType, cb>::value,
                              "Payload needs to be trivially move & copy constructible, and "
                              "trivially destructible to be allowed within a Trivial Task");
            }
            storage_.template doConstruct<PayloadType, hp, cb>();
            if constexpr (sizeof(PayloadType) <= size) {
                new (storage_.data.data()) PayloadType{{std::forward<BoundArgs>(args)...}};
            } else {
                static_assert(kAllowHeap);
                static_assert(alignof(void*) <= align,
                              "Alignment requirement too high to store heap pointer. Increase "
                              "template alignment");
                if constexpr (align <= alignof(std::max_align_t)) {
                    void* heapPtr = ::operator new(sizeof(PayloadType));
                    auto* payload =  // NOLINT
                        new (heapPtr) PayloadType{{std::forward<BoundArgs>(args)...}};
                    new (storage_.data.data()) PayloadType*(payload);
                } else {
                    void* heapPtr =
                        ::operator new(sizeof(PayloadType), std::align_val_t{std::size_t{align}});
                    auto* payload =  // NOLINT
                        new (heapPtr) PayloadType{{std::forward<BoundArgs>(args)...}};
                    new (storage_.data.data()) PayloadType*(payload);
                }
            }
        }
    }

    constexpr void clear() noexcept {
        storage_.invoker = nullptr;
        if constexpr (!kIsTrivial) {
            storage_.doDelete();
        }
    }
};

template <typename Signature, InlineSize size = 16, Triviality t = Triviality::kNonTrivial,
          HeapPolicy hp = HeapPolicy::kDisallow, Alignment alignment = alignof(std::max_align_t)>
using CopyableTask = Task<Signature, Copyability::kCopyable, size, t, false, hp, alignment>;

template <typename Signature, InlineSize size = 16, Triviality t = Triviality::kNonTrivial,
          HeapPolicy hp = HeapPolicy::kDisallow, Alignment alignment = alignof(std::max_align_t)>
using UniqueTask = Task<Signature, Copyability::kNonCopyable, size, t, false, hp, alignment>;

template <typename Signature, Copyability cp = Copyability::kCopyable,
          Triviality t = Triviality::kNonTrivial, Alignment alg = alignof(std::max_align_t)>
using Task8 = Task<Signature, cp, 16, t, false, HeapPolicy::kDisallow, alg>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Triviality t = Triviality::kNonTrivial, Alignment alg = alignof(std::max_align_t)>
using Task16 = Task<Signature, cp, 16, t, false, HeapPolicy::kDisallow, alg>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Triviality t = Triviality::kNonTrivial, Alignment alg = alignof(std::max_align_t)>
using Task32 = Task<Signature, cp, 32, t, false, HeapPolicy::kDisallow, alg>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Triviality t = Triviality::kNonTrivial, Alignment alg = alignof(std::max_align_t)>
using Task48 = Task<Signature, cp, 48, t, false, HeapPolicy::kDisallow, alg>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Triviality t = Triviality::kNonTrivial, Alignment alg = alignof(std::max_align_t)>
using Task64 = Task<Signature, cp, 64, t, false, HeapPolicy::kDisallow, alg>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Triviality t = Triviality::kNonTrivial, Alignment alg = alignof(std::max_align_t)>
using Task128 = Task<Signature, cp, 128, t, false, HeapPolicy::kDisallow, alg>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Triviality t = Triviality::kNonTrivial, Alignment alg = alignof(std::max_align_t)>
using Task256 = Task<Signature, cp, 256, t, false, HeapPolicy::kDisallow, alg>;

template <typename Signature, Copyability cp = Copyability::kCopyable,
          Alignment alg = alignof(std::max_align_t)>
using TrivialTask16 = Task16<Signature, cp, Triviality::kTrivial, alg>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Alignment alg = alignof(std::max_align_t)>
using TrivialTask32 = Task32<Signature, cp, Triviality::kTrivial, alg>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Alignment alg = alignof(std::max_align_t)>
using TrivialTask48 = Task48<Signature, cp, Triviality::kTrivial, alg>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Alignment alg = alignof(std::max_align_t)>
using TrivialTask64 = Task64<Signature, cp, Triviality::kTrivial, alg>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Alignment alg = alignof(std::max_align_t)>
using TrivialTask128 = Task128<Signature, cp, Triviality::kTrivial, alg>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Alignment alg = alignof(std::max_align_t)>
using TrivialTask256 = Task256<Signature, cp, Triviality::kTrivial, alg>;

template <typename Signature, Copyability cp = Copyability::kCopyable,
          Alignment alg = alignof(std::max_align_t)>
using SmallTask =
    Task<Signature, cp, 8, Triviality::kNonTrivial, false, HeapPolicy::kDisallow, alg>;

template <typename Signature, Copyability cp = Copyability::kCopyable, InlineSize size = 16,
          Alignment alg = alignof(std::max_align_t)>
using TrivialTask =
    Task<Signature, cp, size, Triviality::kTrivial, false, HeapPolicy::kDisallow, alg>;

template <typename Signature, Copyability cp = Copyability::kCopyable,
          Alignment alg = alignof(std::max_align_t)>
using TrivialSmallTask = TrivialTask<Signature, cp, 8, alg>;

template <typename Signature, Copyability cp = Copyability::kCopyable>
using TrivialTaskCaptureless = TrivialTask<Signature, cp, 1>;

template <typename Signature, Copyability cp = Copyability::kCopyable>
using StaticTrivialTask = TrivialTask<Signature, cp, 0>;

template <typename Signature, Copyability cp = Copyability::kCopyable, InlineSize size = 16,
          Alignment alg = alignof(std::max_align_t)>
using FlexTask = Task<Signature, cp, size, Triviality::kNonTrivial, false, HeapPolicy::kAllow, alg>;

template <typename Signature, InlineSize size = 16, Alignment alg = alignof(std::max_align_t)>
using UniqueFlexTask = FlexTask<Signature, Copyability::kNonCopyable, size, alg>;

template <typename Signature, Copyability cp = Copyability::kCopyable,
          Alignment alg = alignof(std::max_align_t)>
using FlexTask8 = FlexTask<Signature, cp, 8, alg>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Alignment alg = alignof(std::max_align_t)>
using FlexTask16 = FlexTask<Signature, cp, 16, alg>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Alignment alg = alignof(std::max_align_t)>
using FlexTask32 = FlexTask<Signature, cp, 32, alg>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Alignment alg = alignof(std::max_align_t)>
using FlexTask64 = FlexTask<Signature, cp, 64, alg>;

static_assert(TrivialTask<void(), Copyability::kCopyable, 0>::getSize() == sizeof(void*));

template <typename Sig, Copyability cp, InlineSize size, Triviality t, bool Ext, HeapPolicy hp,
          Alignment align>
Task(Task<Sig, cp, size, t, Ext, hp, align>) -> Task<Sig, cp, size, t, Ext, hp, align>;

template <auto F, typename... BoundArgs>
    requires((sizeof...(BoundArgs) > 0 || !IsTask<std::remove_cvref_t<decltype(F)>>::value) &&
             FunctionTraits<decltype(F)>::kValid &&
             TaskTypeDeductor<DeductorSettings<void>{}, FunctionTag<F>,
                              TaskBuilderRefFilterT<RefQualifier::kNone, BoundArgs>...>::kIsValid)
Task(FunctionTag<F>, BoundArgs&&...) -> Task<
    typename TaskTypeDeductor<DeductorSettings<void>{}, FunctionTag<F>,
                              TaskBuilderRefFilterT<RefQualifier::kNone, BoundArgs>...>::Signature,
    TaskTypeDeductor<DeductorSettings<void>{}, FunctionTag<F>,
                     TaskBuilderRefFilterT<RefQualifier::kNone, BoundArgs>...>::kCopyable,
    TaskTypeDeductor<DeductorSettings<void>{}, FunctionTag<F>,
                     TaskBuilderRefFilterT<RefQualifier::kNone, BoundArgs>...>::kPayloadSize,
    TaskTypeDeductor<DeductorSettings<void>{}, FunctionTag<F>,
                     TaskBuilderRefFilterT<RefQualifier::kNone, BoundArgs>...>::kTriviality,
    false, HeapPolicy::kDisallow,
    TaskTypeDeductor<DeductorSettings<void>{}, FunctionTag<F>,
                     TaskBuilderRefFilterT<RefQualifier::kNone, BoundArgs>...>::kAlignment>;

template <typename F, typename... BoundArgs>
    requires(
        !std::is_same_v<F, RefTag> && !IsFunctionTag<F>::value &&
        FunctionTraits<std::remove_reference_t<
            std::unwrap_reference_t<TaskBuilderRefFilterT<RefQualifier::kNone, F>>>>::kValid &&
        (sizeof...(BoundArgs) > 0 || !IsTask<std::remove_cvref_t<F>>::value) &&
        TaskTypeDeductor<DeductorSettings<void>{}, TaskBuilderRefFilterT<RefQualifier::kNone, F>,
                         TaskBuilderRefFilterT<RefQualifier::kNone, BoundArgs>...>::kIsValid)
Task(F&&, BoundArgs&&...) -> Task<
    typename TaskTypeDeductor<DeductorSettings<void>{},
                              TaskBuilderRefFilterT<RefQualifier::kNone, F>,
                              TaskBuilderRefFilterT<RefQualifier::kNone, BoundArgs>...>::Signature,
    TaskTypeDeductor<DeductorSettings<void>{}, TaskBuilderRefFilterT<RefQualifier::kNone, F>,
                     TaskBuilderRefFilterT<RefQualifier::kNone, BoundArgs>...>::kCopyable,
    TaskTypeDeductor<DeductorSettings<void>{}, TaskBuilderRefFilterT<RefQualifier::kNone, F>,
                     TaskBuilderRefFilterT<RefQualifier::kNone, BoundArgs>...>::kPayloadSize,
    TaskTypeDeductor<DeductorSettings<void>{}, TaskBuilderRefFilterT<RefQualifier::kNone, F>,
                     TaskBuilderRefFilterT<RefQualifier::kNone, BoundArgs>...>::kTriviality,
    false, HeapPolicy::kDisallow,
    TaskTypeDeductor<DeductorSettings<void>{}, TaskBuilderRefFilterT<RefQualifier::kNone, F>,
                     TaskBuilderRefFilterT<RefQualifier::kNone, BoundArgs>...>::kAlignment>;

template <typename F, typename... BoundArgs>
    requires(
        !IsFunctionTag<F>::value &&
        FunctionTraits<std::remove_reference_t<
            std::unwrap_reference_t<TaskBuilderRefFilterT<RefQualifier::kNonConst, F>>>>::kValid &&
        TaskTypeDeductor<DeductorSettings<void>{},
                         TaskBuilderRefFilterT<RefQualifier::kNonConst, F>,
                         TaskBuilderRefFilterT<RefQualifier::kNonConst, BoundArgs>...>::kIsValid)
Task(RefTag, F&&, BoundArgs&&...) -> Task<
    typename TaskTypeDeductor<DeductorSettings<void>{},
                              TaskBuilderRefFilterT<RefQualifier::kNonConst, F>,
                              BoundArgs...>::Signature,
    TaskTypeDeductor<DeductorSettings<void>{}, TaskBuilderRefFilterT<RefQualifier::kNonConst, F>,
                     TaskBuilderRefFilterT<RefQualifier::kNonConst, BoundArgs>...>::kCopyable,
    TaskTypeDeductor<DeductorSettings<void>{}, TaskBuilderRefFilterT<RefQualifier::kNonConst, F>,
                     TaskBuilderRefFilterT<RefQualifier::kNonConst, BoundArgs>...>::kPayloadSize,
    TaskTypeDeductor<DeductorSettings<void>{}, TaskBuilderRefFilterT<RefQualifier::kNonConst, F>,
                     TaskBuilderRefFilterT<RefQualifier::kNonConst, BoundArgs>...>::kTriviality,
    false, HeapPolicy::kDisallow,
    TaskTypeDeductor<DeductorSettings<void>{}, TaskBuilderRefFilterT<RefQualifier::kNonConst, F>,
                     TaskBuilderRefFilterT<RefQualifier::kNonConst, BoundArgs>...>::kAlignment>;

template <auto settings = DeductorSettings<void>{}, typename F, typename... BoundArgs>
    requires(IsDeductorSettings<settings>::value &&
             (!IsTask<TaskBuilderRefFilterT<RefQualifier::kNone, F>>::value ||
              sizeof...(BoundArgs) > 0))
FORCE_INLINE constexpr auto buildTask(F&& f, BoundArgs&&... args) {
    return TaskBuilder<settings, F, BoundArgs...>{std::forward<F>(f),
                                                  std::forward<BoundArgs>(args)...};
}

template <auto settings = DeductorSettings<void>{}, typename F, typename... BoundArgs>
    requires(IsDeductorSettings<settings>::value && IsTask<std::decay_t<F>>::value)
FORCE_INLINE constexpr auto buildTask(F&& f) {
    using TaskSrc = std::decay_t<F>;
    static constexpr auto kSettings =
        DeductorSettings<typename TaskSrc::Signature>{settings.binding,
                                                      settings.cb.value_or(TaskSrc::kCopyability),
                                                      settings.size.value_or(TaskSrc::kSize),
                                                      settings.t.value_or(TaskSrc::kTriviality),
                                                      settings.hp.value_or(TaskSrc::kHeapPolicy),
                                                      settings.alg.value_or(TaskSrc::kAlignment),
                                                      settings.ref};
    return TaskBuilder<kSettings, F, BoundArgs...>{std::forward<F>(f)};
}

template <auto F, auto settings = DeductorSettings<void>{}, typename... BoundArgs>
FORCE_INLINE constexpr auto buildTask(BoundArgs&&... args) {
    return TaskBuilder<settings, FunctionTag<F>, BoundArgs...>{std::forward<BoundArgs>(args)...};
}

template <auto settings = DeductorSettings<void>{}, typename F, typename... BoundArgs>
    requires IsDeductorSettings<settings>::value
FORCE_INLINE constexpr auto makeTask(F&& f, BoundArgs&&... args) {
    return buildTask<settings>(std::forward<F>(f), std::forward<BoundArgs>(args)...).build();
}

template <auto F, auto settings = DeductorSettings<void>{}, typename... BoundArgs>
FORCE_INLINE constexpr auto makeTask(BoundArgs&&... args) {
    return buildTask<F, settings>(std::forward<BoundArgs>(args)...).build();
}

template <typename Signature, typename F, typename... BoundArgs>
FORCE_INLINE constexpr auto makeTask(F&& f, BoundArgs&&... args) {
    return buildTask(std::forward<F>(f), std::forward<BoundArgs>(args)...)
        .template withSignature<Signature>()
        .build();
}

template <typename Signature, auto F, typename... BoundArgs>
FORCE_INLINE constexpr auto makeTask(BoundArgs&&... args) {
    return buildTask<F>(std::forward<BoundArgs>(args)...)
        .template withSignature<Signature>()
        .build();
}

FORCE_INLINE auto bindFrontTask(auto&& f, auto&&... args)
    requires(sizeof...(args) > 0 || !IsNamedTask<std::decay_t<decltype(f)>>::value)
{
    return buildTask(std::forward<decltype(f)>(f), std::forward<decltype(args)>(args)...)
        .bindFront()
        .build();
}

FORCE_INLINE auto bindBackTask(auto&& f, auto&&... args)
    requires(sizeof...(args) > 0 || !IsNamedTask<std::decay_t<decltype(f)>>::value)
{
    return buildTask(std::forward<decltype(f)>(f), std::forward<decltype(args)>(args)...)
        .bindBack()
        .build();
}

template <typename Sig>
FORCE_INLINE auto bindFrontTask(auto&& f, auto&&... args)
    requires(sizeof...(args) > 0 || !IsNamedTask<std::decay_t<decltype(f)>>::value)
{
    return buildTask(std::forward<decltype(f)>(f), std::forward<decltype(args)>(args)...)
        .bindFront()
        .template withSignature<Sig>()
        .build();
}

template <typename Sig>
FORCE_INLINE auto bindBackTask(auto&& f, auto&&... args)
    requires(sizeof...(args) > 0 || !IsNamedTask<std::decay_t<decltype(f)>>::value)
{
    return buildTask(std::forward<decltype(f)>(f), std::forward<decltype(args)>(args)...)
        .bindBack()
        .template withSignature<Sig>()
        .build();
}

template <auto F>
FORCE_INLINE auto bindFrontTask(auto&&... args) {
    return buildTask<F>(std::forward<decltype(args)>(args)...).bindFront().build();
}

template <auto F>
FORCE_INLINE auto bindBackTask(auto&&... args) {
    return buildTask<F>(std::forward<decltype(args)>(args)...).bindBack().build();
}

template <typename Sig, auto F>
FORCE_INLINE auto bindFrontTask(auto&&... args) {
    return buildTask<F>(std::forward<decltype(args)>(args)...)
        .bindFront()
        .template withSignature<Sig>()
        .build();
}

template <typename Sig, auto F>
FORCE_INLINE auto bindBackTask(auto&&... args) {
    return buildTask<F>(std::forward<decltype(args)>(args)...)
        .bindBack()
        .template withSignature<Sig>()
        .build();
}

template <auto settings = DeductorSettings<void>{}, typename F, typename... BoundArgs>
    requires IsDeductorSettings<settings>::value
FORCE_INLINE constexpr auto makeFlexTask(F&& f, BoundArgs&&... args) {
    return buildTask<settings>(std::forward<F>(f), std::forward<BoundArgs>(args)...)
        .allowHeap()
        .build();
}

template <typename Sig, typename F, typename... BoundArgs>
FORCE_INLINE constexpr auto makeFlexTask(F&& f, BoundArgs&&... args) {
    return buildTask<DeductorSettings<Sig>{}>(std::forward<F>(f),
                                              std::forward<decltype(args)>(args)...)
        .allowHeap()
        .build();
}

template <auto F, auto settings = DeductorSettings<void>{}, typename... BoundArgs>
FORCE_INLINE constexpr auto makeFlexTask(BoundArgs&&... args) {
    return buildTask<F, settings>(std::forward<decltype(args)>(args)...).allowHeap().build();
}

template <typename Sig, auto F, typename... BoundArgs>
FORCE_INLINE constexpr auto makeFlexTask(BoundArgs&&... args) {
    return buildTask<F, DeductorSettings<Sig>{}>(std::forward<decltype(args)>(args)...)
        .allowHeap()
        .build();
}

template <auto settings = DeductorSettings<void>{}, typename F, typename... BoundArgs>
    requires IsDeductorSettings<settings>::value
FORCE_INLINE constexpr auto makeRefTask(F&& f, BoundArgs&&... args) {
    return buildTask<settings>(std::forward<F>(f), std::forward<BoundArgs>(args)...)
        .byRef()
        .build();
}

template <auto F, auto settings = DeductorSettings<void>{}, typename... BoundArgs>
    requires IsDeductorSettings<settings>::value
FORCE_INLINE constexpr auto makeRefTask(BoundArgs&&... args) {
    return buildTask<F, settings>(std::forward<BoundArgs>(args)...).byRef().build();
}

}  // namespace fn

#endif  // FN_TASK_HPP