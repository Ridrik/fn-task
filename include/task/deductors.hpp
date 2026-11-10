#ifndef TASK_DEDUCTORS_HPP
#define TASK_DEDUCTORS_HPP

#include <algorithm>
#include <cstddef>
#include <tuple>
#include <type_traits>

#include "function_traits.hpp"
#include "payload.hpp"

namespace fn {

template <typename Signature, bool Copyable, std::size_t N, bool IsTrivial, bool Extended,
          bool AllowHeap, std::size_t Align>
struct Task;

template <typename T>
struct IsTask : std::false_type {};

template <typename Signature, bool Copyable, std::size_t N, bool IsTrivial, bool Extended,
          bool AllowHeap, std::size_t Align>
struct IsTask<Task<Signature, Copyable, N, IsTrivial, Extended, AllowHeap, Align>>
    : std::true_type {};

template <typename S, typename F, typename... BoundArgs>
struct NamedTask;

template <typename>
struct IsNamedTask : std::false_type {};

template <typename S, typename F, typename... BoundArgs>
struct IsNamedTask<NamedTask<S, F, BoundArgs...>> : std::true_type {};

template <typename F>
struct NamedTaskRefTag {};

template <auto F>
struct NamedTaskStaticRefTag {};

template <auto F>
struct StaticFn {};

template <typename S, typename F, typename... BoundArgs>
struct IsNamedTask<NamedTask<S, NamedTaskRefTag<F>, BoundArgs...>> : std::true_type {};

template <typename S, auto F, typename... BoundArgs>
struct IsNamedTask<NamedTask<S, StaticFn<F>, BoundArgs...>> : std::true_type {};

template <typename S, auto F, typename... BoundArgs>
struct IsNamedTask<NamedTask<S, NamedTaskStaticRefTag<F>, BoundArgs...>> : std::true_type {};

template <typename F, typename... BoundArgs>
    requires(HasFunctionTraits<std::decay_t<F>>)
struct TaskTypeDeductor {
    struct StripBoundArgs {
        using Traits = FunctionTraits<std::decay_t<F>>;
        using AllArgs = Traits::args_tuple;

        static constexpr std::size_t kRemainingCount =
            std::tuple_size_v<AllArgs> - sizeof...(BoundArgs);

        using SlicedTuple = slice_first<kRemainingCount, AllArgs>::type;
        using SlicedTupleReverse = slice_last<kRemainingCount, AllArgs>::type;

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
                  std::is_invocable_v<std::decay_t<F>&, Ts...,
                                      std::unwrap_reference_t<typename RefToConstRef<
                                          std::decay_t<BoundArgs>>::Type>&...>) ||
                 (!kIsNormalOrder &&
                  std::is_invocable_v<std::decay_t<F>&,
                                      std::unwrap_reference_t<typename RefToConstRef<
                                          std::decay_t<BoundArgs>>::Type>&...,
                                      TsR...>));
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
    using Signature = TypeTraits::Signature;
    static constexpr bool kIsValid = TypeTraits::kIsValid;

    template <bool Copyable = kCopyable, std::size_t N = kPayloadSize,
              bool IsTrivial = kCanUseTrivial, std::size_t Align = kAlignment>
    using Type = Task<Signature, Copyable, N, IsTrivial, false, false, Align>;

    template <std::size_t N>
    using FlexType = Task<Signature, kCopyable, N, kCanUseTrivial, false, true, kAlignment>;

    template <bool Copyable = kRefCopyable, std::size_t N = kRefPayloadSize,
              bool IsTrivial = kRefCanUseTrivial, std::size_t Align = kRefAlignment>
    using RefType = Task<Signature, Copyable, N, IsTrivial, false, false, Align>;
};

template <auto F, typename... BoundArgs>
    requires(HasFunctionTraits<decltype(F)>)
struct TaskTypeDeductorAuto {
    using F_Type = std::decay_t<decltype(F)>;
    struct StripBoundArgs {
        using Traits = FunctionTraits<F_Type>;
        using AllArgs = typename Traits::args_tuple;

        static constexpr std::size_t kRemainingCount =
            std::tuple_size_v<AllArgs> - sizeof...(BoundArgs);

        using SlicedTuple = slice_first<kRemainingCount, AllArgs>::type;
        using SlicedTupleReverse = slice_last<kRemainingCount, AllArgs>::type;

