#ifndef TASK_DEDUCTORS_HPP
#define TASK_DEDUCTORS_HPP

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

#include "function_traits.hpp"
#include "payload.hpp"

namespace fn {

enum class Triviality : std::uint8_t { kNonTrivial, kTrivial };
enum class Copyability : std::uint8_t { kNonCopyable, kCopyable };
enum class HeapPolicy : std::uint8_t { kDisallow, kAllow };

/* Up to the specified bytes, inline storage will be used for bound args, avoiding heap allocation*/
struct InlineSize {
    constexpr InlineSize(std::size_t n) : bytes{n} {}
    constexpr operator std::size_t() const noexcept {
        return bytes;
    }

    std::size_t bytes{alignof(std::max_align_t)};
};

/* Alignment of storage (bound arguments) data */
struct Alignment {
    constexpr Alignment(std::size_t n) : bytes{n} {}
    constexpr operator std::size_t() const noexcept {
        return bytes;
    }

    std::size_t bytes{16};
};

template <typename, Copyability cb = Copyability::kCopyable>
struct TrivialUse : std::false_type {};

// Captured arguments of type-erased callable are not copy-assigned or move-assigned, and instead
// are destroyed followed by the copy/move constructor. Therefore, to be safe to simply memcpy
// and not need manager pointers, one checks copy/move constructor and destructor triviallity
template <typename Pl>
    requires(std::is_trivially_copy_constructible_v<Pl> &&
             std::is_trivially_move_constructible_v<Pl> && std::is_trivially_destructible_v<Pl>)
struct TrivialUse<Pl, Copyability::kCopyable> : std::true_type {};

template <typename Pl>
    requires(std::is_trivially_move_constructible_v<Pl> && std::is_trivially_destructible_v<Pl>)
struct TrivialUse<Pl, Copyability::kNonCopyable> : std::true_type {};

template <typename Signature, Copyability cb, InlineSize size, Triviality t, bool Extended,
          HeapPolicy hp, Alignment align>
struct Task;

template <typename T>
struct IsTask : std::false_type {};

template <typename Signature, Copyability cb, InlineSize size, Triviality t, bool Extended,
          HeapPolicy hp, Alignment align>
struct IsTask<Task<Signature, cb, size, t, Extended, hp, align>> : std::true_type {};

enum class Binding : std::uint8_t { kAuto, kFront, kBack };

template <std::size_t... Indices>
struct Binder {};

template <>
struct Binder<> {
    constexpr Binder(Binding b) : binding{b} {}
    constexpr Binder() = default;

    constexpr operator Binding() const noexcept {
        return binding;
    }

