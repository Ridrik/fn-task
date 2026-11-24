#ifndef TASK_PAYLOAD_HPP
#define TASK_PAYLOAD_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#if defined(__GNUC__)
#define FORCE_INLINE [[gnu::always_inline]] inline
#elif defined(__clang__)
#define FORCE_INLINE [[clang::always_inline]] inline
#elif defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline
#endif

namespace fn {

template <typename T>
    requires std::is_reference_v<T>
struct CallArgTag {
    T val;

    using type = T;

    constexpr operator T() const noexcept {
        return std::forward<T>(val);
    }
};

template <typename T, bool Invocable = false>
    requires(std::is_lvalue_reference_v<T>)
struct BoundRefWrapTag {
    std::reference_wrapper<std::remove_reference_t<T>> val;

    constexpr operator T() const noexcept {
        return val.get();
    }

    constexpr operator std::remove_reference_t<T>&&() const noexcept {
        return std::move(val.get());
    }

    constexpr std::remove_reference_t<T>&& move() const&& noexcept {
        return std::move(val.get());
    }

    constexpr T get() const noexcept {
        return val.get();
    }
};

enum class RefQualifier : std::uint8_t { kNone, kNonConst, kConst };

constexpr auto ref(auto&& val) noexcept
    requires(std::is_lvalue_reference_v<decltype(val)>)
{
    return BoundRefWrapTag<decltype(val)>{std::ref(val)};
}

constexpr auto cref(auto&& val) noexcept
    requires(std::is_lvalue_reference_v<decltype(val)>)
{
    return BoundRefWrapTag<const std::remove_cvref_t<decltype(val)>&>{std::cref(val)};
}

template <typename T>
struct UnwrapBoundRefWrap {
    using type = T;
};

template <typename T, bool Invocable>
struct UnwrapBoundRefWrap<BoundRefWrapTag<T, Invocable>> {
    static_assert(std::is_lvalue_reference_v<T>);
    using type = T;
};

template <typename T>
using UnwrapBoundRefWrapT = UnwrapBoundRefWrap<T>::type;

template <typename>
struct IsBoundRefWrapper : std::false_type {};

template <typename T, bool Invocable>
struct IsBoundRefWrapper<BoundRefWrapTag<T, Invocable>> : std::true_type {};

template <typename T>
static constexpr auto kIsBoundRefWrapper = IsBoundRefWrapper<T>::value;

template <typename T>
struct IsNotBoundRefWrapper {
    static constexpr auto value = !kIsBoundRefWrapper<T>;  // NOLINT
};

template <typename T>
struct NotBoundLValueReference {
    static constexpr auto value = !std::is_lvalue_reference_v<T>;  // NOLINT
};

template <typename T>
struct IsCallArg : std::false_type {};

template <typename T>
struct IsCallArg<CallArgTag<T>> : std::true_type {};

template <typename T>
struct IsCallArg<CallArgTag<T>&> : std::true_type {};

template <typename T>
struct IsCallArg<const CallArgTag<T>&> : std::true_type {};

template <typename T>
struct IsCallArg<CallArgTag<T>&&> : std::true_type {};

template <typename T>
struct IsCallArg<const CallArgTag<T>&&> : std::true_type {};

template <typename T>
struct UndoIfCallArg {
    using type = T;
};

template <typename T>
    requires IsCallArg<std::remove_cvref_t<T>>::value
struct UndoIfCallArg<T> {
    using type = typename std::remove_cvref_t<T>::type;
};

template <typename T>
using UndoIfCallArgT = UndoIfCallArg<T>::type;

template <typename T>
static constexpr auto IsCallArgB = IsCallArg<T>::value;  // NOLINT

namespace detail {

template <typename... Ts>
struct TypeList {};

template <typename F, typename TupleOfTested, typename TupleOfRemaining,
          template <typename> typename NoExpandPolicy = IsCallArg>
struct invocable_combinator;

// Base case: No arguments remaining, test std::is_invocable with the accumulated tested types
template <typename F, typename... Tested, template <typename> typename Policy>
struct invocable_combinator<F, TypeList<Tested...>, TypeList<>, Policy> {
    static constexpr bool kValid = std::is_invocable_v<F, Tested...>;
    static constexpr bool kNoThrow = std::is_nothrow_invocable_v<F, Tested...>;
    using Selected = TypeList<Tested...>;
};

// Recursive step: Take the next argument T, branch out into its common value/cv-ref variations
template <typename F, typename... Tested, typename T, typename... Rest,
          template <typename> typename Policy>
    requires(!Policy<T>::value)
struct invocable_combinator<F, TypeList<Tested...>, TypeList<T, Rest...>, Policy> {
   private:
    using LRef = invocable_combinator<F, TypeList<Tested..., T&>, TypeList<Rest...>, Policy>;

    template <bool, bool>
    struct Select {
        using RRef = invocable_combinator<F, TypeList<Tested..., std::remove_reference_t<T>&&>,
                                          TypeList<Rest...>, Policy>;

        static constexpr bool kValid = RRef::kValid;
        static constexpr bool kNoThrow = RRef::kNoThrow;
        using Selected = typename RRef::Selected;
    };

    template <bool NoThrow>
    struct Select<true, NoThrow> {
        static constexpr bool kValid = true;
        static constexpr bool kNoThrow = NoThrow;
        using Selected = typename LRef::Selected;
    };

    using Result = Select<LRef::kValid, LRef::kNoThrow>;

