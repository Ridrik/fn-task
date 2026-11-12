#ifndef TASK_NAMED_TASK_HPP
#define TASK_NAMED_TASK_HPP

#include <algorithm>
#include <cstddef>
#include <type_traits>

#include "deductors.hpp"
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
    using UnwrapF = std::remove_reference_t<std::unwrap_ref_decay_t<F>>;
    static_assert(
        is_compatible_invocable_v<
            UnwrapF, std::remove_reference_t<std::unwrap_ref_decay_t<BoundArgs>>&&..., Args...> ||
        is_compatible_invocable_v<
            UnwrapF, Args..., std::remove_reference_t<std::unwrap_ref_decay_t<BoundArgs>>&&...>);
    static constexpr bool kIsFront = is_compatible_invocable_v<
        UnwrapF, std::remove_reference_t<std::unwrap_ref_decay_t<BoundArgs>>&&..., Args...>;
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
    using UnwrapF = std::remove_reference_t<std::unwrap_ref_decay_t<F>>;
    static_assert(
        is_compatible_invocable_v<
            UnwrapF, std::remove_reference_t<std::unwrap_ref_decay_t<BoundArgs>>&&..., Args...> ||
        is_compatible_invocable_v<
            UnwrapF, Args..., std::remove_reference_t<std::unwrap_ref_decay_t<BoundArgs>>&&...>);
    static constexpr bool kIsFront = is_compatible_invocable_v<
        UnwrapF, std::remove_reference_t<std::unwrap_ref_decay_t<BoundArgs>>&&..., Args...>;
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
    using UnwrapF = std::remove_reference_t<std::unwrap_ref_decay_t<F>>;
    static_assert(std::is_invocable_v<const UnwrapF, const BoundArgs..., Args...> ||
                  std::is_invocable_v<const UnwrapF, Args..., const BoundArgs...>);
    static constexpr bool kIsFront =
        std::is_invocable_v<const UnwrapF, const BoundArgs..., Args...>;
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
    using UnwrapF = std::remove_reference_t<std::unwrap_ref_decay_t<F>>;
    static_assert(std::is_invocable_v<const UnwrapF, const BoundArgs..., Args...> ||
                  std::is_invocable_v<const UnwrapF, Args..., const BoundArgs...>);
    static constexpr bool kIsFront =
        std::is_invocable_v<const UnwrapF, const BoundArgs..., Args...>;
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
    using UnwrapF = std::remove_reference_t<std::unwrap_ref_decay_t<F>>;
    static_assert(std::is_invocable_v<const F&, const BoundArgs&..., Args...> ||
                  std::is_invocable_v<const F&, Args..., const BoundArgs&...>);
    static constexpr bool kIsFront = std::is_invocable_v<const F&, const BoundArgs&..., Args...>;
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
    using UnwrapF = std::remove_reference_t<std::unwrap_ref_decay_t<F>>;
    static_assert(std::is_invocable_v<const UnwrapF&, const BoundArgs&..., Args...> ||
                  std::is_invocable_v<const UnwrapF&, Args..., const BoundArgs&...>);
    static constexpr bool kIsFront =
        std::is_invocable_v<const UnwrapF&, const BoundArgs&..., Args...>;
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
    using UnwrapF = std::remove_reference_t<std::unwrap_ref_decay_t<F>>;
    static_assert(std::is_invocable_v<UnwrapF&, BoundArgs&..., Args...> ||
                  std::is_invocable_v<UnwrapF&, Args..., BoundArgs&...>);
    static constexpr bool kIsFront = std::is_invocable_v<UnwrapF&, BoundArgs&..., Args...>;
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
    using UnwrapF = std::remove_reference_t<std::unwrap_ref_decay_t<F>>;
    static_assert(std::is_invocable_v<UnwrapF&, BoundArgs&..., Args...> ||
                  std::is_invocable_v<UnwrapF&, Args..., BoundArgs&...>);
    static constexpr bool kIsFront = std::is_invocable_v<UnwrapF&, BoundArgs&..., Args...>;
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
    using UnwrapF = std::remove_reference_t<std::unwrap_ref_decay_t<F>>;
    static_assert(
        std::is_invocable_v<
            UnwrapF&&, std::remove_reference_t<std::unwrap_ref_decay_t<BoundArgs>>&&..., Args...> ||
        std::is_invocable_v<UnwrapF&&, Args...,
                            std::remove_reference_t<std::unwrap_ref_decay_t<BoundArgs>>&&...>);
    static constexpr bool kIsFront = std::is_invocable_v<
        UnwrapF&&, std::remove_reference_t<std::unwrap_ref_decay_t<BoundArgs>>&&..., Args...>;
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
    using UnwrapF = std::remove_reference_t<std::unwrap_ref_decay_t<F>>;
    static_assert(
        std::is_invocable_v<
            UnwrapF&&, std::remove_reference_t<std::unwrap_ref_decay_t<BoundArgs>>&&..., Args...> ||
        std::is_invocable_v<UnwrapF&&, Args...,
                            std::remove_reference_t<std::unwrap_ref_decay_t<BoundArgs>>&&...>);
    static constexpr bool kIsFront = std::is_invocable_v<
        UnwrapF&&, std::remove_reference_t<std::unwrap_ref_decay_t<BoundArgs>>&&..., Args...>;
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