    Binding binding{Binding::kAuto};
};

template <auto V>
struct IsBinder : std::false_type {};

template <std::size_t... Is, Binder<Is...> binder>
struct IsBinder<binder> : std::true_type {};

template <auto V>
static constexpr auto kIsBinder = IsBinder<V>::value;

template <auto V>
struct IsIndicelessBinder : std::false_type {};

template <Binder<> binder>
struct IsIndicelessBinder<binder> : std::true_type {};

template <auto V>
static constexpr auto kIsIndicelessBinder = IsIndicelessBinder<V>::value;

template <auto bind>
    requires kIsBinder<bind>
struct BinderTag {};

template <typename>
struct IsBinderTag : std::false_type {};

template <std::size_t... Is, Binder<Is...> bind>
struct IsBinderTag<BinderTag<bind>> : std::true_type {};

template <typename T>
static constexpr bool IsBinderTagB = IsBinderTag<T>::value;  // NOLINT

template <std::size_t... Is>
struct IndicesHolder {};

template <auto binder>
    requires kIsBinder<binder> && (!kIsIndicelessBinder<binder>)
struct GetBinderIndicesTuple;

template <std::size_t... Is, Binder<Is...> binder>
    requires(sizeof...(Is) > 0)
struct GetBinderIndicesTuple<binder> {
    using type = IndicesHolder<Is...>;
};

template <auto binder>
    requires kIsBinder<binder> && (!kIsIndicelessBinder<binder>)
using GetBinderIndicesT = GetBinderIndicesTuple<binder>::type;

template <typename SelectedTuple, typename CallTuple, typename RemainingTuple,
          typename RemainingBoundTuple, typename Indices, std::size_t curIdx>
struct rebind_search;

template <typename... SelectedArgs, typename CallArgsTuple, std::size_t curIdx>
struct rebind_search<std::tuple<SelectedArgs...>, CallArgsTuple, std::tuple<>, std::tuple<>,
                     IndicesHolder<>, curIdx> {
    using type = std::tuple<SelectedArgs...>;
    using CallTuple = CallArgsTuple;
};

template <typename... SelectedArgs, typename... CallArgs, typename... RemainingArgs,
          std::size_t curIdx>
    requires(sizeof...(RemainingArgs) > 0)
struct rebind_search<std::tuple<SelectedArgs...>, std::tuple<CallArgs...>,
                     std::tuple<RemainingArgs...>, std::tuple<>, IndicesHolder<>, curIdx> {
    using type = std::tuple<SelectedArgs..., RemainingArgs...>;
    using CallTuple = std::tuple<CallArgs..., RemainingArgs...>;
};

template <typename... SelectedArgs, typename... CallArgs, typename NextArg,
          typename... RemainingArgs, typename NextBoundArg, typename... RemainingBoundArgs,
          std::size_t nextBoundIdx, std::size_t... Is, std::size_t curIdx>
struct rebind_search<
    std::tuple<SelectedArgs...>, std::tuple<CallArgs...>, std::tuple<NextArg, RemainingArgs...>,
    std::tuple<NextBoundArg, RemainingBoundArgs...>, IndicesHolder<nextBoundIdx, Is...>, curIdx> {
    static_assert(sizeof...(Is) == sizeof...(RemainingBoundArgs));
    static_assert(curIdx <= nextBoundIdx);
    using Next = std::conditional_t<
        curIdx == nextBoundIdx,
        rebind_search<std::tuple<SelectedArgs..., NextBoundArg>, std::tuple<CallArgs...>,
                      std::tuple<RemainingArgs...>, std::tuple<RemainingBoundArgs...>,
                      IndicesHolder<Is...>, curIdx + 1>,
        rebind_search<std::tuple<SelectedArgs..., CallArgTag<NextArg&&>>,
                      std::tuple<CallArgs..., NextArg>, std::tuple<RemainingArgs...>,
                      std::tuple<NextBoundArg, RemainingBoundArgs...>,
                      IndicesHolder<nextBoundIdx, Is...>, curIdx + 1>>;
    using type = typename Next::type;
    using CallTuple = Next::CallTuple;
};

template <typename AllTuple, typename BoundTuple, std::size_t... Is>
struct rebind {  // NOLINT
    using searcher =
        rebind_search<std::tuple<>, std::tuple<>, AllTuple, BoundTuple, IndicesHolder<Is...>, 0>;
    using type = typename searcher::type;
    using CallTuple = typename searcher::CallTuple;
};

template <typename SelectedTuple, typename RemainingTuple, typename RemainingBoundTuple,
          typename Indices, std::size_t curIdx>
struct rebind_search_call_and_bound;

template <typename... SelectedArgs, std::size_t curIdx>
struct rebind_search_call_and_bound<std::tuple<SelectedArgs...>, std::tuple<>, std::tuple<>,
                                    IndicesHolder<>, curIdx> {
    using type = std::tuple<SelectedArgs...>;
};

template <typename... SelectedArgs, typename... RemainingArgs, std::size_t curIdx>
struct rebind_search_call_and_bound<std::tuple<SelectedArgs...>, std::tuple<RemainingArgs...>,
                                    std::tuple<>, IndicesHolder<>, curIdx> {
    using type = std::tuple<SelectedArgs..., RemainingArgs...>;
};

template <typename... SelectedArgs, typename... RemainingBoundArgs, std::size_t... Is,
          std::size_t curIdx>
struct rebind_search_call_and_bound<std::tuple<SelectedArgs...>, std::tuple<>,
                                    std::tuple<RemainingBoundArgs...>, IndicesHolder<Is...>,
                                    curIdx> {
    static_assert(sizeof...(Is) == sizeof...(RemainingBoundArgs));
    using type = std::tuple<SelectedArgs..., RemainingBoundArgs...>;
};

template <typename... SelectedArgs, typename NextArg, typename... RemainingArgs,
          typename NextBoundArg, typename... RemainingBoundArgs, std::size_t nextBoundIdx,
          std::size_t... Is, std::size_t curIdx>
struct rebind_search_call_and_bound<
    std::tuple<SelectedArgs...>, std::tuple<NextArg, RemainingArgs...>,
    std::tuple<NextBoundArg, RemainingBoundArgs...>, IndicesHolder<nextBoundIdx, Is...>, curIdx> {
    static_assert(sizeof...(Is) == sizeof...(RemainingBoundArgs));
    static_assert(curIdx <= nextBoundIdx);
    using Next = std::conditional_t<
        curIdx == nextBoundIdx,
        rebind_search_call_and_bound<
            std::tuple<SelectedArgs..., NextBoundArg>, std::tuple<NextArg, RemainingArgs...>,
            std::tuple<RemainingBoundArgs...>, IndicesHolder<Is...>, curIdx + 1>,
        rebind_search_call_and_bound<std::tuple<SelectedArgs..., CallArgTag<NextArg&&>>,
                                     std::tuple<RemainingArgs...>,
                                     std::tuple<NextBoundArg, RemainingBoundArgs...>,
                                     IndicesHolder<nextBoundIdx, Is...>, curIdx + 1>>;
    using type = typename Next::type;
};

template <typename CallTuple, typename BoundTuple, auto binder>
    requires kIsBinder<binder> && (!kIsIndicelessBinder<binder>)
struct rebind_call_and_bound;

template <typename CallTuple, typename BoundTuple, std::size_t... Is, Binder<Is...> binder>
    requires(sizeof...(Is) > 0)
struct rebind_call_and_bound<CallTuple, BoundTuple, binder> {
    using type = rebind_search_call_and_bound<std::tuple<>, CallTuple, BoundTuple,
                                              IndicesHolder<Is...>, 0>::type;
};

template <std::size_t N, typename Tuple, typename Indices = std::make_index_sequence<N>>
struct slice_first;

template <std::size_t N, typename... Args, std::size_t... Is>
struct slice_first<N, std::tuple<Args...>, std::index_sequence<Is...>> {
    using type = std::tuple<std::tuple_element_t<Is, std::tuple<Args...>>...>;
};

template <std::size_t N, typename Tuple, typename Indices = std::make_index_sequence<N>>
struct slice_last;

template <std::size_t N, typename... Args, std::size_t... Is>
struct slice_last<N, std::tuple<Args...>, std::index_sequence<Is...>> {
    static constexpr std::size_t kTotal = sizeof...(Args);
    static_assert(N <= kTotal, "Slice size N cannot exceed total tuple size.");
    using type = std::tuple<std::tuple_element_t<Is + (kTotal - N), std::tuple<Args...>>...>;
};

template <typename S, auto binder, typename F, typename... BoundArgs>
struct NamedTask;

template <typename>
struct IsNamedTask : std::false_type {
    static constexpr bool kIsRef{false};
    static constexpr bool kIsAuto{false};
};

template <typename S, auto binder, typename F, typename... BoundArgs>
struct IsNamedTask<NamedTask<S, binder, F, BoundArgs...>> : std::true_type {
    static constexpr bool kIsRef{false};
    static constexpr bool kIsAuto{false};
};

struct RefTag {};

template <auto F>
struct FunctionTag {};

template <typename T>
struct IsFunctionTag : std::false_type {};

template <auto F>
struct IsFunctionTag<FunctionTag<F>> : std::true_type {};

template <typename T>
static constexpr auto kIsFunctionTag = IsFunctionTag<T>::value;

template <typename S, Binding binding, auto F, typename... BoundArgs>
struct IsNamedTask<NamedTask<S, binding, FunctionTag<F>, BoundArgs...>> : std::true_type {
    static constexpr bool kIsRef{false};
    static constexpr bool kIsAuto{true};
};

struct NullOpt {
    explicit constexpr NullOpt() = default;
};

inline constexpr NullOpt kNullopt{};

template <typename T>
    requires std::is_trivial_v<T> && std::is_standard_layout_v<T>
struct Optional {
    bool mHasValue{false};
    T mValue{};