   public:
    static constexpr bool kValid = Result::kValid;
    static constexpr bool kNoThrow = Result::kNoThrow;
    using Selected = typename Result::Selected;
};

template <typename F, typename... Tested, typename T, typename... Rest,
          template <typename> typename Policy>
    requires(Policy<T>::value)
struct invocable_combinator<F, TypeList<Tested...>, TypeList<T, Rest...>, Policy>
    : invocable_combinator<F, TypeList<Tested..., T /*typename std::remove_cvref_t<T>::type*/>,
                           TypeList<Rest...>, Policy> {};

template <typename F, typename Tuple, typename... Selected, std::size_t... I>
constexpr decltype(auto) invoke_selected(
    F&& f, Tuple& args, TypeList<Selected...> /*unused*/,
    std::index_sequence<I...> /*unused*/) noexcept(std::is_nothrow_invocable_v<F&&, Selected...>) {
    return std::invoke(std::forward<F>(f), std::forward_like<Selected>(std::get<I>(args))...);
}

template <auto F, typename Tuple, typename... Selected, std::size_t... I>
constexpr decltype(auto) invoke_selected(
    Tuple& args, TypeList<Selected...> /*unused*/,
    std::index_sequence<I...> /*unused*/) noexcept(std::is_nothrow_invocable_v<decltype(F),
                                                                               Selected...>) {
    return std::invoke(F, std::forward_like<Selected>(std::get<I>(args))...);
}

template <typename... T>
struct AlwaysFalse : std::false_type {};

template <typename T>
struct is_valid : std::bool_constant<T::kValid> {};  // NOLINT

template <typename T>
struct is_no_throw : std::bool_constant<is_valid<T>::value && T::kNoThrow> {};  // NOLINT

}  // namespace detail

template <template <typename> typename NoExpandPolicy, typename F, typename... Args>
struct is_any_compatible_invocable : std::false_type {};  // NOLINT

template <typename F, typename... Args>
    requires(std::disjunction_v<
             detail::is_valid<detail::invocable_combinator<F, detail::TypeList<>,
                                                           detail::TypeList<Args...>, IsCallArg>>,
             detail::is_valid<detail::invocable_combinator<F&, detail::TypeList<>,
                                                           detail::TypeList<Args...>, IsCallArg>>,
             detail::is_valid<
                 detail::invocable_combinator<std::remove_reference_t<F> &&, detail::TypeList<>,
                                              detail::TypeList<Args...>, IsCallArg>>>)
struct is_any_compatible_invocable<IsCallArg, F, Args...> : std::true_type {};

template <typename F, typename... Args>
    requires(
        (!std::is_lvalue_reference_v<F>) &&
        std::disjunction_v<
            detail::is_valid<detail::invocable_combinator<
                F, detail::TypeList<>, detail::TypeList<Args...>, NotBoundLValueReference>>,
            detail::is_valid<detail::invocable_combinator<
                const F, detail::TypeList<>, detail::TypeList<Args...>, NotBoundLValueReference>>>)
struct is_any_compatible_invocable<NotBoundLValueReference, F, Args...> : std::true_type {
};  // NOLINT

template <typename F, typename... Args>
    requires(
        std::is_lvalue_reference_v<F> &&
        std::disjunction_v<
            detail::is_valid<detail::invocable_combinator<
                F, detail::TypeList<>, detail::TypeList<Args...>, NotBoundLValueReference>>,
            detail::is_valid<
                detail::invocable_combinator<std::remove_reference_t<F> &&, detail::TypeList<>,
                                             detail::TypeList<Args...>, NotBoundLValueReference>>,
            detail::is_valid<detail::invocable_combinator<
                std::add_const_t<std::remove_reference_t<F>> &&, detail::TypeList<>,
                detail::TypeList<Args...>, NotBoundLValueReference>>>)
struct is_any_compatible_invocable<NotBoundLValueReference, F, Args...> : std::true_type {
};  // NOLINT

template <typename F, typename... Args>
struct is_compatible_invocable : std::false_type {};

template <typename F, typename... Args>
    requires((!std::is_lvalue_reference_v<F>) &&
             detail::invocable_combinator<F, detail::TypeList<>, detail::TypeList<Args...>,
                                          NotBoundLValueReference>::kValid)
struct is_compatible_invocable<F, Args...> : std::true_type {};  // NOLINT

template <typename F, typename... Args>
    requires(
        std::is_lvalue_reference_v<F> &&
        std::disjunction_v<
            detail::is_valid<detail::invocable_combinator<
                F, detail::TypeList<>, detail::TypeList<Args...>, NotBoundLValueReference>>,
            detail::is_valid<
                detail::invocable_combinator<std::remove_reference_t<F> &&, detail::TypeList<>,
                                             detail::TypeList<Args...>, NotBoundLValueReference>>>)
struct is_compatible_invocable<F, Args...> : std::true_type {};  // NOLINT

template <typename F, typename... Args>
struct is_any_compatible_nothrow_invocable : std::false_type {};  // NOLINT

template <typename F, typename... Args>
    requires(std::disjunction_v<
             detail::is_no_throw<
                 detail::invocable_combinator<F, detail::TypeList<>, detail::TypeList<Args...>>>,
             detail::is_no_throw<
                 detail::invocable_combinator<F&, detail::TypeList<>, detail::TypeList<Args...>>>,
             detail::is_no_throw<detail::invocable_combinator<
                 std::remove_reference_t<F> &&, detail::TypeList<>, detail::TypeList<Args...>>>>)
struct is_any_compatible_nothrow_invocable<F, Args...> : std::true_type {};

// Main trait: checks if there is any valid combination of cv/ref qualifiers
template <template <typename> typename NoExpandPolicy, typename F, typename... Args>
constexpr bool is_any_compatible_invocable_b =  // NOLINT
    is_any_compatible_invocable<NoExpandPolicy, F, Args...>::value;

template <typename F, typename... Args>
constexpr bool is_any_compatible_nothrow_invocable_b =  // NOLINT
    is_any_compatible_nothrow_invocable<F, Args...>::value;

template <template <typename> typename NoExpandPolicy = IsCallArg, typename F, typename... Args>
    requires detail::invocable_combinator<F, detail::TypeList<>, detail::TypeList<Args...>,
                                          NoExpandPolicy>::kValid
constexpr decltype(auto) invoke_any_compatible(F&& f, Args&&... args) noexcept(
    detail::invocable_combinator<F, detail::TypeList<>, detail::TypeList<Args...>,
                                 NoExpandPolicy>::kNoThrow) {
    using Selection = detail::invocable_combinator<F, detail::TypeList<>, detail::TypeList<Args...>,
                                                   NoExpandPolicy>;
    auto references = std::forward_as_tuple(std::forward<Args>(args)...);
    return detail::invoke_selected(std::forward<F>(f), references, typename Selection::Selected{},
                                   std::index_sequence_for<Args...>{});
}

template <template <typename> typename NoExpandPolicy = IsCallArg, typename F, typename... Args>
    requires(!detail::invocable_combinator<F, detail::TypeList<>, detail::TypeList<Args...>,
                                           NoExpandPolicy>::kValid) &&
            detail::invocable_combinator<F&, detail::TypeList<>, detail::TypeList<Args...>,
                                         NoExpandPolicy>::kValid
constexpr decltype(auto) invoke_any_compatible(
    F&& f, Args&&... args) noexcept(  // NOLINT(cppcoreguidelines-missing-std-forward)
    detail::invocable_combinator<F&, detail::TypeList<>, detail::TypeList<Args...>,
                                 NoExpandPolicy>::kNoThrow) {
    using Selection = detail::invocable_combinator<F&, detail::TypeList<>,
                                                   detail::TypeList<Args...>, NoExpandPolicy>;
    auto references = std::forward_as_tuple(std::forward<Args>(args)...);
    return detail::invoke_selected(f, references, typename Selection::Selected{},
                                   std::index_sequence_for<Args...>{});
}

template <template <typename> typename NoExpandPolicy = IsCallArg, typename F, typename... Args>
    requires(!detail::invocable_combinator<F, detail::TypeList<>, detail::TypeList<Args...>,
                                           NoExpandPolicy>::kValid) &&
            (!detail::invocable_combinator<F&, detail::TypeList<>, detail::TypeList<Args...>,
                                           NoExpandPolicy>::kValid) &&
            detail::invocable_combinator<std::remove_reference_t<F>&&, detail::TypeList<>,
                                         detail::TypeList<Args...>, NoExpandPolicy>::kValid
constexpr decltype(auto) invoke_any_compatible(
    F&& f, Args&&... args) noexcept(  // NOLINT(cppcoreguidelines-missing-std-forward)
    detail::invocable_combinator<std::remove_reference_t<F>&&, detail::TypeList<>,
                                 detail::TypeList<Args...>, NoExpandPolicy>::kNoThrow) {
    using Selection = detail::invocable_combinator<std::remove_reference_t<F>&&, detail::TypeList<>,
                                                   detail::TypeList<Args...>, NoExpandPolicy>;
    auto references = std::forward_as_tuple(std::forward<Args>(args)...);
    return detail::invoke_selected(std::move(f), references,  // NOLINT
                                   typename Selection::Selected{},
                                   std::index_sequence_for<Args...>{});
}

template <auto F, template <typename> typename NoExpandPolicy = IsCallArg, typename... Args>
constexpr decltype(auto) invoke_any_compatible(Args&&... args) {
    using Selection = detail::invocable_combinator<decltype(F), detail::TypeList<>,
                                                   detail::TypeList<Args...>, NoExpandPolicy>;
    static_assert(Selection::kValid,
                  "Callable is not invocable with any compatible cv/ref combination");
    auto references = std::forward_as_tuple(std::forward<Args>(args)...);
    return detail::invoke_selected<F>(references, typename Selection::Selected{},
                                      std::index_sequence_for<Args...>{});
}

template <typename... Ts>
struct PayloadTuple;

template <typename>
struct IsPayloadTuple : std::false_type {};

template <typename... Ts>
struct IsPayloadTuple<PayloadTuple<Ts...>> : std::true_type {};

template <typename Like, typename T>
constexpr decltype(auto) forward_like_custom(T&& value) noexcept {  // NOLINT
    using Qualified = std::conditional_t<
        std::is_reference_v<T>, T,
        std::conditional_t<std::is_const_v<std::remove_reference_t<Like>>, const T, T>>;

    return static_cast<
        std::conditional_t<std::is_lvalue_reference_v<Like>, Qualified&, Qualified&&>>(value);
}

template <typename Like, typename T>
constexpr decltype(auto) forward_like_custom(const T&& value) noexcept {  // NOLINT
    using Qualified = T;
    return static_cast<
        std::conditional_t<std::is_lvalue_reference_v<Like>, Qualified&, Qualified&&>>(value);
}

template <typename Like, typename T>
    requires(!std::is_reference_v<T>)
constexpr decltype(auto) forward_like_custom(T& value) noexcept {  // NOLINT
    using Qualified =
        std::conditional_t<std::is_const_v<std::remove_reference_t<Like>>, const T, T>;

    return static_cast<
        std::conditional_t<std::is_lvalue_reference_v<Like>, Qualified&, Qualified&&>>(value);
}

template <typename Like, typename T>
    requires(!std::is_reference_v<T>)
constexpr decltype(auto) forward_like_custom(const T& value) noexcept {  // NOLINT
    using Qualified = const T;

    return static_cast<
        std::conditional_t<std::is_lvalue_reference_v<Like>, Qualified&, Qualified&&>>(value);
}

template <typename>
struct IsRefWrapper : std::false_type {};

template <typename T>
struct IsRefWrapper<std::reference_wrapper<T>> : std::true_type {};

template <typename T>
static constexpr auto kIsRefWrapper = IsRefWrapper<T>::value;

template <typename>
struct IsRefPayloadTuple : std::false_type {};

template <typename... Ts>
    requires(((std::is_reference_v<Ts> || kIsRefWrapper<Ts> || kIsBoundRefWrapper<Ts>) && ...))
struct IsRefPayloadTuple<PayloadTuple<Ts...>> : std::true_type {};

template <typename T>
static constexpr auto kIsRefPayloadTuple = IsRefPayloadTuple<T>::value;

template <typename T>
struct unwrap_reference_qualifiers {  // NOLINT
    using type = T;
};

template <typename T>
struct unwrap_reference_qualifiers<std::reference_wrapper<T>> {
    using type = T;
};

template <typename T>
struct unwrap_reference_qualifiers<std::reference_wrapper<T>&&> {
    using type = T&&;
};

template <typename T>
struct unwrap_reference_qualifiers<std::reference_wrapper<T> const&&> {
    using type = T&&;
};

template <typename T>
struct unwrap_reference_qualifiers<const std::reference_wrapper<T>> {
    using type = T;
};

template <typename T>
struct unwrap_reference_qualifiers<std::reference_wrapper<T>&> {
    using type = T&;
};

template <typename T>
struct unwrap_reference_qualifiers<const std::reference_wrapper<T>&> {
    using type = T&;
};

template <typename T>
using unwrap_reference_qualifiers_t = typename unwrap_reference_qualifiers<T>::type;

template <bool Ignore = false>
constexpr decltype(auto) unwrap_arg_on_invocation(auto&& arg) noexcept
    requires((!IsBoundRefWrapper<std::remove_cvref_t<decltype(arg)>>::value &&
              !IsCallArg<std::remove_cvref_t<decltype(arg)>>::value) ||
             Ignore)
{
    return std::forward<decltype(arg)>(arg);
}

template <bool Ignore = false>
constexpr decltype(auto) unwrap_arg_on_invocation(auto&& arg) noexcept
    requires(IsCallArg<std::decay_t<decltype(arg)>>::value && !Ignore)
{
    static_assert(std::is_rvalue_reference_v<decltype(arg)>);
    return arg.operator typename std::decay_t<decltype(arg)>::type();
}

template <bool Ignore = false>
    requires(!Ignore)
constexpr decltype(auto) unwrap_arg_on_invocation(auto&& arg) noexcept
    requires(IsBoundRefWrapper<std::remove_cvref_t<decltype(arg)>>::value)
{  // NOLINT
    if constexpr (std::is_lvalue_reference_v<decltype(arg)>) {
        return arg.get();
    } else {
        return std::move(arg).move();  // NOLINT
    }
}

template <>
struct PayloadTuple<> {
    constexpr PayloadTuple() = default;

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) apply([[maybe_unused]] this auto&& /*self*/,
                                                ArgsSoFar&&... args) {
        static_assert(std::is_invocable_v<decltype(F), decltype(args)...>,
                      "F is not callable with given arguments");
        return F(unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...);
    }