template <typename Task, typename F, typename... BoundArgs, typename Ret, typename... Args>
struct NamedTaskInvoker<Task, PayloadImpl<F, BoundArgs...>, Ret(Args...) const&&> {
    using UnwrapF = std::remove_reference_t<std::unwrap_ref_decay_t<F>>;
    static_assert(
        std::is_invocable_v<const UnwrapF&&,
                            const std::remove_reference_t<std::unwrap_ref_decay_t<BoundArgs>>&&...,
                            Args...> ||
        std::is_invocable_v<
            const UnwrapF&&, Args...,
            const std::remove_reference_t<std::unwrap_ref_decay_t<BoundArgs>>&&...>);
    static constexpr bool kIsFront =
        std::is_invocable_v<const UnwrapF&&,
                            const std::remove_reference_t<std::unwrap_ref_decay_t<BoundArgs>>&&...,
                            Args...>;
    FORCE_INLINE Ret operator()(this auto&& self, Args... args) {
        if constexpr (kIsFront) {
            return std::move((static_cast<const Task>(self)).payload_)
                .invokeFront(std::forward<Args>(args)...);
        } else {
            return std::move((static_cast<const Task>(self)).payload_)
                .invoke(std::forward<Args>(args)...);
        }
    }
};

template <typename Task, typename F, typename... BoundArgs, typename Ret, typename... Args>
struct NamedTaskInvoker<Task, PayloadImpl<F, BoundArgs...>, Ret(Args...) const && noexcept> {
    using UnwrapF = std::remove_reference_t<std::unwrap_ref_decay_t<F>>;
    static_assert(
        std::is_invocable_v<const UnwrapF&&,
                            const std::remove_reference_t<std::unwrap_ref_decay_t<BoundArgs>>&&...,
                            Args...> ||
        std::is_invocable_v<
            const UnwrapF&&, Args...,
            const std::remove_reference_t<std::unwrap_ref_decay_t<BoundArgs>>&&...>);
    static constexpr bool kIsFront =
        std::is_invocable_v<const UnwrapF&&,
                            const std::remove_reference_t<std::unwrap_ref_decay_t<BoundArgs>>&&...,
                            Args...>;
    FORCE_INLINE Ret operator()(this auto&& self, Args... args) noexcept {
        if constexpr (kIsFront) {
            return std::move((static_cast<const Task>(self)).payload_)
                .invokeFront(std::forward<Args>(args)...);
        } else {
            return std::move((static_cast<const Task>(self)).payload_)
                .invoke(std::forward<Args>(args)...);
        }
    }
};

template <typename S, typename F, typename... BoundArgs>
struct NamedTask;