    constexpr Optional() noexcept = default;

    constexpr Optional(NullOpt /*unused*/) noexcept {}

    constexpr Optional(T value) noexcept : mHasValue{true}, mValue{value} {}
    constexpr auto& operator=(T value) noexcept {
        mHasValue = true;
        mValue = value;
        return *this;
    }

    [[nodiscard]] constexpr bool has_value() const noexcept {
        return mHasValue;
    }

    constexpr T operator*() const noexcept {
        return mValue;
    }

    [[nodiscard]] constexpr T value_or(T val) const noexcept {
        if (!this->has_value()) {
            return val;
        }
        return this->operator*();
    }
};

template <typename S = void, std::size_t... Is>
struct DeductorSettings;

template <typename S = void, std::size_t... Is>
struct NamedDeductorSettings {
    using Signature = S;
    Binder<Is...> binding{};
    RefQualifier ref{RefQualifier::kNone};

    constexpr NamedDeductorSettings() = default;
    constexpr NamedDeductorSettings(Binding b, RefQualifier r)
        requires(sizeof...(Is) == 0)
        : binding{b}, ref{r} {}

    constexpr NamedDeductorSettings(RefQualifier r)
        requires(sizeof...(Is) > 0)
        : ref{r} {}

    constexpr NamedDeductorSettings(Binding b)
        requires(sizeof...(Is) == 0)
        : binding{b} {}

    [[nodiscard]] constexpr auto toDeductorSettings() const noexcept {
        return DeductorSettings<S, Is...>{*this};
    }

    template <typename NewSignature>
    [[nodiscard]] constexpr auto withSignature() const noexcept {
        if constexpr (sizeof...(Is) == 0) {
            return DeductorSettings<NewSignature>{binding, ref};
        } else {
            return DeductorSettings<NewSignature, Is...>{ref};
        }
    }

    [[nodiscard]] constexpr auto bindFront() const noexcept {
        return NamedDeductorSettings<Signature>{Binding::kFront, ref};
    }

    [[nodiscard]] constexpr auto bindBack() const noexcept {
        return NamedDeductorSettings<Signature>{Binding::kBack, ref};
    }

    template <std::size_t... NewIndices>
        requires(sizeof...(NewIndices) > 0)
    [[nodiscard]] constexpr auto bindTo() const noexcept {
        return NamedDeductorSettings<Signature, NewIndices...>{ref};
    }

    [[nodiscard]] constexpr auto byRef() const noexcept {
        auto copy = *this;
        copy.ref = RefQualifier::kNonConst;
        return copy;
    }

    [[nodiscard]] constexpr auto byCRef() const noexcept {
        auto copy = *this;
        copy.ref = RefQualifier::kConst;
        return copy;
    }
};

template <typename S, std::size_t... Is>
struct DeductorSettings {
    using Signature = S;
    Binder<Is...> binding{};
    Optional<Copyability> cb;
    Optional<std::size_t> size;
    Optional<Triviality> t;
    Optional<HeapPolicy> hp;
    Optional<std::size_t> alg;
    RefQualifier ref{RefQualifier::kNone};

    constexpr DeductorSettings() = default;
    constexpr DeductorSettings(Binding b, Optional<Copyability> c, Optional<std::size_t> s,
                               Optional<Triviality> t, Optional<HeapPolicy> h,
                               Optional<std::size_t> a, RefQualifier r)
        requires(sizeof...(Is) == 0)
        : binding{b}, cb{c}, size{s}, t{t}, hp{h}, alg{a}, ref{r} {}

    constexpr DeductorSettings(Optional<Copyability> c, Optional<std::size_t> s,
                               Optional<Triviality> t, Optional<HeapPolicy> h,
                               Optional<std::size_t> a, RefQualifier r)
        requires(sizeof...(Is) > 0)
        : cb{c}, size{s}, t{t}, hp{h}, alg{a}, ref{r} {}

    constexpr DeductorSettings(Binding b)
        requires(sizeof...(Is) == 0)
        : binding{b} {}

    constexpr DeductorSettings(const NamedDeductorSettings<S, Is...>& namedSettings)
        : binding{namedSettings.binding}, ref{namedSettings.ref} {}

    [[nodiscard]] constexpr const auto& toDeductorSettings() const noexcept {
        return *this;
    }

    template <typename NewSignature>
    [[nodiscard]] constexpr auto withSignature() const noexcept {
        if constexpr (sizeof...(Is) == 0) {
            return DeductorSettings<NewSignature>{binding, cb, size, t, hp, alg, ref};
        } else {
            return DeductorSettings<NewSignature, Is...>{cb, size, t, hp, alg, ref};
        }
    }

    template <std::size_t N>
    [[nodiscard]] constexpr auto withInlineSize() const noexcept {
        auto copy = *this;
        copy.size = N;
        return copy;
    }

    template <std::size_t NewAlignment>
    [[nodiscard]] constexpr auto withAlignment() const noexcept {
        auto copy = *this;
        copy.alg = NewAlignment;
        return copy;
    }

    [[nodiscard]] constexpr auto allowHeap() const noexcept {
        auto copy = *this;
        copy.hp = HeapPolicy::kAllow;
        return copy;
    }

    [[nodiscard]] constexpr auto makeNonTrivial() const noexcept {
        auto copy = *this;
        copy.t = Triviality::kNonTrivial;
        return copy;
    }

    [[nodiscard]] constexpr auto moveOnly() const noexcept {
        auto copy = *this;
        copy.cb = Copyability::kNonCopyable;
        return copy;
    }

    [[nodiscard]] constexpr auto bindFront() const noexcept {
        /*auto copy = *this;
        copy.binding = Binding::kFront;
        return copy;*/
        return DeductorSettings<Signature>{Binding::kFront, cb, size, t, hp, alg, ref};
    }