    template <auto F, template <typename> typename NoExpandPolicy = IsCallArg,
              typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAnyCompatible([[maybe_unused]] this auto&& /*self*/,
                                                             ArgsSoFar&&... args) {
        return fn::invoke_any_compatible<F, NoExpandPolicy>(
            unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...);
    }

    template <auto F, bool Ignore = false, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgs(ArgsSoFar&&... args) && {
        static_assert(std::is_invocable_v<decltype(F), decltype(args)...>,
                      "F is not callable with given arguments");
        return F(unwrap_arg_on_invocation<Ignore>(std::forward<ArgsSoFar>(args))...);
    }

    template <auto F, template <typename> typename NoExpandPolicy = IsCallArg,
              typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgsAnyCompatible(ArgsSoFar&&... args) && {
        return fn::invoke_any_compatible<F, NoExpandPolicy>(
            unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...);
    }

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFront([[maybe_unused]] this auto&& /*self*/,
                                                     auto&& callArgs, ArgsSoFar&&... args)
        requires(IsPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        return std::forward<decltype(callArgs)>(callArgs).template applyAsArgs<F>(
            std::forward<ArgsSoFar>(args)...);
    }

    template <auto F, template <typename> typename NoExpandPolicy = IsCallArg,
              typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFrontAnyCompatible(
        [[maybe_unused]] this auto&& /*self*/, auto&& callArgs, ArgsSoFar&&... args)
        requires(IsPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        return std::forward<decltype(callArgs)>(callArgs)
            .template applyAsArgsAnyCompatible<F, NoExpandPolicy>(std::forward<ArgsSoFar>(args)...);
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) apply([[maybe_unused]] this auto&& /*self*/, F&& f,
                                                ArgsSoFar&&... args) {
        static_assert(std::is_invocable_v<decltype(unwrap_arg_on_invocation(std::forward<F>(f))),
                                          decltype(unwrap_arg_on_invocation(
                                              std::forward<ArgsSoFar>(args)))...>,
                      "F is not callable with given arguments");
        return std::invoke(unwrap_arg_on_invocation(std::forward<F>(f)),
                           unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...);
    }

    template <std::size_t curIdx, typename F, typename... ArgsSoFar, typename NextCallArg,
              typename... ArgsRemaining>
    FORCE_INLINE constexpr decltype(auto) apply(
        this auto&& self, F&& f, PayloadTuple<ArgsSoFar...>&& args,
        PayloadTuple<NextCallArg, ArgsRemaining...>&& argsRemaining) {
        using Self = decltype(self);
        static_assert(curIdx == sizeof...(ArgsSoFar));
        return std::forward_like<Self>(self).template apply<curIdx + 1>(
            std::forward<F>(f),
            PayloadTuple<ArgsSoFar..., NextCallArg>{std::move(args),
                                                    std::forward<NextCallArg>(argsRemaining.head)},
            std::move(argsRemaining).tail);
    }

