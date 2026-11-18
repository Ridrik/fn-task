#ifndef TASK_NAMED_TASK_HPP
#define TASK_NAMED_TASK_HPP

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <utility>

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

template <Binding binding, typename Task, typename Payload, typename Signature,
          bool IsConst = false, bool LVRef = false, bool RVRef = false, bool IsNoExcept = false>
struct NamedTaskInvoker;

template <Binding binding, typename Task, typename F, typename... BoundArgs, typename Ret,
          typename... Args, bool IsConst, bool LVRef, bool RVRef, bool IsNoExcept>
struct NamedTaskInvoker<binding, Task, PayloadImpl<F, BoundArgs...>, Ret(Args...), IsConst, LVRef,
                        RVRef, IsNoExcept> {
    template <typename T>
    using ApplyQualifiers = std::conditional_t<
        LVRef, std::add_lvalue_reference_t<std::conditional_t<IsConst, std::add_const_t<T>, T>>,
        std::conditional_t<RVRef,
                           std::add_rvalue_reference_t<std::remove_reference_t<
                               std::conditional_t<IsConst, std::add_const_t<T>, T>>>,
                           std::conditional_t<IsConst, std::add_const_t<T>, T>>>;

    template <typename T>
    using Convert = ApplyQualifiers<std::unwrap_reference_t<T>>;

    static constexpr bool kCVLess = !IsConst && !LVRef && !RVRef;

    template <typename Ft, typename... Argst>
    static constexpr bool kInvocableVerifierV =
        kCVLess ? (IsNoExcept                                             // NOLINT
                       ? is_compatible_nothrow_invocable_b<Ft, Argst...>  // NOLINT
                       : is_compatible_invocable_b<Ft, Argst...>)
                : (IsNoExcept ? std::is_nothrow_invocable_v<Ft, Argst...>
                              : std::is_invocable_v<Ft, Argst...>);

    FORCE_INLINE Ret operator()(this auto&& self, Args... args) noexcept(IsNoExcept) {
        if constexpr (binding != Binding::kFront &&
                      kInvocableVerifierV<Convert<F>,
                                          CallArgTag<decltype(std::forward<Args>(args))>...,
                                          Convert<BoundArgs>...>) {
            if constexpr (kCVLess) {
                return std::move((const_cast<Task&>(self)).payload_)
                    .invokeCompatible(std::forward<Args>(args)...);
            } else {
                return std::forward_like<ApplyQualifiers<Task>>((const_cast<Task&>(self)).payload_)
                    .invoke(std::forward<Args>(args)...);
            }
        } else if constexpr (binding != Binding::kBack &&
                             kInvocableVerifierV<
                                 Convert<F>, Convert<BoundArgs>...,
                                 CallArgTag<decltype(std::forward<Args>(args))>...>) {
            if constexpr (kCVLess) {
                return std::move((const_cast<Task&>(self)).payload_)
                    .invokeFrontCompatible(std::forward<Args>(args)...);
            } else {
                return std::forward_like<ApplyQualifiers<Task>>((const_cast<Task&>(self)).payload_)
                    .invokeFront(std::forward<Args>(args)...);
            }
        } else {
            static_assert(AlwaysFalse<F>::value,
                          "Given callable is not invocable with the provided arguments");
        }
    }
};

template <Binding binding, typename Task, typename Payload, typename Signature>
    requires(!IsBaseSignature<Signature>)
struct NamedTaskInvoker<binding, Task, Payload, Signature>
    : NamedTaskInvoker<binding, Task, Payload, typename FunctionTraits<Signature>::base_signature,
                       FunctionTraits<Signature>::kIsConst, FunctionTraits<Signature>::kIsLValueRef,
                       FunctionTraits<Signature>::kIsRValueRef,
                       FunctionTraits<Signature>::kIsNoExcept> {};

template <typename S, Binding binding, typename F, typename... BoundArgs>
struct NamedTask;

