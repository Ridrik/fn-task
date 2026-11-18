#ifndef TASK_DEDUCTORS_HPP
#define TASK_DEDUCTORS_HPP

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>

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

template <Binding bind>
struct BindingTag {};

template <typename>
struct IsBindingTag : std::false_type {};

template <Binding bind>
struct IsBindingTag<BindingTag<bind>> : std::true_type {};

template <typename T>
static constexpr bool IsBindingTagB = IsBindingTag<T>::value;  // NOLINT

template <typename S, Binding binding, typename F, typename... BoundArgs>
struct NamedTask;

template <typename>
struct IsNamedTask : std::false_type {
    static constexpr bool kIsRef{false};
    static constexpr bool kIsAuto{false};
};

template <typename S, Binding binding, typename F, typename... BoundArgs>
struct IsNamedTask<NamedTask<S, binding, F, BoundArgs...>> : std::true_type {
    static constexpr bool kIsRef{false};
    static constexpr bool kIsAuto{false};
};

struct RefTag {};

template <typename F>
struct NamedTaskRefTag {};

struct NTRefTagCommon {};

template <auto F>
struct NamedTaskStaticRefTag {};

template <auto F>
struct StaticFn {};

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

template <typename S = void>
struct DeductorSettings {
    using Signature = S;
    Binding binding{Binding::kAuto};
    Optional<Copyability> cb;
    Optional<std::size_t> size;
    Optional<Triviality> t;
    Optional<HeapPolicy> hp;
    Optional<std::size_t> alg;
    bool ref{false};

    template <typename NewSignature>
    [[nodiscard]] constexpr auto withSignature() const noexcept {
        return DeductorSettings<NewSignature>{
            .binding = binding, .cb = cb, .size = size, .t = t, .hp = hp, .alg = alg, .ref = ref};
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
        auto copy = *this;
        copy.binding = Binding::kFront;
        return copy;
    }

    [[nodiscard]] constexpr auto bindBack() const noexcept {
        auto copy = *this;
        copy.binding = Binding::kBack;
        return copy;
    }

    [[nodiscard]] constexpr auto byRef() const noexcept {
        auto copy = *this;
        copy.ref = true;
        return copy;
    }
};

FORCE_INLINE constexpr auto getSettings() noexcept {
    return DeductorSettings<>{};
}

template <auto>
struct IsDeductorSettings : std::false_type {};

template <typename Sig, DeductorSettings<Sig> deductor>
struct IsDeductorSettings<deductor> : std::true_type {};

template <typename S = void>
struct NamedDeductorSettings {
    using Signature = S;
    Binding binding{Binding::kAuto};
    bool ref{false};

    template <typename NewSignature>
    [[nodiscard]] constexpr auto withSignature() const noexcept {
        return DeductorSettings<NewSignature>{.binding = binding, .ref = ref};
    }

    [[nodiscard]] constexpr auto bindFront() const noexcept {
        auto copy = *this;
        copy.binding = Binding::kFront;
        return copy;
    }

    [[nodiscard]] constexpr auto bindBack() const noexcept {
        auto copy = *this;
        copy.binding = Binding::kBack;
        return copy;
    }

    [[nodiscard]] constexpr auto byRef() const noexcept {
        auto copy = *this;
        copy.ref = true;
        return copy;
    }
};

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

template <typename RequestedSig, DeductorSettings<RequestedSig> settings, typename Ft,
          typename... BoundArgs>
    requires(kIsFunctionTag<Ft> ||
             HasFunctionTraits<std::remove_reference_t<std::unwrap_reference_t<Ft>>>)
struct TaskTypeDeductor<settings, Ft, BoundArgs...> {
    using F = std::unwrap_reference_t<GetCallableT<Ft>>;
    static constexpr auto kSettings = settings;
    struct StripBoundArgs {
        using Traits = FunctionTraits<std::remove_cvref_t<F>>;
        using AllArgs = Traits::args_tuple;
        using InternalFuncPtr = Traits::func_ptr;

        static constexpr std::size_t kRemainingCount =
            std::tuple_size_v<AllArgs> - sizeof...(BoundArgs);

        using SlicedTuple = slice_first<kRemainingCount, AllArgs>::type;
        using SlicedTupleReverse = slice_last<kRemainingCount, AllArgs>::type;