    [[nodiscard]] constexpr auto bindBack() const noexcept {
        /*auto copy = *this;
        copy.binding = Binding::kBack;
        return copy;*/
        return DeductorSettings<Signature>{Binding::kBack, cb, size, t, hp, alg, ref};
    }

    template <std::size_t... NewIndices>
        requires(sizeof...(NewIndices) > 0)
    [[nodiscard]] constexpr auto bindTo() const noexcept {
        return DeductorSettings<Signature, NewIndices...>{cb, size, t, hp, alg, ref};
    }

    [[nodiscard]] constexpr auto byRef() const noexcept {
        auto copy = *this;
        copy.ref = RefQualifier::kNonConst;
        return copy;
    }

    [[nodiscard]] constexpr auto byCRef() const noexcept {
        auto copy = *this;
        copy.ref = RefQualifier::kConst;
        return copy;
    }
};

FORCE_INLINE constexpr auto getSettings() noexcept {
    return DeductorSettings<>{};
}

template <auto>
struct IsDeductorSettings : std::false_type {};

template <typename Sig, std::size_t... Is, DeductorSettings<Sig, Is...> deductor>
struct IsDeductorSettings<deductor> : std::true_type {};

template <auto>
struct IsNamedDeductorSettings : std::false_type {};

template <typename Sig, std::size_t... Is, NamedDeductorSettings<Sig, Is...> deductor>
struct IsNamedDeductorSettings<deductor> : std::true_type {};

template <typename T>
struct GetCallable {
    using type = T;
};

template <auto F>
struct GetCallable<FunctionTag<F>> {
    using type = decltype(F);
};

template <typename T>
using GetCallableT = GetCallable<T>::type;

template <typename FHolder, typename... BoundArgs>
struct GetPayload {
    using type = PayloadImpl<FHolder, BoundArgs...>;
};

template <auto F, typename... BoundArgs>
struct GetPayload<FunctionTag<F>, BoundArgs...> {
    using type = PayloadImplArgs<F, BoundArgs...>;
};

template <auto F, typename... BoundArgs>
struct GetPayload<FunctionTag<F>&&, BoundArgs...> {
    using type = PayloadImplArgs<F, BoundArgs...>;
};

template <typename FHolder, typename... BoundArgs>
    requires(kIsFunctionTag<std::decay_t<FHolder>> &&
             !std::is_same_v<std::decay_t<FHolder>, FHolder>)
struct GetPayload<FHolder, BoundArgs...> : GetPayload<std::decay_t<FHolder>, BoundArgs...> {};

template <typename FHolder, typename... BoundArgs>
using GetPayloadT = GetPayload<FHolder, BoundArgs...>::type;

template <auto settings, typename Ft, typename... BoundArgs>
struct TaskTypeDeductor;

template <auto newSettings, typename OldDeductor>
struct UpdateDeductor;

template <auto newSettings, auto settings, typename Ft, typename... BoundArgs>
struct UpdateDeductor<newSettings, TaskTypeDeductor<settings, Ft, BoundArgs...>> {
    using type = TaskTypeDeductor<newSettings, Ft, BoundArgs...>;
};

template <auto newSettings, typename OldDeductor>
using UpdateDeductorT = typename UpdateDeductor<newSettings, OldDeductor>::type;

template <typename RequestedSig, std::size_t... Is, DeductorSettings<RequestedSig, Is...> settings,
          typename Ft, typename... BoundArgs>
    requires(kIsFunctionTag<Ft> ||
             HasFunctionTraits<
                 std::remove_reference_t<std::unwrap_reference_t<UnwrapBoundRefWrapT<Ft>>>>)
struct TaskTypeDeductor<settings, Ft, BoundArgs...> {
    using F = GetCallableT<Ft>;
    static constexpr auto kFIsReference = kIsBoundRefWrapper<F> || kIsRefWrapper<F>;
    static constexpr auto kSettings = settings;
    struct StripBoundArgs {
        using Traits =
            FunctionTraits<std::remove_cvref_t<std::unwrap_reference_t<UnwrapBoundRefWrapT<F>>>>;
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
            template <auto binder,
                      template <template <typename> typename, typename...> typename Tester,
                      template <typename> typename Modifier,
                      template <typename> typename Policy = IsCallArg>
            struct ArgumentProcessor;

            template <Binder<> binder,
                      template <template <typename> typename, typename...> typename Tester,
                      template <typename> typename Modifier, template <typename> typename Policy>
            struct ArgumentProcessor<binder, Tester, Modifier, Policy> {
                static constexpr auto kBindingProcess = Binding{binder};
                static constexpr bool kTestFront =
                    Tester<Policy, Modifier<F>, Modifier<BoundArgs>...,
                           CallArgTag<TsR&&>...>::value;
                static constexpr bool kTestBack =
                    Tester<Policy, Modifier<F>, CallArgTag<Ts&&>..., Modifier<BoundArgs>...>::value;
                static constexpr auto kBindDeduct =
                    (kTestBack && kTestBack)
                        ? (kBindingProcess ==  // NOLINT(readability-avoid-nested-conditional-operator)
                                   Binder<>{Binding::kAuto}
                               ? Binder<>{Binding::kBack}
                               : Binder<>{kBindingProcess})
                        : (kTestBack ? Binder<>{Binding::kBack} : Binder<>{Binding::kFront});

                static constexpr bool kIsValid =
                    kBindingProcess == Binding::kFront
                        ? kTestFront
                        : (kBindingProcess ==  // NOLINT(readability-avoid-nested-conditional-operator)
                                   Binding::kBack
                               ? kTestBack
                               : (kTestFront || kTestBack));
                using base_signature = std::conditional_t<Binding{kBindDeduct} == Binding::kFront,
                                                          Ret(TsR...), Ret(Ts...)>;
            };

            template <std::size_t... is, Binder<is...> binder,
                      template <template <typename> typename, typename...> typename Tester,
                      template <typename> typename Modifier, template <typename> typename Policy>
                requires(sizeof...(is) > 0 && sizeof...(is) == sizeof...(BoundArgs))
            struct ArgumentProcessor<binder, Tester, Modifier, Policy> {
                using rebinder = rebind<AllArgs, std::tuple<Modifier<BoundArgs>...>, is...>;
                using TestTuple = rebinder::type;
                using CallTuple = rebinder::CallTuple;

