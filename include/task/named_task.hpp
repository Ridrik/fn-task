#ifndef TASK_NAMED_TASK_HPP
#define TASK_NAMED_TASK_HPP

#include <type_traits>

#include <task/function_traits.hpp>
#include <task/payload.hpp>

namespace fn {

template <auto F, typename... Args>
struct StaticPayloadSelector {
    using type = PayloadArgsT<F, Args...>;
};

template <typename F, typename... Args>
struct TypePayloadSelector {
    using type = PayloadT<F, Args...>;
};

template <typename Derived, bool isFront, typename ReturnType, typename... CallArgs>
struct TaskInvokeBase {
    FORCE_INLINE ReturnType invoke(this auto&& self, CallArgs... args) {
        using Self = decltype(self);
        using DerivedRef = std::conditional_t<std::is_const_v<std::remove_reference_t<Self>>,
                                              const Derived, Derived>;
        auto&& derived = static_cast<DerivedRef&&>(self);
        static_assert(IsPayload<decltype(derived.payload), Self, CallArgs...>::value);
        if constexpr (isFront) {
            static_assert(
                IsPayload<decltype(derived.payload), Self, CallArgs...>::kIsInvocableFront,
                "F is not invocable with provided arguments. Check whether a different "
                "attribute is needed (such as non-const NamedTask, r-value invocation)");
            return std::forward_like<Self>(derived.payload)
                .invokeFront(std::forward<CallArgs>(args)...);
        } else {
            static_assert(IsPayload<decltype(derived.payload), Self, CallArgs...>::kIsInvocable,
                          "F is not invocable with provided arguments. Check whether a different "
                          "attribute is needed (such as non-const NamedTask, r-value invocation)");
            return std::forward_like<Self>(derived.payload).invoke(std::forward<CallArgs>(args)...);
        }
    }