        template <typename Ret, typename Tuple, typename TupleReverse, typename FilteredBoundArgs>
        struct rebuild;
        template <typename Ret, typename... Ts, typename... TsR, typename... FiltBoundArgs>
        struct rebuild<Ret, std::tuple<Ts...>, std::tuple<TsR...>, std::tuple<FiltBoundArgs...>> {
            template <Binding bindProcess, template <typename...> typename Tester,
                      template <typename> typename Modifier>
            struct ArgumentProcessor {
                template <typename ArgFilter>
                struct ModifierFilter;

                template <typename ArgFilter>
                    requires IsCallArgB<ArgFilter>
                struct ModifierFilter<ArgFilter> {
                    using type = ArgFilter;
                };

                template <typename ArgFilter>
                    requires(!IsCallArgB<ArgFilter>)
                struct ModifierFilter<ArgFilter> {
                    using type = Modifier<ArgFilter>;
                };

                static constexpr bool kTestFront =
                    Tester<F, typename ModifierFilter<FiltBoundArgs>::type..., TsR...>::value;
                static constexpr bool kTestBack =
                    Tester<F, Ts..., typename ModifierFilter<FiltBoundArgs>::type...>::value;
                static constexpr Binding kBindDeduct =
                    (kTestBack && kTestBack)
                        ? (bindProcess ==  // NOLINT(readability-avoid-nested-conditional-operator)
                                   Binding::kAuto
                               ? Binding::kBack
                               : bindProcess)
                        : (kTestBack ? Binding::kBack : Binding::kFront);

                static constexpr bool kIsValid =
                    bindProcess == Binding::kFront
                        ? kTestFront
                        : (bindProcess ==  // NOLINT(readability-avoid-nested-conditional-operator)
                                   Binding::kBack
                               ? kTestBack
                               : (kTestFront || kTestBack));
            };

            template <typename T>
            using NoMod = T;

            template <typename T>
            using MakeMovable = std::add_rvalue_reference_t<std::remove_reference_t<T>>;

            template <Binding bindProcess, template <typename...> typename Tester,
                      template <typename> typename Modifier>
            using ArgumentProcessorV = ArgumentProcessor<bindProcess, Tester, Modifier>::kValid;

            using Processor = ArgumentProcessor<settings.binding, is_compatible_invocable, NoMod>;
            static constexpr bool kIsValid = Processor::kIsValid;
            static constexpr Binding kBinderDeduct = Processor::kBindDeduct;
            static constexpr bool kIsReverse = kBinderDeduct == Binding::kFront;
            static constexpr bool kIsConst =
                Traits::kIsConst &&
                ArgumentProcessor<kBinderDeduct, std::is_invocable, std::add_const_t>::kIsValid;
            static constexpr bool kIsRValRef =
                Traits::kIsRValueRef &&
                ArgumentProcessor<kBinderDeduct, std::is_invocable, MakeMovable>::kIsValid;
            static constexpr bool kIsLValRef =
                Traits::kIsLValueRef && ArgumentProcessor<kBinderDeduct, std::is_invocable,
                                                          std::add_lvalue_reference_t>::kIsValid;
            using Signature = std::conditional_t<
                Traits::kIsNoExcept,
                // --- NOEXCEPT BRANCH ---
                std::conditional_t<
                    kIsReverse,
                    std::conditional_t<
                        kIsConst,
                        std::conditional_t<
                            kIsLValRef, Ret(TsR...) const & noexcept,
                            std::conditional_t<kIsRValRef, Ret(TsR...) const && noexcept,
                                               Ret(TsR...) const noexcept>>,
                        std::conditional_t<kIsLValRef, Ret(TsR...) & noexcept,
                                           std::conditional_t<kIsRValRef, Ret(TsR...) && noexcept,
                                                              Ret(TsR...) noexcept>>>,
                    std::conditional_t<
                        kIsConst,
                        std::conditional_t<
                            kIsLValRef, Ret(Ts...) const & noexcept,
                            std::conditional_t<kIsRValRef, Ret(Ts...) const && noexcept,
                                               Ret(Ts...) const noexcept>>,
                        std::conditional_t<kIsLValRef, Ret(Ts...) & noexcept,
                                           std::conditional_t<kIsRValRef, Ret(Ts...) && noexcept,
                                                              Ret(Ts...) noexcept>>>>,
                // --- NON-NOEXCEPT BRANCH ---
                std::conditional_t<
                    kIsReverse,
                    std::conditional_t<
                        kIsConst,
                        std::conditional_t<
                            kIsLValRef, Ret(TsR...) const&,
                            std::conditional_t<kIsRValRef, Ret(TsR...) const&&, Ret(TsR...) const>>,
                        std::conditional_t<
                            kIsLValRef, Ret(TsR...) &,
                            std::conditional_t<kIsRValRef, Ret(TsR...) &&, Ret(TsR...)>>>,
                    std::conditional_t<
                        kIsConst,
                        std::conditional_t<
                            kIsLValRef, Ret(Ts...) const&,
                            std::conditional_t<kIsRValRef, Ret(Ts...) const&&, Ret(Ts...) const>>,
                        std::conditional_t<
                            kIsLValRef, Ret(Ts...) &,
                            std::conditional_t<kIsRValRef, Ret(Ts...) &&, Ret(Ts...)>>>>>;
        };