                template <typename, typename>
                struct UnwrapTester;

                template <typename... TestArgs, typename... CallArgs>
                struct UnwrapTester<std::tuple<TestArgs...>, std::tuple<CallArgs...>> {
                    static constexpr auto kValid = Tester<Policy, Modifier<F>, TestArgs...>::value;
                    using base_signature = Ret(CallArgs...);
                };

                static constexpr auto kIsValid = UnwrapTester<TestTuple, CallTuple>::kValid;
                static constexpr auto kBindDeduct = Binder<is...>{};
                using base_signature = UnwrapTester<TestTuple, CallTuple>::base_signature;
            };

            template <typename BaseSignature, bool IsConst, bool LVRef, bool RVRef, bool NoExcept>
            struct ToFullSignature;

            template <typename r, typename... cArgs, bool IsConst, bool LVRef, bool RVRef,
                      bool NoExcept>
            struct ToFullSignature<r(cArgs...), IsConst, LVRef, RVRef, NoExcept> {
                using Signature = std::conditional_t<
                    Traits::kIsNoExcept,
                    // --- NOEXCEPT BRANCH ---
                    std::conditional_t<
                        IsConst,
                        std::conditional_t<LVRef, r(cArgs...) const & noexcept,
                                           std::conditional_t<RVRef, r(cArgs...) const && noexcept,
                                                              r(cArgs...) const noexcept>>,
                        std::conditional_t<LVRef, r(cArgs...) & noexcept,
                                           std::conditional_t<RVRef, r(cArgs...) && noexcept,
                                                              r(cArgs...) noexcept>>>,
                    // --- NON-NOEXCEPT BRANCH ---

                    std::conditional_t<
                        IsConst,
                        std::conditional_t<
                            LVRef, r(cArgs...) const&,
                            std::conditional_t<RVRef, r(cArgs...) const&&, r(cArgs...) const>>,
                        std::conditional_t<
                            LVRef, r(cArgs...) &,
                            std::conditional_t<RVRef, r(cArgs...) &&, r(cArgs...)>>>>;
            };

            template <typename T>
            using NoMod = T;

            template <typename T>
            using NonRefMovable = T&&;

            template <typename T>
            using MakeReferencesMovable =
                std::conditional_t<std::is_lvalue_reference_v<T>, std::remove_reference_t<T>&&, T>;

            using Processor =
                ArgumentProcessor<settings.binding, is_any_compatible_invocable, NoMod>;
            static constexpr bool kIsValid = Processor::kIsValid;
            static constexpr auto kBinderDeduct = Processor::kBindDeduct;
            using base_signature = Processor::base_signature;
            static constexpr bool kIsConst =
                (Traits::kIsConst || kFIsReference) &&
                ArgumentProcessor<kBinderDeduct, is_any_compatible_invocable,
                                  std::add_const_t>::kIsValid;
            static constexpr bool kIsRValRef =
                (Traits::kIsRValueRef || kFIsReference) &&
                ArgumentProcessor<kBinderDeduct, is_any_compatible_invocable, NonRefMovable,
                                  IgnoreExpansionPolicyOnRefInvocation>::kIsValid;
            static constexpr bool kIsLValRef =
                (Traits::kIsLValueRef || kFIsReference) &&
                ArgumentProcessor<kBinderDeduct, is_any_compatible_invocable,
                                  std::add_lvalue_reference_t,
                                  IgnoreExpansionPolicyOnRefInvocation>::kIsValid;

            static constexpr bool kBothRefValid = kIsLValRef && kIsRValRef;
            static constexpr bool kLValRefEffective = kIsLValRef && !kBothRefValid;
            static constexpr bool kRValRefEffective = kIsRValRef && !kBothRefValid;
            using Signature = ToFullSignature<base_signature, kIsConst, kLValRefEffective,
                                              kRValRefEffective, Traits::kIsNoExcept>::Signature;
        };

        using type = rebuild<typename Traits::return_type, SlicedTuple, SlicedTupleReverse>;
    };

    // To complete one needs case where F is static (no decay), but in that case it would be trivial
    // anyway, hence not urgent to change for now
    using PayloadType = GetPayloadT<Ft, BoundArgs...>;

    static constexpr auto kPayloadCopyable = std::is_copy_constructible_v<PayloadType>
                                                 ? Copyability::kCopyable
                                                 : Copyability::kNonCopyable;
    static constexpr auto kCopyValid =
        !settings.cb.has_value() ||
        !(*settings.cb == Copyability::kCopyable && kPayloadCopyable == Copyability::kNonCopyable);
    static constexpr auto kCopyable = settings.cb.has_value() ? *settings.cb : kPayloadCopyable;
    static constexpr auto kPayloadTriviality =
        TrivialUse<PayloadType, kCopyable>::value ? Triviality::kTrivial : Triviality::kNonTrivial;
    static constexpr auto kTrivialValid =
        !settings.t.has_value() ||
        !(*settings.t == Triviality::kTrivial && kPayloadTriviality != Triviality::kTrivial);
    static constexpr auto kTriviality =
        settings.t.has_value()
            ? *settings.t
            : ((settings.hp.has_value() && *settings.hp == HeapPolicy::kAllow)  // NOLINT
                   ? Triviality::kNonTrivial
                   : kPayloadTriviality);
    static constexpr Alignment kPayloadAlignment =
        std::max(alignof(PayloadType), alignof(std::max_align_t));
    static constexpr auto kAlignmentValid =
        !settings.alg.has_value() || *settings.alg >= alignof(PayloadType);
    static constexpr auto kAlignment =
        settings.alg.has_value() ? Alignment{*settings.alg} : kPayloadAlignment;
    static constexpr auto kPayloadSize = sizeof(PayloadType);
    static constexpr auto kSizeValid =
        !settings.size.has_value() || *settings.size >= kPayloadSize ||
        (settings.hp.has_value() && *settings.hp == HeapPolicy::kAllow &&
         *settings.size >= sizeof(std::byte*));
    static constexpr auto kSize =
        settings.size.has_value()
            ? *settings.size
            : ((settings.hp.has_value() && *settings.hp == HeapPolicy::kAllow)  // NOLINT
                   ? std::min<std::size_t>(kPayloadSize, 16U)
                   : kPayloadSize);
    static constexpr auto kHeapPolicy =
        settings.hp.has_value() ? *settings.hp : HeapPolicy::kDisallow;
    using TypeTraits = StripBoundArgs::type;
    using Signature = std::conditional_t<std::is_void_v<RequestedSig>,
                                         typename TypeTraits::Signature, RequestedSig>;
    static constexpr auto kSignatureValid =
        kIsSignatureCompatibleB<typename TypeTraits::Signature, Signature>;
    static constexpr auto kInvocableValid = TypeTraits::kIsValid;
    static constexpr bool kIsValid = kInvocableValid && kSignatureValid && kCopyValid &&
                                     kTrivialValid && kAlignmentValid && kSizeValid;