    template <std::size_t curIdx, typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) apply(this auto&& self, F&& f,
                                                PayloadTuple<ArgsSoFar...>&& args,
                                                PayloadTuple<>&& /*unused*/) {
        static_assert(curIdx == sizeof...(ArgsSoFar));
        return std::move(args).applyAsArgs(std::forward<F>(f));
    }

    template <template <typename> typename NoExpandPolicy = IsCallArg, std::size_t curIdx,
              typename F, typename... ArgsSoFar, typename NextCallArg, typename... ArgsRemaining>
    FORCE_INLINE constexpr decltype(auto) applyAnyCompatible(
        this auto&& self, F&& f, PayloadTuple<ArgsSoFar...>&& args,
        PayloadTuple<NextCallArg, ArgsRemaining...>&& argsRemaining) {
        using Self = decltype(self);
        static_assert(curIdx == sizeof...(ArgsSoFar));
        return std::forward_like<Self>(self)
            .template applyAnyCompatible<NoExpandPolicy, curIdx + 1>(
                std::forward<F>(f),
                PayloadTuple<ArgsSoFar..., NextCallArg>{
                    std::move(args), std::forward<NextCallArg>(argsRemaining.head)},
                std::move(argsRemaining).tail);
    }

    template <template <typename> typename NoExpandPolicy = IsCallArg, std::size_t curIdx,
              typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAnyCompatible(this auto&& self, F&& f,
                                                             PayloadTuple<ArgsSoFar...>&& args,
                                                             PayloadTuple<>&& /*unused*/) {
        static_assert(curIdx == sizeof...(ArgsSoFar));
        return std::move(args).template applyAsArgsAnyCompatible<NoExpandPolicy>(
            std::forward<F>(f));
    }

    template <auto F, template <typename> typename NoExpandPolicy = IsCallArg, std::size_t curIdx,
              typename... ArgsSoFar, typename NextCallArg, typename... ArgsRemaining>
        requires(!std::is_same_v<decltype(F), std::size_t>)
    FORCE_INLINE constexpr decltype(auto) apply(
        this auto&& self, PayloadTuple<ArgsSoFar...>&& args,
        PayloadTuple<NextCallArg, ArgsRemaining...>&& argsRemaining) {
        using Self = decltype(self);
        static_assert(curIdx == sizeof...(ArgsSoFar));
        return std::forward_like<Self>(self).template apply<F, NoExpandPolicy, curIdx + 1>(
            PayloadTuple<ArgsSoFar..., NextCallArg>{std::move(args),
                                                    std::forward<NextCallArg>(argsRemaining.head)},
            std::move(argsRemaining).tail);
    }

    template <auto F, template <typename> typename NoExpandPolicy = IsCallArg, std::size_t curIdx,
              typename... ArgsSoFar>
        requires(!std::is_same_v<decltype(F), std::size_t>)
    FORCE_INLINE constexpr decltype(auto) apply(this auto&& self, PayloadTuple<ArgsSoFar...>&& args,
                                                PayloadTuple<>&& /*unused*/) {
        static_assert(curIdx == sizeof...(ArgsSoFar));
        return std::move(args).template applyAsArgs<F, NoExpandPolicy>();
    }

    template <auto F, template <typename> typename NoExpandPolicy = IsCallArg, std::size_t curIdx,
              typename... ArgsSoFar, typename NextCallArg, typename... ArgsRemaining>
        requires(!std::is_same_v<decltype(F), std::size_t>)
    FORCE_INLINE constexpr decltype(auto) applyAnyCompatible(
        this auto&& self, PayloadTuple<ArgsSoFar...>&& args,
        PayloadTuple<NextCallArg, ArgsRemaining...>&& argsRemaining) {
        using Self = decltype(self);
        static_assert(curIdx == sizeof...(ArgsSoFar));
        return std::forward_like<Self>(self)
            .template applyAnyCompatible<F, NoExpandPolicy, curIdx + 1>(
                PayloadTuple<ArgsSoFar..., NextCallArg>{
                    std::move(args), std::forward<NextCallArg>(argsRemaining.head)},
                std::move(argsRemaining).tail);
    }

    template <auto F, template <typename> typename NoExpandPolicy = IsCallArg, std::size_t curIdx,
              typename... ArgsSoFar>
        requires(!std::is_same_v<decltype(F), std::size_t>)
    FORCE_INLINE constexpr decltype(auto) applyAnyCompatible(this auto&& self,
                                                             PayloadTuple<ArgsSoFar...>&& args,
                                                             PayloadTuple<>&& /*unused*/) {
        static_assert(curIdx == sizeof...(ArgsSoFar));
        return std::move(args).template applyAsArgsAnyCompatible<F, NoExpandPolicy>();
    }

    template <template <typename> typename NoExpandPolicy = IsCallArg, typename F,
              typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAnyCompatible([[maybe_unused]] this auto&& /*self*/,
                                                             F&& f, ArgsSoFar&&... args) {
        return fn::invoke_any_compatible<NoExpandPolicy>(
            unwrap_arg_on_invocation(std::forward<F>(f)),
            unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...);
    }

    template <bool Ignore = false, typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgs(F&& f, ArgsSoFar&&... args) && {
        static_assert(std::is_invocable_v<
                          decltype(unwrap_arg_on_invocation(std::declval<decltype(f)>())),
                          decltype(unwrap_arg_on_invocation(std::declval<decltype(args)>()))...>,
                      "F is not callable with given arguments");
        return std::invoke(unwrap_arg_on_invocation(std::forward<F>(f)),
                           unwrap_arg_on_invocation<Ignore>(std::forward<ArgsSoFar>(args))...);
    }

    template <template <typename> typename NoExpandPolicy = IsCallArg, typename F,
              typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgsAnyCompatible(F&& f, ArgsSoFar&&... args) && {
        return fn::invoke_any_compatible<NoExpandPolicy>(
            unwrap_arg_on_invocation(std::forward<F>(f)),
            unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...);
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFront([[maybe_unused]] this auto&& /*self*/, F&& f,
                                                     auto&& callArgs, ArgsSoFar&&... args)
        requires(IsPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        return std::forward<decltype(callArgs)>(callArgs).template applyAsArgs<F>(
            std::forward<F>(f), std::forward<ArgsSoFar>(args)...);
    }

    template <template <typename> typename NoExpandPolicy = IsCallArg, typename F,
              typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFrontAnyCompatible(
        [[maybe_unused]] this auto&& /*self*/, F&& f, auto&& callArgs, ArgsSoFar&&... args)
        requires(IsPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        return std::forward<decltype(callArgs)>(callArgs)
            .template applyAsArgsAnyCompatible<F, NoExpandPolicy>(std::forward<F>(f),
                                                                  std::forward<ArgsSoFar>(args)...);
    }
};

template <typename T, typename... Ts>
struct PayloadTuple<T, Ts...> {
    T head;
    [[no_unique_address]] PayloadTuple<Ts...> tail{};

    template <typename... T1>
        requires(sizeof...(T1) > 0)
    PayloadTuple(PayloadTuple<T1...>&& p1, auto&& newTail)
        : head{std::forward<decltype(p1.head)>(p1.head)},
          tail{std::move(p1).tail, std::forward<decltype(newTail)>(newTail)} {}

    PayloadTuple(PayloadTuple<>&& /*unused*/, auto&& newTail)
        : head{std::forward<decltype(newTail)>(newTail)} {
        static_assert(sizeof...(Ts) == 0);
    }

