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
#include <type_traits>
#include <utility>

#include <task/deductors.hpp>
#include <task/function_traits.hpp>
#include <task/payload.hpp>

namespace fn {

template <typename... T>
struct AlwaysFalse : std::false_type {};

enum class Op : std::uint8_t { kDestroy, kClone, kMove, kAssign, kMoveAssign };

template <Alignment alignment, InlineSize size, Triviality t, bool Extended = false,
          typename Signature = void()>
struct Storage;

template <typename Signature, typename Task = int, InlineSize size = 0>
struct InvokerBuilder;

template <typename Task, InlineSize size, typename Ret, typename... Args>
struct InvokerBuilder<Ret(Args...) &, Task, size> {
    using Type = Ret (*)(void*, Args...);
    using TypeNoBuffer = Ret (*)(Args...);

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size == 0)
    FORCE_INLINE static constexpr TypeNoBuffer getInvoker() {
        static_assert(sizeof...(BoundArgs) == 0);
        return [](Args... args) -> Ret { return F(std::forward<Args>(args)...); };
    }

    template <typename PayloadType, typename F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) -> Ret {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<F&, Args...>,
                              "F is not callable with the provided arguments!");
                return (*static_cast<PayloadType*>(s)).invoke(std::forward<Args>(args)...);
            } else if constexpr (std::is_invocable_v<F&, Args...,
                                                     std::unwrap_ref_decay_t<BoundArgs>&...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return (*static_cast<PayloadType*>(s)).invoke(std::forward<Args>(args)...);
                } else {
                    return (**static_cast<PayloadType**>(s)).invoke(std::forward<Args>(args)...);
                }
            } else if constexpr (std::is_invocable_v<F&, std::unwrap_ref_decay_t<BoundArgs>&...,
                                                     Args...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return (*static_cast<PayloadType*>(s)).invokeFront(std::forward<Args>(args)...);
                } else {
                    return (**static_cast<PayloadType**>(s))
                        .invokeFront(std::forward<Args>(args)...);
                }
            } else if constexpr (is_compatible_invocable_v<
                                     F, std::unwrap_ref_decay_t<BoundArgs>&&..., Args...> ||
                                 is_compatible_invocable_v<
                                     F, Args..., std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                static_assert(AlwaysFalse<F>::value,
                              "F is not callable with the provided arguments since it wouldn't "
                              "preserve signature qualifier");
            } else {
                static_assert(AlwaysFalse<F>::value,
                              "F is not callable with the provided arguments.");
            }
        };
    }

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<decltype(F), Args...>,
                              "F is not callable with the provided arguments!");
                return F(std::forward<Args>(args)...);
            } else if constexpr (std::is_invocable_v<decltype(F), Args...,
                                                     std::unwrap_ref_decay_t<BoundArgs>&...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return (*static_cast<PayloadType*>(s)).invoke(std::forward<Args>(args)...);
                } else {
                    return (**static_cast<PayloadType**>(s)).invoke(std::forward<Args>(args)...);
                }
            } else if constexpr (std::is_invocable_v<decltype(F),
                                                     std::unwrap_ref_decay_t<BoundArgs>&...,
                                                     Args...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return (*static_cast<PayloadType*>(s)).invokeFront(std::forward<Args>(args)...);
                } else {
                    return (**static_cast<PayloadType**>(s))
                        .invokeFront(std::forward<Args>(args)...);
                }
            } else if constexpr (is_compatible_invocable_v<decltype(F),
                                                           std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                           Args...> ||
                                 is_compatible_invocable_v<
                                     decltype(F), Args...,
                                     std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                static_assert(AlwaysFalse<decltype(F)>::value,
                              "F is not callable with the provided arguments since it wouldn't "
                              "preserve signature qualifier");

            } else {
                static_assert(AlwaysFalse<decltype(F)>::value,
                              "F is not callable with the provided arguments!");
            }
        };
    }

    FORCE_INLINE Ret operator()(this auto&& self, Args... args) {
        if constexpr (size == 0) {
            return (static_cast<Task>(self)).storage_.invoker(std::forward<Args>(args)...);
        } else {
            return (static_cast<Task>(self))
                .storage_.invoker(
                    static_cast<void*>((static_cast<Task>(self)).storage_.data.data()),
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

template <typename Task, InlineSize size, typename Ret, typename... Args>
struct InvokerBuilder<Ret(Args...) & noexcept, Task, size> {
    using Type = Ret (*)(void*, Args...) noexcept;
    using TypeNoBuffer = Ret (*)(Args...) noexcept;

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size == 0)
    FORCE_INLINE static constexpr TypeNoBuffer getInvoker() {
        static_assert(sizeof...(BoundArgs) == 0);
        return [](Args... args) noexcept -> Ret { return F(std::forward<Args>(args)...); };
    }

    template <typename PayloadType, typename F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) noexcept {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<F&, Args...>,
                              "F is not callable with the provided arguments!");
                return (*static_cast<PayloadType*>(s)).invoke(std::forward<Args>(args)...);
            } else if constexpr (std::is_invocable_v<F&, Args...,
                                                     std::unwrap_ref_decay_t<BoundArgs>&...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return (*static_cast<PayloadType*>(s)).invoke(std::forward<Args>(args)...);
                } else {
                    return (**static_cast<PayloadType**>(s)).invoke(std::forward<Args>(args)...);
                }
            } else if constexpr (std::is_invocable_v<F&, std::unwrap_ref_decay_t<BoundArgs>&...,
                                                     Args...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return (*static_cast<PayloadType*>(s)).invokeFront(std::forward<Args>(args)...);
                } else {
                    return (**static_cast<PayloadType**>(s))
                        .invokeFront(std::forward<Args>(args)...);
                }
            } else if constexpr (is_compatible_invocable_v<
                                     F, std::unwrap_ref_decay_t<BoundArgs>&&..., Args...> ||
                                 is_compatible_invocable_v<
                                     F, Args..., std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                static_assert(AlwaysFalse<F>::value,
                              "F is not callable with the provided arguments since it wouldn't "
                              "preserve signature qualifier");
            } else {
                static_assert(AlwaysFalse<F>::value,
                              "F is not callable with the provided arguments!");
            }
        };
    }

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) noexcept {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<decltype(F), Args...>,
                              "F is not callable with the provided arguments!");
                return F(std::forward<Args>(args)...);
            } else if constexpr (std::is_invocable_v<decltype(F), Args...,
                                                     std::unwrap_ref_decay_t<BoundArgs>&...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return (*static_cast<PayloadType*>(s)).invoke(std::forward<Args>(args)...);
                } else {
                    return (**static_cast<PayloadType**>(s)).invoke(std::forward<Args>(args)...);
                }
            } else if constexpr (std::is_invocable_v<decltype(F),
                                                     std::unwrap_ref_decay_t<BoundArgs>&...,
                                                     Args...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return (*static_cast<PayloadType*>(s)).invokeFront(std::forward<Args>(args)...);
                } else {
                    return (**static_cast<PayloadType**>(s))
                        .invokeFront(std::forward<Args>(args)...);
                }
            } else if constexpr (is_compatible_invocable_v<decltype(F),
                                                           std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                           Args...> ||
                                 is_compatible_invocable_v<
                                     decltype(F), Args...,
                                     std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                static_assert(AlwaysFalse<decltype(F)>::value,
                              "F is not callable with the provided arguments since it wouldn't "
                              "preserve signature qualifier");
            } else {
                static_assert(AlwaysFalse<decltype(F)>::value,
                              "F is not callable with the provided arguments!");
            }
        };
    }

    FORCE_INLINE Ret operator()(this auto&& self, Args... args) noexcept {
        if constexpr (size == 0) {
            return (static_cast<Task>(self)).storage_.invoker(std::forward<Args>(args)...);
        } else {
            return (static_cast<Task>(self))
                .storage_.invoker(
                    static_cast<void*>((static_cast<Task>(self)).storage_.data.data()),
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

template <typename Task, InlineSize size, typename Ret, typename... Args>
struct InvokerBuilder<Ret(Args...) const&, Task, size> {
    using Type = Ret (*)(void*, Args...);
    using TypeNoBuffer = Ret (*)(Args...);

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size == 0)
    FORCE_INLINE static constexpr TypeNoBuffer getInvoker() {
        static_assert(sizeof...(BoundArgs) == 0);
        return [](Args... args) -> Ret { return F(std::forward<Args>(args)...); };
    }

    template <typename PayloadType, typename F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) -> Ret {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<const F&, Args...>,
                              "F is not callable with the provided arguments!");
                return static_cast<const PayloadType*>(s)->invoke(std::forward<Args>(args)...);
            } else {
                static_assert(PayloadNoRvalueRef<PayloadType>::value);
                if constexpr (std::is_invocable_v<
                                  const F&, Args...,
                                  const std::unwrap_ref_decay_t<
                                      typename RefToConstRef<BoundArgs>::Type>&...>) {
                    if constexpr (sizeof(PayloadType) <= size) {
                        return static_cast<const PayloadType*>(s)->invoke(
                            std::forward<Args>(args)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invoke(std::forward<Args>(args)...);
                    }
                } else if constexpr (std::is_invocable_v<
                                         const F&,
                                         const std::unwrap_ref_decay_t<
                                             typename RefToConstRef<BoundArgs>::Type>&...,
                                         Args...>) {
                    if constexpr (sizeof(PayloadType) <= size) {
                        return static_cast<const PayloadType*>(s)->invokeFront(
                            std::forward<Args>(args)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invokeFront(std::forward<Args>(args)...);
                    }
                } else if constexpr (is_compatible_invocable_v<
                                         F, std::unwrap_ref_decay_t<BoundArgs>&&..., Args...> ||
                                     is_compatible_invocable_v<
                                         F, Args..., std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                    static_assert(AlwaysFalse<F>::value,
                                  "F is not callable with the provided arguments since it wouldn't "
                                  "preserve signature qualifier");
                } else {
                    static_assert(AlwaysFalse<F>::value,
                                  "F is not callable with the provided arguments!");
                }
            }
        };
    }

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<decltype(F), Args...>,
                              "F is not callable with the provided arguments!");
                return F(std::forward<Args>(args)...);
            } else {
                static_assert(PayloadNoRvalueRef<PayloadType>::value);
                if constexpr (std::is_invocable_v<
                                  decltype(F), Args...,
                                  const std::unwrap_ref_decay_t<
                                      typename RefToConstRef<BoundArgs>::Type>&...>) {
                    if constexpr (sizeof(PayloadType) <= size) {
                        return static_cast<const PayloadType*>(s)->invoke(
                            std::forward<Args>(args)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invoke(std::forward<Args>(args)...);
                    }
                } else if constexpr (std::is_invocable_v<
                                         decltype(F),
                                         const std::unwrap_ref_decay_t<
                                             typename RefToConstRef<BoundArgs>::Type>&...,
                                         Args...>) {
                    if constexpr (sizeof(PayloadType) <= size) {
                        return static_cast<const PayloadType*>(s)->invokeFront(
                            std::forward<Args>(args)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invokeFront(std::forward<Args>(args)...);
                    }
                } else if constexpr (is_compatible_invocable_v<
                                         decltype(F), std::unwrap_ref_decay_t<BoundArgs>&&...,
                                         Args...> ||
                                     is_compatible_invocable_v<
                                         decltype(F), Args...,
                                         std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                    static_assert(AlwaysFalse<decltype(F)>::value,
                                  "F is not callable with the provided arguments since it wouldn't "
                                  "preserve signature qualifier");
                } else {
                    static_assert(AlwaysFalse<decltype(F)>::value,
                                  "F is not callable with the provided arguments!");
                }
            }
        };
    }

    FORCE_INLINE Ret operator()(this auto&& self, Args... args) {
        if constexpr (size == 0) {
            return (static_cast<Task>(self)).storage_.invoker(std::forward<Args>(args)...);
        } else {
            return (static_cast<Task>(self))
                .storage_.invoker(
                    static_cast<void*>((static_cast<Task>(self)).storage_.data.data()),
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

template <typename Task, InlineSize size, typename Ret, typename... Args>
struct InvokerBuilder<Ret(Args...) const & noexcept, Task, size> {
    using Type = Ret (*)(void*, Args...) noexcept;
    using TypeNoBuffer = Ret (*)(Args...) noexcept;

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size == 0)
    FORCE_INLINE static constexpr TypeNoBuffer getInvoker() {
        static_assert(sizeof...(BoundArgs) == 0);
        return [](Args... args) noexcept -> Ret { return F(std::forward<Args>(args)...); };
    }

    template <typename PayloadType, typename F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) noexcept -> Ret {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<const F&, Args...>,
                              "F is not callable with the provided arguments!");
                return static_cast<const PayloadType*>(s)->invoke(std::forward<Args>(args)...);
            } else {
                static_assert(PayloadNoRvalueRef<PayloadType>::value);
                if constexpr (std::is_invocable_v<
                                  const F&, Args...,
                                  const std::unwrap_ref_decay_t<
                                      typename RefToConstRef<BoundArgs>::Type>&...>) {
                    if constexpr (sizeof(PayloadType) <= size) {
                        return static_cast<const PayloadType*>(s)->invoke(
                            std::forward<Args>(args)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invoke(std::forward<Args>(args)...);
                    }
                } else if constexpr (std::is_invocable_v<
                                         const F&,
                                         const std::unwrap_ref_decay_t<
                                             typename RefToConstRef<BoundArgs>::Type>&...,
                                         Args...>) {
                    if constexpr (sizeof(PayloadType) <= size) {
                        return static_cast<const PayloadType*>(s)->invokeFront(
                            std::forward<Args>(args)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invokeFront(std::forward<Args>(args)...);
                    }
                } else if constexpr (is_compatible_invocable_v<
                                         F, std::unwrap_ref_decay_t<BoundArgs>&&..., Args...> ||
                                     is_compatible_invocable_v<
                                         F, Args..., std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                    static_assert(AlwaysFalse<F>::value,
                                  "F is not callable with the provided arguments since it wouldn't "
                                  "preserve signature qualifier");
                } else {
                    static_assert(AlwaysFalse<F>::value,
                                  "F is not callable with the provided arguments!");
                }
            }
        };
    }

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) noexcept {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<decltype(F), Args...>,
                              "F is not callable with the provided arguments!");
                return F(std::forward<Args>(args)...);
            } else {
                static_assert(PayloadNoRvalueRef<PayloadType>::value);
                if constexpr (std::is_invocable_v<
                                  decltype(F), Args...,
                                  const std::unwrap_ref_decay_t<
                                      typename RefToConstRef<BoundArgs>::Type>&...>) {
                    if constexpr (sizeof(PayloadType) <= size) {
                        return static_cast<const PayloadType*>(s)->invoke(
                            std::forward<Args>(args)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invoke(std::forward<Args>(args)...);
                    }
                } else if constexpr (std::is_invocable_v<
                                         decltype(F),
                                         const std::unwrap_ref_decay_t<
                                             typename RefToConstRef<BoundArgs>::Type>&...,
                                         Args...>) {
                    if constexpr (sizeof(PayloadType) <= size) {
                        return static_cast<const PayloadType*>(s)->invokeFront(
                            std::forward<Args>(args)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invokeFront(std::forward<Args>(args)...);
                    }
                } else if constexpr (is_compatible_invocable_v<
                                         decltype(F), std::unwrap_ref_decay_t<BoundArgs>&&...,
                                         Args...> ||
                                     is_compatible_invocable_v<
                                         decltype(F), Args...,
                                         std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                    static_assert(AlwaysFalse<decltype(F)>::value,
                                  "F is not callable with the provided arguments since it wouldn't "
                                  "preserve signature qualifier");
                } else {
                    static_assert(AlwaysFalse<decltype(F)>::value,
                                  "F is not callable with the provided arguments!");
                }
            }
        };
    }
    FORCE_INLINE Ret operator()(this auto&& self, Args... args) noexcept {
        if constexpr (size == 0) {
            return (static_cast<Task>(self)).storage_.invoker(std::forward<Args>(args)...);
        } else {
            return (static_cast<Task>(self))
                .storage_.invoker(
                    static_cast<void*>((static_cast<Task>(self)).storage_.data.data()),
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

template <typename Task, InlineSize size, typename Ret, typename... Args>
struct InvokerBuilder<Ret(Args...) const, Task, size> {
    using Type = Ret (*)(void*, Args...);
    using TypeNoBuffer = Ret (*)(Args...);

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size == 0)
    FORCE_INLINE static constexpr TypeNoBuffer getInvoker() {
        static_assert(sizeof...(BoundArgs) == 0);
        return [](Args... args) -> Ret { return F(std::forward<Args>(args)...); };
    }

    template <typename PayloadType, typename F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) -> Ret {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<const F, Args...>,
                              "F is not callable with the provided arguments!");
                return static_cast<const PayloadType*>(s)->invoke(std::forward<Args>(args)...);
            } else {
                static_assert(PayloadNoRvalueRef<PayloadType>::value);
                if constexpr (std::is_invocable_v<
                                  const F, Args...,
                                  const std::unwrap_ref_decay_t<
                                      typename RefToConstRef<BoundArgs>::Type>...>) {
                    if constexpr (sizeof(PayloadType) <= size) {
                        return static_cast<const PayloadType*>(s)->invoke(
                            std::forward<Args>(args)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invoke(std::forward<Args>(args)...);
                    }
                } else if constexpr (std::is_invocable_v<
                                         const F,
                                         const std::unwrap_ref_decay_t<
                                             typename RefToConstRef<BoundArgs>::Type>...,
                                         Args...>) {
                    if constexpr (sizeof(PayloadType) <= size) {
                        return static_cast<const PayloadType*>(s)->invokeFront(
                            std::forward<Args>(args)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invokeFront(std::forward<Args>(args)...);
                    }
                } else if constexpr (is_compatible_invocable_v<
                                         F, std::unwrap_ref_decay_t<BoundArgs>&&..., Args...> ||
                                     is_compatible_invocable_v<
                                         F, Args..., std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                    static_assert(AlwaysFalse<F>::value,
                                  "F is not callable with the provided arguments since it wouldn't "
                                  "preserve signature qualifier");
                } else {
                    static_assert(AlwaysFalse<F>::value,
                                  "F is not callable with the provided arguments!");
                }
            }
        };
    }

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<decltype(F), Args...>,
                              "F is not callable with the provided arguments!");
                return F(std::forward<Args>(args)...);
            } else {
                static_assert(PayloadNoRvalueRef<PayloadType>::value);
                if constexpr (std::is_invocable_v<
                                  decltype(F), Args...,
                                  const std::unwrap_ref_decay_t<
                                      typename RefToConstRef<BoundArgs>::Type>...>) {
                    if constexpr (sizeof(PayloadType) <= size) {
                        return static_cast<const PayloadType*>(s)->invoke(
                            std::forward<Args>(args)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invoke(std::forward<Args>(args)...);
                    }
                } else if constexpr (std::is_invocable_v<
                                         decltype(F),
                                         const std::unwrap_ref_decay_t<
                                             typename RefToConstRef<BoundArgs>::Type>...,
                                         Args...>) {
                    if constexpr (sizeof(PayloadType) <= size) {
                        return static_cast<const PayloadType*>(s)->invokeFront(
                            std::forward<Args>(args)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invokeFront(std::forward<Args>(args)...);
                    }
                } else if constexpr (is_compatible_invocable_v<
                                         decltype(F), std::unwrap_ref_decay_t<BoundArgs>&&...,
                                         Args...> ||
                                     is_compatible_invocable_v<
                                         decltype(F), Args...,
                                         std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                    static_assert(AlwaysFalse<decltype(F)>::value,
                                  "F is not callable with the provided arguments since it wouldn't "
                                  "preserve signature qualifier");
                } else {
                    static_assert(AlwaysFalse<decltype(F)>::value,
                                  "F is not callable with the provided arguments!");
                }
            }
        };
    }

    FORCE_INLINE Ret operator()(this auto&& self, Args... args) {
        if constexpr (size == 0) {
            return (static_cast<Task>(self)).storage_.invoker(std::forward<Args>(args)...);
        } else {
            return (static_cast<Task>(self))
                .storage_.invoker(
                    static_cast<void*>((static_cast<Task>(self)).storage_.data.data()),
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

template <typename Task, InlineSize size, typename Ret, typename... Args>
struct InvokerBuilder<Ret(Args...) const noexcept, Task, size> {
    using Type = Ret (*)(void*, Args...) noexcept;
    using TypeNoBuffer = Ret (*)(Args...) noexcept;

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size == 0)
    FORCE_INLINE static constexpr TypeNoBuffer getInvoker() {
        static_assert(sizeof...(BoundArgs) == 0);
        return [](Args... args) noexcept -> Ret { return F(std::forward<Args>(args)...); };
    }

    template <typename PayloadType, typename F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) noexcept -> Ret {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<const F, Args...>,
                              "F is not callable with the provided arguments!");
                return static_cast<const PayloadType*>(s)->invoke(std::forward<Args>(args)...);
            } else {
                static_assert(PayloadNoRvalueRef<PayloadType>::value);
                if constexpr (std::is_invocable_v<
                                  const F, Args...,
                                  const std::unwrap_ref_decay_t<
                                      typename RefToConstRef<BoundArgs>::Type>...>) {
                    if constexpr (sizeof(PayloadType) <= size) {
                        return static_cast<const PayloadType*>(s)->invoke(
                            std::forward<Args>(args)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invoke(std::forward<Args>(args)...);
                    }
                } else if constexpr (std::is_invocable_v<
                                         const F,
                                         const std::unwrap_ref_decay_t<
                                             typename RefToConstRef<BoundArgs>::Type>...,
                                         Args...>) {
                    if constexpr (sizeof(PayloadType) <= size) {
                        return static_cast<const PayloadType*>(s)->invokeFront(
                            std::forward<Args>(args)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invokeFront(std::forward<Args>(args)...);
                    }
                } else if constexpr (is_compatible_invocable_v<
                                         F, std::unwrap_ref_decay_t<BoundArgs>&&..., Args...> ||
                                     is_compatible_invocable_v<
                                         F, Args..., std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                    static_assert(AlwaysFalse<F>::value,
                                  "F is not callable with the provided arguments since it wouldn't "
                                  "preserve signature qualifier");
                } else {
                    static_assert(AlwaysFalse<F>::value,
                                  "F is not callable with the provided arguments!");
                }
            }
        };
    }

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) noexcept {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<decltype(F), Args...>,
                              "F is not callable with the provided arguments!");
                return F(std::forward<Args>(args)...);
            } else {
                static_assert(PayloadNoRvalueRef<PayloadType>::value);
                if constexpr (std::is_invocable_v<
                                  decltype(F), Args...,
                                  const std::unwrap_ref_decay_t<
                                      typename RefToConstRef<BoundArgs>::Type>...>) {
                    if constexpr (sizeof(PayloadType) <= size) {
                        return static_cast<const PayloadType*>(s)->invoke(
                            std::forward<Args>(args)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invoke(std::forward<Args>(args)...);
                    }
                } else if constexpr (std::is_invocable_v<
                                         decltype(F),
                                         const std::unwrap_ref_decay_t<
                                             typename RefToConstRef<BoundArgs>::Type>...,
                                         Args...>) {
                    if constexpr (sizeof(PayloadType) <= size) {
                        return static_cast<const PayloadType*>(s)->invokeFront(
                            std::forward<Args>(args)...);
                    } else {
                        return (*static_cast<const PayloadType**>(s))
                            ->invokeFront(std::forward<Args>(args)...);
                    }
                } else if constexpr (is_compatible_invocable_v<
                                         decltype(F), std::unwrap_ref_decay_t<BoundArgs>&&...,
                                         Args...> ||
                                     is_compatible_invocable_v<
                                         decltype(F), Args...,
                                         std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                    static_assert(AlwaysFalse<decltype(F)>::value,
                                  "F is not callable with the provided arguments since it wouldn't "
                                  "preserve signature qualifier");
                } else {
                    static_assert(AlwaysFalse<decltype(F)>::value,
                                  "F is not callable with the provided arguments!");
                }
            }
        };
    }
    FORCE_INLINE Ret operator()(this auto&& self, Args... args) noexcept {
        if constexpr (size == 0) {
            return (static_cast<Task>(self)).storage_.invoker(std::forward<Args>(args)...);
        } else {
            return (static_cast<Task>(self))
                .storage_.invoker(
                    static_cast<void*>((static_cast<Task>(self)).storage_.data.data()),
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

template <typename Task, InlineSize size, typename Ret, typename... Args>
struct InvokerBuilder<Ret(Args...) &&, Task, size> {
    using Type = Ret (*)(void*, Args...);
    using TypeNoBuffer = Ret (*)(Args...);

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size == 0)
    FORCE_INLINE static constexpr TypeNoBuffer getInvoker() {
        static_assert(sizeof...(BoundArgs) == 0);
        return [](Args... args) -> Ret { return F(std::forward<Args>(args)...); };
    }

    template <typename PayloadType, typename F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) -> Ret {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<F&&, Args...>,
                              "F is not callable with the provided arguments!");
                return std::move(*static_cast<PayloadType*>(s)).invoke(std::forward<Args>(args)...);
            } else if constexpr (std::is_invocable_v<F&&, Args...,
                                                     std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<PayloadType*>(s))
                        .invoke(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<PayloadType**>(s))
                        .invoke(std::forward<Args>(args)...);
                }
            } else if constexpr (std::is_invocable_v<F&&, std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                     Args...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<PayloadType*>(s))
                        .invokeFront(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<PayloadType**>(s))
                        .invokeFront(std::forward<Args>(args)...);
                }
            } else if constexpr (is_compatible_invocable_v<
                                     F, std::unwrap_ref_decay_t<BoundArgs>&&..., Args...> ||
                                 is_compatible_invocable_v<
                                     F, Args..., std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                static_assert(AlwaysFalse<F>::value,
                              "F is not callable with the provided arguments since it wouldn't "
                              "preserve signature qualifier");
            } else {
                static_assert(AlwaysFalse<F>::value,
                              "F is not callable with the provided arguments!");
            }
        };
    }

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<decltype(F), Args...>,
                              "F is not callable with the provided arguments!");
                return F(std::forward<Args>(args)...);
            } else if constexpr (std::is_invocable_v<decltype(F), Args...,
                                                     std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<PayloadType*>(s))
                        .invoke(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<PayloadType**>(s))
                        .invoke(std::forward<Args>(args)...);
                }
            } else if constexpr (std::is_invocable_v<decltype(F),
                                                     std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                     Args...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<PayloadType*>(s))
                        .invokeFront(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<PayloadType**>(s))
                        .invokeFront(std::forward<Args>(args)...);
                }
            } else if constexpr (is_compatible_invocable_v<decltype(F),
                                                           std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                           Args...> ||
                                 is_compatible_invocable_v<
                                     decltype(F), Args...,
                                     std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                static_assert(AlwaysFalse<decltype(F)>::value,
                              "F is not callable with the provided arguments since it wouldn't "
                              "preserve signature qualifier");
            } else {
                static_assert(AlwaysFalse<decltype(F)>::value,
                              "F is not callable with the provided arguments!");
            }
        };
    }

    FORCE_INLINE Ret operator()(this auto&& self, Args... args) {
        if constexpr (size == 0) {
            return (static_cast<Task>(self)).storage_.invoker(std::forward<Args>(args)...);
        } else {
            return (static_cast<Task>(self))
                .storage_.invoker(
                    static_cast<void*>((static_cast<Task>(self)).storage_.data.data()),
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

template <typename Task, InlineSize size, typename Ret, typename... Args>
struct InvokerBuilder<Ret(Args...) && noexcept, Task, size> {
    using Type = Ret (*)(void*, Args...) noexcept;
    using TypeNoBuffer = Ret (*)(Args...) noexcept;

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size == 0)
    FORCE_INLINE static constexpr TypeNoBuffer getInvoker() {
        static_assert(sizeof...(BoundArgs) == 0);
        return [](Args... args) noexcept -> Ret { return F(std::forward<Args>(args)...); };
    }

    template <typename PayloadType, typename F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) noexcept -> Ret {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<F&&, Args...>,
                              "F is not callable with the provided arguments!");
                return std::move(*static_cast<PayloadType*>(s)).invoke(std::forward<Args>(args)...);
            } else if constexpr (std::is_invocable_v<F&&, Args...,
                                                     std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                static_assert(std::is_nothrow_invocable_v<F&&, Args...,
                                                          std::unwrap_ref_decay_t<BoundArgs>&&...>,
                              "Callable must be noexcept given its signature");
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<PayloadType*>(s))
                        .invoke(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<PayloadType**>(s))
                        .invoke(std::forward<Args>(args)...);
                }
            } else if constexpr (std::is_invocable_v<F&&, std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                     Args...>) {
                static_assert(
                    std::is_nothrow_invocable_v<F&&, std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                Args...>,
                    "Callable must be noexcept given its signature");
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<PayloadType*>(s))
                        .invokeFront(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<PayloadType**>(s))
                        .invokeFront(std::forward<Args>(args)...);
                }
            } else if constexpr (is_compatible_invocable_v<
                                     F, std::unwrap_ref_decay_t<BoundArgs>&&..., Args...> ||
                                 is_compatible_invocable_v<
                                     F, Args..., std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                static_assert(AlwaysFalse<F>::value,
                              "F is not callable with the provided arguments since it wouldn't "
                              "preserve signature qualifier");
            } else {
                static_assert(AlwaysFalse<F>::value,
                              "F is not callable with the provided arguments!");
            }
        };
    }

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) noexcept {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<decltype(F), Args...>,
                              "F is not callable with the provided arguments!");
                return F(std::forward<Args>(args)...);
            } else if constexpr (std::is_invocable_v<decltype(F), Args...,
                                                     std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                static_assert(std::is_nothrow_invocable_v<decltype(F), Args...,
                                                          std::unwrap_ref_decay_t<BoundArgs>&&...>,
                              "Callable must be noexcept given its signature");
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<PayloadType*>(s))
                        .invoke(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<PayloadType**>(s))
                        .invoke(std::forward<Args>(args)...);
                }
            } else if constexpr (std::is_invocable_v<decltype(F),
                                                     std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                     Args...>) {
                static_assert(
                    std::is_nothrow_invocable_v<decltype(F),
                                                std::unwrap_ref_decay_t<BoundArgs>&&..., Args...>,
                    "Callable must be noexcept given its signature");
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<PayloadType*>(s))
                        .invokeFront(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<PayloadType**>(s))
                        .invokeFront(std::forward<Args>(args)...);
                }
            } else if constexpr (is_compatible_invocable_v<decltype(F),
                                                           std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                           Args...> ||
                                 is_compatible_invocable_v<
                                     decltype(F), Args...,
                                     std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                static_assert(AlwaysFalse<decltype(F)>::value,
                              "F is not callable with the provided arguments since it wouldn't "
                              "preserve signature qualifier");
            } else {
                static_assert(AlwaysFalse<decltype(F)>::value,
                              "F is not callable with the provided arguments!");
            }
        };
    }

    FORCE_INLINE Ret operator()(this auto&& self, Args... args) noexcept {
        if constexpr (size == 0) {
            return (static_cast<Task>(self)).storage_.invoker(std::forward<Args>(args)...);
        } else {
            return (static_cast<Task>(self))
                .storage_.invoker(
                    static_cast<void*>((static_cast<Task>(self)).storage_.data.data()),
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

template <typename Task, InlineSize size, typename Ret, typename... Args>
struct InvokerBuilder<Ret(Args...) const&&, Task, size> {
    using Type = Ret (*)(void*, Args...);
    using TypeNoBuffer = Ret (*)(Args...);

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size == 0)
    FORCE_INLINE static constexpr TypeNoBuffer getInvoker() {
        static_assert(sizeof...(BoundArgs) == 0);
        return [](Args... args) -> Ret { return F(std::forward<Args>(args)...); };
    }

    template <typename PayloadType, typename F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) -> Ret {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<const F&&, Args...>,
                              "F is not callable with the provided arguments!");
                return std::move(*static_cast<const PayloadType*>(s))
                    .invoke(std::forward<Args>(args)...);
            } else if constexpr (std::is_invocable_v<
                                     const F&&, Args...,
                                     const std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<const PayloadType*>(s))
                        .invoke(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<const PayloadType**>(s))
                        .invoke(std::forward<Args>(args)...);
                }
            } else if constexpr (std::is_invocable_v<const F&&,
                                                     const std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                     Args...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<const PayloadType*>(s))
                        .invokeFront(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<const PayloadType**>(s))
                        .invokeFront(std::forward<Args>(args)...);
                }
            } else if constexpr (is_compatible_invocable_v<
                                     F, std::unwrap_ref_decay_t<BoundArgs>&&..., Args...> ||
                                 is_compatible_invocable_v<
                                     F, Args..., std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                static_assert(AlwaysFalse<F>::value,
                              "F is not callable with the provided arguments since it wouldn't "
                              "preserve signature qualifier");
            } else {
                static_assert(AlwaysFalse<F>::value,
                              "F is not callable with the provided arguments!");
            }
        };
    }

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<decltype(F), Args...>,
                              "F is not callable with the provided arguments!");
                return F(std::forward<Args>(args)...);
            } else if constexpr (std::is_invocable_v<
                                     decltype(F), Args...,
                                     const std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<const PayloadType*>(s))
                        .invoke(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<const PayloadType**>(s))
                        .invoke(std::forward<Args>(args)...);
                }
            } else if constexpr (std::is_invocable_v<decltype(F),
                                                     const std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                     Args...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<const PayloadType*>(s))
                        .invokeFront(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<const PayloadType**>(s))
                        .invokeFront(std::forward<Args>(args)...);
                }
            } else if constexpr (is_compatible_invocable_v<decltype(F),
                                                           std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                           Args...> ||
                                 is_compatible_invocable_v<
                                     decltype(F), Args...,
                                     std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                static_assert(AlwaysFalse<decltype(F)>::value,
                              "F is not callable with the provided arguments since it wouldn't "
                              "preserve signature qualifier");
            } else {
                static_assert(AlwaysFalse<decltype(F)>::value,
                              "F is not callable with the provided arguments!");
            }
        };
    }

    FORCE_INLINE Ret operator()(this auto&& self, Args... args) {
        if constexpr (size == 0) {
            return (static_cast<Task>(self)).storage_.invoker(std::forward<Args>(args)...);
        } else {
            return (static_cast<Task>(self))
                .storage_.invoker(
                    static_cast<void*>((static_cast<Task>(self)).storage_.data.data()),
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

template <typename Task, InlineSize size, typename Ret, typename... Args>
struct InvokerBuilder<Ret(Args...) const && noexcept, Task, size> {
    using Type = Ret (*)(void*, Args...) noexcept;
    using TypeNoBuffer = Ret (*)(Args...) noexcept;

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size == 0)
    FORCE_INLINE static constexpr TypeNoBuffer getInvoker() {
        static_assert(sizeof...(BoundArgs) == 0);
        return [](Args... args) noexcept -> Ret { return F(std::forward<Args>(args)...); };
    }

    template <typename PayloadType, typename F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) noexcept -> Ret {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<const F&&, Args...>,
                              "F is not callable with the provided arguments!");
                return std::move(*static_cast<PayloadType*>(s)).invoke(std::forward<Args>(args)...);
            } else if constexpr (std::is_invocable_v<
                                     const F&&, Args...,
                                     const std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                static_assert(
                    std::is_nothrow_invocable_v<const F&&, Args...,
                                                const std::unwrap_ref_decay_t<BoundArgs>&&...>,
                    "Callable must be noexcept given its signature");
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<const PayloadType*>(s))
                        .invoke(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<const PayloadType**>(s))
                        .invoke(std::forward<Args>(args)...);
                }
            } else if constexpr (std::is_invocable_v<const F&&,
                                                     const std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                     Args...>) {
                static_assert(
                    std::is_nothrow_invocable_v<
                        const F&&, const std::unwrap_ref_decay_t<BoundArgs>&&..., Args...>,
                    "Callable must be noexcept given its signature");
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<const PayloadType*>(s))
                        .invokeFront(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<const PayloadType**>(s))
                        .invokeFront(std::forward<Args>(args)...);
                }
            } else if constexpr (is_compatible_invocable_v<
                                     F, std::unwrap_ref_decay_t<BoundArgs>&&..., Args...> ||
                                 is_compatible_invocable_v<
                                     F, Args..., std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                static_assert(AlwaysFalse<F>::value,
                              "F is not callable with the provided arguments since it wouldn't "
                              "preserve signature qualifier");
            } else {
                static_assert(AlwaysFalse<F>::value,
                              "F is not callable with the provided arguments!");
            }
        };
    }

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) noexcept {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<decltype(F), Args...>,
                              "F is not callable with the provided arguments!");
                return F(std::forward<Args>(args)...);
            } else if constexpr (std::is_invocable_v<
                                     decltype(F), Args...,
                                     const std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                static_assert(
                    std::is_nothrow_invocable_v<decltype(F), Args...,
                                                const std::unwrap_ref_decay_t<BoundArgs>&&...>,
                    "Callable must be noexcept given its signature");
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<const PayloadType*>(s))
                        .invoke(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<const PayloadType**>(s))
                        .invoke(std::forward<Args>(args)...);
                }
            } else if constexpr (std::is_invocable_v<decltype(F),
                                                     const std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                     Args...>) {
                static_assert(
                    std::is_nothrow_invocable_v<
                        decltype(F), const std::unwrap_ref_decay_t<BoundArgs>&&..., Args...>,
                    "Callable must be noexcept given its signature");
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<const PayloadType*>(s))
                        .invokeFront(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<const PayloadType**>(s))
                        .invokeFront(std::forward<Args>(args)...);
                }
            } else if constexpr (is_compatible_invocable_v<decltype(F),
                                                           std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                           Args...> ||
                                 is_compatible_invocable_v<
                                     decltype(F), Args...,
                                     std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                static_assert(AlwaysFalse<decltype(F)>::value,
                              "F is not callable with the provided arguments since it wouldn't "
                              "preserve signature qualifier");
            } else {
                static_assert(AlwaysFalse<decltype(F)>::value,
                              "F is not callable with the provided arguments!");
            }
        };
    }

    FORCE_INLINE Ret operator()(this auto&& self, Args... args) noexcept {
        if constexpr (size == 0) {
            return (static_cast<Task>(self)).storage_.invoker(std::forward<Args>(args)...);
        } else {
            return (static_cast<Task>(self))
                .storage_.invoker(
                    static_cast<void*>((static_cast<Task>(self)).storage_.data.data()),
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

template <typename Task, InlineSize size, typename Ret, typename... Args>
struct InvokerBuilder<Ret(Args...), Task, size> {
    using Type = Ret (*)(void*, Args...);
    using TypeNoBuffer = Ret (*)(Args...);

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size == 0)
    FORCE_INLINE static constexpr TypeNoBuffer getInvoker() {
        static_assert(sizeof...(BoundArgs) == 0);
        return [](Args... args) -> Ret { return F(std::forward<Args>(args)...); };
    }

    template <typename PayloadType, typename F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) -> Ret {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<F, Args...>,
                              "F is not callable with the provided arguments!");
                return (*static_cast<PayloadType*>(s)).invoke(std::forward<Args>(args)...);
            } else if constexpr (is_compatible_invocable_v<
                                     F, Args..., std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<PayloadType*>(s))
                        .invokeCompatible(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<PayloadType**>(s))
                        .invokeCompatible(std::forward<Args>(args)...);
                }
            } else if constexpr (is_compatible_invocable_v<
                                     F, std::unwrap_ref_decay_t<BoundArgs>&&..., Args...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<PayloadType*>(s))
                        .invokeFrontCompatible(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<PayloadType**>(s))
                        .invokeFrontCompatible(std::forward<Args>(args)...);
                }
            } else {
                static_assert(AlwaysFalse<F>::value,
                              "F is not callable with the provided arguments!");
            }
        };
    }

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<decltype(F), Args...>,
                              "F is not callable with the provided arguments!");
                return F(std::forward<Args>(args)...);
            } else if constexpr (is_compatible_invocable_v<
                                     decltype(F), Args...,
                                     std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<PayloadType*>(s))
                        .invokeCompatible(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<PayloadType**>(s))
                        .invokeCompatible(std::forward<Args>(args)...);
                }
            } else if constexpr (is_compatible_invocable_v<decltype(F),
                                                           std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                           Args...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<PayloadType*>(s))
                        .invokeFrontCompatible(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<PayloadType**>(s))
                        .invokeFrontCompatible(std::forward<Args>(args)...);
                }
            } else {
                static_assert(AlwaysFalse<decltype(F)>::value,
                              "F is not callable with the provided arguments!");
            }
        };
    }

    FORCE_INLINE Ret operator()(this auto&& self, Args... args) {
        if constexpr (size == 0) {
            return (static_cast<Task>(self)).storage_.invoker(std::forward<Args>(args)...);
        } else {
            return (static_cast<Task>(self))
                .storage_.invoker(
                    static_cast<void*>((static_cast<Task>(self)).storage_.data.data()),
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

template <typename Task, InlineSize size, typename Ret, typename... Args>
struct InvokerBuilder<Ret(Args...) noexcept, Task, size> {
    using Type = Ret (*)(void*, Args...) noexcept;
    using TypeNoBuffer = Ret (*)(Args...) noexcept;

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size == 0)
    FORCE_INLINE static constexpr TypeNoBuffer getInvoker() {
        static_assert(sizeof...(BoundArgs) == 0);
        static_assert(std::is_nothrow_invocable_v<decltype(F), Args...>,
                      "Callable must be noexcept following its signature");
        return [](Args... args) noexcept -> Ret { return F(std::forward<Args>(args)...); };
    }

    template <typename PayloadType, typename F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) noexcept -> Ret {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<F, Args...>,
                              "F is not callable with the provided arguments!");
                return (*static_cast<PayloadType*>(s)).invoke(std::forward<Args>(args)...);
            } else if constexpr (is_compatible_invocable_v<
                                     F, Args..., std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<PayloadType*>(s))
                        .invokeCompatible(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<PayloadType**>(s))
                        .invokeCompatible(std::forward<Args>(args)...);
                }
            } else if constexpr (is_compatible_invocable_v<
                                     F, std::unwrap_ref_decay_t<BoundArgs>&&..., Args...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<PayloadType*>(s))
                        .invokeFrontCompatible(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<PayloadType**>(s))
                        .invokeFrontCompatible(std::forward<Args>(args)...);
                }
            } else {
                static_assert(AlwaysFalse<F>::value,
                              "F is not callable with the provided arguments!");
            }
        };
    }

    template <typename PayloadType, auto F, typename... BoundArgs>
        requires(size > 0)
    FORCE_INLINE static constexpr Type getInvoker() {
        return [](void* s, Args... args) noexcept {
            if constexpr (sizeof...(BoundArgs) == 0) {
                static_assert(std::is_invocable_v<decltype(F), Args...>,
                              "F is not callable with the provided arguments!");
                return F(std::forward<Args>(args)...);
            } else if constexpr (is_compatible_invocable_v<
                                     decltype(F), Args...,
                                     std::unwrap_ref_decay_t<BoundArgs>&&...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<PayloadType*>(s))
                        .invokeCompatible(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<PayloadType**>(s))
                        .invokeCompatible(std::forward<Args>(args)...);
                }
            } else if constexpr (is_compatible_invocable_v<decltype(F),
                                                           std::unwrap_ref_decay_t<BoundArgs>&&...,
                                                           Args...>) {
                if constexpr (sizeof(PayloadType) <= size) {
                    return std::move(*static_cast<PayloadType*>(s))
                        .invokeFrontCompatible(std::forward<Args>(args)...);
                } else {
                    return std::move(**static_cast<PayloadType**>(s))
                        .invokeFrontCompatible(std::forward<Args>(args)...);
                }
            } else {
                static_assert(AlwaysFalse<decltype(F)>::value,
                              "F is not callable with the provided arguments!");
            }
        };
    }

    FORCE_INLINE Ret operator()(this auto&& self, Args... args) noexcept {
        if constexpr (size == 0) {
            return (static_cast<Task>(self)).storage_.invoker(std::forward<Args>(args)...);
        } else {
            return (static_cast<Task>(self))
                .storage_.invoker(
                    static_cast<void*>((static_cast<Task>(self)).storage_.data.data()),
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
    constexpr void doConstructFromTrivial(void* dst,
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

template <typename Signature, Copyability cb = Copyability::kCopyable,
          InlineSize size = alignof(std::max_align_t), Triviality t = Triviality::kTrivial,
          bool Extended = false, HeapPolicy hp = HeapPolicy::kDisallow,
          Alignment align = alignof(std::max_align_t)>
struct Task;

template <auto F>
struct FunctionTag {};

template <typename T>
struct IsFunctionTag : std::false_type {};

template <auto F>
struct IsFunctionTag<FunctionTag<F>> : std::true_type {};

struct RefTag {};

template <typename S, Copyability cb, InlineSize size, Triviality t, bool Extended, HeapPolicy hp,
          Alignment align>
struct Task : public InvokerBuilder<S, Task<S, cb, size, t, Extended, hp, align>, size> {
    template <typename, Copyability, InlineSize, Triviality, bool, HeapPolicy, Alignment>
    friend struct Task;

    template <typename, typename, InlineSize>
    friend struct InvokerBuilder;

    // NOLINTBEGIN
    static constexpr bool IsTrivial = t == Triviality::kTrivial;
    static constexpr bool AllowHeap = hp == HeapPolicy::kAllow;
    static constexpr bool Copyable = cb == Copyability::kCopyable;
    // NOLINTEND

    static_assert(!IsTrivial || !AllowHeap, "Trivial Task should not allow heap usage");
    static_assert(size > 0 || IsTrivial, "0 buffer size is only allowed on trivial types");
    static_assert(!Extended || !AllowHeap,
                  "Extended mode with heap under development. Switch one off");
    static_assert(align > 0 && (align & (align - 1)) == 0, "Alignment must be a power of 2");
    static_assert(Copyable || !Extended, "Move only types under development for Extended option.");

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
        (tSrc == Triviality::kNonTrivial || IsTrivial || !AllowHeap || sizeSrc <= size))
        requires std::same_as<std::remove_cvref_t<decltype(other)>,
                              Task<SigOther, cbSrc, sizeSrc, tSrc, ExtSrc, hpSrc>>
    {
        using SrcType = std::remove_cvref_t<decltype(other)>;
        static_assert(std::is_const_v<std::remove_reference_t<decltype(other)>> ||
                          std::is_rvalue_reference_v<decltype(other)>,
                      "T must be const& or rvalue&&");
        constexpr bool kIsMovable = std::is_rvalue_reference_v<decltype(other)>;
        auto& dest = *this;
        static_assert(size >= sizeSrc || (AllowHeap && SrcType::IsTrivial && !SrcType::AllowHeap),
                      "Payload too large to move into this storage");
        static_assert(
            !SrcType::AllowHeap || AllowHeap,
            "Cannot convert a task with heap storage option into one where it is disabled");
        static_assert(kMutable || !MutSrc);
        static_assert(!IsTrivial || SrcType::IsTrivial,
                      "Cannot copy/move non-trivial task into trivial one");
        static_assert(SrcType::Copyable || (op != Op::kClone && op != Op::kAssign));

        static_assert((Extended == ExtSrc) || (IsTrivial && SrcType::IsTrivial));
        static_assert(op != Op::kDestroy, "Called Task converter with invalid (destroy) flag");
        static_assert((kIsMovable && (op == Op::kMove || op == Op::kMoveAssign)) ||
                      (op == Op::kClone || op == Op::kAssign));
        dest.storage_.invoker = other.storage_.invoker;

        if constexpr (SrcType::IsTrivial) {
            if constexpr (IsTrivial) {
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
        return Copyable;
    }

    [[nodiscard]] static constexpr auto isMoveOnly() noexcept -> bool {
        return !Copyable;
    }

    [[nodiscard]] static constexpr auto isTrivial() noexcept -> bool {
        return IsTrivial;
    }

    [[nodiscard]] static constexpr auto isMutable() noexcept -> bool {
        return kMutable;
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

    template <typename SigOther, InlineSize sizeSrc, Triviality tSrc, bool Ext2, HeapPolicy hpSrc>
    Task& operator=(
        const Task<SigOther, Copyability::kCopyable, sizeSrc, tSrc, Ext2, hpSrc, align>& other)
        requires((kMutable || !Task<SigOther, Copyability::kCopyable, sizeSrc, tSrc, Ext2, hpSrc,
                                    align>::kMutable) &&
                 (!IsTrivial || tSrc == Triviality::kTrivial) &&
                 (IsTrivial || tSrc == Triviality::kTrivial || Extended == Ext2) &&
                 (AllowHeap || hpSrc == HeapPolicy::kDisallow) &&
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
                 (!IsTrivial || tSrc == Triviality::kTrivial) &&
                 (IsTrivial || tSrc == Triviality::kTrivial || Extended == Ext2) &&
                 (AllowHeap || hpSrc == HeapPolicy::kDisallow) &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    {
        doConvert<
            Op::kClone, SigOther, Copyability::kCopyable, sizeSrc,
            Task<SigOther, Copyability::kCopyable, sizeSrc, tSrc, Ext2, hpSrc, align>::kMutable,
            tSrc, Ext2, hpSrc>(other);
    };

    Task(const Task& /*other*/)
        requires(IsTrivial && Copyable)
    = default;

    Task(const Task& other)
        requires(!IsTrivial && Copyable)
    {
        doConvert<Op::kClone, Signature, cb, size, kMutable, t, Extended, hp>(other);
    }

    Task(const Task& /*other*/)
        requires(!Copyable)
    = delete;

    Task& operator=(const Task& /*other*/)
        requires(IsTrivial && Copyable)
    = default;

    Task& operator=(const Task& other)
        requires(!IsTrivial && Copyable)
    {
        if (this == &other) {
            return *this;
        }
        doConvert<Op::kAssign, Signature, cb, size, kMutable, t, Extended, hp>(other);
        return *this;
    }

    Task& operator=(const Task& /*other*/)
        requires(!Copyable)
    = delete;

    template <typename SigOther, Copyability cbSrc, InlineSize sizeSrc, Triviality tSrc, bool Ext2,
              HeapPolicy hpSrc>
    Task(Task<SigOther, cbSrc, sizeSrc, tSrc, Ext2, hpSrc, align>&& other) noexcept(
        tSrc == Triviality::kNonTrivial || IsTrivial || !AllowHeap || sizeSrc <= size)
        requires((kMutable ||
                  !Task<SigOther, cbSrc, sizeSrc, tSrc, Ext2, hpSrc, align>::kMutable) &&
                 (!IsTrivial || tSrc == Triviality::kTrivial) &&
                 (IsTrivial || tSrc == Triviality::kTrivial || Extended == Ext2) &&
                 (AllowHeap || hpSrc == HeapPolicy::kDisallow) &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    {
        doConvert<Op::kMove, SigOther, cbSrc, sizeSrc,
                  Task<SigOther, cbSrc, sizeSrc, tSrc, Ext2, hpSrc, align>::kMutable, tSrc, Ext2,
                  hpSrc>(std::move(other));
    }

    template <typename SigOther, Copyability cbSrc, InlineSize sizeSrc, Triviality tSrc, bool Ext2,
              HeapPolicy hpSrc>
    Task& operator=(Task<SigOther, cbSrc, sizeSrc, tSrc, Ext2, hpSrc, align>&& other) noexcept(
        tSrc == Triviality::kNonTrivial || IsTrivial || !AllowHeap || sizeSrc <= size)
        requires((kMutable ||
                  !Task<SigOther, cbSrc, sizeSrc, tSrc, Ext2, hpSrc, align>::kMutable) &&
                 (!IsTrivial || tSrc == Triviality::kTrivial) &&
                 (IsTrivial || tSrc == Triviality::kTrivial || Extended == Ext2) &&
                 (AllowHeap || hpSrc == HeapPolicy::kDisallow) &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    {
        doConvert<Op::kMoveAssign, SigOther, cbSrc, sizeSrc,
                  Task<SigOther, cbSrc, sizeSrc, tSrc, Ext2, hpSrc, align>::kMutable, tSrc, Ext2,
                  hpSrc>(std::move(other));
        return *this;
    }

    Task(Task&& /*other*/) noexcept
        requires IsTrivial
    = default;

    Task(Task&& other) noexcept
        requires(!IsTrivial)
    {
        doConvert<Op::kMove, Signature, cb, size, kMutable, t, Extended, hp>(std::move(other));
    }

    Task& operator=(Task&& /*other*/) noexcept
        requires IsTrivial
    = default;

    Task& operator=(Task&& other) noexcept
        requires(!IsTrivial)
    {
        if (this == &other) {
            return *this;
        }
        doConvert<Op::kMoveAssign, Signature, cb, size, kMutable, t, Extended, hp>(
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
        requires IsTrivial
    = default;

    constexpr ~Task() noexcept
        requires(!IsTrivial)
    {
        storage_.doDelete();
    }

    enum class ArgsOrder : std::uint8_t { kBack, kFront };

    template <auto F, typename... BoundArgs>
    constexpr Task([[maybe_unused]] FunctionTag<F> /*unused*/, BoundArgs&&... args)  // NOLINT
        : storage_{.invoker = InvokerBuilder<Signature, std::remove_cv_t<decltype(*this)>, size>::
                       template getInvoker<PayloadArgsT<F, BoundArgs...>, F, BoundArgs...>()} {
        using PayloadType = PayloadArgsT<F, BoundArgs...>;
        if constexpr (!std::is_empty_v<PayloadType>) {
            static_assert(PayloadNoRvalueRef<PayloadType>::value);
            static_assert(
                !Copyable || std::is_copy_constructible_v<PayloadType>,
                "Given payload is not copy constructible. Check types or create a unique task");
            static_assert(
                std::is_move_constructible_v<PayloadType> && std::is_destructible_v<PayloadType>,
                "Given payload is not constructible, move constructible or destructible");
            static_assert(
                sizeof(PayloadType) <= size || (AllowHeap && sizeof(PayloadType*) <= size),
                "Size exceeds buffer. Increase Stack size or activate option to allow heap "
                "storage (template option, or via FlexTask aliases)");
            static_assert(alignof(PayloadType) <= align,
                          "Alignment requirement too high for buffer. Increase template alignment");
            if constexpr (IsTrivial) {
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
                static_assert(AllowHeap);
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
                 !IsFunctionTag<std::remove_cvref_t<decltype(f)>>::value &&
                 (sizeof...(BoundArgs) > 0 || !IsTask<std::remove_cvref_t<decltype(f)>>::value))
        : storage_{
              .invoker = InvokerBuilder<Signature, std::remove_cv_t<decltype(*this)>, size>::
                  template getInvoker<PayloadT<decltype(f), BoundArgs...>,
                                      std::remove_cvref_t<std::unwrap_ref_decay_t<decltype(f)>>,
                                      std::decay_t<BoundArgs>...>()} {
        using F = decltype(f);
        using PayloadType = PayloadT<F, BoundArgs...>;
        static_assert(PayloadNoRvalueRef<PayloadType>::value);
        static_assert(sizeof(PayloadType) <= size || (AllowHeap && sizeof(PayloadType*) <= size),
                      "Size exceeds buffer. Increase Stack size or activate option to allow heap "
                      "storage (template option, or via FlexTask aliases)");
        static_assert(alignof(PayloadType) <= align,
                      "Alignment requirement too high for buffer. Increase template alignment");
        static_assert(
            !Copyable || std::is_copy_constructible_v<PayloadType>,
            "Given payload is not copy constructible. Check types or create a unique task");
        static_assert(
            std::is_move_constructible_v<PayloadType> && std::is_destructible_v<PayloadType>,
            "Given payload is not move constructible or destructible");
        if constexpr (IsTrivial) {
            static_assert(TrivialUse<PayloadType, cb>::value,
                          "Payload needs to be trivially move & copy constructible, and "
                          "trivially destructible to be allowed within a Trivial Task");
        }
        storage_.template doConstruct<PayloadType, hp, cb>();
        if constexpr (sizeof(PayloadType) <= size) {
            new (storage_.data.data())
                PayloadType{std::forward<F>(f), {std::forward<BoundArgs>(args)...}};
        } else {
            static_assert(AllowHeap);
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
    constexpr Task([[maybe_unused]] RefTag /*unused*/, F&& f, BoundArgs&&... args)  // NOLINT
        requires(!IsFunctionTag<std::remove_cvref_t<decltype(f)>>::value)
        : storage_{.invoker =
                       InvokerBuilder<Signature, std::remove_cv_t<decltype(*this)>,
                                      size>::template getInvoker<PayloadImpl<F, BoundArgs...>,
                                                                 std::decay_t<F>, BoundArgs...>()} {
        static_assert(
            (((std::is_lvalue_reference_v<decltype(args)> &&
               std::is_lvalue_reference_v<BoundArgs>) ||
              (!std::is_rvalue_reference_v<BoundArgs> && !std::is_lvalue_reference_v<BoundArgs> &&
               std::is_rvalue_reference_v<decltype(args)>)) &&
             ...));
        using PayloadType = PayloadImpl<F, BoundArgs...>;
        static_assert(PayloadNoRvalueRef<PayloadType>::value);
        static_assert(sizeof(PayloadType) <= size || (AllowHeap && sizeof(PayloadType*) <= size),
                      "Size exceeds buffer. Increase Stack size or activate option to allow heap "
                      "storage (template option, or via FlexTask aliases)");
        static_assert(alignof(PayloadType) <= align,
                      "Alignment requirement too high for buffer. Increase template alignment");
        static_assert(
            !Copyable || std::is_copy_constructible_v<PayloadType>,
            "Given payload is not copy constructible. Check types or create a unique task");
        static_assert(
            std::is_move_constructible_v<PayloadType> && std::is_destructible_v<PayloadType>,
            "Given payload is not constructible, move constructible or destructible");
        if constexpr (IsTrivial) {
            static_assert(TrivialUse<PayloadType, cb>::value,
                          "Payload needs to be trivially move & copy constructible, and "
                          "trivially destructible to be allowed within a Trivial Task");
        }
        storage_.template doConstruct<PayloadType, hp, cb>();
        if constexpr (sizeof(PayloadType) <= size) {
            new (storage_.data.data())
                PayloadType{std::forward<F>(f), {std::forward<BoundArgs>(args)...}};
        } else {
            static_assert(AllowHeap);
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
    constexpr Task([[maybe_unused]] RefTag /*unused*/,                               // NOLINT
                   [[maybe_unused]] FunctionTag<F> /*unused*/, BoundArgs&&... args)  // NOLINT
        : storage_{.invoker = InvokerBuilder<Signature, std::remove_cv_t<decltype(*this)>, size>::
                       template getInvoker<PayloadImplArgs<F, BoundArgs...>, F, BoundArgs...>()} {
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
                sizeof(PayloadType) <= size || (AllowHeap && sizeof(PayloadType*) <= size),
                "Size exceeds buffer. Increase Stack size or activate option to allow heap "
                "storage (template option, or via FlexTask aliases)");
            static_assert(alignof(PayloadType) <= align,
                          "Alignment requirement too high for buffer. Increase template alignment");
            static_assert(
                !Copyable || std::is_copy_constructible_v<PayloadType>,
                "Given payload is not copy constructible. Check types or create a unique task");
            static_assert(
                std::is_move_constructible_v<PayloadType> && std::is_destructible_v<PayloadType>,
                "Given payload is not constructible, move constructible or destructible");
            if constexpr (IsTrivial) {
                static_assert(TrivialUse<PayloadType, cb>::value,
                              "Payload needs to be trivially move & copy constructible, and "
                              "trivially destructible to be allowed within a Trivial Task");
            }
            storage_.template doConstruct<PayloadType, hp, cb>();
            if constexpr (sizeof(PayloadType) <= size) {
                new (storage_.data.data()) PayloadType{{std::forward<BoundArgs>(args)...}};
            } else {
                static_assert(AllowHeap);
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
        if constexpr (!IsTrivial) {
            storage_.doDelete();
        }
    }
};

template <typename Signature, InlineSize size = 64, Triviality t = Triviality::kNonTrivial,
          bool Extended = false, HeapPolicy hp = HeapPolicy::kDisallow,
          Alignment alignment = alignof(std::max_align_t)>
using CopyableTask = Task<Signature, Copyability::kCopyable, size, t, Extended, hp, alignment>;

template <typename Signature, InlineSize size = 64, Triviality t = Triviality::kNonTrivial,
          bool Extended = false, HeapPolicy hp = HeapPolicy::kDisallow,
          Alignment alignment = alignof(std::max_align_t)>
using UniqueTask = Task<Signature, Copyability::kNonCopyable, size, t, Extended, hp, alignment>;

template <typename Signature, Copyability cp = Copyability::kCopyable,
          Triviality t = Triviality::kNonTrivial, bool Extended = false>
using Task16 = Task<Signature, cp, 16, t, Extended, HeapPolicy::kDisallow>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Triviality t = Triviality::kNonTrivial, bool Extended = false>
using Task32 = Task<Signature, cp, 32, t, Extended, HeapPolicy::kDisallow>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Triviality t = Triviality::kNonTrivial, bool Extended = false>
using Task48 = Task<Signature, cp, 48, t, Extended, HeapPolicy::kDisallow>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Triviality t = Triviality::kNonTrivial, bool Extended = false>
using Task64 = Task<Signature, cp, 64, t, Extended, HeapPolicy::kDisallow>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Triviality t = Triviality::kNonTrivial, bool Extended = false>
using Task128 = Task<Signature, cp, 128, t, Extended, HeapPolicy::kDisallow>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Triviality t = Triviality::kNonTrivial, bool Extended = false>
using Task256 = Task<Signature, cp, 256, t, Extended, HeapPolicy::kDisallow>;

template <typename Signature, Copyability cp = Copyability::kCopyable>
using TrivialTask16 = Task16<Signature, cp, Triviality::kTrivial, false>;
template <typename Signature, Copyability cp = Copyability::kCopyable>
using TrivialTask32 = Task32<Signature, cp, Triviality::kTrivial, false>;
template <typename Signature, Copyability cp = Copyability::kCopyable>
using TrivialTask48 = Task48<Signature, cp, Triviality::kTrivial, false>;
template <typename Signature, Copyability cp = Copyability::kCopyable>
using TrivialTask64 = Task64<Signature, cp, Triviality::kTrivial, false>;
template <typename Signature, Copyability cp = Copyability::kCopyable>
using TrivialTask128 = Task128<Signature, cp, Triviality::kTrivial, false>;
template <typename Signature, Copyability cp = Copyability::kCopyable>
using TrivialTask256 = Task256<Signature, cp, Triviality::kTrivial, false>;

template <typename Signature, Copyability cp = Copyability::kCopyable, bool Extended = false>
using SmallTask = Task<Signature, cp, 16, Triviality::kNonTrivial, Extended, HeapPolicy::kDisallow>;

template <typename Signature, Copyability cp = Copyability::kCopyable, InlineSize size = 64,
          HeapPolicy hp = HeapPolicy::kDisallow>
using TrivialTask = Task<Signature, cp, size, Triviality::kTrivial, false, hp>;

template <typename Signature, Copyability cp = Copyability::kCopyable,
          HeapPolicy hp = HeapPolicy::kDisallow>
using TrivialSmallTask = TrivialTask<Signature, cp, 16, hp>;

template <typename Signature, Copyability cp = Copyability::kCopyable,
          HeapPolicy hp = HeapPolicy::kDisallow>
using TrivialSmallTaskMut = TrivialTask<Signature, cp, 16, hp>;

template <typename Signature, Copyability cp = Copyability::kCopyable>
using TrivialTaskCaptureless = TrivialTask<Signature, cp, 1>;

template <typename Signature, Copyability cp = Copyability::kCopyable>
using StaticTrivialTask = TrivialTask<Signature, cp, 0>;

template <typename Signature, Copyability cp = Copyability::kCopyable, InlineSize size = 64,
          Triviality t = Triviality::kNonTrivial>
using FlexTask = Task<Signature, cp, size, t, false, HeapPolicy::kAllow>;

template <typename Signature, InlineSize size = 64, Triviality t = Triviality::kNonTrivial>
using UniqueFlexTask = FlexTask<Signature, Copyability::kNonCopyable, size, t>;

template <typename Signature, Copyability cp = Copyability::kCopyable,
          Triviality t = Triviality::kNonTrivial>
using FlexTask16 = FlexTask<Signature, cp, 16, t>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Triviality t = Triviality::kNonTrivial>
using FlexTask32 = FlexTask<Signature, cp, 32, t>;
template <typename Signature, Copyability cp = Copyability::kCopyable,
          Triviality t = Triviality::kNonTrivial>
using FlexTask64 = FlexTask<Signature, cp, 64, t>;

static_assert(TrivialTask<void(), Copyability::kCopyable, 0>::getSize() == sizeof(void*));

template <typename Signature, Copyability cp = Copyability::kCopyable>
using TrivialTaskCapturelessMut = TrivialTask<Signature, cp, 1>;

template <typename Sig, Copyability cp, InlineSize size, Triviality t, bool Ext, HeapPolicy hp,
          Alignment align>
Task(Task<Sig, cp, size, t, Ext, hp, align>) -> Task<Sig, cp, size, t, Ext, hp, align>;

template <auto F, typename... BoundArgs>
    requires((sizeof...(BoundArgs) > 0 || !IsTask<std::remove_cvref_t<decltype(F)>>::value) &&
             FunctionTraits<decltype(F)>::kValid && TaskTypeDeductorAuto<F, BoundArgs...>::kIsValid)
Task(FunctionTag<F>, BoundArgs&&...)
    -> Task<typename TaskTypeDeductorAuto<F, BoundArgs...>::TypeTraits::Signature,
            TaskTypeDeductorAuto<F, BoundArgs...>::kCopyable,
            TaskTypeDeductorAuto<F, BoundArgs...>::kPayloadSize,
            TaskTypeDeductorAuto<F, BoundArgs...>::kTriviality, false, HeapPolicy::kDisallow,
            TaskTypeDeductorAuto<F, BoundArgs...>::kAlignment>;

template <typename F, typename... BoundArgs>
    requires(!std::is_same_v<F, RefTag> && !IsFunctionTag<F>::value &&
             FunctionTraits<std::decay_t<F>>::kValid &&
             (sizeof...(BoundArgs) > 0 || !IsTask<std::remove_cvref_t<F>>::value) &&
             TaskTypeDeductor<F, BoundArgs...>::kIsValid)
Task(F&&, BoundArgs&&...)
    -> Task<typename TaskTypeDeductor<F, BoundArgs...>::TypeTraits::Signature,
            TaskTypeDeductor<F, BoundArgs...>::kCopyable,
            TaskTypeDeductor<F, BoundArgs...>::kPayloadSize,
            TaskTypeDeductor<F, BoundArgs...>::kTriviality, false, HeapPolicy::kDisallow,
            TaskTypeDeductor<F, BoundArgs...>::kAlignment>;

template <typename F, typename... BoundArgs>
    requires(!IsFunctionTag<F>::value && FunctionTraits<std::decay_t<F>>::kValid &&
             TaskTypeDeductor<F, BoundArgs...>::kIsValid)
Task(RefTag, F&&, BoundArgs&&...)
    -> Task<typename TaskTypeDeductor<F, BoundArgs...>::TypeTraits::Signature,
            TaskTypeDeductor<F, BoundArgs...>::kRefCopyable,
            TaskTypeDeductor<F, BoundArgs...>::kRefPayloadSize,
            TaskTypeDeductor<F, BoundArgs...>::kRefTriviality, false, HeapPolicy::kDisallow,
            TaskTypeDeductor<F, BoundArgs...>::kRefAlignment>;

template <auto F, typename... BoundArgs>
    requires(FunctionTraits<decltype(F)>::kValid && TaskTypeDeductorAuto<F, BoundArgs...>::kIsValid)
Task(RefTag, FunctionTag<F>, BoundArgs&&...)
    -> Task<typename TaskTypeDeductorAuto<F, BoundArgs...>::TypeTraits::Signature,
            TaskTypeDeductorAuto<F, BoundArgs...>::kRefCopyable,
            TaskTypeDeductorAuto<F, BoundArgs...>::kRefPayloadSize,
            TaskTypeDeductorAuto<F, BoundArgs...>::kRefCanUseTrivial, false, HeapPolicy::kDisallow,
            TaskTypeDeductorAuto<F, BoundArgs...>::kRefAlignment>;

template <typename F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductor<F, Args...>::template Type<> makeTask(
    F&& f, Args&&... args) {
    static_assert(HasFunctionTraits<std::remove_reference_t<std::unwrap_ref_decay_t<F>>>,
                  "Given callable is invalid");
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

template <std::size_t N = alignof(std::max_align_t), typename F, typename... Args>
FORCE_INLINE constexpr auto makeFlexTask(F&& f, Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    using Deductor = TaskTypeDeductor<F, Args...>;
    static_assert(Deductor::kIsValid, "Forwarded callable is not callable with given arguments");
    return typename Deductor::template FlexType<N>{std::forward<F>(f), std::forward<Args>(args)...};
}

template <typename Sig, InlineSize size = alignof(std::max_align_t), typename F, typename... Args>
FORCE_INLINE constexpr auto makeFlexTask(F&& f, Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    using Deductor = TaskTypeDeductor<F, Args...>;
    static_assert(Deductor::kIsValid, "Forwarded callable is not callable with given arguments");
    static_assert(std::is_same_v<typename Deductor::Signature, Sig>,
                  "Forwarded callable and arguments do not match requested signature");
    return
        typename Deductor::template FlexType<size>{std::forward<F>(f), std::forward<Args>(args)...};
}

template <std::size_t N = alignof(std::max_align_t), auto F, typename... Args>
FORCE_INLINE constexpr auto makeFlexTask(Args&&... args) {
    static_assert(HasFunctionTraits<decltype(F)>, "Given callable is invalid");
    using Deductor = TaskTypeDeductorAuto<F, Args...>;
    static_assert(Deductor::kIsValid, "Forwarded callable is not callable with given arguments");
    return typename Deductor::template FlexType<N>{FunctionTag<F>{}, std::forward<Args>(args)...};
}

template <typename Sig, InlineSize size = alignof(std::max_align_t), auto F, typename... Args>
FORCE_INLINE constexpr auto makeFlexTask(Args&&... args) {
    static_assert(HasFunctionTraits<decltype(F)>, "Given callable is invalid");
    using Deductor = TaskTypeDeductorAuto<F, Args...>;
    static_assert(Deductor::kIsValid, "Forwarded callable is not callable with given arguments");
    static_assert(std::is_same_v<typename Deductor::Signature, Sig>,
                  "Forwarded callable and arguments do not match requested signature");
    return
        typename Deductor::template FlexType<size>{FunctionTag<F>{}, std::forward<Args>(args)...};
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
FORCE_INLINE constexpr typename TaskTypeDeductor<F, Args...>::template Type<> makeMutTask(
    F&& f, Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    static_assert(TaskTypeDeductor<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(TaskTypeDeductor<F, Args...>::template Type<>::kMutable,
                  "Callable is not mutable");
    return Task(std::forward<F>(f), std::forward<Args>(args)...);
}

template <auto F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductorAuto<F, Args...>::template Type<> makeMutTask(
    Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<decltype(F)>>, "Given callable is invalid");
    static_assert(TaskTypeDeductorAuto<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(TaskTypeDeductorAuto<F, Args...>::template Type<>::kMutable,
                  "Callable is not mutable");
    return Task(FunctionTag<F>{}, std::forward<Args>(args)...);
}

template <typename Sig, typename F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductor<F, Args...>::template Type<> makeMutTask(
    F&& f, Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    static_assert(std::is_same_v<typename TaskTypeDeductor<F, Args...>::TypeTraits::Signature, Sig>,
                  "Forwarded callable and arguments do not match requested signature");
    static_assert(TaskTypeDeductor<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(TaskTypeDeductor<F, Args...>::template Type<>::kMutable,
                  "Callable is not mutable");
    return Task(std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename Sig, auto F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductorAuto<F, Args...>::template Type<> makeMutTask(
    Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<decltype(F)>>, "Given callable is invalid");
    static_assert(
        std::is_same_v<typename TaskTypeDeductorAuto<F, Args...>::TypeTraits::Signature, Sig>,
        "Forwarded callable and arguments do not match requested signature");
    static_assert(TaskTypeDeductorAuto<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(TaskTypeDeductorAuto<F, Args...>::template Type<>::kMutable,
                  "Callable is not mutable");
    return Task(FunctionTag<F>{}, std::forward<Args>(args)...);
}

template <typename F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductor<F, Args...>::template RefType<> makeMutRefTask(
    F&& f, Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    static_assert(TaskTypeDeductor<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(TaskTypeDeductor<F, Args...>::template Type<>::kMutable,
                  "Callable is not mutable");
    return Task(RefTag{}, std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename Sig, typename F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductor<F, Args...>::template RefType<> makeMutRefTask(
    F&& f, Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    static_assert(std::is_same_v<typename TaskTypeDeductor<F, Args...>::TypeTraits::Signature, Sig>,
                  "Forwarded callable and arguments do not match requested signature");
    static_assert(TaskTypeDeductor<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(TaskTypeDeductor<F, Args...>::template Type<>::kMutable,
                  "Callable is not mutable");

    return Task(RefTag{}, std::forward<F>(f), std::forward<Args>(args)...);
}

template <auto F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductorAuto<F, Args...>::template RefType<> makeMutRefTask(
    Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<decltype(F)>>, "Given callable is invalid");
    static_assert(TaskTypeDeductorAuto<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(TaskTypeDeductorAuto<F, Args...>::template Type<>::kMutable,
                  "Callable is not mutable");
    return Task(RefTag{}, FunctionTag<F>{}, std::forward<Args>(args)...);
}

template <typename Sig, auto F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductorAuto<F, Args...>::template RefType<> makeMutRefTask(
    Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<decltype(F)>>, "Given callable is invalid");
    static_assert(TaskTypeDeductorAuto<F, Args...>::template Type<>::kMutable,
                  "Callable is not mutable");
    static_assert(TaskTypeDeductorAuto<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(
        std::is_same_v<typename TaskTypeDeductorAuto<F, Args...>::TypeTraits::Signature, Sig>,
        "Forwarded callable and arguments do not match requested signature");
    return Task(RefTag{}, FunctionTag<F>{}, std::forward<Args>(args)...);
}

template <typename F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductor<F, Args...>::template Type<> makeImmutTask(
    F&& f, Args&&... args) {
    static_assert(!TaskTypeDeductor<F, Args...>::template Type<>::kMutable,
                  "Callable is not immutable");
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    static_assert(TaskTypeDeductor<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    return Task(std::forward<F>(f), std::forward<Args>(args)...);
}

template <auto F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductorAuto<F, Args...>::template Type<> makeImmutTask(
    Args&&... args) {
    using Deductor = TaskTypeDeductorAuto<F, Args...>;
    using TaskType = typename Deductor::template Type<>;
    static_assert(HasFunctionTraits<std::decay_t<decltype(F)>>, "Given callable is invalid");
    static_assert(Deductor::kIsValid, "Forwarded callable is not callable with given arguments");
    static_assert(!TaskType::kMutable, "Callable is not immutable");
    return Task(FunctionTag<F>{}, std::forward<Args>(args)...);
}

template <typename Sig, typename F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductor<F, Args...>::template Type<> makeImmutTask(
    F&& f, Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    static_assert(TaskTypeDeductor<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(!TaskTypeDeductor<F, Args...>::template Type<>::kMutable,
                  "Callable is not immutable");
    static_assert(std::is_same_v<typename TaskTypeDeductor<F, Args...>::TypeTraits::Signature, Sig>,
                  "Forwarded callable and arguments do not match requested signature");
    return Task(std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename Sig, auto F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductorAuto<F, Args...>::template Type<> makeImmutTask(
    Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<decltype(F)>>, "Given callable is invalid");
    static_assert(TaskTypeDeductorAuto<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(!TaskTypeDeductorAuto<F, Args...>::template Type<>::kMutable,
                  "Callable is not immutable");
    static_assert(
        std::is_same_v<typename TaskTypeDeductorAuto<F, Args...>::TypeTraits::Signature, Sig>,
        "Forwarded callable and arguments do not match requested signature");
    return Task(FunctionTag<F>{}, std::forward<Args>(args)...);
}

template <typename F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductor<F, Args...>::template RefType<> makeImmutRefTask(
    F&& f, Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    static_assert(TaskTypeDeductor<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(!TaskTypeDeductor<F, Args...>::template Type<>::kMutable,
                  "Callable is not immutable");
    return Task(RefTag{}, std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename Sig, typename F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductor<F, Args...>::template RefType<> makeImmutRefTask(
    F&& f, Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<F>>, "Given callable is invalid");
    static_assert(std::is_same_v<typename TaskTypeDeductor<F, Args...>::TypeTraits::Signature, Sig>,
                  "Forwarded callable and arguments do not match requested signature");
    static_assert(TaskTypeDeductor<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(!TaskTypeDeductor<F, Args...>::template Type<>::kMutable,
                  "Callable is not immutable");
    return Task(RefTag{}, std::forward<F>(f), std::forward<Args>(args)...);
}

template <auto F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductorAuto<F, Args...>::template RefType<>
makeImmutRefTask(Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<decltype(F)>>, "Given callable is invalid");
    static_assert(TaskTypeDeductorAuto<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(!TaskTypeDeductorAuto<F, Args...>::template Type<>::kMutable,
                  "Callable is not immutable");
    return Task(RefTag{}, FunctionTag<F>{}, std::forward<Args>(args)...);
}

template <typename Sig, auto F, typename... Args>
FORCE_INLINE constexpr typename TaskTypeDeductorAuto<F, Args...>::template RefType<>
makeImmutRefTask(Args&&... args) {
    static_assert(HasFunctionTraits<std::decay_t<decltype(F)>>, "Given callable is invalid");
    static_assert(TaskTypeDeductorAuto<F, Args...>::kIsValid,
                  "Forwarded callable is not callable with given arguments");
    static_assert(!TaskTypeDeductorAuto<F, Args...>::template Type<>::kMutable,
                  "Callable is not immutable");
    static_assert(
        std::is_same_v<typename TaskTypeDeductorAuto<F, Args...>::TypeTraits::Signature, Sig>,
        "Forwarded callable and arguments do not match requested signature");
    return Task(RefTag{}, FunctionTag<F>{}, std::forward<Args>(args)...);
}

}  // namespace fn

#endif  // FN_TASK_HPP