    template <typename Sig = Signature>
    using Type = Task<Sig, kCopyable, kSize, kTriviality, false, kHeapPolicy, kAlignment>;

    template <typename Sig = Signature>
    using NamedType = NamedTask<Sig, settings.binding, Ft, BoundArgs...>;

    template <typename Sig = Signature, InlineSize size = 16>
    using HeapType = Task<Sig, kCopyable, size, kTriviality, false, HeapPolicy::kAllow, kAlignment>;

    template <typename NewSignature>
    static consteval auto withSignature() noexcept {}

    template <std::size_t N>
    static consteval auto withInlineSize() noexcept {
        constexpr auto kNewDeductor =
            UpdateDeductorT<kSettings.template withInlineSize<N>(),
                            TaskTypeDeductor<settings, Ft, BoundArgs...>>{};
        static_assert(decltype(kNewDeductor)::kIsValid,
                      "Requested Signature is not compatible with initial/deducted signature");
        return kNewDeductor;
    }

    template <std::size_t N>
        requires(N != 0 && std::has_single_bit(N))
    static consteval auto withAlignment() noexcept {
        constexpr auto kNewDeductor =
            UpdateDeductorT<settings.template withAlignment<N>(),
                            TaskTypeDeductor<settings, Ft, BoundArgs...>>{};
        static_assert(decltype(kNewDeductor)::kIsValid,
                      "Requested Signature is not compatible with initial/deducted alignment");
        return kNewDeductor;
    }

    static consteval auto allowHeap() noexcept {
        return UpdateDeductorT<settings.allowHeap(),
                               TaskTypeDeductor<settings, Ft, BoundArgs...>>{};
    }

    static consteval auto moveOnly() noexcept {
        constexpr auto kNewDeductor =
            UpdateDeductorT<settings.moveOnly(), TaskTypeDeductor<settings, Ft, BoundArgs...>>{};
        static_assert(decltype(kNewDeductor)::kIsValid,
                      "Task is invalid under new copyability setting");
        return kNewDeductor;
    }
};

template <auto settings, typename Ft, typename... BoundArgs>
struct TaskBuilder;

template <RefQualifier q, typename T, bool Invocable = false>
struct AddRefWrapFilter {
    using type = AddRefWrapperToRef_t<T, q == RefQualifier::kConst, Invocable>;
};

template <typename T, bool Invocable>
struct AddRefWrapFilter<RefQualifier::kNone, T, Invocable> {
    using type = T;
};

template <RefQualifier q, typename T, bool Invocable = false>
using AddRefWrapFilterT = AddRefWrapFilter<q, T, Invocable>::type;

template <RefQualifier q, typename T, bool Invocable = false>
struct TaskBuilderRefFilter {
    using type = AddRefWrapperToRef_t<T, q == RefQualifier::kConst, Invocable>;
};

template <typename T, bool Invocable>
struct TaskBuilderRefFilter<RefQualifier::kNone, T, Invocable> {
    using type = std::decay_t<T>;
};

template <RefQualifier Ref, typename T, bool Invocable = false>
using TaskBuilderRefFilterT = TaskBuilderRefFilter<Ref, T, Invocable>::type;

template <auto settings, typename Ft, typename... BoundArgs>
    requires((kIsFunctionTag<Ft> ||
              HasFunctionTraits<
                  std::remove_reference_t<std::unwrap_reference_t<UnwrapBoundRefWrapT<Ft>>>>) &&
             (IsNamedDeductorSettings<settings>::value || IsDeductorSettings<settings>::value))