    PayloadTuple(auto&& first, auto&&... args)
        : head{std::forward<decltype(first)>(first)}, tail{std::forward<decltype(args)>(args)...} {}

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) apply(this auto&& self, ArgsSoFar&&... args) {
        using Self = decltype(self);
        if constexpr (sizeof...(Ts) == 0) {
            static_assert(std::is_invocable_v<decltype(F), decltype(args)...,
                                              decltype(unwrap_arg_on_invocation(
                                                  forward_like_custom<Self, T>(self.head)))>,
                          "F is not callable with given arguments");
            return F(unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...,
                     unwrap_arg_on_invocation(forward_like_custom<Self, T>(self.head)));
        } else {
            return std::forward_like<Self>(self.tail).template apply<F>(
                std::forward<ArgsSoFar>(args)..., forward_like_custom<Self, T>(self.head));
        }
    }

    template <auto F, template <typename> typename NoExpandPolicy = IsCallArg,
              typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAnyCompatible(this auto&& self,
                                                             ArgsSoFar&&... args) {
        using Self = decltype(self);
        if constexpr (sizeof...(Ts) == 0) {
            return fn::invoke_any_compatible<F, NoExpandPolicy>(
                unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...,
                unwrap_arg_on_invocation(forward_like_custom<Self, T>(self.head)));
        } else {
            return std::forward_like<Self>(self.tail)
                .template applyAnyCompatible<F, NoExpandPolicy>(
                    std::forward<ArgsSoFar>(args)..., forward_like_custom<Self, T>(self.head));
        }
    }

    template <auto F, bool Ignore = false, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgs(ArgsSoFar&&... args) && {
        if constexpr (sizeof...(Ts) == 0) {
            static_assert(
                std::is_invocable_v<
                    decltype(F),
                    decltype(unwrap_arg_on_invocation<Ignore>(std::forward<ArgsSoFar>(args)))...,
                    decltype(unwrap_arg_on_invocation<Ignore>(std::forward<T>(head)))>,
                "F is not callable with given arguments");
            return F(unwrap_arg_on_invocation<Ignore>(std::forward<ArgsSoFar>(args))...,
                     unwrap_arg_on_invocation<Ignore>(std::forward<T>(head)));
        } else {
            return std::move(tail).template applyAsArgs<F, Ignore>(std::forward<ArgsSoFar>(args)...,
                                                                   std::forward<T>(head));
        }
    }

    template <auto F, template <typename> typename NoExpandPolicy = IsCallArg,
              typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgsAnyCompatible(ArgsSoFar&&... args) && {
        if constexpr (sizeof...(Ts) == 0) {
            return fn::invoke_any_compatible<F, NoExpandPolicy>(
                unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...,
                unwrap_arg_on_invocation(std::forward<T>(head)));
        } else {
            return std::move(tail).template applyAsArgsAnyCompatible<F, NoExpandPolicy>(
                std::forward<ArgsSoFar>(args)..., std::forward<T>(head));
        }
    }

    template <auto F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFront(this auto&& self, auto&& callArgs,
                                                     ArgsSoFar&&... args)
        requires(IsPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        using Self = decltype(self);
        if constexpr (sizeof...(Ts) == 0) {
            return std::forward<decltype(callArgs)>(callArgs).template applyAsArgs<F>(
                std::forward<ArgsSoFar>(args)..., forward_like_custom<Self, T>(self.head));
        } else {
            return std::forward_like<Self>(self.tail).template applyFront<F>(
                std::forward<decltype(callArgs)>(callArgs), std::forward<ArgsSoFar>(args)...,
                forward_like_custom<Self, T>(self.head));
        }
    }

    template <auto F, template <typename> typename NoExpandPolicy = IsCallArg,
              typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFrontAnyCompatible(this auto&& self, auto&& callArgs,
                                                                  ArgsSoFar&&... args)
        requires(IsPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        using Self = decltype(self);
        if constexpr (sizeof...(Ts) == 0) {
            return std::forward<decltype(callArgs)>(callArgs)
                .template applyAsArgsAnyCompatible<F, NoExpandPolicy>(
                    std::forward<ArgsSoFar>(args)..., forward_like_custom<Self, T>(self.head));
        } else {
            return std::forward_like<Self>(self.tail)
                .template applyFrontAnyCompatible<F, NoExpandPolicy>(
                    std::forward<decltype(callArgs)>(callArgs), std::forward<ArgsSoFar>(args)...,
                    forward_like_custom<Self, T>(self.head));
        }
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) apply(this auto&& self, F&& f, ArgsSoFar&&... args) {
        using Self = decltype(self);
        if constexpr (sizeof...(Ts) == 0) {
            static_assert(
                std::is_invocable_v<
                    decltype(unwrap_arg_on_invocation(std::forward<F>(f))),
                    decltype(unwrap_arg_on_invocation(std::declval<decltype(args)>()))...,
                    decltype(unwrap_arg_on_invocation(forward_like_custom<Self, T>(self.head)))>,
                "F is not callable with given arguments");
            return std::invoke(unwrap_arg_on_invocation(std::forward<F>(f)),
                               unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...,
                               unwrap_arg_on_invocation(forward_like_custom<Self, T>(self.head)));
        } else {
            return std::forward_like<Self>(self.tail).apply(
                std::forward<F>(f), std::forward<ArgsSoFar>(args)...,
                forward_like_custom<Self, T>(self.head));
        }
    }

    template <std::size_t curIdx, std::size_t NextBoundArgIdx, std::size_t... Is, typename F,
              typename... ArgsSoFar, typename NextCallArg, typename... ArgsRemaining>
    FORCE_INLINE constexpr decltype(auto) apply(
        this auto&& self, F&& f, PayloadTuple<ArgsSoFar...>&& args,
        PayloadTuple<NextCallArg, ArgsRemaining...>&& remainingTuple) {
        static_assert(curIdx <= NextBoundArgIdx);
        static_assert(curIdx == sizeof...(ArgsSoFar));
        static constexpr auto kPickBoundArg = curIdx == NextBoundArgIdx;
        using Self = decltype(self);
        if constexpr (kPickBoundArg) {
            return std::forward_like<Self>(self.tail).template apply<curIdx + 1, Is...>(
                std::forward<F>(f),
                PayloadTuple<ArgsSoFar..., decltype(forward_like_custom<Self, T>(self.head))>{
                    std::move(args), forward_like_custom<Self, T>(self.head)},
                std::move(remainingTuple));
        } else {
            return std::forward_like<Self>(self).template apply<curIdx + 1, NextBoundArgIdx, Is...>(
                std::forward<F>(f),
                PayloadTuple<ArgsSoFar..., NextCallArg>{
                    std::move(args), std::forward<NextCallArg>(remainingTuple.head)},
                std::move(remainingTuple).tail);
        }
    }

    template <std::size_t curIdx, typename F, typename... ArgsSoFar, typename NextCallArg,
              typename... ArgsRemaining>
    FORCE_INLINE constexpr decltype(auto) apply(
        this auto&& self, F&& f, PayloadTuple<ArgsSoFar...>&& /*unused*/,  // NOLINT
        PayloadTuple<NextCallArg, ArgsRemaining...>&& /*unused*/) {        // NOLINT
        static_assert(false);
    }

    template <std::size_t curIdx, std::size_t NextBoundArgIdx, std::size_t... Is, typename F,
              typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) apply(this auto&& self, F&& f,
                                                PayloadTuple<ArgsSoFar...>&& args,
                                                PayloadTuple<>&& remainingTuple) {
        static_assert(curIdx == sizeof...(ArgsSoFar));
        static_assert(sizeof...(Is) > 0 || sizeof...(Ts) == 0);
        using Self = decltype(self);
        return std::forward_like<Self>(self.tail).template apply<curIdx + 1, Is...>(
            std::forward<F>(f),
            PayloadTuple<ArgsSoFar..., decltype(forward_like_custom<Self, T>(self.head))>{
                std::move(args), forward_like_custom<Self, T>(self.head)},
            std::move(remainingTuple));
    }

    template <template <typename> typename NoExpandPolicy = IsCallArg, std::size_t curIdx,
              std::size_t NextBoundArgIdx, std::size_t... Is, typename F, typename... ArgsSoFar,
              typename NextCallArg, typename... ArgsRemaining>
    FORCE_INLINE constexpr decltype(auto) applyAnyCompatible(
        this auto&& self, F&& f, PayloadTuple<ArgsSoFar...>&& args,
        PayloadTuple<NextCallArg, ArgsRemaining...>&& remainingTuple) {
        static_assert(curIdx <= NextBoundArgIdx);
        static_assert(curIdx == sizeof...(ArgsSoFar));
        static constexpr auto kPickBoundArg = curIdx == NextBoundArgIdx;
        using Self = decltype(self);
        if constexpr (kPickBoundArg) {
            return std::forward_like<Self>(self.tail)
                .template applyAnyCompatible<NoExpandPolicy, curIdx + 1, Is...>(
                    std::forward<F>(f),
                    PayloadTuple<ArgsSoFar..., decltype(forward_like_custom<Self, T>(self.head))>{
                        std::move(args), forward_like_custom<Self, T>(self.head)},
                    std::move(remainingTuple));
        } else {
            return std::forward_like<Self>(self)
                .template applyAnyCompatible<NoExpandPolicy, curIdx + 1, NextBoundArgIdx, Is...>(
                    std::forward<F>(f),
                    PayloadTuple<ArgsSoFar..., NextCallArg>{
                        std::move(args), std::forward<NextCallArg>(remainingTuple.head)},
                    std::move(remainingTuple).tail);
        }
    }

    template <template <typename> typename NoExpandPolicy = IsCallArg, std::size_t curIdx,
              typename F, typename... ArgsSoFar, typename NextCallArg, typename... ArgsRemaining>
    FORCE_INLINE constexpr decltype(auto) applyAnyCompatible(
        this auto&& self, F&& f, PayloadTuple<ArgsSoFar...>&& /*unused*/,  // NOLINT
        PayloadTuple<NextCallArg, ArgsRemaining...>&& /*unused*/) {        // NOLINT
        static_assert(false);
    }

    template <template <typename> typename NoExpandPolicy = IsCallArg, std::size_t curIdx,
              std::size_t NextBoundArgIdx, std::size_t... Is, typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAnyCompatible(this auto&& self, F&& f,
                                                             PayloadTuple<ArgsSoFar...>&& args,
                                                             PayloadTuple<>&& remainingTuple) {
        static_assert(curIdx == sizeof...(ArgsSoFar));
        static_assert(sizeof...(Is) > 0 || sizeof...(Ts) == 0);
        using Self = decltype(self);
        return std::forward_like<Self>(self.tail)
            .template applyAnyCompatible<NoExpandPolicy, curIdx + 1, Is...>(
                std::forward<F>(f),
                PayloadTuple<ArgsSoFar..., decltype(forward_like_custom<Self, T>(self.head))>{
                    std::move(args), forward_like_custom<Self, T>(self.head)},
                std::move(remainingTuple));
    }

    template <auto F, std::size_t curIdx, std::size_t NextBoundArgIdx, std::size_t... Is,
              typename... ArgsSoFar, typename NextCallArg, typename... ArgsRemaining>
        requires(!std::is_same_v<decltype(F), std::size_t>)
    FORCE_INLINE constexpr decltype(auto) apply(
        this auto&& self, PayloadTuple<ArgsSoFar...>&& args,
        PayloadTuple<NextCallArg, ArgsRemaining...>&& remainingTuple) {
        static_assert(curIdx <= NextBoundArgIdx);
        static_assert(curIdx == sizeof...(ArgsSoFar));
        static constexpr auto kPickBoundArg = curIdx == NextBoundArgIdx;
        using Self = decltype(self);
        if constexpr (kPickBoundArg) {
            return std::forward_like<Self>(self.tail).template apply<F, curIdx + 1, Is...>(
                PayloadTuple<ArgsSoFar..., decltype(forward_like_custom<Self, T>(self.head))>{
                    std::move(args), forward_like_custom<Self, T>(self.head)},
                std::move(remainingTuple));
        } else {
            return std::forward_like<Self>(self)
                .template apply<F, curIdx + 1, NextBoundArgIdx, Is...>(
                    PayloadTuple<ArgsSoFar..., NextCallArg>{
                        std::move(args), std::forward<NextCallArg>(remainingTuple.head)},
                    std::move(remainingTuple).tail);
        }
    }

    template <auto F, std::size_t curIdx, typename... ArgsSoFar, typename NextCallArg,
              typename... ArgsRemaining>
        requires(!std::is_same_v<decltype(F), std::size_t>)
    FORCE_INLINE constexpr decltype(auto) apply(
        this auto&& self, PayloadTuple<ArgsSoFar...>&& /*unused*/,   // NOLINT
        PayloadTuple<NextCallArg, ArgsRemaining...>&& /*unused*/) {  // NOLINT
        static_assert(false);
    }

    template <auto F, std::size_t curIdx, std::size_t NextBoundArgIdx, std::size_t... Is,
              typename... ArgsSoFar>
        requires(!std::is_same_v<decltype(F), std::size_t>)
    FORCE_INLINE constexpr decltype(auto) apply(this auto&& self, PayloadTuple<ArgsSoFar...>&& args,
                                                PayloadTuple<>&& remainingTuple) {
        static_assert(curIdx == sizeof...(ArgsSoFar));
        static_assert(sizeof...(Is) > 0 || sizeof...(Ts) == 0);
        using Self = decltype(self);
        return std::forward_like<Self>(self.tail).template apply<F, curIdx + 1, Is...>(
            PayloadTuple<ArgsSoFar..., decltype(forward_like_custom<Self, T>(self.head))>{
                std::move(args), forward_like_custom<Self, T>(self.head)},
            std::move(remainingTuple));
    }

    template <auto F, template <typename> typename NoExpandPolicy = IsCallArg, std::size_t curIdx,
              std::size_t NextBoundArgIdx, std::size_t... Is, typename... ArgsSoFar,
              typename NextCallArg, typename... ArgsRemaining>
        requires(!std::is_same_v<decltype(F), std::size_t>)
    FORCE_INLINE constexpr decltype(auto) applyAnyCompatible(
        this auto&& self, PayloadTuple<ArgsSoFar...>&& args,
        PayloadTuple<NextCallArg, ArgsRemaining...>&& remainingTuple) {
        static_assert(curIdx <= NextBoundArgIdx);
        static_assert(curIdx == sizeof...(ArgsSoFar));
        static constexpr auto kPickBoundArg = curIdx == NextBoundArgIdx;
        using Self = decltype(self);
        if constexpr (kPickBoundArg) {
            return std::forward_like<Self>(self.tail)
                .template applyAnyCompatible<F, NoExpandPolicy, curIdx + 1, Is...>(
                    PayloadTuple<ArgsSoFar..., decltype(forward_like_custom<Self, T>(self.head))>{
                        std::move(args), forward_like_custom<Self, T>(self.head)},
                    std::move(remainingTuple));
        } else {
            return std::forward_like<Self>(self)
                .template applyAnyCompatible<F, NoExpandPolicy, curIdx + 1, NextBoundArgIdx, Is...>(
                    PayloadTuple<ArgsSoFar..., NextCallArg>{
                        std::move(args), std::forward<NextCallArg>(remainingTuple.head)},
                    std::move(remainingTuple).tail);
        }
    }

    template <auto F, template <typename> typename NoExpandPolicy = IsCallArg, std::size_t curIdx,
              typename... ArgsSoFar, typename NextCallArg, typename... ArgsRemaining>
        requires(!std::is_same_v<decltype(F), std::size_t>)
    FORCE_INLINE constexpr decltype(auto) applyAnyCompatible(
        this auto&& self, PayloadTuple<ArgsSoFar...>&& /*unused*/,   // NOLINT
        PayloadTuple<NextCallArg, ArgsRemaining...>&& /*unused*/) {  // NOLINT
        static_assert(false);
    }

    template <auto F, template <typename> typename NoExpandPolicy = IsCallArg, std::size_t curIdx,
              std::size_t NextBoundArgIdx, std::size_t... Is, typename... ArgsSoFar>
        requires(!std::is_same_v<decltype(F), std::size_t>)
    FORCE_INLINE constexpr decltype(auto) applyAnyCompatible(this auto&& self,
                                                             PayloadTuple<ArgsSoFar...>&& args,
                                                             PayloadTuple<>&& remainingTuple) {
        static_assert(curIdx == sizeof...(ArgsSoFar));
        static_assert(sizeof...(Is) > 0 || sizeof...(Ts) == 0);
        using Self = decltype(self);
        return std::forward_like<Self>(self.tail)
            .template applyAnyCompatible<F, NoExpandPolicy, curIdx + 1, Is...>(
                PayloadTuple<ArgsSoFar..., decltype(forward_like_custom<Self, T>(self.head))>{
                    std::move(args), forward_like_custom<Self, T>(self.head)},
                std::move(remainingTuple));
    }

    template <template <typename> typename NoExpandPolicy = IsCallArg, typename F,
              typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAnyCompatible(this auto&& self, F&& f,
                                                             ArgsSoFar&&... args) {
        using Self = decltype(self);
        if constexpr (sizeof...(Ts) == 0) {
            static_assert(
                is_any_compatible_invocable_b<
                    NoExpandPolicy, decltype(unwrap_arg_on_invocation(std::forward<F>(f))),
                    decltype(unwrap_arg_on_invocation(std::declval<decltype(args)>()))...,
                    decltype(unwrap_arg_on_invocation(forward_like_custom<Self, T>(self.head)))>,
                "F is not callable with given arguments");
            return fn::invoke_any_compatible<NoExpandPolicy>(
                unwrap_arg_on_invocation(std::forward<F>(f)),
                unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...,
                unwrap_arg_on_invocation(forward_like_custom<Self, T>(self.head)));
        } else {
            return std::forward_like<Self>(self.tail).template applyAnyCompatible<NoExpandPolicy>(
                std::forward<F>(f), std::forward<ArgsSoFar>(args)...,
                forward_like_custom<Self, T>(self.head));
        }
    }

    template <bool Ignore = false, typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgs(F&& f, ArgsSoFar&&... args) && {
        if constexpr (sizeof...(Ts) == 0) {
            static_assert(
                std::is_invocable_v<
                    decltype(unwrap_arg_on_invocation<Ignore>(std::forward<F>(f))),
                    decltype(unwrap_arg_on_invocation<Ignore>(std::declval<decltype(args)>()))...,
                    decltype(unwrap_arg_on_invocation<Ignore>(std::forward<T>(head)))>,
                "F is not callable with given arguments");
            return std::invoke(unwrap_arg_on_invocation<Ignore>(std::forward<F>(f)),
                               unwrap_arg_on_invocation<Ignore>(std::forward<ArgsSoFar>(args))...,
                               unwrap_arg_on_invocation<Ignore>(std::forward<T>(head)));
        } else {
            return std::move(tail).template applyAsArgs<Ignore>(
                std::forward<F>(f), std::forward<ArgsSoFar>(args)..., std::forward<T>(head));
        }
    }

    template <template <typename> typename NoExpandPolicy = IsCallArg, typename F,
              typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyAsArgsAnyCompatible(F&& f, ArgsSoFar&&... args) && {
        if constexpr (sizeof...(Ts) == 0) {
            return fn::invoke_any_compatible<NoExpandPolicy>(
                unwrap_arg_on_invocation(std::forward<F>(f)),
                unwrap_arg_on_invocation(std::forward<ArgsSoFar>(args))...,
                unwrap_arg_on_invocation(std::forward<T>(head)));
        } else {
            return std::move(tail).template applyAsArgsAnyCompatible<NoExpandPolicy>(
                std::forward<F>(f), std::forward<ArgsSoFar>(args)..., std::forward<T>(head));
        }
    }

    template <typename F, typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFront(this auto&& self, F&& f, auto&& callArgs,
                                                     ArgsSoFar&&... args)
        requires(IsPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        using Self = decltype(self);
        if constexpr (sizeof...(Ts) == 0) {
            return std::forward<decltype(callArgs)>(callArgs).applyAsArgs(
                std::forward<F>(f), std::forward<ArgsSoFar>(args)...,
                forward_like_custom<Self, T>(self.head));
        } else {
            return std::forward_like<Self>(self.tail).template applyFront<F>(
                std::forward<F>(f), std::forward<decltype(callArgs)>(callArgs),
                std::forward<ArgsSoFar>(args)..., forward_like_custom<Self, T>(self.head));
        }
    }

    template <template <typename> typename NoExpandPolicy = IsCallArg, typename F,
              typename... ArgsSoFar>
    FORCE_INLINE constexpr decltype(auto) applyFrontAnyCompatible(this auto&& self, F&& f,
                                                                  auto&& callArgs,
                                                                  ArgsSoFar&&... args)
        requires(IsPayloadTuple<std::remove_cvref_t<decltype(callArgs)>>::value &&
                 std::is_rvalue_reference_v<decltype(callArgs)>)
    {
        using Self = decltype(self);
        if constexpr (sizeof...(Ts) == 0) {
            return std::forward<decltype(callArgs)>(callArgs)
                .template applyAsArgsAnyCompatible<NoExpandPolicy>(
                    std::forward<F>(f), std::forward<ArgsSoFar>(args)...,
                    forward_like_custom<Self, T>(self.head));
        } else {
            return std::forward_like<Self>(self.tail)
                .template applyFrontAnyCompatible<NoExpandPolicy>(
                    std::forward<F>(f), std::forward<decltype(callArgs)>(callArgs),
                    std::forward<ArgsSoFar>(args)..., forward_like_custom<Self, T>(self.head));
        }
    }
};

