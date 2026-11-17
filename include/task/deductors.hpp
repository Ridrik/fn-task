#ifndef TASK_DEDUCTORS_HPP
#define TASK_DEDUCTORS_HPP

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
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

template <typename F>
struct NamedTaskRefTag {};

struct NTRefTagCommon {};

template <auto F>
struct NamedTaskStaticRefTag {};

template <auto F>
struct StaticFn {};

template <typename S, Binding binding, auto F, typename... BoundArgs>
struct IsNamedTask<NamedTask<S, binding, StaticFn<F>, BoundArgs...>> : std::true_type {
    static constexpr bool kIsRef{false};
    static constexpr bool kIsAuto{true};
};

#include <type_traits>

struct NullOpt {
    explicit constexpr NullOpt() = default;
};

inline constexpr NullOpt kNullopt{};

template <typename T>
    requires std::is_trivial_v<T> && std::is_standard_layout_v<T>
struct Optional {
    bool mHasValue = false;
    T mValue{};

    constexpr Optional() noexcept = default;

    constexpr Optional(NullOpt /*unused*/) noexcept {}

    constexpr Optional(T value) noexcept : mHasValue(true), mValue(value) {}
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
};

struct DeductorSettings {
    Binding binding{Binding::kAuto};
    Optional<Copyability> cb;
    Optional<std::size_t> size;
    Optional<HeapPolicy> hp;
    Optional<std::size_t> alg;

    [[nodiscard]] consteval auto withInlineSize(std::size_t newSize) const noexcept {
        auto copy = *this;
        copy.size = newSize;
        return copy;
    }

    [[nodiscard]] consteval auto withAlignment(std::size_t newAlignment) const noexcept {
        auto copy = *this;
        copy.alg = newAlignment;
        return copy;
    }

    [[nodiscard]] consteval auto allowHeap() const noexcept {
        auto copy = *this;
        copy.hp = HeapPolicy::kAllow;
        return copy;
    }

    [[nodiscard]] consteval auto setCopyable(bool copyable) const noexcept {
        auto copy = *this;
        copy.cb = copyable ? Copyability::kCopyable : Copyability::kNonCopyable;
        return copy;
    }

    [[nodiscard]] consteval auto bindFront() const noexcept {
        auto copy = *this;
        copy.binding = Binding::kFront;
        return copy;
    }

    [[nodiscard]] consteval auto bindBack() const noexcept {
        auto copy = *this;
        copy.binding = Binding::kBack;
        return copy;
    }
};

template <DeductorSettings settings, typename Ft, typename... BoundArgs>
struct TaskTypeDeductor;

template <DeductorSettings newSettings, typename OldDeductor>
struct UpdateDeductor;

template <DeductorSettings newSettings, DeductorSettings settings, typename Ft,
          typename... BoundArgs>
struct UpdateDeductor<newSettings, TaskTypeDeductor<settings, Ft, BoundArgs...>> {
    using type = TaskTypeDeductor<newSettings, Ft, BoundArgs...>;
};

template <DeductorSettings newSettings, typename OldDeductor>
using UpdateDeductorT = UpdateDeductor<newSettings, OldDeductor>::type;

template <DeductorSettings settings, typename Ft, typename... BoundArgs>
    requires(HasFunctionTraits<std::remove_reference_t<std::unwrap_reference_t<Ft>>>)
struct TaskTypeDeductor<settings, Ft, BoundArgs...> {
    using F = std::unwrap_reference_t<Ft>;
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
                        ? (bindProcess == Binding::kAuto ? Binding::kBack : bindProcess)
                        : (kTestBack ? Binding::kBack : Binding::kFront);

                static constexpr bool kIsValid =
                    bindProcess == Binding::kFront
                        ? kTestFront
                        : (bindProcess == Binding::kBack ? kTestBack : (kTestFront || kTestBack));
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
    using PayloadType = PayloadImpl<Ft, BoundArgs...>;