struct TaskBuilder<settings, Ft, BoundArgs...>
    : TaskTypeDeductor<settings.toDeductorSettings(), TaskBuilderRefFilterT<settings.ref, Ft, true>,
                       TaskBuilderRefFilterT<settings.ref, BoundArgs>...> {
    TaskBuilder(auto&& f,  // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
                auto&&... args)
        requires(!kIsFunctionTag<Ft>)
        : transitPayload{.f = {std::forward<decltype(f)>(f)},
                         .args = {std::forward<decltype(args)>(args)...}} {}

    TaskBuilder(BoundArgs&&... args)
        requires(kIsFunctionTag<Ft>)
        : transitPayload{.args = {std::forward<BoundArgs>(args)...}} {}

    TaskBuilder() = delete;
    TaskBuilder(const TaskBuilder&) = delete;
    TaskBuilder& operator=(const TaskBuilder&) = delete;
    TaskBuilder(TaskBuilder&&) = delete;
    TaskBuilder& operator=(TaskBuilder&&) = delete;
    ~TaskBuilder() = default;

    static constexpr auto kNamedTaskBuilder = IsNamedDeductorSettings<settings>::value;

    static constexpr auto kInvocable{true};
    using Payload = GetPayloadT<AddRefWrapFilterT<settings.ref, Ft&&, kInvocable>,
                                AddRefWrapFilterT<settings.ref, BoundArgs&&>...>;
    Payload transitPayload;

    static_assert(kIsRefPayloadTuple<decltype(transitPayload.args)>);

    TaskBuilder(Payload&& payloadOther) : transitPayload{std::move(payloadOther)} {}

    static consteval auto getSettings() noexcept {
        return settings;
    }

    using Deductor =
        TaskTypeDeductor<DeductorSettings{settings}, TaskBuilderRefFilterT<settings.ref, Ft, true>,
                         TaskBuilderRefFilterT<settings.ref, BoundArgs>...>;

    template <typename NewSignature>
    FORCE_INLINE constexpr auto withSignature() && noexcept {
        return TaskBuilder<settings.template withSignature<NewSignature>(), Ft, BoundArgs...>{
            std::move(transitPayload)};
    }

    template <std::size_t N>
        requires(!kNamedTaskBuilder)
    FORCE_INLINE constexpr auto withInlineSize() && noexcept {
        return TaskBuilder<settings.template withInlineSize<N>(), Ft, BoundArgs...>{
            std::move(transitPayload)};
    }

    template <std::size_t N>
        requires(!kNamedTaskBuilder && N != 0 && std::has_single_bit(N))
    FORCE_INLINE constexpr auto withAlignment() && noexcept {
        return TaskBuilder<settings.template withAlignment<N>(), Ft, BoundArgs...>{
            std::move(transitPayload)};
    }

    FORCE_INLINE constexpr auto makeNonTrivial() && noexcept
        requires(!kNamedTaskBuilder)
    {
        return TaskBuilder<settings.makeNonTrivial(), Ft, BoundArgs...>{std::move(transitPayload)};
    }

    FORCE_INLINE constexpr auto allowHeap() && noexcept
        requires(!kNamedTaskBuilder)
    {
        return TaskBuilder<settings.allowHeap(), Ft, BoundArgs...>{std::move(transitPayload)};
    }

    FORCE_INLINE constexpr auto moveOnly() && noexcept
        requires(!kNamedTaskBuilder)
    {
        return TaskBuilder<settings.moveOnly(), Ft, BoundArgs...>{std::move(transitPayload)};
    }

    FORCE_INLINE constexpr auto bindFront() && noexcept {
        return TaskBuilder<settings.bindFront(), Ft, BoundArgs...>{std::move(transitPayload)};
    }

    FORCE_INLINE constexpr auto bindBack() && noexcept {
        return TaskBuilder<settings.bindBack(), Ft, BoundArgs...>{std::move(transitPayload)};
    }

    template <std::size_t... Is>
        requires(sizeof...(Is) > 0)
    FORCE_INLINE constexpr auto bindTo() && noexcept {
        return TaskBuilder<settings.template bindTo<Is...>(), Ft, BoundArgs...>{
            std::move(transitPayload)};
    }

    template <std::size_t StartIdx>
    FORCE_INLINE constexpr auto bindFrom() && noexcept {
        return [this]<std::size_t... Is>(std::index_sequence<Is...>) {
            return TaskBuilder<settings.template bindTo<(StartIdx + Is)...>(), Ft, BoundArgs...>{
                std::move(transitPayload)};
        }(std::make_index_sequence<sizeof...(BoundArgs)>{});
    }

    FORCE_INLINE constexpr auto byRef() && noexcept {
        auto&& args = std::move(transitPayload.args);
        static constexpr bool kIgnoreRefWrapperRValueConversion{true};
        if constexpr (kIsFunctionTag<Ft>) {
            return std::move(args).template applyAsArgs<kIgnoreRefWrapperRValueConversion>(
                []<typename... Args>(Args&&... args) {  // NOLINT
                    return TaskBuilder<settings.byRef(), Ft, BoundArgs...>{
                        std::forward<Args>(args)...};
                });
        } else {
            return std::move(args).template applyAsArgs<kIgnoreRefWrapperRValueConversion>(
                [this]<typename... Args>(Args&&... args) {  // NOLINT
                    return TaskBuilder<settings.byRef(), Ft, BoundArgs...>{
                        std::forward<decltype(transitPayload.f)>(transitPayload.f),
                        std::forward<Args>(args)...};
                });
        }
    }

    FORCE_INLINE constexpr auto byCRef() && noexcept {
        auto&& args = std::move(transitPayload.args);
        static constexpr bool kIgnoreRefWrapperRValueConversion{true};
        if constexpr (kIsFunctionTag<Ft>) {
            return std::move(args).template applyAsArgs<kIgnoreRefWrapperRValueConversion>(
                []<typename... Args>(Args&&... args) {  // NOLINT
                    return TaskBuilder<settings.byCRef(), Ft, BoundArgs...>{
                        std::forward<Args>(args)...};
                });
        } else {
            return std::move(args).template applyAsArgs<kIgnoreRefWrapperRValueConversion>(
                [this]<typename... Args>(Args&&... args) {  // NOLINT
                    return TaskBuilder<settings.byCRef(), Ft, BoundArgs...>{
                        std::forward<decltype(transitPayload.f)>(transitPayload.f),
                        std::forward<Args>(args)...};
                });
        }
    }

    template <auto newSettings>
        requires(IsDeductorSettings<newSettings>::value)
    FORCE_INLINE constexpr auto withSettings() && noexcept {
        return TaskBuilder<newSettings, Ft, BoundArgs...>{std::move(transitPayload)};
    }

    template<typename T>
    FORCE_INLINE constexpr auto like() && noexcept
        requires(!kNamedTaskBuilder && IsTask<std::remove_cvref_t<T>>::value)
    {
        using TaskSrc = std::remove_cvref_t<T>;
        auto tmp = std::move(*this)
                       .template withSignature<typename TaskSrc::Signature>()
                       .template withInlineSize<TaskSrc::kSize>()
                       .template withAlignment<TaskSrc::kAlignment>();
        if constexpr (!TaskSrc::kCopyable) {
            tmp = std::move(tmp).moveOnly();
        }
        if constexpr (!TaskSrc::kIsTrivial) {
            tmp = std::move(tmp).makeNonTrivial();
        }
        if constexpr (TaskSrc::kAllowHeap) {
            tmp = std::move(tmp).allowHeap();
        }
        return TaskBuilder<tmp.getSettings(), Ft, BoundArgs...>{std::move(tmp.transitPayload)};
    }

    FORCE_INLINE constexpr auto like(auto& task) && noexcept
        requires(!kNamedTaskBuilder && IsTask<std::remove_cvref_t<decltype(task)>>::value)
    {
        return std::move(*this).template like<decltype(task)>();
    }

    FORCE_INLINE constexpr auto build() && noexcept
        requires kNamedTaskBuilder
    {
        static_assert(Deductor::kInvocableValid,
                      "Given callable, bound args and binding are not valid for invocation");
        auto&& args = std::move(transitPayload.args);
        static constexpr bool kIgnoreRefWrapperRValueConversion{true};
        if constexpr (kIsFunctionTag<Ft>) {
            return std::move(args).template applyAsArgs<kIgnoreRefWrapperRValueConversion>(
                []<typename... Args>(Args&&... args) {  // NOLINT
                    return typename Deductor::template NamedType<>{
                        std::forward<decltype(args)>(args)...};
                });
        } else {
            return std::move(args).template applyAsArgs<kIgnoreRefWrapperRValueConversion>(
                [this]<typename... Args>(Args&&... args) {  // NOLINT
                    if constexpr (sizeof...(args) == 0 &&
                                  IsNamedTask<std::decay_t<decltype(transitPayload.f)>>::value &&
                                  settings.ref == RefQualifier::kNone) {
                        return std::decay_t<decltype(transitPayload.f)>{
                            std::forward<decltype(transitPayload.f)>(transitPayload.f)};
                    } else {
                        return typename Deductor::template NamedType<>{
                            std::forward<decltype(transitPayload.f)>(transitPayload.f),
                            std::forward<decltype(args)>(args)...};
                    }
                });
        }
    }

    FORCE_INLINE constexpr auto build() && noexcept
        requires(!kNamedTaskBuilder)
    {
        static_assert(Deductor::kInvocableValid,
                      "Given callable, bound args and binding are not valid for invocation");
        static_assert(Deductor::kSignatureValid,
                      "Requested signature is not convertible from deduced one");
        static_assert(Deductor::kCopyValid, "Task is invalid under new copyability setting");
        static_assert(Deductor::kTrivialValid, "New Trivial setting is not valid");
        static_assert(
            Deductor::kSizeValid,
            "Size is invalid for deducted args. Check that it is enough to hold its payload, or "
            "that it's enough to hold a byte pointer with heap policy enabled");
        static_assert(Deductor::kAlignmentValid,
                      "Requested alignment is not valid. It must not be lower than the alignment "
                      "of the payload itself");
        static_assert(Deductor::kIsValid,
                      "Given callable and args are not valid, and a Task cannot be built with "
                      "these. Check for invocation and given factory settings");
        auto&& args = std::move(transitPayload.args);
        static constexpr bool kIgnoreRefWrapperRValueConversion{true};
        if constexpr (kIsFunctionTag<Ft>) {
            return std::move(args).template applyAsArgs<kIgnoreRefWrapperRValueConversion>(
                []<typename... Args>(Args&&... args) {  // NOLINT
                    return typename Deductor::template Type<>{
                        Ft{}, BinderTag<settings.binding>{}, std::forward<decltype(args)>(args)...};
                });
        } else {
            return std::move(args).template applyAsArgs<kIgnoreRefWrapperRValueConversion>(
                [this]<typename... Args>(Args&&... args) {  // NOLINT
                    if constexpr (sizeof...(BoundArgs) ==
                                  0) {  // Accounts for 'f' being a Task, where one
                                        // shouldn't put BindingTag in it but let
                                        // constructor Task to Task trigger
                        return typename Deductor::template Type<>{
                            std::forward<decltype(transitPayload.f)>(transitPayload.f)};
                    } else {
                        return typename Deductor::template Type<>{
                            BinderTag<settings.binding>{},
                            std::forward<decltype(transitPayload.f)>(transitPayload.f),
                            std::forward<decltype(args)>(args)...};
                    }
                });
        }
    }
};