template <std::size_t I, typename... Ts>
struct PayloadTupleGetter;

template <typename T>
struct RefWrapToRef {
    using Type = T;
};

template <typename T>
struct RefWrapToRef<BoundRefWrapTag<T>> {
    using Type = T;
};

template <typename T, typename... Ts>
struct PayloadTupleGetter<0, T, Ts...> {
    static constexpr RefWrapToRef<T>::Type& get(PayloadTuple<T, Ts...>& tuple) {
        return tuple.head;
    }

    static constexpr const RefWrapToRef<T>::Type& get(const PayloadTuple<T, Ts...>& tuple) {
        return tuple.head;
    }

    static constexpr RefWrapToRef<T>::Type& getWrapToRef(PayloadTuple<T, Ts...>& tuple) {
        return tuple.head;
    }

    static constexpr const RefWrapToRef<T>::Type& getWrapToRef(
        const PayloadTuple<T, Ts...>& tuple) {
        return tuple.head;
    }
};

template <std::size_t I, typename T, typename... Ts>
    requires(I > 0)
struct PayloadTupleGetter<I, T, Ts...> {
    static constexpr decltype(auto) get(PayloadTuple<T, Ts...>& tuple) {
        return PayloadTupleGetter<I - 1, Ts...>::get(tuple.tail);
    }