    FORCE_INLINE ReturnType operator()(this auto&& self, CallArgs... args) {
        using Self = decltype(self);
        using DerivedRef = std::conditional_t<std::is_const_v<std::remove_reference_t<Self>>,
                                              const Derived, Derived>;
        auto&& derived = static_cast<DerivedRef&&>(self);
        static_assert(IsPayload<decltype(derived.payload), Self, CallArgs...>::value);
        static_assert(IsPayload<decltype(derived.payload), Self, CallArgs...>::kIsInvocableFront ||
                          IsPayload<decltype(derived.payload), Self, CallArgs...>::kIsInvocable,
                      "F is not invocable with provided arguments. Check whether a different "
                      "attribute is needed (such as non-const NamedTask, r-value invocation)");
        return std::forward_like<decltype(self)>(self).invoke(std::forward<CallArgs>(args)...);
    }
};

template <typename Signature, typename F, typename... Args>
struct NamedTask;

template <typename ReturnType, typename... CallArgs, typename F, typename... BoundArgs>
struct NamedTask<ReturnType(CallArgs...), F, BoundArgs...>
    : TaskInvokeBase<
          NamedTask<ReturnType(CallArgs...), F, BoundArgs...>,
          std::is_invocable_v<F, std::unwrap_ref_decay_t<BoundArgs>&..., CallArgs...> ||
              std::is_invocable_v<F, std::unwrap_ref_decay_t<BoundArgs>&&..., CallArgs...>,
          ReturnType, CallArgs...> {
    using Signature = ReturnType(CallArgs...);
    using CallableImpl = F;
    PayloadT<CallableImpl, BoundArgs...> payload{};
    static_assert(
        std::is_invocable_v<F, CallArgs..., std::unwrap_ref_decay_t<BoundArgs>&...> ||
            std::is_invocable_v<F, CallArgs..., std::unwrap_ref_decay_t<BoundArgs>&&...> ||
            std::is_invocable_v<F, std::unwrap_ref_decay_t<BoundArgs>&..., CallArgs...> ||
            std::is_invocable_v<F, std::unwrap_ref_decay_t<BoundArgs>&&..., CallArgs...>,
        "Given Function is not invocable with provided signature and bound arguments");

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

    NamedTask(const NamedTask&) = default;
    NamedTask& operator=(const NamedTask&) = default;
    NamedTask(NamedTask&&) = default;
    NamedTask& operator=(NamedTask&&) = default;
    ~NamedTask() = default;

    template <typename OtherF>
        requires(std::is_convertible_v<OtherF, CallableImpl>)
    NamedTask& operator=(const NamedTask<ReturnType(CallArgs...), OtherF, BoundArgs...>& other) {
        this->payload = other.payload;
        return *this;
    }

    template <typename OtherF>
        requires(std::is_convertible_v<OtherF, CallableImpl>)
    NamedTask& operator=(NamedTask<ReturnType(CallArgs...), OtherF, BoundArgs...>&& other) {
        this->payload = std::move(other).payload;
        return *this;
    }

    NamedTask(F&& f, BoundArgs&&... args)
        requires(sizeof...(BoundArgs) > 0 || !std::is_same_v<std::remove_cvref_t<F>, NamedTask>)
        : payload{std::forward<F>(f), {std::forward<BoundArgs>(args)...}} {}
};

template <auto F>
struct StaticFn {};

template <typename ReturnType, typename... CallArgs, auto F, typename... BoundArgs>
struct NamedTask<ReturnType(CallArgs...), StaticFn<F>, BoundArgs...>
    : TaskInvokeBase<
          NamedTask<ReturnType(CallArgs...), StaticFn<F>, BoundArgs...>,
          std::is_invocable_v<decltype(F), std::unwrap_ref_decay_t<BoundArgs>&..., CallArgs...> ||
              std::is_invocable_v<decltype(F), std::unwrap_ref_decay_t<BoundArgs>&&...,
                                  CallArgs...>,
          ReturnType, CallArgs...> {
    using Signature = ReturnType(CallArgs...);
    PayloadArgsT<F, BoundArgs...> payload;

    static_assert(
        std::is_invocable_v<decltype(F), CallArgs..., std::unwrap_ref_decay_t<BoundArgs>&...> ||
            std::is_invocable_v<decltype(F), CallArgs...,
                                std::unwrap_ref_decay_t<BoundArgs>&&...> ||
            std::is_invocable_v<decltype(F), std::unwrap_ref_decay_t<BoundArgs>&..., CallArgs...> ||
            std::is_invocable_v<decltype(F), std::unwrap_ref_decay_t<BoundArgs>&&..., CallArgs...>,
        "Given Function is not invocable with provided signature and bound arguments");

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

    NamedTask(const NamedTask&) = default;
    NamedTask& operator=(const NamedTask&) = default;
    NamedTask(NamedTask&&) = default;
    NamedTask& operator=(NamedTask&&) = default;
    ~NamedTask() = default;

    template <auto OtherF>
    NamedTask& operator=(
        const NamedTask<ReturnType(CallArgs...), StaticFn<OtherF>, BoundArgs...>& other) {
        this->payload = other.payload;
        return *this;
    }

    template <auto OtherF>
    NamedTask& operator=(
        NamedTask<ReturnType(CallArgs...), StaticFn<OtherF>, BoundArgs...>&& other) {
        this->payload = std::move(other).payload;
        return *this;
    }

    NamedTask(BoundArgs&&... args) : payload{{std::forward<BoundArgs>(args)...}} {}
};

template <typename F, typename... BoundArgs>
    requires(HasFunctionTraits<F>)
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
                std::is_invocable_v<F, Ts..., std::unwrap_ref_decay_t<BoundArgs>&&...> ||
                std::is_invocable_v<F, Ts..., std::unwrap_ref_decay_t<BoundArgs>&...> ||
                std::is_invocable_v<F, std::unwrap_ref_decay_t<BoundArgs>&&..., TsR...> ||
                std::is_invocable_v<F, std::unwrap_ref_decay_t<BoundArgs>&..., TsR...>;
            static constexpr bool kIsReverse =
                std::is_invocable_v<F, std::unwrap_ref_decay_t<BoundArgs>&&..., TsR...> ||
                std::is_invocable_v<F, std::unwrap_ref_decay_t<BoundArgs>&..., TsR...>;
            using R = Ret;
            using Args = std::conditional_t<kIsReverse, std::tuple<TsR...>, std::tuple<Ts...>>;
            using Signature = std::conditional_t<kIsReverse, Ret(TsR...), Ret(Ts...)>;
            static constexpr bool kMutable =
                (!kIsReverse &&
                 !std::is_invocable_v<
                     F, Ts...,
                     const std::unwrap_ref_decay_t<typename RefToConstRef<BoundArgs>::Type>&...>) ||
                (kIsReverse &&
                 !std::is_invocable_v<
                     F, const std::unwrap_ref_decay_t<typename RefToConstRef<BoundArgs>::Type>&...,
                     TsR...>);
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
                std::is_invocable_v<F, Ts..., std::unwrap_ref_decay_t<BoundArgs>&&...> ||
                std::is_invocable_v<F, Ts..., std::unwrap_ref_decay_t<BoundArgs>&...> ||
                std::is_invocable_v<F, std::unwrap_ref_decay_t<BoundArgs>&&..., TsR...> ||
                std::is_invocable_v<F, std::unwrap_ref_decay_t<BoundArgs>&..., TsR...>;
            static constexpr bool kIsReverse =
                std::is_invocable_v<F, std::unwrap_ref_decay_t<BoundArgs>&&..., TsR...> ||
                std::is_invocable_v<F, std::unwrap_ref_decay_t<BoundArgs>&..., TsR...>;
            using R = Ret;
            using Args = std::conditional_t<kIsReverse, std::tuple<TsR...>, std::tuple<Ts...>>;
            using Signature = std::conditional_t<kIsReverse, Ret(TsR...), Ret(Ts...)>;
            static constexpr bool kMutable =
                (!kIsReverse &&
                 !std::is_invocable_v<
                     F, Ts...,
                     const std::unwrap_ref_decay_t<typename RefToConstRef<BoundArgs>::Type>&...>) ||
                (kIsReverse &&
                 !std::is_invocable_v<
                     F, const std::unwrap_ref_decay_t<typename RefToConstRef<BoundArgs>::Type>&...,
                     TsR...>);
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

template <auto F, typename... BoundArgs>
FORCE_INLINE auto makeNamedTask(BoundArgs&&... args) {
    return NamedTask<typename NamedTaskTypeDeductorAuto<F, BoundArgs...>::Signature, StaticFn<F>,
                     BoundArgs...>{std::forward<BoundArgs>(args)...};
}

template <typename F>
    requires(requires { typename FunctionTraits<F>::func_ptr; } &&
             std::is_convertible_v<F, typename FunctionTraits<F>::func_ptr>)
FORCE_INLINE auto toFuncPtr(F&& f) {
    return typename FunctionTraits<F>::func_ptr{std::forward<F>(f)};
}

template <bool StoreFuncAsPtr = false, typename F, typename... BoundArgs>
    requires(!StoreFuncAsPtr || (requires { typename FunctionTraits<F>::func_ptr; } &&
                                 std::is_convertible_v<F, typename FunctionTraits<F>::func_ptr>))
FORCE_INLINE auto makeNamedTask(F&& f, BoundArgs&&... args) {
    if constexpr (StoreFuncAsPtr) {
        return NamedTask<typename NamedTaskTypeDeductor<typename FunctionTraits<F>::func_ptr,
                                                        BoundArgs...>::Signature,
                         typename FunctionTraits<F>::func_ptr, BoundArgs...>{
            std::forward<F>(f), std::forward<BoundArgs>(args)...};
    } else {
        return NamedTask<typename NamedTaskTypeDeductor<F, BoundArgs...>::Signature, F,
                         BoundArgs...>{std::forward<F>(f), std::forward<BoundArgs>(args)...};
    }
}

}  // namespace fn

#endif  // TASK_NAMED_TASK_HPP