        using type = rebuild<typename Traits::return_type, SlicedTuple, SlicedTupleReverse,
                             std::tuple<std::unwrap_reference_t<BoundArgs>...>>;
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
    static constexpr auto kSize = settings.size.has_value() ? *settings.size : kPayloadSize;
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
    using NamedType =
        NamedTask<Sig,
                  settings.binding == Binding::kAuto ? TypeTraits::kBinderDeduct : settings.binding,
                  Ft, BoundArgs...>;

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

template <bool, typename T>
struct TaskBuilderRefFilter {
    using type = T;
};

template <typename T>
struct TaskBuilderRefFilter<false, T> {
    using type = std::decay_t<T>;
};

template <bool Ref, typename T>
using TaskBuilderRefFilterT = TaskBuilderRefFilter<Ref, T>::type;

template <typename Signature, DeductorSettings<Signature> settings, typename Ft,
          typename... BoundArgs>
    requires(kIsFunctionTag<Ft> ||
             HasFunctionTraits<std::remove_reference_t<std::unwrap_reference_t<Ft>>>)
struct TaskBuilder<settings, Ft, BoundArgs...>
    : TaskTypeDeductor<settings, TaskBuilderRefFilterT<settings.ref, Ft>,
                       TaskBuilderRefFilterT<settings.ref, BoundArgs>...> {
    TaskBuilder(Ft&& f,  // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
                BoundArgs&&... args)
        requires(!kIsFunctionTag<Ft>)
        : transitPayload{.f = std::forward<Ft>(f), .args = {std::forward<BoundArgs>(args)...}} {}

    TaskBuilder(BoundArgs&&... args)
        requires(kIsFunctionTag<Ft>)
        : transitPayload{.args = {std::forward<BoundArgs>(args)...}} {}

    TaskBuilder() = delete;
    TaskBuilder(const TaskBuilder&) = delete;
    TaskBuilder& operator=(const TaskBuilder&) = delete;
    TaskBuilder(TaskBuilder&&) = delete;
    TaskBuilder& operator=(TaskBuilder&&) = delete;
    ~TaskBuilder() = default;

    GetPayloadT<Ft&&, BoundArgs&&...> transitPayload;

    static_assert(kIsRefPayloadTuple<decltype(transitPayload.args)>);

    TaskBuilder(GetPayloadT<Ft&&, BoundArgs&&...>&& payloadOther)
        : transitPayload{std::move(payloadOther)} {}

    static consteval auto getSettings() noexcept {
        return settings;
    }

    using Deductor = TaskTypeDeductor<settings, TaskBuilderRefFilterT<settings.ref, Ft>,
                                      TaskBuilderRefFilterT<settings.ref, BoundArgs>...>;

    template <typename NewSignature>
    FORCE_INLINE constexpr auto withSignature() && noexcept {
        return TaskBuilder<settings.template withSignature<NewSignature>(), Ft, BoundArgs...>{
            std::move(transitPayload)};
    }

    template <std::size_t N>
    FORCE_INLINE constexpr auto withInlineSize() && noexcept {
        using NewDeductor = UpdateDeductorT<settings.template withInlineSize<N>(), Deductor>;
        return TaskBuilder<settings.template withInlineSize<N>(), Ft, BoundArgs...>{
            std::move(transitPayload)};
    }

    template <std::size_t N>
        requires(N != 0 && std::has_single_bit(N))
    FORCE_INLINE constexpr auto withAlignment() && noexcept {
        using NewDeductor = UpdateDeductorT<settings.template withAlignment<N>(), Deductor>;
        return TaskBuilder<settings.template withAlignment<N>(), Ft, BoundArgs...>{
            std::move(transitPayload)};
    }

    FORCE_INLINE constexpr auto makeNonTrivial() && noexcept {
        using NewDeductor = UpdateDeductorT<settings.makeNonTrivial(), Deductor>;
        return TaskBuilder<settings.makeNonTrivial(), Ft, BoundArgs...>{std::move(transitPayload)};
    }

    FORCE_INLINE constexpr auto allowHeap() && noexcept {
        return TaskBuilder<settings.allowHeap(), Ft, BoundArgs...>{std::move(transitPayload)};
    }

    FORCE_INLINE constexpr auto moveOnly() && noexcept {
        using NewDeductor = UpdateDeductorT<settings.moveOnly(), Deductor>;
        return TaskBuilder<settings.moveOnly(), Ft, BoundArgs...>{std::move(transitPayload)};
    }

    FORCE_INLINE constexpr auto bindFront() && noexcept {
        return TaskBuilder<settings.bindFront(), Ft, BoundArgs...>{std::move(transitPayload)};
    }

    FORCE_INLINE constexpr auto bindBack() && noexcept {
        return TaskBuilder<settings.bindBack(), Ft, BoundArgs...>{std::move(transitPayload)};
    }

    FORCE_INLINE constexpr auto byRef() && noexcept {
        return TaskBuilder<settings.byRef(), Ft, BoundArgs...>{std::move(transitPayload)};
    }

    template <auto newSettings>
        requires(IsDeductorSettings<newSettings>::value)
    FORCE_INLINE constexpr auto withSettings() && noexcept {
        return TaskBuilder<newSettings, Ft, BoundArgs...>{std::move(transitPayload)};
    }

    FORCE_INLINE constexpr auto buildNamed() && noexcept {
        static_assert(Deductor::kInvocableValid,
                      "Given callable, bound args and binding are not valid for invocation");
        auto&& args = std::move(transitPayload.args);
        static constexpr bool kIgnoreRefWrapperRValueConversion{true};
        return std::move(args).template applyAsArgs<kIgnoreRefWrapperRValueConversion>(
            [this]<typename... Args>(Args&&... args) {
                if constexpr (kIsFunctionTag<Ft>) {
                    return
                        typename Deductor::template NamedType<>{Ft{}, std::forward<Args>(args)...};
                } else {
                    return typename Deductor::template NamedType<>{
                        std::forward<Ft>(transitPayload.f), std::forward<Args>(args)...};
                }
            });
    }

    FORCE_INLINE constexpr auto build() && noexcept {
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
        return std::move(args).template applyAsArgs<kIgnoreRefWrapperRValueConversion>(
            [this]<typename... Args>(Args&&... args) {
                if constexpr (settings.ref) {
                    if constexpr (kIsFunctionTag<Ft>) {
                        return typename Deductor::template Type<>{RefTag{}, Ft{},
                                                                  BindingTag<settings.binding>{},
                                                                  std::forward<Args>(args)...};
                    } else {
                        return typename Deductor::template Type<>{
                            RefTag{}, BindingTag<settings.binding>{},
                            std::forward<Ft>(transitPayload.f), std::forward<Args>(args)...};
                    }
                } else {
                    if constexpr (kIsFunctionTag<Ft>) {
                        return typename Deductor::template Type<>{
                            Ft{}, BindingTag<settings.binding>{}, std::forward<Args>(args)...};
                    } else {
                        if constexpr (sizeof...(BoundArgs) ==
                                      0) {  // Accounts for 'f' being a Task, where one shouldn't
                                            // put BindingTag in it but let constructor Task to Task
                                            // trigger
                            return typename Deductor::template Type<>{
                                std::forward<Ft>(transitPayload.f)};
                        } else {
                            return typename Deductor::template Type<>{
                                BindingTag<settings.binding>{}, std::forward<Ft>(transitPayload.f),
                                std::forward<Args>(args)...};
                        }
                    }
                }
            });
    }
};

template <auto F_auto, DeductorSettings settings, typename... BoundArgs>
    requires(HasFunctionTraits<decltype(F_auto)>)
struct TaskTypeDeductorAuto {
    struct StripBoundArgs {
        using F = std::decay_t<decltype(F_auto)>;
        using F_Unwrapped = F;
        using Traits = FunctionTraits<F>;
        using AllArgs = typename Traits::args_tuple;

