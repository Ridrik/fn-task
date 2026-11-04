#ifndef TASK_FUNCTION_TRAITS_HPP
#define TASK_FUNCTION_TRAITS_HPP

#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "payload.hpp"

namespace fn {

template <typename T>
struct RefToConstRef {
    using Type = T;
};

template <typename U>
struct RefToConstRef<std::reference_wrapper<U>> {
    using Type = std::reference_wrapper<const U>;
};

template <typename, bool Copyable = true>
struct TrivialUse : std::false_type {};

// Captured arguments of type-erased callable are not copy-assigned or move-assigned, and instead
// are destroyed followed by the copy/move constructor. Therefore, to be safe to simply memcpy
// and not need manager pointers, one checks copy/move constructor and destructor triviallity
template <typename Pl>
    requires(std::is_trivially_copy_constructible_v<Pl> &&
             std::is_trivially_move_constructible_v<Pl> && std::is_trivially_destructible_v<Pl>)
struct TrivialUse<Pl, true> : std::true_type {};

template <typename Pl>
    requires(std::is_trivially_move_constructible_v<Pl> && std::is_trivially_destructible_v<Pl>)
struct TrivialUse<Pl, false> : std::true_type {};

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

template <typename T>
struct FunctionTraits {
    static constexpr bool kValid{false};
    static constexpr bool kIsNoExcept{false};
    static constexpr bool kIsConst{false};
    static constexpr bool kIsLValueRef{false};
    static constexpr bool kIsRValueRef{false};
};

template <typename T>
    requires(requires { &T::operator(); })
struct FunctionTraits<T> : FunctionTraits<decltype(&T::operator())> {};

template <typename Ret, typename... Args>
struct FunctionTraits<Ret (*)(Args...)> {
    static constexpr bool kValid{true};
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
    using payload_tuple = PayloadTuple<Args...>;
    using func_ptr = Ret (*)(Args...);
    using signature = Ret(Args...);
    using signature_no_except = Ret(Args...) noexcept;
    using base_signature = signature;
    using base_signature_no_except = signature_no_except;
    static constexpr bool kIsNoExcept{false};
    static constexpr bool kIsConst{false};
    static constexpr bool kIsLValueRef{false};
    static constexpr bool kIsRValueRef{false};
};

template <typename Ret, typename... Args>
struct FunctionTraits<Ret (*)(Args...) noexcept> {
    static constexpr bool kValid{true};
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
    using payload_tuple = PayloadTuple<Args...>;
    using func_ptr = Ret (*)(Args...) noexcept;
    using signature = Ret(Args...) noexcept;
    using signature_no_except = Ret(Args...) noexcept;
    using base_signature = Ret(Args...);
    using base_signature_no_except = signature_no_except;
    static constexpr bool kIsNoExcept{true};
    static constexpr bool kIsConst{false};
    static constexpr bool kIsLValueRef{false};
    static constexpr bool kIsRValueRef{false};
};

// NOLINTBEGIN
#define DEFINE_FUNC_TRAITS(QUALIFIERS, IS_CONST, IS_VOLATILE, IS_LVAL, IS_RVAL) \
    template <typename Ret, typename... Args>                                   \
    struct FunctionTraits<Ret(Args...) QUALIFIERS> {                            \
        static constexpr bool kValid{true};                                     \
        using return_type = Ret;                                                \
        using args_tuple = std::tuple<Args...>;                                 \
        using payload_tuple = PayloadTuple<Args...>;                            \
        using signature = Ret(Args...) QUALIFIERS;                              \
        using base_signature = Ret(Args...);                                    \
        using base_signature_noexcept = Ret(Args...) noexcept;                  \
        using signature_no_except = Ret(Args...) QUALIFIERS noexcept;           \
        using func_ptr = Ret (*)(Args...);                                      \
        static constexpr bool kIsConst{IS_CONST};                               \
        static constexpr bool kIsVolatile{IS_VOLATILE};                         \
        static constexpr bool kIsLValueRef{IS_LVAL};                            \
        static constexpr bool kIsRValueRef{IS_RVAL};                            \
        static constexpr bool kIsNoExcept{false};                               \
    }

#define DEFINE_FUNC_TRAITS_NOEXCEPT(QUALIFIERS, IS_CONST, IS_VOLATILE, IS_LVAL, IS_RVAL) \
    template <typename Ret, typename... Args>                                            \
    struct FunctionTraits<Ret(Args...) QUALIFIERS noexcept> {                            \
        static constexpr bool kValid{true};                                              \
        using return_type = Ret;                                                         \
        using args_tuple = std::tuple<Args...>;                                          \
        using payload_tuple = PayloadTuple<Args...>;                                     \
        using signature = Ret(Args...) QUALIFIERS noexcept;                              \
        using base_signature = Ret(Args...);                                             \
        using base_signature_noexcept = Ret(Args...) noexcept;                           \
        using signature_no_except = Ret(Args...) QUALIFIERS noexcept;                    \
        using func_ptr = Ret (*)(Args...) noexcept;                                      \
        static constexpr bool kIsConst{IS_CONST};                                        \
        static constexpr bool kIsVolatile{IS_VOLATILE};                                  \
        static constexpr bool kIsLValueRef{IS_LVAL};                                     \
        static constexpr bool kIsRValueRef{IS_RVAL};                                     \
        static constexpr bool kIsNoExcept{true};                                         \
    }
// NOLINTEND

DEFINE_FUNC_TRAITS(, false, false, false, false);
DEFINE_FUNC_TRAITS(const, true, false, false, false);
DEFINE_FUNC_TRAITS(volatile, false, true, false, false);
DEFINE_FUNC_TRAITS(const volatile, true, true, false, false);
DEFINE_FUNC_TRAITS(&, false, false, true, false);
DEFINE_FUNC_TRAITS(const&, true, false, true, false);
DEFINE_FUNC_TRAITS(volatile&, false, true, true, false);
DEFINE_FUNC_TRAITS(const volatile&, true, true, true, false);
DEFINE_FUNC_TRAITS(&&, false, false, false, true);
DEFINE_FUNC_TRAITS(const&&, true, false, false, true);
DEFINE_FUNC_TRAITS(volatile&&, false, true, false, true);
DEFINE_FUNC_TRAITS(const volatile&&, true, true, false, true);

// Noexcept Variants
DEFINE_FUNC_TRAITS_NOEXCEPT(, false, false, false, false);
DEFINE_FUNC_TRAITS_NOEXCEPT(const, true, false, false, false);
DEFINE_FUNC_TRAITS_NOEXCEPT(volatile, false, true, false, false);
DEFINE_FUNC_TRAITS_NOEXCEPT(const volatile, true, true, false, false);
DEFINE_FUNC_TRAITS_NOEXCEPT(&, false, false, true, false);
DEFINE_FUNC_TRAITS_NOEXCEPT(const&, true, false, true, false);
DEFINE_FUNC_TRAITS_NOEXCEPT(volatile&, false, true, true, false);
DEFINE_FUNC_TRAITS_NOEXCEPT(const volatile&, true, true, true, false);
DEFINE_FUNC_TRAITS_NOEXCEPT(&&, false, false, false, true);
DEFINE_FUNC_TRAITS_NOEXCEPT(const&&, true, false, false, true);
DEFINE_FUNC_TRAITS_NOEXCEPT(volatile&&, false, true, false, true);
DEFINE_FUNC_TRAITS_NOEXCEPT(const volatile&&, true, true, false, true);

#undef DEFINE_FUNC_TRAITS
#undef DEFINE_FUNC_TRAITS_NOEXCEPT

// NOLINTBEGIN
#define DEFINE_MEMBER_FUNCTION_TRAITS(QUALIFIERS, IS_CONST, IS_LVAL, IS_RVAL) \
    template <typename Class, typename Ret, typename... Args>                 \
    struct FunctionTraits<Ret (Class::*)(Args...) QUALIFIERS> {               \
        static constexpr bool kValid{true};                                   \
        using return_type = Ret;                                              \
        using args_tuple = std::tuple<Args...>;                               \
        using payload_tuple = PayloadTuple<Args...>;                          \
        using func_ptr = Ret (*)(Args...);                                    \
        using signature = Ret(Args...) QUALIFIERS;                            \
        using base_signature = Ret(Args...);                                  \
        using base_signature_noexcept = Ret(Args...) noexcept;                \
        using signature_no_except = Ret(Args...) QUALIFIERS noexcept;         \
        static constexpr bool kIsNoExcept{false};                             \
        static constexpr bool kIsConst{IS_CONST};                             \
        static constexpr bool kIsLValueRef{IS_LVAL};                          \
        static constexpr bool kIsRValueRef{IS_RVAL};                          \
    }

#define DEFINE_MEMBER_FUNCTION_TRAITS_NOEXCEPT(QUALIFIERS, IS_CONST, IS_LVAL, IS_RVAL) \
    template <typename Class, typename Ret, typename... Args>                          \
    struct FunctionTraits<Ret (Class::*)(Args...) QUALIFIERS noexcept> {               \
        static constexpr bool kValid{true};                                            \
        using return_type = Ret;                                                       \
        using args_tuple = std::tuple<Args...>;                                        \
        using payload_tuple = PayloadTuple<Args...>;                                   \
        using func_ptr = Ret (*)(Args...) noexcept;                                    \
        using signature = Ret(Args...) QUALIFIERS noexcept;                            \
        using base_signature = Ret(Args...);                                           \
        using base_signature_noexcept = Ret(Args...) noexcept;                         \
        using signature_no_except = Ret(Args...) QUALIFIERS noexcept;                  \
        static constexpr bool kIsNoExcept{true};                                       \
        static constexpr bool kIsConst{IS_CONST};                                      \
        static constexpr bool kIsLValueRef{IS_LVAL};                                   \
        static constexpr bool kIsRValueRef{IS_RVAL};                                   \
    }
// NOLINTEND

DEFINE_MEMBER_FUNCTION_TRAITS(, false, false, false);
DEFINE_MEMBER_FUNCTION_TRAITS(const, true, false, false);
DEFINE_MEMBER_FUNCTION_TRAITS(volatile, false, false, false);
DEFINE_MEMBER_FUNCTION_TRAITS(const volatile, true, false, false);
DEFINE_MEMBER_FUNCTION_TRAITS(&, false, true, false);
DEFINE_MEMBER_FUNCTION_TRAITS(const&, true, true, false);
DEFINE_MEMBER_FUNCTION_TRAITS(volatile&, false, true, false);
DEFINE_MEMBER_FUNCTION_TRAITS(const volatile&, true, true, false);
DEFINE_MEMBER_FUNCTION_TRAITS(&&, false, false, true);
DEFINE_MEMBER_FUNCTION_TRAITS(const&&, true, false, true);
DEFINE_MEMBER_FUNCTION_TRAITS(volatile&&, false, false, true);
DEFINE_MEMBER_FUNCTION_TRAITS(const volatile&&, true, false, true);

DEFINE_MEMBER_FUNCTION_TRAITS_NOEXCEPT(, false, false, false);
DEFINE_MEMBER_FUNCTION_TRAITS_NOEXCEPT(const, true, false, false);
DEFINE_MEMBER_FUNCTION_TRAITS_NOEXCEPT(volatile, false, false, false);
DEFINE_MEMBER_FUNCTION_TRAITS_NOEXCEPT(const volatile, true, false, false);
DEFINE_MEMBER_FUNCTION_TRAITS_NOEXCEPT(&, false, true, false);
DEFINE_MEMBER_FUNCTION_TRAITS_NOEXCEPT(const&, true, true, false);
DEFINE_MEMBER_FUNCTION_TRAITS_NOEXCEPT(volatile&, false, true, false);
DEFINE_MEMBER_FUNCTION_TRAITS_NOEXCEPT(const volatile&, true, true, false);
DEFINE_MEMBER_FUNCTION_TRAITS_NOEXCEPT(&&, false, false, true);
DEFINE_MEMBER_FUNCTION_TRAITS_NOEXCEPT(const&&, true, false, true);
DEFINE_MEMBER_FUNCTION_TRAITS_NOEXCEPT(volatile&&, false, false, true);
DEFINE_MEMBER_FUNCTION_TRAITS_NOEXCEPT(const volatile&&, true, false, true);

#undef DEFINE_MEMBER_FUNCTION_TRAITS
#undef DEFINE_MEMBER_FUNCTION_TRAITS_NOEXCEPT

template <typename T>
    requires requires { typename T::Signature; }
struct FunctionTraits<T> : FunctionTraits<typename T::Signature> {};

template <typename T>
concept HasFunctionTraits = FunctionTraits<T>::kValid;

template <typename T>
struct AddConst {
    using Type = T const;
};

template <typename T>
    requires(FunctionTraits<T>::kValid)
struct AddNoExcept {
    using Type = FunctionTraits<T>::signature_no_except;
};

template <typename Sig>
concept IsBaseSignature = std::is_same_v<Sig, typename FunctionTraits<Sig>::base_signature>;

template <typename Sig>
concept IsBaseSignatureAndNoExcept =
    std::is_same_v<Sig, typename FunctionTraits<Sig>::base_signature_noexcept>;

template <typename SigBase, typename SigTarget>
concept BaseSignatureMatches = std::is_same_v<typename FunctionTraits<SigBase>::base_signature,
                                              typename FunctionTraits<SigTarget>::base_signature>;

template <typename Sig>
concept IsNoExcept = FunctionTraits<Sig>::kIsNoExcept;

template <typename Sig>
concept IsConst = FunctionTraits<Sig>::kIsConst;

template <typename Sig>
concept IsLvRefSignature = FunctionTraits<Sig>::kIsLValueRef;

template <typename Sig>
concept IsRvRefSignature = FunctionTraits<Sig>::kIsRValueRef;

template <typename SigFrom, typename SigTo>
concept SignatureConstConvertible = BaseSignatureMatches<SigFrom, SigTo> && IsConst<SigFrom> &&
                                    IsConst<SigTo> && (IsNoExcept<SigFrom> || !IsNoExcept<SigTo>);

template <typename SigFrom, typename SigTo>
concept SignatureConstToNonConstConvertible =
    BaseSignatureMatches<SigFrom, SigTo> && IsConst<SigFrom> && !IsConst<SigTo> &&
    ((IsRvRefSignature<SigFrom> && IsRvRefSignature<SigTo>) ||
     (!IsRvRefSignature<SigFrom> && IsLvRefSignature<SigTo>)) &&
    (IsNoExcept<SigFrom> || !IsNoExcept<SigTo>);

template <typename SigFrom, typename SigTo>
concept AnySignatureConvertibleToItsBase =
    IsBaseSignature<SigTo> && BaseSignatureMatches<SigTo, SigFrom>;

template <typename SigFrom, typename SigTo>
concept AnySignatureConvertibleToItsBaseNoExcept =
    IsBaseSignatureAndNoExcept<SigTo> && BaseSignatureMatches<SigTo, SigFrom> &&
    IsNoExcept<SigFrom>;

template <typename SigFrom, typename SigTo>
concept NoExceptMapsToThrowingSignature =
    !IsNoExcept<SigTo> && std::is_same_v<SigFrom, typename AddNoExcept<SigTo>::Type>;

template <typename SigFrom, typename SigTo>
struct IsSignatureCompatible : std::false_type {};

template <typename Sig>
struct IsSignatureCompatible<Sig, Sig> : std::true_type {};

template <typename SigFrom, typename SigTo>
    requires(AnySignatureConvertibleToItsBase<SigFrom, SigTo>)
struct IsSignatureCompatible<SigFrom, SigTo> : std::true_type {};

// Any signature with noexcept maps to base signature (qualifier-less) with noexcept
template <typename SigFrom, typename SigTo>
    requires(AnySignatureConvertibleToItsBaseNoExcept<SigFrom, SigTo>)
struct IsSignatureCompatible<SigFrom, SigTo> : std::true_type {};

template <typename SigFrom, typename SigTo>
    requires(NoExceptMapsToThrowingSignature<SigFrom, SigTo>)
struct IsSignatureCompatible<SigFrom, SigTo> : std::true_type {};

template <typename SigFrom, typename SigTo>
    requires(!NoExceptMapsToThrowingSignature<SigFrom, SigTo> &&
             SignatureConstConvertible<SigFrom, SigTo>)
struct IsSignatureCompatible<SigFrom, SigTo> : std::true_type {};

template <typename SigFrom, typename SigTo>
    requires(!NoExceptMapsToThrowingSignature<SigFrom, SigTo> &&
             SignatureConstToNonConstConvertible<SigFrom, SigTo>)
struct IsSignatureCompatible<SigFrom, SigTo> : std::true_type {};

}  // namespace fn

#endif