    static constexpr auto kPayloadCopyable = std::is_copy_constructible_v<PayloadType>
                                                 ? Copyability::kCopyable
                                                 : Copyability::kNonCopyable;
    static constexpr auto kCopyValid =
        !settings.cb.has_value() ||
        (*settings.cb != Copyability::kCopyable && kPayloadCopyable == Copyability::kNonCopyable);
    static constexpr auto kCopyable = settings.cb.has_value() ? *settings.cb : kPayloadCopyable;
    static constexpr auto kTriviality =
        TrivialUse<PayloadType, kCopyable>::value ? Triviality::kTrivial : Triviality::kNonTrivial;
    static constexpr Alignment kAlignment =
        std::max(alignof(PayloadType), alignof(std::max_align_t));
    static constexpr auto kPayloadSize = sizeof(PayloadType);
    static constexpr auto kSizeValid = !settings.size.has_value() ||
                                       *settings.size >= kPayloadSize || !settings.hp.has_value() ||
                                       *settings.hp == HeapPolicy::kAllow;
    static constexpr auto kSize = settings.size.has_value() ? *settings.size : kPayloadSize;
    static constexpr auto kHeapPolicy =
        settings.hp.has_value() ? *settings.hp : HeapPolicy::kDisallow;
    using TypeTraits = StripBoundArgs::type;
    using Signature = TypeTraits::Signature;
    static constexpr bool kIsValid = TypeTraits::kIsValid && kCopyValid && kSizeValid;

    template <typename Sig = Signature>
    using Type = Task<Sig, kCopyable, kSize, kTriviality, false, kHeapPolicy, kAlignment>;

    template <typename Sig = Signature>
    using NamedType = NamedTask<Sig, TypeTraits::kBinderDeduct, Ft, BoundArgs...>;

    template <typename Sig = Signature, InlineSize size = 16>
    using HeapType = Task<Sig, kCopyable, size, kTriviality, false, HeapPolicy::kAllow, kAlignment>;

    template <std::size_t N>
    consteval auto withInlineSize() const noexcept {
        constexpr auto kNewDeductor =
            UpdateDeductorT<settings.withInlineSize(N), decltype(*this)>{};
        static_assert(decltype(kNewDeductor)::kIsValid,
                      "Requested Signature is not compatible with initial/deducted signature");
        return kNewDeductor;
    }

    template <std::size_t N>
        requires(N != 0 && std::has_single_bit(N))
    consteval auto withAlignment() const noexcept {
        constexpr auto kNewDeductor = UpdateDeductorT<settings.withAlignment(N), decltype(*this)>{};
        static_assert(decltype(kNewDeductor)::kIsValid,
                      "Requested Signature is not compatible with initial/deducted alignment");
        return kNewDeductor;
    }

    consteval auto allowHeap() const noexcept {
        return UpdateDeductorT<settings.allowHeap(), decltype(*this)>{};
    }

    consteval auto toggleCopyability(bool copyable) const noexcept {
        if constexpr (settings.cb.has_value() &&
                      ((*settings.cb == Copyability::kCopyable && copyable) ||
                       (*settings.cb == Copyability::kNonCopyable && !copyable))) {
            return *this;
        }
        constexpr auto kNewDeductor =
            UpdateDeductorT<settings.setCopyable(copyable), decltype(*this)>{};
        static_assert(decltype(kNewDeductor)::kIsValid,
                      "Task is invalid under new copyability setting");
        return kNewDeductor;
    }
};

template <typename Deductor>
struct TaskBuilder;

template <DeductorSettings settings, typename Ft, typename... BoundArgs>
    requires(HasFunctionTraits<std::remove_reference_t<std::unwrap_reference_t<Ft>>>)
struct TaskBuilder<TaskTypeDeductor<settings, Ft, BoundArgs...>> {
    TaskBuilder(Ft&& f, BoundArgs&&... args)
        : transitPayload{.f = std::forward<Ft>(f), .args = {std::forward<BoundArgs>(args)...}} {}