    static constexpr decltype(auto) get(const PayloadTuple<T, Ts...>& tuple) {
        return PayloadTupleGetter<I - 1, Ts...>::get(tuple.tail);
    }

    static constexpr decltype(auto) getWrapToRef(PayloadTuple<T, Ts...>& tuple) {
        return PayloadTupleGetter<I - 1, Ts...>::getWrapToRef(tuple.tail);
    }

    static constexpr decltype(auto) getWrapToRef(const PayloadTuple<T, Ts...>& tuple) {
        return PayloadTupleGetter<I - 1, Ts...>::getWrapToRef(tuple.tail);
    }
};

template <typename F, typename... Args>
struct PayloadImpl {
    F f;
    [[no_unique_address]] PayloadTuple<Args...> args;

    FORCE_INLINE constexpr auto invoke(this auto&& self, auto&&... t) -> decltype(auto) {
        using Self = decltype(self);
        static_assert((std::is_reference_v<decltype(t)> && ...));
        return std::forward_like<Self>(self.args).apply(
            std::forward_like<Self>(self.f),
            CallArgTag<decltype(t)>{std::forward<decltype(t)>(t)}...);
    }

    template <std::size_t... Is>
        requires(sizeof...(Is) > 0)
    FORCE_INLINE constexpr auto invoke(this auto&& self, auto&&... t) -> decltype(auto) {
        using Self = decltype(self);
        static_assert((std::is_reference_v<decltype(t)> && ...));
        return std::forward_like<Self>(self.args).template apply<0, Is...>(
            std::forward_like<Self>(self.f), PayloadTuple<>{},
            PayloadTuple<CallArgTag<decltype(t)>...>{
                CallArgTag<decltype(t)>{std::forward<decltype(t)>(t)}...});
    }

    template <template <typename> typename NoExpandPolicy = IsCallArg, std::size_t... Is>
        requires(sizeof...(Is) > 0)
    FORCE_INLINE constexpr auto invokeCompatible(this auto&& self, auto&&... t) -> decltype(auto) {
        using Self = decltype(self);
        static_assert((std::is_reference_v<decltype(t)> && ...));
        return std::forward_like<Self>(self.args)
            .template applyAnyCompatible<NoExpandPolicy, 0, Is...>(
                std::forward_like<Self>(self.f), PayloadTuple<>{},
                PayloadTuple<CallArgTag<decltype(t)>...>{
                    CallArgTag<decltype(t)>{std::forward<decltype(t)>(t)}...});
    }