        template <typename Ret, typename Tuple, typename TupleReverse>
        struct rebuild;
        template <typename Ret, typename... Ts, typename... TsR>
        struct rebuild<Ret, std::tuple<Ts...>, std::tuple<TsR...>> {
            static constexpr bool kIsValid =
                is_compatible_invocable_v<std::decay_t<F_Type>&&, Ts...,
                                          std::unwrap_ref_decay_t<BoundArgs>&&...> ||
                is_compatible_invocable_v<std::decay_t<F_Type>&&,
                                          std::unwrap_ref_decay_t<BoundArgs>&&..., TsR...>;
            static constexpr bool kIsNormalOrder =
                is_compatible_invocable_v<std::decay_t<F_Type>&&, Ts...,
                                          std::unwrap_ref_decay_t<BoundArgs>&&...>;
            static constexpr bool kIsReverse = !kIsNormalOrder;
            using R = Ret;
            using Args = std::conditional_t<kIsReverse, std::tuple<TsR...>, std::tuple<Ts...>>;
            static constexpr bool kIsConst =
                Traits::kIsConst &&
                ((kIsNormalOrder &&
                  std::is_invocable_v<const std::decay_t<F_Type>, Ts...,
                                      const std::unwrap_reference_t<typename RefToConstRef<
                                          std::decay_t<BoundArgs>>::Type>...>) ||
                 (!kIsNormalOrder &&
                  std::is_invocable_v<const std::decay_t<F_Type>,
                                      const std::unwrap_reference_t<
                                          typename RefToConstRef<std::decay_t<BoundArgs>>::Type>...,
                                      TsR...>));
            static constexpr bool kIsRValRef =
                Traits::kIsRValueRef &&
                ((kIsNormalOrder &&
                  std::is_invocable_v<std::decay_t<F_Type>&&, Ts...,
                                      std::unwrap_reference_t<typename RefToConstRef<
                                          std::decay_t<BoundArgs>>::Type>&&...>) ||
                 (!kIsNormalOrder &&
                  std::is_invocable_v<std::decay_t<F_Type>&&,
                                      std::unwrap_reference_t<typename RefToConstRef<
                                          std::decay_t<BoundArgs>>::Type>&&...,
                                      TsR...>));
            static constexpr bool kIsLValRef =
                Traits::kIsLValueRef &&
                ((kIsNormalOrder &&
                  std::is_invocable_v<std::decay_t<F_Type>&, Ts...,
                                      std::unwrap_reference_t<typename RefToConstRef<
                                          std::decay_t<BoundArgs>>::Type>&...>) ||
                 (!kIsNormalOrder &&
                  std::is_invocable_v<std::decay_t<F_Type>&,
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
    static constexpr auto kRefPayloadSize =
        sizeof...(BoundArgs) > 0 ? sizeof(PayloadImplArgs<F, BoundArgs...>) : 0;
    using TypeTraits = StripBoundArgs::type;
    using Signature = TypeTraits::Signature;
    static constexpr bool kIsValid = TypeTraits::kIsValid;

    template <bool Copyable = kCopyable, std::size_t N = kPayloadSize,
              bool IsTrivial = kCanUseTrivial, std::size_t Align = kAlignment>
    using Type = Task<Signature, Copyable, N, IsTrivial, false, false, Align>;

    template <std::size_t N>
    using FlexType = Task<Signature, kCopyable, N, kCanUseTrivial, false, true, kAlignment>;

    template <bool Copyable = kRefCopyable, std::size_t N = kRefPayloadSize,
              bool IsTrivial = kRefCanUseTrivial, std::size_t Align = kRefAlignment>
    using RefType = Task<Signature, Copyable, N, IsTrivial, false, false, Align>;
};

template <typename F, typename... BoundArgs>
    requires(HasFunctionTraits<std::decay_t<F>>)
struct NamedTaskTypeDeductor {
    struct StripBoundArgs {
        using Traits = FunctionTraits<std::decay_t<F>>;
        using AllArgs = Traits::args_tuple;
        using InternalFuncPtr = Traits::func_ptr;

        static constexpr std::size_t kRemainingCount =
            std::tuple_size_v<AllArgs> - sizeof...(BoundArgs);

        using SlicedTuple = slice_first<kRemainingCount, AllArgs>::type;
        using SlicedTupleReverse = slice_last<kRemainingCount, AllArgs>::type;

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
                  std::is_invocable_v<std::decay_t<F>&, Ts...,
                                      std::unwrap_reference_t<typename RefToConstRef<
                                          std::decay_t<BoundArgs>>::Type>&...>) ||
                 (!kIsNormalOrder &&
                  std::is_invocable_v<std::decay_t<F>&,
                                      std::unwrap_reference_t<typename RefToConstRef<
                                          std::decay_t<BoundArgs>>::Type>&...,
                                      TsR...>));
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

// Support for std::bind, std::bind_front, std::bind_back... (Note, it's implementation defined,
// though this works at least for gcc across Windows and Linux currently)

template <template <typename, typename...> typename T, typename Fd, typename... BoundArgs>
    requires(!IsTask<T<Fd, BoundArgs...>>::value && !IsNamedTask<T<Fd, BoundArgs...>>::value &&
             HasFunctionTraits<std::decay_t<Fd>> && TaskTypeDeductor<Fd, BoundArgs...>::kIsValid)
struct FunctionTraits<T<Fd, BoundArgs...>>
    : FunctionTraits<typename TaskTypeDeductor<Fd, BoundArgs...>::TypeTraits::Signature> {};

template <template <bool, typename, typename...> typename T, bool B, typename Fd,
          typename... BoundArgs>
    requires(!IsTask<T<B, Fd, BoundArgs...>>::value &&
             !IsNamedTask<T<B, Fd, BoundArgs...>>::value && HasFunctionTraits<std::decay_t<Fd>> &&
             TaskTypeDeductor<Fd, BoundArgs...>::kIsValid)
struct FunctionTraits<T<B, Fd, BoundArgs...>>
    : FunctionTraits<typename TaskTypeDeductor<Fd, BoundArgs...>::TypeTraits::Signature> {};

}  // namespace fn

#endif  // TASK_DEDUCTORS_HPP