    PayloadImpl<Ft&&, BoundArgs&&...> transitPayload;
    using Deductor = TaskTypeDeductor<settings, std::decay_t<Ft>, std::decay_t<BoundArgs>...>;

    template <std::size_t N>
    consteval auto withInlineSize() const noexcept {
        using NewDeductor = UpdateDeductorT<settings.withInlineSize(N), Deductor>;
        static_assert(NewDeductor::kIsValid,
                      "Requested Signature is not compatible with initial/deducted signature");
        return TaskBuilder<NewDeductor>{std::move(transitPayload)};
    }

    template <std::size_t N>
        requires(N != 0 && std::has_single_bit(N))
    consteval auto withAlignment() const noexcept {
        using NewDeductor = UpdateDeductorT<settings.withAlignment(N), Deductor>;
        static_assert(NewDeductor::kIsValid,
                      "Requested Signature is not compatible with initial/deducted alignment");
        return TaskBuilder<NewDeductor>{std::move(transitPayload)};
    }

    consteval auto allowHeap() const noexcept {
        return TaskBuilder<UpdateDeductorT<settings.allowHeap(), Deductor>>{
            std::move(transitPayload)};
    }

    consteval auto toggleCopyability(bool copyable) const noexcept
        requires((!settings.cb.has_value() ||
                  ((*settings.cb != Copyability::kCopyable || !copyable) &&
                   (*settings.cb != Copyability::kNonCopyable || copyable))))
    {
        using NewDeductor = UpdateDeductorT<settings.setCopyable(copyable), decltype(*this)>;
        static_assert(NewDeductor::kIsValid, "Task is invalid under new copyability setting");
        return TaskBuilder<NewDeductor>{std::move(transitPayload)};
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
                        ? (bindProcess == Binding::kAuto ? Binding::kBack : bindProcess)
                        : (kTestBack ? Binding::kBack : Binding::kFront);

                static constexpr bool kIsValid =
                    bindProcess == Binding::kFront
                        ? kTestFront
                        : (bindProcess == Binding::kBack ? kTestBack : (kTestFront || kTestBack));
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
    using NamedType = NamedTask<Sig, TypeTraits::kBinderDeduct, StaticFn<F_auto>, BoundArgs...>;

    template <typename Sig = Signature, InlineSize size = 16>
    using HeapType = Task<Sig, kCopyable, size, kTriviality, false, HeapPolicy::kAllow, kAlignment>;
};

// Support for std::bind, std::bind_front, std::bind_back... (Note, it's implementation defined,
// though this works at least for gcc across Windows and Linux currently)

template <template <typename, typename...> typename T, typename Fd, typename... BoundArgs>
    requires(!IsTask<T<Fd, BoundArgs...>>::value && !IsNamedTask<T<Fd, BoundArgs...>>::value &&
             HasFunctionTraits<std::decay_t<Fd>> &&
             TaskTypeDeductor<{.binding = Binding::kAuto}, Fd, BoundArgs...>::kIsValid)
struct FunctionTraits<T<Fd, BoundArgs...>>
    : FunctionTraits<
          typename TaskTypeDeductor<{.binding = Binding::kAuto}, Fd, BoundArgs...>::Signature> {};

template <template <bool, typename, typename...> typename T, bool B, typename Fd,
          typename... BoundArgs>
    requires(!IsTask<T<B, Fd, BoundArgs...>>::value &&
             !IsNamedTask<T<B, Fd, BoundArgs...>>::value && HasFunctionTraits<std::decay_t<Fd>> &&
             TaskTypeDeductor<{.binding = Binding::kAuto}, Fd, BoundArgs...>::kIsValid)
struct FunctionTraits<T<B, Fd, BoundArgs...>>
    : FunctionTraits<
          typename TaskTypeDeductor<{.binding = Binding::kAuto}, Fd, BoundArgs...>::Signature> {};

}  // namespace fn

#endif  // TASK_DEDUCTORS_HPP