template <typename S, Binding binding, typename F, typename... BoundArgs>
    requires(FunctionTraits<std::remove_reference_t<std::unwrap_reference_t<F>>>::kValid &&
             FunctionTraits<S>::kValid)
struct NamedTask<S, binding, F, BoundArgs...>
    : NamedTaskInvoker<binding, NamedTask<S, binding, F, BoundArgs...>,
                       PayloadImpl<F, BoundArgs...>, S> {
   private:
    template <Binding, typename, typename, typename, bool, bool, bool, bool>
    friend struct NamedTaskInvoker;

    template <typename, Binding, typename, typename...>
    friend struct NamedTask;

    using CallableImpl = F;
    using PayloadType = PayloadImpl<CallableImpl, BoundArgs...>;
    mutable PayloadType payload_{};

   public:
    using Signature = S;
    using CallableHolder = F;
    static constexpr auto kBinding = binding;

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
    NamedTask(const NamedTask<SigOther, binding, OtherF, BoundArgs...>& other)
        : payload_{other.payload_} {}

    template <typename SigOther, auto OtherF>
        requires(std::is_convertible_v<decltype(OtherF), F> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(const NamedTask<SigOther, binding, StaticFn<OtherF>, BoundArgs...>& other)
        : payload_{.f = static_cast<FunctionTraits<decltype(OtherF)>::func_ptr>(OtherF),
                   .args = other.payload_.args} {}

    template <typename SigOther, typename OtherF>
        requires(std::is_convertible_v<OtherF, F> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(const NamedTask<SigOther, binding, OtherF, BoundArgs...>& other) {
        this->payload_ = other.payload_;
        return *this;
    }

    template <typename SigOther, auto OtherF>
        requires(std::is_convertible_v<decltype(OtherF), F> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(
        const NamedTask<SigOther, binding, StaticFn<OtherF>, BoundArgs...>& other) {
        this->payload_ = {.f = static_cast<FunctionTraits<decltype(OtherF)>::func_ptr>(OtherF),
                          .args = other.payload_.args};
        return *this;
    }
    template <typename SigOther, typename OtherF>
        requires(std::is_convertible_v<OtherF, F> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(NamedTask<SigOther, binding, OtherF, BoundArgs...>&& other)
        : payload_{std::move(other).payload_} {}

    template <typename SigOther, auto OtherF>
        requires(std::is_convertible_v<decltype(OtherF), F> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(NamedTask<SigOther, binding, StaticFn<OtherF>, BoundArgs...>&& other)
        : payload_{.f = static_cast<decltype(OtherF)*>(OtherF),
                   .args = std::move(other).payload_.args} {}

    template <typename SigOther, typename OtherF>
        requires(std::is_convertible_v<OtherF, F> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(NamedTask<SigOther, binding, OtherF, BoundArgs...>&& other) {
        this->payload_ = std::move(other).payload_;
        return *this;
    }

    template <typename SigOther, auto OtherF>
        requires(std::is_convertible_v<decltype(OtherF), F> &&
                 IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(NamedTask<SigOther, binding, StaticFn<OtherF>, BoundArgs...>&& other) {
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

template <typename S, Binding binding, auto F, typename... BoundArgs>
struct NamedTask<S, binding, StaticFn<F>, BoundArgs...>
    : NamedTaskInvoker<binding, NamedTask<S, binding, StaticFn<F>, BoundArgs...>,
                       PayloadImpl<decltype(F), BoundArgs...>, S> {
   private:
    template <Binding, typename, typename, typename, bool, bool, bool, bool>
    friend struct NamedTaskInvoker;

    template <typename, Binding, typename, typename...>
    friend struct NamedTask;

    using PayloadType = PayloadImplArgs<F, BoundArgs...>;
    mutable PayloadType payload_;

   public:
    using Signature = S;
    using CallableHolder = StaticFn<F>;
    using CallableImpl = decltype(F);
    static constexpr auto kBinding = binding;

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
    NamedTask(const NamedTask<SigOther, binding, StaticFn<F>, BoundArgs...>& other)
        : payload_{.args = other.payload_.args} {}

    template <typename SigOther>
        requires(IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(const NamedTask<SigOther, binding, StaticFn<F>, BoundArgs...>& other) {
        this->payload_.args = other.payload_.args;
        return *this;
    }

    template <typename SigOther>
        requires(IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask(NamedTask<SigOther, binding, StaticFn<F>, BoundArgs...>&& other)
        : payload_{.args = std::move(other).payload_.args} {}

    template <typename SigOther>
        requires(IsSignatureCompatible<SigOther, Signature>::value)
    NamedTask& operator=(NamedTask<SigOther, binding, StaticFn<F>, BoundArgs...>&& other) {
        this->payload.args = std::move(other).payload.args;
        return *this;
    }

    NamedTask(auto&&... args) : payload_{{std::forward<decltype(args)>(args)...}} {}
};

template <typename F, typename... BoundArgs>
    requires(sizeof...(BoundArgs) > 0 || !IsNamedTask<std::decay_t<F>>::value)
NamedTask(F&&, BoundArgs&&...)
    -> NamedTask<typename TaskTypeDeductor<DeductorSettings<void>{.binding = Binding::kAuto},
                                           std::decay_t<F>, std::decay_t<BoundArgs>...>::Signature,
                 Binding::kAuto, std::decay_t<F>, std::decay_t<BoundArgs>...>;

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

template <Binding binding = Binding::kAuto, typename F, typename... BoundArgs>
    requires((sizeof...(BoundArgs) > 0 || !IsNamedTask<std::decay_t<F>>::value))
FORCE_INLINE auto makeNamedTask(F&& f, BoundArgs&&... args) {
    using Deductor = TaskTypeDeductor<DeductorSettings<void>{.binding = binding}, std::decay_t<F>,
                                      std::decay_t<BoundArgs>...>;
    static_assert(Deductor::kIsValid, "Given function, arguments and binding are not invocable");
    return typename TaskTypeDeductor<DeductorSettings<void>{.binding = binding}, std::decay_t<F>,
                                     std::decay_t<BoundArgs>...>::template NamedType<>{
        std::forward<F>(f), std::forward<BoundArgs>(args)...};
}

FORCE_INLINE auto bindFrontNamedTask(auto&& f, auto&&... args)
    requires((sizeof...(args) > 0 || !IsNamedTask<std::decay_t<decltype(f)>>::value))
{
    return makeNamedTask<Binding::kFront>(std::forward<decltype(f)>(f),
                                          std::forward<decltype(args)>(args)...);
}

FORCE_INLINE auto bindBackNamedTask(auto&& f, auto&&... args) {
    return makeNamedTask<Binding::kBack>(std::forward<decltype(f)>(f),
                                         std::forward<decltype(args)>(args)...);
}

template <auto F, Binding binding = Binding::kAuto, typename... BoundArgs>
    requires HasFunctionTraits<std::decay_t<std::unwrap_reference<decltype(F)>>> &&
             TaskTypeDeductorAuto<F, {.binding = binding}, BoundArgs...>::kIsValid
FORCE_INLINE auto makeNamedTask(BoundArgs&&... args) {
    return typename TaskTypeDeductorAuto<F, {.binding = binding},
                                         std::decay_t<BoundArgs>...>::template NamedType<>{
        std::forward<BoundArgs>(args)...};
}

template <auto F>
FORCE_INLINE auto bindFrontNamedTask(auto&&... args) {
    return makeNamedTask<F, Binding::kFront>(std::forward<decltype(args)>(args)...);
}

template <auto F>
FORCE_INLINE auto bindBackNamedTask(auto&&... args) {
    return makeNamedTask<F, Binding::kBack>(std::forward<decltype(args)>(args)...);
}

FORCE_INLINE auto makeNamedTask(auto&& f)
    requires IsNamedTask<std::decay_t<decltype(f)>>::value
{
    using NamedTaskSrc = std::decay_t<decltype(f)>;
    return NamedTaskSrc{std::forward<decltype(f)>(f)};
}

template <Binding binding = Binding::kAuto, typename F, typename... BoundArgs>
    requires(sizeof...(BoundArgs) > 0 || !IsNamedTask<std::decay_t<F>>::value)
FORCE_INLINE auto makeRefNamedTask(F&& f, BoundArgs&&... args) {
    return typename TaskTypeDeductor<DeductorSettings<void>{.binding = binding},
                                     AddRefWrapperToRef_t<F>,
                                     AddRefWrapperToRef_t<BoundArgs>...>::template NamedType<>{
        std::forward<F>(f), std::forward<BoundArgs>(args)...};
}

FORCE_INLINE auto bindFrontRefNamedTask(auto&& f, auto&&... args)
    requires((sizeof...(args) > 0 || !IsNamedTask<std::decay_t<decltype(f)>>::value))
{
    return makeRefNamedTask<Binding::kFront>(std::forward<decltype(f)>(f),
                                             std::forward<decltype(args)>(args)...);
}

FORCE_INLINE auto bindBackRefNamedTask(auto&& f, auto&&... args) {
    return makeRefNamedTask<Binding::kBack>(std::forward<decltype(f)>(f),
                                            std::forward<decltype(args)>(args)...);
}

FORCE_INLINE auto makeRefNamedTask(auto&& f)
    requires IsNamedTask<std::decay_t<decltype(f)>>::value
{
    using NamedTaskSrc = std::decay_t<decltype(f)>;
    return NamedTask<typename NamedTaskSrc::Signature, NamedTaskSrc::kBinding,
                     AddRefWrapperToRef_t<decltype(f)>>{std::forward<decltype(f)>(f)};
}

template <auto F, Binding binding = Binding::kAuto, typename... BoundArgs>
    requires HasFunctionTraits<std::decay_t<std::unwrap_reference<decltype(F)>>> &&
             TaskTypeDeductorAuto<F, {.binding = binding}, BoundArgs...>::kIsValid
FORCE_INLINE auto makeRefNamedTask(BoundArgs&&... args) {
    return typename TaskTypeDeductorAuto<F, {.binding = binding},
                                         AddRefWrapperToRef_t<BoundArgs>...>::template NamedType<>{
        std::forward<BoundArgs>(args)...};
}

template <auto F>
FORCE_INLINE auto bindFrontRefNamedTask(auto&&... args) {
    return makeRefNamedTask<F, Binding::kFront>(std::forward<decltype(args)>(args)...);
}

template <auto F>
FORCE_INLINE auto bindBackRefNamedTask(auto&&... args) {
    return makeRefNamedTask<F, Binding::kBack>(std::forward<decltype(args)>(args)...);
}

template <typename T>
concept ErasableToPointer = requires(T t) { static_cast<void*>(t); };

template <Binding binding = Binding::kAuto, typename F, typename BoundArg>
    requires(requires { typename FunctionTraits<std::decay_t<F>>::func_ptr; } &&
             std::is_convertible_v<F, typename FunctionTraits<std::decay_t<F>>::func_ptr>)
FORCE_INLINE auto makeThisTask(F&& f, BoundArg&& arg)
    requires ErasableToPointer<decltype(arg)>
{
    using ptr = typename FunctionTraits<std::decay_t<F>>::func_ptr;
    return NamedTask<typename TaskTypeDeductor<DeductorSettings<void>{.binding = binding}, ptr,
                                               void*>::Signature,
                     binding, ptr, void*>{std::forward<F>(f), std::forward<BoundArg>(arg)};
}

template <auto F, Binding binding = Binding::kAuto, typename BoundArg>
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
    return NamedTask<typename TaskTypeDeductor<DeductorSettings<void>{.binding = binding}, ptr,
                                               void*>::Signature,
                     binding, ptr, void*>{static_cast<ptr>(F), std::forward<BoundArg>(arg)};
}

}  // namespace fn

#endif  // TASK_NAMED_TASK_HPP