        static constexpr std::size_t kRemainingCount =
            std::tuple_size_v<AllArgs> - sizeof...(BoundArgs);

        using SlicedTuple = typename slice_first<kRemainingCount, AllArgs>::type;
        using SlicedTupleReverse = typename slice_last<kRemainingCount, AllArgs>::type;

        template <typename Ret, typename Tuple, typename TupleReverse, typename FiltBoundARgs>
        struct rebuild;
        template <typename Ret, typename... Ts, typename... TsR, typename... FiltBoundArgs>
        struct rebuild<Ret, std::tuple<Ts...>, std::tuple<TsR...>, std::tuple<FiltBoundArgs...>> {
            template <Binding bindProcess, template <typename...> typename Tester,
                      template <typename> typename Modifier>
            struct ArgumentProcessor {
                template <typename ArgFilter>
                struct ModifierFilter;

                template <typename ArgFilter>
                    requires IsCallArgB<ArgFilter>
                struct ModifierFilter<ArgFilter> {
                    using type = ArgFilter;
                };

                template <typename ArgFilter>
                    requires(!IsCallArgB<ArgFilter>)
                struct ModifierFilter<ArgFilter> {
                    using type = Modifier<ArgFilter>;
                };

                static constexpr bool kTestFront =
                    Tester<F, typename ModifierFilter<FiltBoundArgs>::type..., TsR...>::value;
                static constexpr bool kTestBack =
                    Tester<F, Ts..., typename ModifierFilter<FiltBoundArgs>::type...>::value;
                static constexpr Binding kBindDeduct =
                    (kTestBack && kTestBack)
                        ? (bindProcess ==  // NOLINT(readability-avoid-nested-conditional-operator)
                                   Binding::kAuto
                               ? Binding::kBack
                               : bindProcess)
                        : (kTestBack ? Binding::kBack : Binding::kFront);