// Support for std::bind, std::bind_front, std::bind_back... (Note, it's implementation defined,
// though this works at least for gcc across Windows and Linux currently)

template <template <typename, typename...> typename T, typename Fd, typename... BoundArgs>
    requires(!IsTask<T<Fd, BoundArgs...>>::value && !IsNamedTask<T<Fd, BoundArgs...>>::value &&
             HasFunctionTraits<std::remove_cvref_t<std::unwrap_reference_t<Fd>>> &&
             TaskTypeDeductor<DeductorSettings<void>{}, Fd, BoundArgs...>::kIsValid)
struct FunctionTraits<T<Fd, BoundArgs...>>
    : FunctionTraits<
          typename TaskTypeDeductor<DeductorSettings<void>{}, Fd, BoundArgs...>::Signature> {};

template <template <bool, typename, typename...> typename T, bool B, typename Fd,
          typename... BoundArgs>
    requires(!IsTask<T<B, Fd, BoundArgs...>>::value &&
             !IsNamedTask<T<B, Fd, BoundArgs...>>::value &&
             HasFunctionTraits<std::remove_cvref_t<std::unwrap_reference_t<Fd>>> &&
             TaskTypeDeductor<DeductorSettings<void>{}, Fd, BoundArgs...>::kIsValid)
struct FunctionTraits<T<B, Fd, BoundArgs...>>
    : FunctionTraits<
          typename TaskTypeDeductor<DeductorSettings<void>{}, Fd, BoundArgs...>::Signature> {};

// libc++ implementation
template <template <typename, typename> typename T, typename Fd, typename... BoundArgs>
    requires(!IsTask<T<Fd, std::tuple<BoundArgs...>>>::value &&
             !IsNamedTask<T<Fd, std::tuple<BoundArgs...>>>::value &&
             HasFunctionTraits<std::remove_cvref_t<std::unwrap_reference_t<Fd>>> &&
             TaskTypeDeductor<DeductorSettings<void>{}, Fd, BoundArgs...>::kIsValid)
struct FunctionTraits<T<Fd, std::tuple<BoundArgs...>>>
    : FunctionTraits<
          typename TaskTypeDeductor<DeductorSettings<void>{}, Fd, BoundArgs...>::Signature> {};

}  // namespace fn

#endif  // TASK_DEDUCTORS_HPP