template <typename S, typename F, typename... BoundArgs>
    requires(FunctionTraits<std::remove_reference_t<std::unwrap_reference_t<F>>>::kValid &&
             FunctionTraits<S>::kValid)
struct NamedTask<S, F, BoundArgs...>
    : NamedTaskInvoker<NamedTask<S, F, BoundArgs...>, PayloadImpl<F, BoundArgs...>, S> {
   private:
    template <typename, typename, typename>
    friend struct NamedTaskInvoker;

    template <typename, typename, typename...>
    friend struct NamedTask;

    using CallableImpl = F;
    using PayloadType = PayloadImpl<CallableImpl, BoundArgs...>;
    mutable PayloadType payload_{};

   public:
    using Signature = S;
    using CallableHolder = F;

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
        return PayloadTupleGetter<I, BoundArgs...>::get(self.payload_.args);
    }

    NamedTask(const NamedTask&) = default;
    NamedTask& operator=(const NamedTask&) = default;
    NamedTask(NamedTask&&) = default;
    NamedTask& operator=(NamedTask&&) = default;
    ~NamedTask() = default;

    template <typename SigOther, typename OtherF>
        requires(std::is_convertible_v<OtherF, F> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(const NamedTask<SigOther, OtherF, BoundArgs...>& other) : payload_{other.payload_} {}

    template <typename SigOther, auto OtherF>
        requires(std::is_convertible_v<decltype(OtherF), F> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(const NamedTask<SigOther, StaticFn<OtherF>, BoundArgs...>& other)
        : payload_{.f = static_cast<FunctionTraits<decltype(OtherF)>::func_ptr>(OtherF),
                   .args = other.payload_.args} {}

    template <typename SigOther, typename OtherF>
        requires(std::is_convertible_v<OtherF, F> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(const NamedTask<SigOther, OtherF, BoundArgs...>& other) {
        this->payload_ = other.payload_;
        return *this;
    }

    template <typename SigOther, auto OtherF>
        requires(std::is_convertible_v<decltype(OtherF), F> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(const NamedTask<SigOther, StaticFn<OtherF>, BoundArgs...>& other) {
        this->payload_ = {.f = static_cast<FunctionTraits<decltype(OtherF)>::func_ptr>(OtherF),
                          .args = other.payload_.args};
        return *this;
    }
    template <typename SigOther, typename OtherF>
        requires(std::is_convertible_v<OtherF, F> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(NamedTask<SigOther, OtherF, BoundArgs...>&& other)
        : payload_{std::move(other).payload_} {}

    template <typename SigOther, auto OtherF>
        requires(std::is_convertible_v<decltype(OtherF), F> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(NamedTask<SigOther, StaticFn<OtherF>, BoundArgs...>&& other)
        : payload_{.f = static_cast<decltype(OtherF)*>(OtherF),
                   .args = std::move(other).payload_.args} {}

    template <typename SigOther, typename OtherF>
        requires(std::is_convertible_v<OtherF, F> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(NamedTask<SigOther, OtherF, BoundArgs...>&& other) {
        this->payload_ = std::move(other).payload_;
        return *this;
    }

    template <typename SigOther, auto OtherF>
        requires(std::is_convertible_v<decltype(OtherF), F> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(NamedTask<SigOther, StaticFn<OtherF>, BoundArgs...>&& other) {
        this->payload_ = {.f = static_cast<decltype(OtherF)*>(OtherF),
                          .args = std::move(other).payload_.args};
        return *this;
    }

    NamedTask(auto&& f, auto&&... args)  // NOLINT
        requires(sizeof...(args) > 0 || !IsNamedTask<std::decay_t<decltype(f)>>::value ||
                 (IsSignatureCompatible<typename std::decay_t<decltype(f)>::Signature,
                                        Signature>::value &&
                  !std::is_convertible_v<typename std::decay_t<decltype(f)>::CallableImpl,
                                         CallableImpl>))
        : payload_{.f = std::forward<decltype(f)>(f),
                   .args = {std::forward<decltype(args)>(args)...}} {}
};

template <typename S, auto F, typename... BoundArgs>
struct NamedTask<S, StaticFn<F>, BoundArgs...>
    : NamedTaskInvoker<NamedTask<S, StaticFn<F>, BoundArgs...>,
                       PayloadImpl<decltype(F), BoundArgs...>, S> {
   private:
    template <typename, typename, typename>
    friend struct NamedTaskInvoker;

    template <typename, typename, typename...>
    friend struct NamedTask;

    using PayloadType = PayloadImplArgs<F, BoundArgs...>;
    mutable PayloadType payload_;

   public:
    using Signature = S;
    using CallableHolder = StaticFn<F>;
    using CallableImpl = decltype(F);

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
        return PayloadTupleGetter<I, BoundArgs...>::get(self.payload_.args);
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

    template <typename SigOther>
        requires(IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(const NamedTask<SigOther, StaticFn<F>, BoundArgs...>& other) {
        this->payload_.args = other.payload_.args;
        return *this;
    }

    template <typename SigOther>
        requires(IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(NamedTask<SigOther, StaticFn<F>, BoundArgs...>&& other)
        : payload_{.args = std::move(other).payload_.args} {}

    template <typename SigOther>
        requires(IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(NamedTask<SigOther, StaticFn<F>, BoundArgs...>&& other) {
        this->payload.args = std::move(other).payload.args;
        return *this;
    }

    NamedTask(auto&&... args) : payload_{{std::forward<decltype(args)>(args)...}} {}
};

template <typename F, typename... BoundArgs>
    requires(sizeof...(BoundArgs) > 0 || !IsNamedTask<std::decay_t<F>>::value)
NamedTask(F&&, BoundArgs&&...) -> NamedTask<
    typename NamedTaskTypeDeductor<std::decay_t<F>, std::decay_t<BoundArgs>...>::Signature,
    std::decay_t<F>, std::decay_t<BoundArgs>...>;

template <auto F, typename... BoundArgs>
    requires HasFunctionTraits<std::decay_t<std::unwrap_reference<decltype(F)>>> &&
             NamedTaskTypeDeductorAuto<F, BoundArgs...>::kIsValid
FORCE_INLINE auto makeNamedTask(BoundArgs&&... args) {
    return typename NamedTaskTypeDeductorAuto<F, std::decay_t<BoundArgs>...>::Type{
        std::forward<BoundArgs>(args)...};
}

template <auto F, typename... BoundArgs>
    requires HasFunctionTraits<std::decay_t<std::unwrap_reference<decltype(F)>>> &&
             NamedTaskTypeDeductorAuto<F, BoundArgs...>::kIsValid
FORCE_INLINE auto makeRefNamedTask(BoundArgs&&... args) {
    return typename NamedTaskTypeDeductorAuto<F, AddRefWrapperToRef_t<BoundArgs>...>::Type{
        std::forward<BoundArgs>(args)...};
}

template <typename F>
    requires(requires { typename FunctionTraits<std::unwrap_reference<F>>::func_ptr; } &&
             std::is_convertible_v<
                 F, typename FunctionTraits<std::decay_t<std::unwrap_reference<F>>>::func_ptr>)
FORCE_INLINE auto toFuncPtr(F&& f) {
    return typename FunctionTraits<std::decay_t<std::unwrap_reference<F>>>::func_ptr{
        std::forward<F>(f)};
}

template <auto F>
    requires(
        requires {
            typename FunctionTraits<std::decay_t<std::unwrap_reference<decltype(F)>>>::func_ptr;
        } &&
        std::is_convertible_v<
            decltype(F),
            typename FunctionTraits<std::decay_t<std::unwrap_reference<decltype(F)>>>::func_ptr>)
FORCE_INLINE auto toFuncPtr() {
    return static_cast<
        typename FunctionTraits<std::decay_t<std::unwrap_reference<decltype(F)>>>::func_ptr>(F);
}

template <bool StoreFuncAsPtr = false, typename F, typename... BoundArgs>
    requires((sizeof...(BoundArgs) > 0 || !IsNamedTask<std::decay_t<F>>::value) &&
             (!StoreFuncAsPtr ||
              (requires {
                  typename FunctionTraits<std::decay_t<std::unwrap_reference<F>>>::func_ptr;
              } &&
               std::is_convertible_v<
                   F, typename FunctionTraits<std::decay_t<std::unwrap_reference<F>>>::func_ptr>)))
FORCE_INLINE auto makeNamedTask(F&& f, BoundArgs&&... args) {
    if constexpr (StoreFuncAsPtr) {
        using func_ptr = typename FunctionTraits<std::decay_t<std::unwrap_reference<F>>>::func_ptr;
        return typename NamedTaskTypeDeductor<func_ptr, std::decay_t<BoundArgs>...>::Type{
            std::forward<F>(f), std::forward<BoundArgs>(args)...};
    } else {
        return typename NamedTaskTypeDeductor<std::decay_t<F>, std::decay_t<BoundArgs>...>::Type{
            std::forward<F>(f), std::forward<BoundArgs>(args)...};
    }
}

template <bool StoreFuncAsPtr = false>
FORCE_INLINE auto makeNamedTask(auto&& f)
    requires IsNamedTask<std::decay_t<decltype(f)>>::value
{
    // static_assert(!IsNamedTask<std::decay_t<decltype(f)>>::kIsRef, "");
    using NamedTaskSrc = std::decay_t<decltype(f)>;
    return NamedTaskSrc{std::forward<decltype(f)>(f)};
}

template <bool StoreFuncAsPtr = false, typename F, typename... BoundArgs>
    requires((sizeof...(BoundArgs) > 0 || !IsNamedTask<std::decay_t<F>>::value) &&
             (!StoreFuncAsPtr || (requires {
                 typename FunctionTraits<std::decay_t<F>>::func_ptr;
             } && std::is_convertible_v<F, typename FunctionTraits<std::decay_t<F>>::func_ptr>)))
FORCE_INLINE auto makeRefNamedTask(F&& f, BoundArgs&&... args) {
    if constexpr (StoreFuncAsPtr) {
        using func_ptr = typename FunctionTraits<std::decay_t<F>>::func_ptr;
        return typename NamedTaskTypeDeductor<func_ptr, AddRefWrapperToRef_t<BoundArgs>...>::Type{
            std::forward<F>(f), std::forward<BoundArgs>(args)...};
    } else {
        return typename NamedTaskTypeDeductor<AddRefWrapperToRef_t<F>,
                                              AddRefWrapperToRef_t<BoundArgs>...>::Type{
            std::forward<F>(f), std::forward<BoundArgs>(args)...};
    }
}

template <bool StoreFuncAsPtr = false>
FORCE_INLINE auto makeRefNamedTask(auto&& f)
    requires IsNamedTask<std::decay_t<decltype(f)>>::value
{
    using NamedTaskSrc = std::decay_t<decltype(f)>;
    return NamedTask<typename NamedTaskSrc::Signature, AddRefWrapperToRef_t<decltype(f)>>{
        std::forward<decltype(f)>(f)};
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

template <auto F, typename BoundArg>
    requires(
        requires {
            typename FunctionTraits<std::decay_t<std::unwrap_reference<decltype(F)>>>::func_ptr;
        } &&
        std::is_convertible_v<
            decltype(F),
            typename FunctionTraits<std::decay_t<std::unwrap_reference<decltype(F)>>>::func_ptr>)
FORCE_INLINE auto makeThisTask(BoundArg&& arg)
    requires ErasableToPointer<decltype(arg)>
{
    using ptr = typename FunctionTraits<std::decay_t<std::unwrap_reference<decltype(F)>>>::func_ptr;
    return NamedTask<typename NamedTaskTypeDeductor<ptr, void*>::Signature, ptr, void*>{
        static_cast<ptr>(F), std::forward<BoundArg>(arg)};
}

}  // namespace fn

#endif  // TASK_NAMED_TASK_HPP