                static constexpr bool kIsValid =
                    bindProcess == Binding::kFront
                        ? kTestFront
                        : (bindProcess ==  // NOLINT(readability-avoid-nested-conditional-operator)
                                   Binding::kBack
                               ? kTestBack
                               : (kTestFront || kTestBack));
            };

            template <typename T>
            using NoMod = T;

            template <typename T>
            using MakeMovable = std::add_rvalue_reference_t<std::remove_reference_t<T>>;

            template <Binding bindProcess, template <typename...> typename Tester,
                      template <typename> typename Modifier>
            using ArgumentProcessorV = ArgumentProcessor<bindProcess, Tester, Modifier>::kValid;

            static constexpr bool kIsNormalOrder =
                is_compatible_invocable_b<F, Ts..., FiltBoundArgs...>;
            using Processor = ArgumentProcessor<settings.binding, is_compatible_invocable, NoMod>;
            static constexpr bool kIsValid = Processor::kIsValid;
            static constexpr Binding kBinderDeduct = Processor::kBindDeduct;
            static constexpr bool kIsReverse = !kIsNormalOrder;
            using R = Ret;
            using Args = std::conditional_t<kIsReverse, std::tuple<TsR...>, std::tuple<Ts...>>;
            static constexpr bool kIsConst =
                Traits::kIsConst &&
                ArgumentProcessor<kBinderDeduct, std::is_invocable, std::add_const_t>::kIsValid;
            static constexpr bool kIsRValRef =
                Traits::kIsRValueRef &&
                ArgumentProcessor<kBinderDeduct, std::is_invocable, MakeMovable>::kIsValid;
            static constexpr bool kIsLValRef =
                Traits::kIsLValueRef && ArgumentProcessor<kBinderDeduct, std::is_invocable,
                                                          std::add_lvalue_reference_t>::kIsValid;
            using Signature = std::conditional_t<
                Traits::kIsNoExcept,
                // --- NOEXCEPT BRANCH ---
                std::conditional_t<
                    kIsReverse,
                    std::conditional_t<
                        kIsConst,
                        std::conditional_t<
                            kIsLValRef, Ret(TsR...) const & noexcept,
                            std::conditional_t<kIsRValRef, Ret(TsR...) const && noexcept,
                                               Ret(TsR...) const noexcept>>,
                        std::conditional_t<kIsLValRef, Ret(TsR...) & noexcept,
                                           std::conditional_t<kIsRValRef, Ret(TsR...) && noexcept,
                                                              Ret(TsR...) noexcept>>>,
                    std::conditional_t<
                        kIsConst,
                        std::conditional_t<
                            kIsLValRef, Ret(Ts...) const & noexcept,
                            std::conditional_t<kIsRValRef, Ret(Ts...) const && noexcept,
                                               Ret(Ts...) const noexcept>>,
                        std::conditional_t<kIsLValRef, Ret(Ts...) & noexcept,
                                           std::conditional_t<kIsRValRef, Ret(Ts...) && noexcept,
                                                              Ret(Ts...) noexcept>>>>,
                // --- NON-NOEXCEPT BRANCH ---
                std::conditional_t<
                    kIsReverse,
                    std::conditional_t<
                        kIsConst,
                        std::conditional_t<
                            kIsLValRef, Ret(TsR...) const&,
                            std::conditional_t<kIsRValRef, Ret(TsR...) const&&, Ret(TsR...) const>>,
                        std::conditional_t<
                            kIsLValRef, Ret(TsR...) &,
                            std::conditional_t<kIsRValRef, Ret(TsR...) &&, Ret(TsR...)>>>,
                    std::conditional_t<
                        kIsConst,
                        std::conditional_t<
                            kIsLValRef, Ret(Ts...) const&,
                            std::conditional_t<kIsRValRef, Ret(Ts...) const&&, Ret(Ts...) const>>,
                        std::conditional_t<
                            kIsLValRef, Ret(Ts...) &,
                            std::conditional_t<kIsRValRef, Ret(Ts...) &&, Ret(Ts...)>>>>>;
        };

