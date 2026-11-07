#ifndef TASK_NAMED_TASK_HPP
#define TASK_NAMED_TASK_HPP

#include <algorithm>
#include <cstddef>
#include <tuple>
#include <type_traits>

#include "function_traits.hpp"
#include "payload.hpp"

namespace fn {

template <auto F, typename... Args>
struct StaticPayloadSelector {
    using type = PayloadArgsT<F, Args...>;
};

template <typename F, typename... Args>
struct TypePayloadSelector {
    using type = PayloadT<F, Args...>;
};

template <typename Task, typename Payload, typename Signature>
struct NamedTaskInvoker;

template <typename Task, typename F, typename... BoundArgs, typename Ret, typename... Args>
struct NamedTaskInvoker<Task, PayloadImpl<F, BoundArgs...>, Ret(Args...)> {
    static_assert(is_compatible_invocable_v<F, BoundArgs&&..., Args...> ||
                  is_compatible_invocable_v<F, Args..., BoundArgs&&...>);
    static constexpr bool kIsFront = is_compatible_invocable_v<F, BoundArgs&&..., Args...>;
    FORCE_INLINE Ret operator()(this auto&& self, Args... args) {
        if constexpr (kIsFront) {
            return std::move((static_cast<Task>(self)).payload_)
                .invokeFrontCompatible(std::forward<Args>(args)...);
        } else {
            return std::move((static_cast<Task>(self)).payload_)
                .invokeCompatible(std::forward<Args>(args)...);
        }
    }
};

template <typename Task, typename F, typename... BoundArgs, typename Ret, typename... Args>
struct NamedTaskInvoker<Task, PayloadImpl<F, BoundArgs...>, Ret(Args...) noexcept> {
    static_assert(is_compatible_invocable_v<F, BoundArgs&&..., Args...> ||
                  is_compatible_invocable_v<F, Args..., BoundArgs&&...>);
    static constexpr bool kIsFront = is_compatible_invocable_v<F, BoundArgs&&..., Args...>;
    FORCE_INLINE Ret operator()(this auto&& self, Args... args) {
        if constexpr (kIsFront) {
            return std::move((static_cast<Task>(self)).payload_)
                .invokeFrontCompatible(std::forward<Args>(args)...);
        } else {
            return std::move((static_cast<Task>(self)).payload_)
                .invokeCompatible(std::forward<Args>(args)...);
        }
    }
};

template <typename Task, typename F, typename... BoundArgs, typename Ret, typename... Args>
struct NamedTaskInvoker<Task, PayloadImpl<F, BoundArgs...>, Ret(Args...) const> {
    static_assert(std::is_invocable_v<F, const BoundArgs..., Args...> ||
                  std::is_invocable_v<F, Args..., const BoundArgs...>);
    static constexpr bool kIsFront = std::is_invocable_v<F, const BoundArgs..., Args...>;
    FORCE_INLINE Ret operator()(this auto&& self, Args... args) {
        if constexpr (kIsFront) {
            return (static_cast<const Task>(self))
                .payload_.invokeFront(std::forward<Args>(args)...);
        } else {
            return (static_cast<const Task>(self)).payload_.invoke(std::forward<Args>(args)...);
        }
    }
};

template <typename Task, typename F, typename... BoundArgs, typename Ret, typename... Args>
struct NamedTaskInvoker<Task, PayloadImpl<F, BoundArgs...>, Ret(Args...) const noexcept> {
    static_assert(std::is_invocable_v<F, const BoundArgs..., Args...> ||
                  std::is_invocable_v<F, Args..., const BoundArgs...>);
    static constexpr bool kIsFront = std::is_invocable_v<F, const BoundArgs..., Args...>;
    FORCE_INLINE Ret operator()(this auto&& self, Args... args) noexcept {
        if constexpr (kIsFront) {
            return (static_cast<const Task>(self))
                .payload_.invokeFront(std::forward<Args>(args)...);
        } else {
            return (static_cast<const Task>(self)).payload_.invoke(std::forward<Args>(args)...);
        }
    }
};

template <typename Task, typename F, typename... BoundArgs, typename Ret, typename... Args>
struct NamedTaskInvoker<Task, PayloadImpl<F, BoundArgs...>, Ret(Args...) const&> {
    static_assert(std::is_invocable_v<F, const BoundArgs&..., Args...> ||
                  std::is_invocable_v<F, Args..., const BoundArgs&...>);
    static constexpr bool kIsFront = std::is_invocable_v<F, const BoundArgs&..., Args...>;
    FORCE_INLINE Ret operator()(this auto&& self, Args... args) {
        if constexpr (kIsFront) {
            return (static_cast<const Task&>(self))
                .payload_.invokeFront(std::forward<Args>(args)...);
        } else {
            return (static_cast<const Task&>(self)).payload_.invoke(std::forward<Args>(args)...);
        }
    }
};

template <typename Task, typename F, typename... BoundArgs, typename Ret, typename... Args>
struct NamedTaskInvoker<Task, PayloadImpl<F, BoundArgs...>, Ret(Args...) const & noexcept> {
    static_assert(std::is_invocable_v<F, const BoundArgs&..., Args...> ||
                  std::is_invocable_v<F, Args..., const BoundArgs&...>);
    static constexpr bool kIsFront = std::is_invocable_v<F, const BoundArgs&..., Args...>;
    FORCE_INLINE Ret operator()(this auto&& self, Args... args) noexcept {
        if constexpr (kIsFront) {
            return (static_cast<const Task&>(self))
                .payload_.invokeFront(std::forward<Args>(args)...);
        } else {
            return (static_cast<const Task&>(self)).payload_.invoke(std::forward<Args>(args)...);
        }
    }
};

template <typename Task, typename F, typename... BoundArgs, typename Ret, typename... Args>
struct NamedTaskInvoker<Task, PayloadImpl<F, BoundArgs...>, Ret(Args...) &> {
    static_assert(std::is_invocable_v<F, BoundArgs&..., Args...> ||
                  std::is_invocable_v<F, Args..., BoundArgs&...>);
    static constexpr bool kIsFront = std::is_invocable_v<F, BoundArgs&..., Args...>;
    FORCE_INLINE Ret operator()(this auto&& self, Args... args) {
        if constexpr (kIsFront) {
            return (static_cast<Task>(self)).payload_.invokeFront(std::forward<Args>(args)...);
        } else {
            return (static_cast<Task>(self)).payload_.invoke(std::forward<Args>(args)...);
        }
    }
};

template <typename Task, typename F, typename... BoundArgs, typename Ret, typename... Args>
struct NamedTaskInvoker<Task, PayloadImpl<F, BoundArgs...>, Ret(Args...) & noexcept> {
    static_assert(std::is_invocable_v<F, BoundArgs&..., Args...> ||
                  std::is_invocable_v<F, Args..., BoundArgs&...>);
    static constexpr bool kIsFront = std::is_invocable_v<F, BoundArgs&..., Args...>;
    FORCE_INLINE Ret operator()(this auto&& self, Args... args) noexcept {
        if constexpr (kIsFront) {
            return (static_cast<Task>(self)).payload_.invokeFront(std::forward<Args>(args)...);
        } else {
            return (static_cast<Task>(self)).payload_.invoke(std::forward<Args>(args)...);
        }
    }
};

template <typename Task, typename F, typename... BoundArgs, typename Ret, typename... Args>
struct NamedTaskInvoker<Task, PayloadImpl<F, BoundArgs...>, Ret(Args...) &&> {
    static_assert(std::is_invocable_v<F, BoundArgs&&..., Args...> ||
                  std::is_invocable_v<F, Args..., BoundArgs&&...>);
    static constexpr bool kIsFront = std::is_invocable_v<F, BoundArgs&&..., Args...>;
    FORCE_INLINE Ret operator()(this auto&& self, Args... args) {
        if constexpr (kIsFront) {
            return std::move((static_cast<Task>(self)).payload_)
                .invokeFront(std::forward<Args>(args)...);
        } else {
            return std::move((static_cast<Task>(self)).payload_)
                .invoke(std::forward<Args>(args)...);
        }
    }
};

template <typename Task, typename F, typename... BoundArgs, typename Ret, typename... Args>
struct NamedTaskInvoker<Task, PayloadImpl<F, BoundArgs...>, Ret(Args...) && noexcept> {
    static_assert(std::is_invocable_v<F, BoundArgs&&..., Args...> ||
                  std::is_invocable_v<F, Args..., BoundArgs&&...>);
    static constexpr bool kIsFront = std::is_invocable_v<F, BoundArgs&&..., Args...>;
    FORCE_INLINE Ret operator()(this auto&& self, Args... args) noexcept {
        if constexpr (kIsFront) {
            return std::move((static_cast<Task>(self)).payload_)
                .invokeFront(std::forward<Args>(args)...);
        } else {
            return std::move((static_cast<Task>(self)).payload_)
                .invoke(std::forward<Args>(args)...);
        }
    }
};

template <auto F>
struct StaticFn {};

template <typename F>
struct NamedTaskRefTag {};

template <auto F>
struct NamedTaskStaticRefTag {};

template <typename S, typename F, typename... BoundArgs>
struct NamedTask;

template <typename>
struct IsNamedTask : std::false_type {};

template <typename S, typename F, typename... BoundArgs>
struct IsNamedTask<NamedTask<S, F, BoundArgs...>> : std::true_type {};

template <typename S, typename F, typename... BoundArgs>
struct IsNamedTask<NamedTask<S, NamedTaskRefTag<F>, BoundArgs...>> : std::true_type {};

template <typename S, auto F, typename... BoundArgs>
struct IsNamedTask<NamedTask<S, StaticFn<F>, BoundArgs...>> : std::true_type {};

template <typename S, auto F, typename... BoundArgs>
struct IsNamedTask<NamedTask<S, NamedTaskStaticRefTag<F>, BoundArgs...>> : std::true_type {};

template <typename S, typename F, typename... BoundArgs>
    requires(FunctionTraits<std::decay_t<F>>::kValid && FunctionTraits<S>::kValid)
struct NamedTask<S, F, BoundArgs...>
    : NamedTaskInvoker<NamedTask<S, F, BoundArgs...>, PayloadT<F, BoundArgs...>, S> {
   private:
    template <typename, typename, typename>
    friend struct NamedTaskInvoker;

    template <typename, typename, typename...>
    friend struct NamedTask;

    using CallableImpl = F;
    using PayloadType = PayloadT<CallableImpl, BoundArgs...>;
    mutable PayloadType payload_{};

   public:
    using Signature = S;

    [[nodiscard]] static constexpr auto getSize() noexcept -> std::size_t {
        return sizeof(NamedTask);
    }

    template <typename Sig>
    [[nodiscard]] static constexpr auto matchesSignature() noexcept -> bool {
        return std::is_same_v<Signature, Sig>;
    }

    template <typename R, typename... Args>
        requires(sizeof...(Args) > 0)
    [[nodiscard]] static constexpr auto matchesSignature() noexcept -> bool {
        return NamedTask::matchesSignature<R(Args...)>();
    }

    template <std::size_t I>
    constexpr decltype(auto) getBoundArg(this auto&& self) noexcept {
        static_assert(I < sizeof...(BoundArgs), "Exceeds number of captured arguments");
        return PayloadTupleGetter<I, std::decay_t<BoundArgs>...>::get(self.payload_.args);
    }

    NamedTask(const NamedTask&) = default;
    NamedTask& operator=(const NamedTask&) = default;
    NamedTask(NamedTask&&) = default;
    NamedTask& operator=(NamedTask&&) = default;
    ~NamedTask() = default;

    template <typename SigOther, typename OtherF>
        requires(std::is_convertible_v<OtherF, CallableImpl> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(const NamedTask<SigOther, OtherF, BoundArgs...>& other) : payload_{other.payload_} {}

    template <typename SigOther, typename OtherF, typename... OtherBoundArgs>
        requires(std::is_convertible_v<OtherF, CallableImpl> &&
                 std::is_convertible_v<typename NamedTask<SigOther, NamedTaskRefTag<OtherF>,
                                                          OtherBoundArgs...>::PayloadType,
                                       PayloadType> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(const NamedTask<SigOther, NamedTaskRefTag<OtherF>, OtherBoundArgs...>& other)
        : payload_{other.payload_} {}

    template <typename SigOther, auto OtherF>
        requires(std::is_convertible_v<decltype(OtherF), CallableImpl> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(const NamedTask<SigOther, StaticFn<OtherF>, BoundArgs...>& other)
        : payload_{.f = static_cast<FunctionTraits<decltype(OtherF)>::func_ptr>(OtherF),
                   .args = other.payload_.args} {}

    template <typename SigOther, auto OtherF, typename... OtherBoundArgs>
        requires(std::is_convertible_v<decltype(OtherF), CallableImpl> &&
                 std::is_convertible_v<typename NamedTask<SigOther, NamedTaskStaticRefTag<OtherF>,
                                                          OtherBoundArgs...>::PayloadType,
                                       PayloadType> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(const NamedTask<SigOther, NamedTaskStaticRefTag<OtherF>, OtherBoundArgs...>& other)
        : payload_{.f = static_cast<FunctionTraits<decltype(OtherF)>::func_ptr>(OtherF),
                   .args = other.payload_.args} {}

    template <typename SigOther, typename OtherF>
        requires(std::is_convertible_v<OtherF, CallableImpl> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(const NamedTask<SigOther, OtherF, BoundArgs...>& other) {
        this->payload_ = other.payload_;
        return *this;
    }

    template <typename SigOther, typename OtherF, typename... OtherBoundArgs>
        requires(std::is_convertible_v<OtherF, CallableImpl> &&
                 std::is_convertible_v<typename NamedTask<SigOther, NamedTaskRefTag<OtherF>,
                                                          OtherBoundArgs...>::PayloadType,
                                       PayloadType> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(
        const NamedTask<SigOther, NamedTaskRefTag<OtherF>, OtherBoundArgs...>& other) {
        this->payload_ = other.payload_;
        return *this;
    }

    template <typename SigOther, auto OtherF>
        requires(std::is_convertible_v<decltype(OtherF), CallableImpl> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(const NamedTask<SigOther, StaticFn<OtherF>, BoundArgs...>& other) {
        this->payload_ = {.f = static_cast<FunctionTraits<decltype(OtherF)>::func_ptr>(OtherF),
                          .args = other.payload_.args};
        return *this;
    }

    template <typename SigOther, auto OtherF, typename... OtherBoundArgs>
        requires(std::is_convertible_v<decltype(OtherF), CallableImpl> &&
                 std::is_convertible_v<typename NamedTask<SigOther, NamedTaskStaticRefTag<OtherF>,
                                                          OtherBoundArgs...>::PayloadType,
                                       PayloadType> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(
        const NamedTask<SigOther, NamedTaskStaticRefTag<OtherF>, OtherBoundArgs...>& other) {
        this->payload_ = {.f = static_cast<FunctionTraits<decltype(OtherF)>::func_ptr>(OtherF),
                          .args = other.payload_.args};
        return *this;
    }

    template <typename SigOther, typename OtherF>
        requires(std::is_convertible_v<OtherF, CallableImpl> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(NamedTask<SigOther, OtherF, BoundArgs...>&& other)
        : payload_{std::move(other).payload_} {}

    template <typename SigOther, typename OtherF, typename... OtherBoundArgs>
        requires(std::is_convertible_v<OtherF, CallableImpl> &&
                 std::is_convertible_v<typename NamedTask<SigOther, NamedTaskRefTag<OtherF>,
                                                          OtherBoundArgs...>::PayloadType,
                                       PayloadType> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(NamedTask<SigOther, NamedTaskRefTag<OtherF>, OtherBoundArgs...>&& other)
        : payload_{std::move(other).payload_} {}

    template <typename SigOther, auto OtherF>
        requires(std::is_convertible_v<decltype(OtherF), CallableImpl> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(NamedTask<SigOther, StaticFn<OtherF>, BoundArgs...>&& other)
        : payload_{.f = static_cast<decltype(OtherF)*>(OtherF),
                   .args = std::move(other).payload_.args} {}

    template <typename SigOther, auto OtherF, typename... OtherBoundArgs>
        requires(std::is_convertible_v<decltype(OtherF), CallableImpl> &&
                 std::is_convertible_v<typename NamedTask<SigOther, NamedTaskStaticRefTag<OtherF>,
                                                          OtherBoundArgs...>::PayloadType,
                                       PayloadType> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(NamedTask<SigOther, NamedTaskStaticRefTag<OtherF>, OtherBoundArgs...>&& other)
        : payload_{.f = static_cast<decltype(OtherF)*>(OtherF),
                   .args = std::move(other).payload_.args} {}

    template <typename SigOther, typename OtherF>
        requires(std::is_convertible_v<OtherF, CallableImpl> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(NamedTask<SigOther, OtherF, BoundArgs...>&& other) {
        this->payload_ = std::move(other).payload_;
        return *this;
    }

    template <typename SigOther, typename OtherF, typename... OtherBoundArgs>
        requires(std::is_convertible_v<OtherF, CallableImpl> &&
                 std::is_convertible_v<typename NamedTask<SigOther, NamedTaskRefTag<OtherF>,
                                                          OtherBoundArgs...>::PayloadType,
                                       PayloadType> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(NamedTask<SigOther, NamedTaskRefTag<OtherF>, OtherBoundArgs...>&& other) {
        this->payload_ = std::move(other).payload_;
        return *this;
    }

    template <typename SigOther, auto OtherF>
        requires(std::is_convertible_v<decltype(OtherF), CallableImpl> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(NamedTask<SigOther, StaticFn<OtherF>, BoundArgs...>&& other) {
        this->payload_ = {.f = static_cast<decltype(OtherF)*>(OtherF),
                          .args = std::move(other).payload_.args};
        return *this;
    }

    template <typename SigOther, auto OtherF, typename... OtherBoundArgs>
        requires(std::is_convertible_v<decltype(OtherF), CallableImpl> &&
                 std::is_convertible_v<typename NamedTask<SigOther, NamedTaskStaticRefTag<OtherF>,
                                                          OtherBoundArgs...>::PayloadType,
                                       PayloadType> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(
        NamedTask<SigOther, NamedTaskStaticRefTag<OtherF>, OtherBoundArgs...>&& other) {
        this->payload_ = {.f = static_cast<decltype(OtherF)*>(OtherF),
                          .args = std::move(other).payload_.args};
        return *this;
    }

    NamedTask(F&& f, BoundArgs&&... args)  // NOLINT
        requires(sizeof...(BoundArgs) > 0 || !IsNamedTask<std::decay_t<F>>::value)
        : payload_{.f = std::forward<F>(f), .args = {std::forward<BoundArgs>(args)...}} {}
};

template <typename S, typename F, typename... BoundArgs>
struct NamedTask<S, NamedTaskRefTag<F>, BoundArgs...>
    : NamedTaskInvoker<NamedTask<S, NamedTaskRefTag<F>, BoundArgs...>,
                       PayloadNamedRef<F, BoundArgs...>, S> {
   private:
    template <typename, typename, typename>
    friend struct NamedTaskInvoker;

    template <typename, typename, typename...>
    friend struct NamedTask;

    using CallableImpl = F;
    using PayloadType = PayloadNamedRef<CallableImpl, BoundArgs...>;
    mutable PayloadType payload_{};

   public:
    using Signature = S;

    [[nodiscard]] static constexpr auto getSize() noexcept -> std::size_t {
        return sizeof(NamedTask);
    }

    template <typename Sig>
    [[nodiscard]] static constexpr auto matchesSignature() noexcept -> bool {
        return std::is_same_v<Signature, Sig>;
    }

    template <typename R, typename... Args>
        requires(sizeof...(Args) > 0)
    [[nodiscard]] static constexpr auto matchesSignature() noexcept -> bool {
        return NamedTask::matchesSignature<R(Args...)>();
    }

    template <std::size_t I>
    constexpr decltype(auto) getBoundArg(this auto&& self) noexcept {
        static_assert(I < sizeof...(BoundArgs), "Exceeds number of captured arguments");
        return PayloadTupleGetter<I, AddRefWrapperToRef_t<BoundArgs>...>::getWrapToRef(
            self.payload_.args);
    }

    NamedTask(const NamedTask&) = default;
    NamedTask& operator=(const NamedTask&) = default;
    NamedTask(NamedTask&&) = default;
    NamedTask& operator=(NamedTask&&) = default;
    ~NamedTask() = default;

    template <typename SigOther, typename OtherF>
        requires(std::is_convertible_v<OtherF, CallableImpl> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(const NamedTask<SigOther, NamedTaskRefTag<OtherF>, BoundArgs...>& other)
        : payload_{other.payload_} {}

    template <typename SigOther, typename OtherF, typename... OtherBoundArgs>
        requires(std::is_convertible_v<OtherF, CallableImpl> &&
                 std::is_convertible_v<
                     typename NamedTask<SigOther, OtherF, OtherBoundArgs...>::PayloadType,
                     PayloadType> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(const NamedTask<SigOther, OtherF, OtherBoundArgs...>& other)
        : payload_{other.payload_} {}

    template <typename SigOther, auto OtherF, typename... OtherBoundArgs>
        requires(std::is_convertible_v<decltype(OtherF), CallableImpl> &&
                 std::is_convertible_v<
                     typename NamedTask<SigOther, StaticFn<OtherF>, OtherBoundArgs...>::PayloadType,
                     PayloadType> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(const NamedTask<SigOther, StaticFn<OtherF>, OtherBoundArgs...>& other)
        : payload_{other.payload_} {}

    template <typename SigOther, auto OtherF>
        requires(std::is_convertible_v<decltype(OtherF), CallableImpl> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(const NamedTask<SigOther, NamedTaskStaticRefTag<OtherF>, BoundArgs...>& other)
        : payload_{.f = static_cast<FunctionTraits<decltype(OtherF)>::func_ptr>(OtherF),
                   .args = other.payload_.args} {}

    template <typename SigOther, typename OtherF>
        requires(std::is_convertible_v<OtherF, CallableImpl> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(const NamedTask<SigOther, NamedTaskRefTag<OtherF>, BoundArgs...>& other) {
        this->payload_ = other.payload_;
        return *this;
    }

    template <typename SigOther, typename OtherF, typename... OtherBoundArgs>
        requires(std::is_convertible_v<OtherF, CallableImpl> &&
                 std::is_convertible_v<
                     typename NamedTask<SigOther, OtherF, OtherBoundArgs...>::PayloadType,
                     PayloadType> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(const NamedTask<SigOther, OtherF, OtherBoundArgs...>& other) {
        this->payload_ = other.payload_;
        return *this;
    }

    template <typename SigOther, auto OtherF, typename... OtherBoundArgs>
        requires(std::is_convertible_v<decltype(OtherF), CallableImpl> &&
                 std::is_convertible_v<
                     typename NamedTask<SigOther, StaticFn<OtherF>, OtherBoundArgs...>::PayloadType,
                     PayloadType> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(const NamedTask<SigOther, StaticFn<OtherF>, OtherBoundArgs...>& other) {
        this->payload_ = other.payload_;
        return *this;
    }

    template <typename SigOther, auto OtherF>
        requires(std::is_convertible_v<decltype(OtherF), CallableImpl> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(
        const NamedTask<SigOther, NamedTaskStaticRefTag<OtherF>, BoundArgs...>& other) {
        this->payload_ = {.f = static_cast<FunctionTraits<decltype(OtherF)>::func_ptr>(OtherF),
                          .args = other.payload_.args};
        return *this;
    }

    template <typename SigOther, typename OtherF>
        requires(std::is_convertible_v<OtherF, CallableImpl> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(NamedTask<SigOther, NamedTaskRefTag<OtherF>, BoundArgs...>&& other)
        : payload_{std::move(other).payload_} {}

    template <typename SigOther, typename OtherF, typename... OtherBoundArgs>
        requires(std::is_convertible_v<OtherF, CallableImpl> &&
                 std::is_convertible_v<
                     typename NamedTask<SigOther, OtherF, OtherBoundArgs...>::PayloadType,
                     PayloadType> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(NamedTask<SigOther, OtherF, OtherBoundArgs...>&& other)
        : payload_{std::move(other).payload_} {}

    template <typename SigOther, auto OtherF>
        requires(std::is_convertible_v<decltype(OtherF), CallableImpl> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(NamedTask<SigOther, NamedTaskStaticRefTag<OtherF>, BoundArgs...>&& other)
        : payload_{.f = static_cast<decltype(OtherF)*>(OtherF),
                   .args = std::move(other).payload_.args} {}

    template <typename SigOther, auto OtherF, typename... OtherBoundArgs>
        requires(std::is_convertible_v<decltype(OtherF), CallableImpl> &&
                 std::is_convertible_v<
                     typename NamedTask<SigOther, StaticFn<OtherF>, OtherBoundArgs...>::PayloadType,
                     PayloadType> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(NamedTask<SigOther, StaticFn<OtherF>, OtherBoundArgs...>&& other)
        : payload_{.f = static_cast<decltype(OtherF)*>(OtherF),
                   .args = std::move(other).payload_.args} {}

    template <typename SigOther, typename OtherF>
        requires(std::is_convertible_v<OtherF, CallableImpl> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(NamedTask<SigOther, NamedTaskRefTag<OtherF>, BoundArgs...>&& other) {
        this->payload_ = std::move(other).payload_;
        return *this;
    }

    template <typename SigOther, typename OtherF, typename... OtherBoundArgs>
        requires(std::is_convertible_v<OtherF, CallableImpl> &&
                 std::is_convertible_v<
                     typename NamedTask<SigOther, OtherF, OtherBoundArgs...>::PayloadType,
                     PayloadType> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(NamedTask<SigOther, OtherF, OtherBoundArgs...>&& other) {
        this->payload_ = std::move(other).payload_;
        return *this;
    }

    template <typename SigOther, auto OtherF>
        requires(std::is_convertible_v<decltype(OtherF), CallableImpl> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(NamedTask<SigOther, NamedTaskStaticRefTag<OtherF>, BoundArgs...>&& other) {
        this->payload_ = {.f = static_cast<decltype(OtherF)*>(OtherF),
                          .args = std::move(other).payload_.args};
        return *this;
    }

    template <typename SigOther, auto OtherF, typename... OtherBoundArgs>
        requires(std::is_convertible_v<decltype(OtherF), CallableImpl> &&
                 std::is_convertible_v<
                     typename NamedTask<SigOther, StaticFn<OtherF>, OtherBoundArgs...>::PayloadType,
                     PayloadType> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(NamedTask<SigOther, StaticFn<OtherF>, OtherBoundArgs...>&& other) {
        this->payload_ = {.f = static_cast<decltype(OtherF)*>(OtherF),
                          .args = std::move(other).payload_.args};
        return *this;
    }

    NamedTask(F&& f, BoundArgs&&... args)  // NOLINT
        requires(sizeof...(BoundArgs) > 0 || !IsNamedTask<std::decay_t<F>>::value)
        : payload_{.f = std::forward<F>(f), .args = {std::forward<BoundArgs>(args)...}} {}

    // For deductors
    NamedTask([[maybe_unused]] NamedTaskRefTag<F> /*unused*/, F&& f, BoundArgs&&... args)  // NOLINT
        requires(sizeof...(BoundArgs) > 0 || !IsNamedTask<std::decay_t<F>>::value)
        : payload_{.f = std::forward<F>(f), .args = {std::forward<BoundArgs>(args)...}} {}
};

template <typename S, auto F, typename... BoundArgs>
struct NamedTask<S, StaticFn<F>, BoundArgs...>
    : NamedTaskInvoker<NamedTask<S, StaticFn<F>, BoundArgs...>, PayloadT<decltype(F), BoundArgs...>,
                       S> {
   private:
    template <typename, typename, typename>
    friend struct NamedTaskInvoker;

    template <typename, typename, typename...>
    friend struct NamedTask;

    using PayloadType = PayloadArgsT<F, BoundArgs...>;
    mutable PayloadType payload_;

   public:
    using Signature = S;

    [[nodiscard]] static constexpr auto getSize() noexcept -> std::size_t {
        return sizeof(NamedTask);
    }

    template <typename Sig>
    [[nodiscard]] static constexpr auto matchesSignature() noexcept -> bool {
        return std::is_same_v<Signature, Sig>;
    }

    template <typename R, typename... Args>
        requires(sizeof...(Args) > 0)
    [[nodiscard]] static constexpr auto matchesSignature() noexcept -> bool {
        return NamedTask::matchesSignature<R(Args...)>();
    }

    template <std::size_t I>
    constexpr decltype(auto) getBoundArg(this auto&& self) noexcept {
        static_assert(I < sizeof...(BoundArgs), "Exceeds number of captured arguments");
        return PayloadTupleGetter<I, std::decay_t<BoundArgs>...>::get(self.payload_.args);
    }

    NamedTask(const NamedTask&) = default;
    NamedTask& operator=(const NamedTask&) = default;
    NamedTask(NamedTask&&) = default;
    NamedTask& operator=(NamedTask&&) = default;
    ~NamedTask() = default;

    template <typename SigOther>
        requires(IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(const NamedTask<SigOther, StaticFn<F>, BoundArgs...>& other)
        : payload_{.args = other.payload_.args} {}

    template <typename SigOther, typename... OtherBoundArgs>
        requires(IsSignatureCompatible<SigOther, Signature>::value &&
                 std::is_convertible_v<typename NamedTask<SigOther, NamedTaskStaticRefTag<F>,
                                                          OtherBoundArgs...>::PayloadType,
                                       PayloadType>)
    NamedTask(const NamedTask<SigOther, NamedTaskStaticRefTag<F>, OtherBoundArgs...>& other)
        : payload_{.args = other.payload_.args} {}

    template <typename SigOther>
        requires(IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(const NamedTask<SigOther, StaticFn<F>, BoundArgs...>& other) {
        this->payload_.args = other.payload_.args;
        return *this;
    }

    template <typename SigOther, typename... OtherBoundArgs>
        requires(IsSignatureCompatible<SigOther, Signature>::value &&
                 std::is_convertible_v<typename NamedTask<SigOther, NamedTaskStaticRefTag<F>,
                                                          OtherBoundArgs...>::PayloadType,
                                       PayloadType>)
    NamedTask& operator=(
        const NamedTask<SigOther, NamedTaskStaticRefTag<F>, OtherBoundArgs...>& other) {
        this->payload_.args = other.payload_.args;
        return *this;
    }

    template <typename SigOther>
        requires(IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(NamedTask<SigOther, StaticFn<F>, BoundArgs...>&& other)
        : payload_{.args = std::move(other).payload_.args} {}

    template <typename SigOther, typename... OtherBoundArgs>
        requires(IsSignatureCompatible<SigOther, Signature>::value &&
                 std::is_convertible_v<typename NamedTask<SigOther, NamedTaskStaticRefTag<F>,
                                                          OtherBoundArgs...>::PayloadType,
                                       PayloadType>)
    NamedTask(NamedTask<SigOther, NamedTaskStaticRefTag<F>, OtherBoundArgs...>&& other)
        : payload_{.args = std::move(other).payload_.args} {}

    template <typename SigOther>
        requires(IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(NamedTask<SigOther, StaticFn<F>, BoundArgs...>&& other) {
        this->payload.args = std::move(other).payload.args;
        return *this;
    }

    template <typename SigOther, typename... OtherBoundArgs>
        requires(IsSignatureCompatible<SigOther, Signature>::value &&
                 std::is_convertible_v<typename NamedTask<SigOther, NamedTaskStaticRefTag<F>,
                                                          OtherBoundArgs...>::PayloadType,
                                       PayloadType>)
    NamedTask& operator=(NamedTask<SigOther, NamedTaskStaticRefTag<F>, OtherBoundArgs...>&& other) {
        this->payload.args = std::move(other).payload.args;
        return *this;
    }

    NamedTask(BoundArgs&&... args) : payload_{{std::forward<BoundArgs>(args)...}} {}
};

template <typename S, auto F, typename... BoundArgs>
struct NamedTask<S, NamedTaskStaticRefTag<F>, BoundArgs...>
    : NamedTaskInvoker<NamedTask<S, NamedTaskStaticRefTag<F>, BoundArgs...>,
                       PayloadNamedRef<decltype(F), BoundArgs...>, S> {
   private:
    template <typename, typename, typename>
    friend struct NamedTaskInvoker;

    template <typename, typename, typename...>
    friend struct NamedTask;

    using PayloadType = PayloadStaticNamedRef<F, BoundArgs...>;
    mutable PayloadType payload_;

   public:
    using Signature = S;

    [[nodiscard]] static constexpr auto getSize() noexcept -> std::size_t {
        return sizeof(NamedTask);
    }

    template <typename Sig>
    [[nodiscard]] static constexpr auto matchesSignature() noexcept -> bool {
        return std::is_same_v<Signature, Sig>;
    }

    template <typename R, typename... Args>
        requires(sizeof...(Args) > 0)
    [[nodiscard]] static constexpr auto matchesSignature() noexcept -> bool {
        return NamedTask::matchesSignature<R(Args...)>();
    }

    template <std::size_t I>
    constexpr decltype(auto) getBoundArg(this auto&& self) noexcept {
        static_assert(I < sizeof...(BoundArgs), "Exceeds number of captured arguments");
        return PayloadTupleGetter<I, AddRefWrapperToRef_t<BoundArgs>...>::getWrapToRef(
            self.payload_.args);
    }

    NamedTask(const NamedTask&) = default;
    NamedTask& operator=(const NamedTask&) = default;
    NamedTask(NamedTask&&) = default;
    NamedTask& operator=(NamedTask&&) = default;
    ~NamedTask() = default;

    template <typename SigOther>
        requires(IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(const NamedTask<SigOther, NamedTaskStaticRefTag<F>, BoundArgs...>& other)
        : payload_{.args = other.payload_.args} {}

    template <typename SigOther, typename... OtherBoundArgs>
        requires(IsSignatureCompatible<SigOther, Signature>::value &&
                 std::is_convertible_v<
                     typename NamedTask<SigOther, StaticFn<F>, OtherBoundArgs...>::PayloadType,
                     PayloadType>)
    NamedTask(const NamedTask<SigOther, StaticFn<F>, OtherBoundArgs...>& other)
        : payload_{.args = other.payload_.args} {}

    template <typename SigOther>
        requires(IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(const NamedTask<SigOther, NamedTaskStaticRefTag<F>, BoundArgs...>& other) {
        this->payload_.args = other.payload_.args;
        return *this;
    }

    template <typename SigOther, typename... OtherBoundArgs>
        requires(IsSignatureCompatible<SigOther, Signature>::value &&
                 std::is_convertible_v<
                     typename NamedTask<SigOther, StaticFn<F>, OtherBoundArgs...>::PayloadType,
                     PayloadType>)
    NamedTask& operator=(const NamedTask<SigOther, StaticFn<F>, OtherBoundArgs...>& other) {
        this->payload_.args = other.payload_.args;
        return *this;
    }

    template <typename SigOther>
        requires(IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(NamedTask<SigOther, NamedTaskStaticRefTag<F>, BoundArgs...>&& other)
        : payload_{.args = std::move(other).payload_.args} {}

    template <typename SigOther, typename... OtherBoundArgs>
        requires(IsSignatureCompatible<SigOther, Signature>::value &&
                 std::is_convertible_v<
                     typename NamedTask<SigOther, StaticFn<F>, OtherBoundArgs...>::PayloadType,
                     PayloadType>)
    NamedTask(NamedTask<SigOther, StaticFn<F>, OtherBoundArgs...>&& other)
        : payload_{.args = std::move(other).payload_.args} {}

    template <typename SigOther>
        requires(IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(NamedTask<SigOther, NamedTaskStaticRefTag<F>, BoundArgs...>&& other) {
        this->payload.args = std::move(other).payload.args;
        return *this;
    }

    template <typename SigOther, typename... OtherBoundArgs>
        requires(IsSignatureCompatible<SigOther, Signature>::value &&
                 std::is_convertible_v<
                     typename NamedTask<SigOther, StaticFn<F>, OtherBoundArgs...>::PayloadType,
                     PayloadType>)
    NamedTask& operator=(NamedTask<SigOther, StaticFn<F>, OtherBoundArgs...>&& other) {
        this->payload_.args = std::move(other).payload_.args;
        return *this;
    }

    NamedTask(BoundArgs&&... args) : payload_{{std::forward<BoundArgs>(args)...}} {}

    // For deductors
    NamedTask([[maybe_unused]] NamedTaskStaticRefTag<F> /*unused*/, BoundArgs&&... args)  // NOLINT
        : payload_{{std::forward<BoundArgs>(args)...}} {}
};

template <typename F, typename... BoundArgs>
    requires(HasFunctionTraits<std::decay_t<F>>)
struct NamedTaskTypeDeductor {
    struct StripBoundArgs {
        using Traits = FunctionTraits<std::decay_t<F>>;
        using AllArgs = typename Traits::args_tuple;
        using InternalFuncPtr = typename Traits::func_ptr;

        static constexpr std::size_t kRemainingCount =
            std::tuple_size_v<AllArgs> - sizeof...(BoundArgs);

        using SlicedTuple = typename slice_first<kRemainingCount, AllArgs>::type;
        using SlicedTupleReverse = typename slice_last<kRemainingCount, AllArgs>::type;

        template <typename Ret, typename Tuple, typename TupleReverse>
        struct rebuild;
        template <typename Ret, typename... Ts, typename... TsR>
        struct rebuild<Ret, std::tuple<Ts...>, std::tuple<TsR...>> {
            static constexpr bool kIsValid =
                is_compatible_invocable_v<std::decay_t<F>, Ts...,
                                          std::unwrap_ref_decay_t<BoundArgs>&&...> ||
                is_compatible_invocable_v<std::decay_t<F>, std::unwrap_ref_decay_t<BoundArgs>&&...,
                                          TsR...>;
            static constexpr bool kIsNormalOrder =
                is_compatible_invocable_v<std::decay_t<F>&&, Ts...,
                                          std::unwrap_ref_decay_t<BoundArgs>&&...>;
            static constexpr bool kIsReverse = !kIsNormalOrder;
            using R = Ret;
            using Args = std::conditional_t<kIsReverse, std::tuple<TsR...>, std::tuple<Ts...>>;
            static constexpr bool kIsConst =
                Traits::kIsConst &&
                    (kIsNormalOrder &&
                     std::is_invocable_v<const std::decay_t<F>, Ts...,
                                         const std::unwrap_reference_t<typename RefToConstRef<
                                             std::decay_t<BoundArgs>>::Type>...>) ||
                (!kIsNormalOrder &&
                 std::is_invocable_v<const std::decay_t<F>,
                                     const std::unwrap_reference_t<
                                         typename RefToConstRef<std::decay_t<BoundArgs>>::Type>...,
                                     TsR...>);
            static constexpr bool kIsRValRef =
                Traits::kIsRValueRef &&
                    (kIsNormalOrder &&
                     std::is_invocable_v<std::decay_t<F>&&, Ts...,
                                         std::unwrap_reference_t<typename RefToConstRef<
                                             std::decay_t<BoundArgs>>::Type>&&...>) ||
                (!kIsNormalOrder &&
                 std::is_invocable_v<std::decay_t<F>&&,
                                     std::unwrap_reference_t<typename RefToConstRef<
                                         std::decay_t<BoundArgs>>::Type>&&...,
                                     TsR...>);
            static constexpr bool kIsLValRef =
                Traits::kIsLValueRef &&
                    (kIsNormalOrder &&
                     std::is_invocable_v<std::decay_t<F>&, Ts...,
                                         std::unwrap_reference_t<typename RefToConstRef<
                                             std::decay_t<BoundArgs>>::Type>&...>) ||
                (!kIsNormalOrder &&
                 std::is_invocable_v<std::decay_t<F>&,
                                     std::unwrap_reference_t<
                                         typename RefToConstRef<std::decay_t<BoundArgs>>::Type>&...,
                                     TsR...>);
            using Signature = std::conditional_t<
                Traits::kIsNoExcept,
                // --- NOEXCEPT BRANCH ---
                std::conditional_t<
                    kIsReverse,
                    std::conditional_t<
                        kIsConst,
                        std::conditional_t<
                            kIsRValRef, Ret(TsR...) const && noexcept,
                            std::conditional_t<kIsLValRef, Ret(TsR...) const & noexcept,
                                               Ret(TsR...) const noexcept>>,
                        std::conditional_t<kIsRValRef, Ret(TsR...) && noexcept,
                                           std::conditional_t<kIsLValRef, Ret(TsR...) & noexcept,
                                                              Ret(TsR...) noexcept>>>,
                    std::conditional_t<
                        kIsConst,
                        std::conditional_t<
                            kIsRValRef, Ret(Ts...) const && noexcept,
                            std::conditional_t<kIsLValRef, Ret(Ts...) const & noexcept,
                                               Ret(Ts...) const noexcept>>,
                        std::conditional_t<kIsRValRef, Ret(Ts...) && noexcept,
                                           std::conditional_t<kIsLValRef, Ret(Ts...) & noexcept,
                                                              Ret(Ts...) noexcept>>>>,
                // --- NON-NOEXCEPT BRANCH ---
                std::conditional_t<
                    kIsReverse,
                    std::conditional_t<
                        kIsConst,
                        std::conditional_t<
                            kIsRValRef, Ret(TsR...) const&&,
                            std::conditional_t<kIsLValRef, Ret(TsR...) const&, Ret(TsR...) const>>,
                        std::conditional_t<
                            kIsRValRef, Ret(TsR...) &&,
                            std::conditional_t<kIsLValRef, Ret(TsR...) &, Ret(TsR...)>>>,
                    std::conditional_t<
                        kIsConst,
                        std::conditional_t<
                            kIsRValRef, Ret(Ts...) const&&,
                            std::conditional_t<kIsLValRef, Ret(Ts...) const&, Ret(Ts...) const>>,
                        std::conditional_t<
                            kIsRValRef, Ret(Ts...) &&,
                            std::conditional_t<kIsLValRef, Ret(Ts...) &, Ret(Ts...)>>>>>;
        };

        using type = rebuild<typename Traits::return_type, SlicedTuple, SlicedTupleReverse>;
    };

    // To complete one needs case where F is static (no decay), but in that case it would be trivial
    // anyway, hence not urgent to change for now
    using TypeTraits = StripBoundArgs::type;
    using Signature = typename TypeTraits::Signature;
    static constexpr bool kIsValid = TypeTraits::kIsValid;

    using Type = NamedTask<Signature, F, BoundArgs...>;
};

template <auto F_auto, typename... BoundArgs>
    requires(HasFunctionTraits<decltype(F_auto)>)
struct NamedTaskTypeDeductorAuto {
    struct StripBoundArgs {
        using F = decltype(F_auto);
        using Traits = FunctionTraits<std::decay_t<F>>;
        using AllArgs = typename Traits::args_tuple;

        static constexpr std::size_t kRemainingCount =
            std::tuple_size_v<AllArgs> - sizeof...(BoundArgs);

        using SlicedTuple = typename slice_first<kRemainingCount, AllArgs>::type;
        using SlicedTupleReverse = typename slice_last<kRemainingCount, AllArgs>::type;

        template <typename Ret, typename Tuple, typename TupleReverse>
        struct rebuild;
        template <typename Ret, typename... Ts, typename... TsR>
        struct rebuild<Ret, std::tuple<Ts...>, std::tuple<TsR...>> {
            static constexpr bool kIsValid =
                is_compatible_invocable_v<std::decay_t<F>, Ts...,
                                          std::unwrap_ref_decay_t<BoundArgs>&&...> ||
                is_compatible_invocable_v<std::decay_t<F>, std::unwrap_ref_decay_t<BoundArgs>&&...,
                                          TsR...>;
            static constexpr bool kIsNormalOrder =
                is_compatible_invocable_v<std::decay_t<F>, Ts...,
                                          std::unwrap_ref_decay_t<BoundArgs>&&...>;
            static constexpr bool kIsReverse = !kIsNormalOrder;
            using R = Ret;
            using Args = std::conditional_t<kIsReverse, std::tuple<TsR...>, std::tuple<Ts...>>;
            static constexpr bool kIsConst =
                Traits::kIsConst &&
                ((kIsNormalOrder &&
                  std::is_invocable_v<const std::decay_t<F>, Ts...,
                                      const std::unwrap_reference_t<typename RefToConstRef<
                                          std::decay_t<BoundArgs>>::Type>...>) ||
                 (!kIsNormalOrder &&
                  std::is_invocable_v<const std::decay_t<F>,
                                      const std::unwrap_reference_t<
                                          typename RefToConstRef<std::decay_t<BoundArgs>>::Type>...,
                                      TsR...>));
            static constexpr bool kIsRValRef =
                Traits::kIsRValueRef &&
                ((kIsNormalOrder &&
                  std::is_invocable_v<std::decay_t<F>&&, Ts...,
                                      std::unwrap_reference_t<typename RefToConstRef<
                                          std::decay_t<BoundArgs>>::Type>&&...>) ||
                 (!kIsNormalOrder &&
                  std::is_invocable_v<std::decay_t<F>&&,
                                      std::unwrap_reference_t<typename RefToConstRef<
                                          std::decay_t<BoundArgs>>::Type>&&...,
                                      TsR...>));
            static constexpr bool kIsLValRef =
                Traits::kIsLValueRef &&
                ((kIsNormalOrder &&
                  std::is_invocable_v<const std::decay_t<F>&, Ts...,
                                      std::unwrap_reference_t<typename RefToConstRef<
                                          std::decay_t<BoundArgs>>::Type>&...>) ||
                 (!kIsNormalOrder &&
                  std::is_invocable_v<const std::decay_t<F>&,
                                      std::unwrap_reference_t<typename RefToConstRef<
                                          std::decay_t<BoundArgs>>::Type>&...,
                                      TsR...>));
            static constexpr bool kIsQualifierLess = !kIsRValRef && !kIsLValRef && kIsConst;

            using Signature = std::conditional_t<
                Traits::kIsNoExcept,
                // --- NOEXCEPT BRANCH ---
                std::conditional_t<
                    kIsReverse,
                    std::conditional_t<
                        kIsConst,
                        std::conditional_t<
                            kIsRValRef, Ret(TsR...) const && noexcept,
                            std::conditional_t<kIsLValRef, Ret(TsR...) const & noexcept,
                                               Ret(TsR...) const noexcept>>,
                        std::conditional_t<kIsRValRef, Ret(TsR...) && noexcept,
                                           std::conditional_t<kIsLValRef, Ret(TsR...) & noexcept,
                                                              Ret(TsR...) noexcept>>>,
                    std::conditional_t<
                        kIsConst,
                        std::conditional_t<
                            kIsRValRef, Ret(Ts...) const && noexcept,
                            std::conditional_t<kIsLValRef, Ret(Ts...) const & noexcept,
                                               Ret(Ts...) const noexcept>>,
                        std::conditional_t<kIsRValRef, Ret(Ts...) && noexcept,
                                           std::conditional_t<kIsLValRef, Ret(Ts...) & noexcept,
                                                              Ret(Ts...) noexcept>>>>,
                // --- NON-NOEXCEPT BRANCH ---
                std::conditional_t<
                    kIsReverse,
                    std::conditional_t<
                        kIsConst,
                        std::conditional_t<
                            kIsRValRef, Ret(TsR...) const&&,
                            std::conditional_t<kIsLValRef, Ret(TsR...) const&, Ret(TsR...) const>>,
                        std::conditional_t<
                            kIsRValRef, Ret(TsR...) &&,
                            std::conditional_t<kIsLValRef, Ret(TsR...) &, Ret(TsR...)>>>,
                    std::conditional_t<
                        kIsConst,
                        std::conditional_t<
                            kIsRValRef, Ret(Ts...) const&&,
                            std::conditional_t<kIsLValRef, Ret(Ts...) const&, Ret(Ts...) const>>,
                        std::conditional_t<
                            kIsRValRef, Ret(Ts...) &&,
                            std::conditional_t<kIsLValRef, Ret(Ts...) &, Ret(Ts...)>>>>>;
        };

        using type = rebuild<typename Traits::return_type, SlicedTuple, SlicedTupleReverse>;
    };

    // To complete one needs case where F is static (no decay), but in that case it would be trivial
    // anyway, hence not urgent to change for now
    using TypeTraits = StripBoundArgs::type;
    using Signature = typename TypeTraits::Signature;
    static constexpr bool kIsValid = TypeTraits::kIsValid;

    using Type = NamedTask<Signature, StaticFn<F_auto>, BoundArgs...>;
};

template <typename F, typename... BoundArgs>
NamedTask(F&&, BoundArgs&&...)
    -> NamedTask<typename NamedTaskTypeDeductor<F, BoundArgs...>::Signature, F, BoundArgs...>;

template <typename F, typename... BoundArgs>
NamedTask(NamedTaskRefTag<F>, F&&, BoundArgs&&...)
    -> NamedTask<typename NamedTaskTypeDeductor<F, BoundArgs...>::Signature, NamedTaskRefTag<F>,
                 BoundArgs...>;

template <auto F, typename... BoundArgs>
NamedTask(NamedTaskStaticRefTag<F>, BoundArgs&&...)
    -> NamedTask<typename NamedTaskTypeDeductor<decltype(F), BoundArgs...>::Signature,
                 NamedTaskStaticRefTag<F>, BoundArgs...>;

template <auto F, typename... BoundArgs>
    requires HasFunctionTraits<decltype(F)> && NamedTaskTypeDeductorAuto<F, BoundArgs...>::kIsValid
FORCE_INLINE auto makeNamedTask(BoundArgs&&... args) {
    return NamedTask<typename NamedTaskTypeDeductorAuto<F, BoundArgs...>::Signature, StaticFn<F>,
                     BoundArgs...>{std::forward<BoundArgs>(args)...};
}

template <auto F, typename... BoundArgs>
    requires HasFunctionTraits<decltype(F)> &&
             NamedTaskTypeDeductorAuto<F, AddRefWrapperToRef_t<BoundArgs>...>::kIsValid
FORCE_INLINE auto makeRefNamedTask(BoundArgs&&... args) {
    return NamedTask<
        typename NamedTaskTypeDeductorAuto<F, AddRefWrapperToRef_t<BoundArgs>...>::Signature,
        NamedTaskStaticRefTag<F>, AddRefWrapperToRef_t<BoundArgs>...>{
        NamedTaskStaticRefTag<F>{}, std::forward<BoundArgs>(args)...};
}

template <typename F>
    requires(requires { typename FunctionTraits<std::decay_t<F>>::func_ptr; } &&
             std::is_convertible_v<F, typename FunctionTraits<std::decay_t<F>>::func_ptr>)
FORCE_INLINE auto toFuncPtr(F&& f) {
    return typename FunctionTraits<std::decay_t<F>>::func_ptr{std::forward<F>(f)};
}

template <auto F>
    requires(requires { typename FunctionTraits<std::decay_t<decltype(F)>>::func_ptr; } &&
             std::is_convertible_v<decltype(F),
                                   typename FunctionTraits<std::decay_t<decltype(F)>>::func_ptr>)
FORCE_INLINE auto toFuncPtr() {
    return static_cast<typename FunctionTraits<std::decay_t<decltype(F)>>::func_ptr>(F);
}

template <bool StoreFuncAsPtr = false, typename F, typename... BoundArgs>
    requires(!StoreFuncAsPtr || (requires {
        typename FunctionTraits<std::decay_t<F>>::func_ptr;
    } && std::is_convertible_v<F, typename FunctionTraits<std::decay_t<F>>::func_ptr>))
FORCE_INLINE auto makeNamedTask(F&& f, BoundArgs&&... args) {
    if constexpr (StoreFuncAsPtr) {
        using func_ptr = typename FunctionTraits<std::decay_t<F>>::func_ptr;
        return NamedTask<typename NamedTaskTypeDeductor<func_ptr, BoundArgs...>::Signature,
                         func_ptr, BoundArgs...>{std::forward<F>(f),
                                                 std::forward<BoundArgs>(args)...};
    } else {
        return NamedTask<typename NamedTaskTypeDeductor<F, BoundArgs...>::Signature, F,
                         BoundArgs...>{std::forward<F>(f), std::forward<BoundArgs>(args)...};
    }
}

template <bool StoreFuncAsPtr = false, typename F, typename... BoundArgs>
    requires(!StoreFuncAsPtr || (requires {
        typename FunctionTraits<std::decay_t<F>>::func_ptr;
    } && std::is_convertible_v<F, typename FunctionTraits<std::decay_t<F>>::func_ptr>))
FORCE_INLINE auto makeRefNamedTask(F&& f, BoundArgs&&... args) {
    if constexpr (StoreFuncAsPtr) {
        using func_ptr = typename FunctionTraits<std::decay_t<F>>::func_ptr;
        return NamedTask<
            typename NamedTaskTypeDeductor<func_ptr, AddRefWrapperToRef_t<BoundArgs>...>::Signature,
            NamedTaskRefTag<func_ptr>, BoundArgs...>{NamedTaskRefTag<F>{}, std::forward<F>(f),
                                                     std::forward<BoundArgs>(args)...};
    } else {
        return NamedTask<
            typename NamedTaskTypeDeductor<F, AddRefWrapperToRef_t<BoundArgs>...>::Signature,
            NamedTaskRefTag<F>, BoundArgs...>{NamedTaskRefTag<F>{}, std::forward<F>(f),
                                              std::forward<BoundArgs>(args)...};
    }
}

template <typename T>
concept ErasableToPointer = requires(T t) { static_cast<void*>(t); };

template <typename F, typename BoundArg>
    requires(requires { typename FunctionTraits<std::decay_t<F>>::func_ptr; } &&
             std::is_convertible_v<F, typename FunctionTraits<std::decay_t<F>>::func_ptr>)
FORCE_INLINE auto makeThisTask(F&& f, BoundArg&& arg)
    requires ErasableToPointer<decltype(arg)>
{
    using ptr = typename FunctionTraits<std::decay_t<F>>::func_ptr;
    return NamedTask<typename NamedTaskTypeDeductor<ptr, void*>::Signature, ptr, void*>{
        std::forward<F>(f), std::forward<BoundArg>(arg)};
}

template <typename F, typename BoundArg>
    requires(requires { typename FunctionTraits<std::decay_t<F>>::func_ptr; } &&
             std::is_convertible_v<F, typename FunctionTraits<std::decay_t<F>>::func_ptr>)
FORCE_INLINE auto makeThisRefTask(F&& f, BoundArg&& arg)
    requires ErasableToPointer<decltype(arg)>
{
    using ptr = typename FunctionTraits<std::decay_t<F>>::func_ptr;
    return NamedTask<typename NamedTaskTypeDeductor<ptr, void*>::Signature, NamedTaskRefTag<ptr>,
                     void*>{NamedTaskRefTag<F>{}, std::forward<F>(f), std::forward<BoundArg>(arg)};
}

template <auto F, typename BoundArg>
    requires(requires { typename FunctionTraits<std::decay_t<decltype(F)>>::func_ptr; } &&
             std::is_convertible_v<decltype(F),
                                   typename FunctionTraits<std::decay_t<decltype(F)>>::func_ptr>)
FORCE_INLINE auto makeThisTask(BoundArg&& arg)
    requires ErasableToPointer<decltype(arg)>
{
    using ptr = typename FunctionTraits<std::decay_t<decltype(F)>>::func_ptr;
    return NamedTask<typename NamedTaskTypeDeductor<ptr, void*>::Signature, ptr, void*>{
        static_cast<ptr>(F), std::forward<BoundArg>(arg)};
}

}  // namespace fn

#endif  // TASK_NAMED_TASK_HPP