    template <template <typename> typename NoExpandPolicy = IsCallArg>
    FORCE_INLINE constexpr auto invokeCompatible(this auto&& self, auto&&... t) -> decltype(auto) {
        using Self = decltype(self);
        static_assert((std::is_reference_v<decltype(t)> && ...));
        return std::forward_like<Self>(self.args).template applyAnyCompatible<NoExpandPolicy>(
            std::forward_like<Self>(self.f),
            CallArgTag<decltype(t)>{std::forward<decltype(t)>(t)}...);
    }

    FORCE_INLINE constexpr auto invokeFront(this auto&& self, auto&&... t) -> decltype(auto) {
        static_assert((std::is_reference_v<decltype(t)> && ...));
        using Self = decltype(self);
        return std::forward_like<Self>(self.args).applyFront(
            std::forward_like<Self>(self.f),
            PayloadTuple<CallArgTag<decltype(t)>...>{std::forward<decltype(t)>(t)...});
    }

    template <template <typename> typename NoExpandPolicy = IsCallArg>
    FORCE_INLINE constexpr auto invokeFrontCompatible(this auto&& self, auto&&... t)
        -> decltype(auto) {
        static_assert((std::is_reference_v<decltype(t)> && ...));
        using Self = decltype(self);
        return std::forward_like<Self>(self.args).template applyFrontAnyCompatible<NoExpandPolicy>(
            std::forward_like<Self>(self.f),
            PayloadTuple<CallArgTag<decltype(t)>...>{std::forward<decltype(t)>(t)...});
    }
};

template <auto F, typename... Args>
struct PayloadImplArgs {
    using F_Type = decltype(F);
    [[no_unique_address]] PayloadTuple<Args...> args;

    FORCE_INLINE constexpr auto invoke(this auto&& self, auto&&... t) -> decltype(auto) {
        static_assert((std::is_reference_v<decltype(t)> && ...));
        using Self = decltype(self);
        return std::forward_like<Self>(self.args).template apply<F>(
            CallArgTag<decltype(t)>{std::forward<decltype(t)>(t)}...);
    }

    template <template <typename> typename NoExpandPolicy = IsCallArg>
    FORCE_INLINE constexpr auto invokeCompatible(this auto&& self, auto&&... t) -> decltype(auto) {
        static_assert((std::is_reference_v<decltype(t)> && ...));
        using Self = decltype(self);
        return std::forward_like<Self>(self.args).template applyAnyCompatible<F, NoExpandPolicy>(
            CallArgTag<decltype(t)>{std::forward<decltype(t)>(t)}...);
    }

    template <std::size_t... Is>
        requires(sizeof...(Is) > 0)
    FORCE_INLINE constexpr auto invoke(this auto&& self, auto&&... t) -> decltype(auto) {
        using Self = decltype(self);
        static_assert((std::is_reference_v<decltype(t)> && ...));
        return std::forward_like<Self>(self.args).template apply<F, 0, Is...>(
            std::forward_like<Self>(self.f), PayloadTuple<>{},
            PayloadTuple<CallArgTag<decltype(t)>...>{
                CallArgTag<decltype(t)>{std::forward<decltype(t)>(t)}...});
    }

    template <template <typename> typename NoExpandPolicy = IsCallArg, std::size_t... Is>
        requires(sizeof...(Is) > 0)
    FORCE_INLINE constexpr auto invokeCompatible(this auto&& self, auto&&... t) -> decltype(auto) {
        using Self = decltype(self);
        static_assert((std::is_reference_v<decltype(t)> && ...));
        return std::forward_like<Self>(self.args)
            .template applyAnyCompatible<F, NoExpandPolicy, 0, Is...>(
                std::forward_like<Self>(self.f), PayloadTuple<>{},
                PayloadTuple<CallArgTag<decltype(t)>...>{
                    CallArgTag<decltype(t)>{std::forward<decltype(t)>(t)}...});
    }

    FORCE_INLINE constexpr auto invokeFront(this auto&& self, auto&&... t) -> decltype(auto) {
        static_assert((std::is_reference_v<decltype(t)> && ...));
        using Self = decltype(self);
        return std::forward_like<Self>(self.args).template applyFront<F>(
            PayloadTuple<CallArgTag<decltype(t)>...>{std::forward<decltype(t)>(t)...});
    }

    template <template <typename> typename NoExpandPolicy = IsCallArg>
    FORCE_INLINE constexpr auto invokeFrontCompatible(this auto&& self, auto&&... t)
        -> decltype(auto) {
        static_assert((std::is_reference_v<decltype(t)> && ...));
        using Self = decltype(self);
        return std::forward_like<Self>(self.args)
            .template applyFrontAnyCompatible<F, NoExpandPolicy>(
                PayloadTuple<CallArgTag<decltype(t)>...>{std::forward<decltype(t)>(t)...});
    }
};

template <typename... Args>
struct PayloadNoRvalueRef : std::false_type {};

template <typename... Args>
    requires((!std::is_rvalue_reference_v<Args>) && ...)
struct PayloadNoRvalueRef<PayloadTuple<Args...>> : std::true_type {};

template <typename F, typename... Args>
    requires(!std::is_rvalue_reference_v<F> && PayloadNoRvalueRef<PayloadTuple<Args...>>::value)
struct PayloadNoRvalueRef<PayloadImpl<F, Args...>> : std::true_type {};

template <auto F, typename... Args>
    requires(PayloadNoRvalueRef<PayloadTuple<Args...>>::value)
struct PayloadNoRvalueRef<PayloadImplArgs<F, Args...>> : std::true_type {};

template <typename T, bool Const, bool Invocable>
struct AddRefWrapperToRef {
    using Type = T;
};

template <typename T, bool Invocable>
struct AddRefWrapperToRef<T&, false, Invocable> {
    using Type = BoundRefWrapTag<T&, Invocable>;
};

template <typename T, bool Invocable>
struct AddRefWrapperToRef<T&, true, Invocable> {
    using Type = BoundRefWrapTag<const T&, Invocable>;
};

template <typename T, bool Const, bool Invocable>
using AddRefWrapperToRef_t = typename AddRefWrapperToRef<T, Const, Invocable>::Type;

template <typename F, typename... Args>
using PayloadT = PayloadImpl<std::decay_t<F>, std::decay_t<Args>...>;

template <auto F, typename... Args>
using PayloadArgsT = PayloadImplArgs<F, std::decay_t<Args>...>;

template <typename, typename, typename...>
struct IsPayload : std::false_type {};

template <typename From, typename To>
using ForwardLikeT = decltype(std::forward_like<From>(std::declval<To>()));

template <typename F, typename... BoundArgs, typename Self, typename... CallArgs>
struct IsPayload<PayloadImpl<F, BoundArgs...>, Self, CallArgs...> : std::true_type {
    using F_qual = ForwardLikeT<Self, F>;
    static constexpr bool kIsInvocable =
        std::is_invocable_v<F_qual, CallArgs..., ForwardLikeT<Self, BoundArgs>...>;

    static constexpr bool kIsInvocableFront =
        std::is_invocable_v<F_qual, ForwardLikeT<Self, BoundArgs>..., CallArgs...>;
};

template <auto F, typename... BoundArgs, typename Self, typename... CallArgs>
struct IsPayload<PayloadImplArgs<F, BoundArgs...>, Self, CallArgs...> : std::true_type {
    using F_qual = ForwardLikeT<Self, decltype(F)>;
    static constexpr bool kIsInvocable =
        std::is_invocable_v<F_qual, CallArgs..., ForwardLikeT<Self, BoundArgs>...>;

    static constexpr bool kIsInvocableFront =
        std::is_invocable_v<F_qual, ForwardLikeT<Self, BoundArgs>..., CallArgs...>;
};

}  // namespace fn

#endif  // TASK_PAYLOAD_HPP