        using type = rebuild<typename Traits::return_type, SlicedTuple, SlicedTupleReverse,
                             std::tuple<std::unwrap_reference_t<BoundArgs>...>>;
    };

    // To complete one needs case where F is static (no decay), but in that case it would be trivial
    // anyway, hence not urgent to change for now
    using PayloadType = PayloadImplArgs<F_auto, BoundArgs...>;
    static constexpr auto kCopyable = std::is_copy_constructible_v<PayloadType>
                                          ? Copyability::kCopyable
                                          : Copyability::kNonCopyable;

    static constexpr Alignment kAlignment =
        std::max(alignof(PayloadType), alignof(std::max_align_t));
    static constexpr auto kTriviality =
        TrivialUse<PayloadType, kCopyable>::value ? Triviality::kTrivial : Triviality::kNonTrivial;
    static constexpr auto kPayloadSize = sizeof...(BoundArgs) > 0 ? sizeof(PayloadType) : 0;
    using TypeTraits = StripBoundArgs::type;
    using Signature = TypeTraits::Signature;
    static constexpr bool kIsValid = TypeTraits::kIsValid;

    template <typename Sig = Signature>
    using Type =
        Task<Sig, kCopyable, kPayloadSize, kTriviality, false, HeapPolicy::kDisallow, kAlignment>;

    template <typename Sig = Signature>
    using NamedType = NamedTask<Sig, TypeTraits::kBinderDeduct, FunctionTag<F_auto>, BoundArgs...>;

    template <typename Sig = Signature, InlineSize size = 16>
    using HeapType = Task<Sig, kCopyable, size, kTriviality, false, HeapPolicy::kAllow, kAlignment>;
};

// Support for std::bind, std::bind_front, std::bind_back... (Note, it's implementation defined,
// though this works at least for gcc across Windows and Linux currently)

template <template <typename, typename...> typename T, typename Fd, typename... BoundArgs>
    requires(!IsTask<T<Fd, BoundArgs...>>::value && !IsNamedTask<T<Fd, BoundArgs...>>::value &&
             HasFunctionTraits<std::decay_t<Fd>> &&
             TaskTypeDeductor<DeductorSettings<void>{.binding = Binding::kAuto}, Fd,
                              BoundArgs...>::kIsValid)
struct FunctionTraits<T<Fd, BoundArgs...>>
    : FunctionTraits<typename TaskTypeDeductor<DeductorSettings<void>{.binding = Binding::kAuto},
                                               Fd, BoundArgs...>::Signature> {};

template <template <bool, typename, typename...> typename T, bool B, typename Fd,
          typename... BoundArgs>
    requires(!IsTask<T<B, Fd, BoundArgs...>>::value &&
             !IsNamedTask<T<B, Fd, BoundArgs...>>::value && HasFunctionTraits<std::decay_t<Fd>> &&
             TaskTypeDeductor<DeductorSettings<void>{.binding = Binding::kAuto}, Fd,
                              BoundArgs...>::kIsValid)
struct FunctionTraits<T<B, Fd, BoundArgs...>>
    : FunctionTraits<typename TaskTypeDeductor<DeductorSettings<void>{.binding = Binding::kAuto},
                                               Fd, BoundArgs...>::Signature> {};

}  // namespace fn

#endif  // TASK_DEDUCTORS_HPP