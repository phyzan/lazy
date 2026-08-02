#ifndef LAZY_UNARY_DECLS_HPP
#define LAZY_UNARY_DECLS_HPP

#include "../rules.hpp"

/**
 * @brief Declare a new unary function node type and its overloaded free function.
 *
 * Expands to:
 * 1. A struct `OP<T, Arg>` inheriting `Unary<OP<T,Arg>, T, Arg>` and
 *    `CustomUnaryRules<T>`, with `tag = TAG`
 * 2. A free function `FUNC(U&&)` constrained to any expression type, returning
 *    OP<detail::value_type<U>, std::decay_t<U>>(std::forward<U>(arg))`.
 *
 * @param FUNC     The function name (e.g. `abs`, `sqrt`).
 * @param OP       The node struct name (e.g. `Abs`, `Sqrt`).
 * @param TAG      The dispatch tag type (e.g. `ABS`, `SQRT`).
 */
#define LAZY_DEFINE_UNARY_OP(FUNC, OP, TAG)                             \
template<typename T, typename Arg>                                              \
struct OP : public Unary<OP<T, Arg>, T, Arg>, public CustomUnaryRules<T> {                \
    using Base = Unary<OP<T, Arg>, T, Arg>;                                     \
    static constexpr bool is##OP = true;                                        \
    using tag = TAG;                                                             \
};                                                                              \
                                                                                \
\
template<typename U>                                                            \
requires (                                                                      \
    requires { typename std::decay_t<U>::value_type; } &&                         \
    traits::isLazyExpr<std::decay_t<U>, typename std::decay_t<U>::value_type>                 \
)                                                                               \
LAZY_FORCE_INLINE auto FUNC(U&& arg) {                                               \
    using T = detail::value_type<U>;                                              \
    auto arg_expr = make_expr<T>(std::forward<U>(arg));                         \
    return OP<T, std::decay_t<decltype(arg_expr)>>(std::move(arg_expr));        \
}

/**
 * @brief Open a specialisation block for unary function rules for type `Type`.
 *
 * Usage:
 * @code
 *   LAZY_SPECIALIZE_FUNCTIONS(MyType) {
 *       using T = MyType;
 *       using Base = UnaryOpRules<CustomUnaryRules<T>, T>;
 *       using Base::evaluate; using Base::eval_rule;
 *       LAZY_EVALUATE_FUNC(T, a, NEG, T) { out = my_neg(a); }
 *   };
 * @endcode
 *
 * @param Type The arithmetic type to specialise for.
 */
#define LAZY_SPECIALIZE_FUNCTIONS(Type)\
template<>\
struct CustomUnaryRules<Type> : public UnaryOpRules<CustomUnaryRules<Type>, Type>

/**
 * @brief Declare an `evaluate` overload for a specific unary function and argument type.
 *
 * Generates a `static LAZY_FORCE_INLINE void evaluate(tag, T& out, const ARG& arg)`
 * declaration inside a `LAZY_SPECIALIZE_FUNCTIONS` block.
 *
 * @param T    The arithmetic value type.
 * @param arg  Name for the argument parameter.
 * @param tag  The operation tag type (e.g. `ABS`, `SQRT`, `NEG`).
 * @param ARG  The C++ type of the argument (typically `T`).
 */
#define LAZY_EVALUATE_FUNC(T, arg, tag, ARG)\
LAZY_FORCE_INLINE static void evaluate(tag, T& out, const ARG& arg)


namespace lazy::tags{

// ====================== UNARY FUNCTION TAGS ===========================

/// @brief Tag for unary negation `-x`.
struct NEG : public Tag{};
/// @brief Tag for `abs(x)`.
struct ABS : public Tag{};
/// @brief Tag for `sqrt(x)`.
struct SQRT : public Tag{};
/// @brief Tag for `exp(x)`.
struct EXP : public Tag{};
/// @brief Tag for `log(x)`.
struct LOG : public Tag{};
/// @brief Tag for `sin(x)`.
struct SIN : public Tag{};
/// @brief Tag for `cos(x)`.
struct COS : public Tag{};
/// @brief Tag for `tan(x)`.
struct TAN : public Tag{};
/// @brief Tag for `cot(x)`.
struct COT : public Tag{};
/// @brief Tag for `sec(x)`.
struct SEC : public Tag{};
/// @brief Tag for `csc(x)`.
struct CSC : public Tag{};
/// @brief Tag for `asin(x)`.
struct ASIN : public Tag{};
/// @brief Tag for `acos(x)`.
struct ACOS : public Tag{};
/// @brief Tag for `atan(x)`.
struct ATAN : public Tag{};
/// @brief Tag for `acot(x)`.
struct ACOT : public Tag{};
/// @brief Tag for `asec(x)`.
struct ASEC : public Tag{};
/// @brief Tag for `acsc(x)`.
struct ACSC : public Tag{};
/// @brief Tag for `sinh(x)`.
struct SINH : public Tag{};
/// @brief Tag for `cosh(x)`.
struct COSH : public Tag{};
/// @brief Tag for `tanh(x)`.
struct TANH : public Tag{};
/// @brief Tag for `erf(x)`.
struct ERF : public Tag{};

} // namespace lazy::tags

namespace lazy::detail{



template<typename Derived, typename T, typename Arg> struct Unary;
template<typename T, typename Arg>
struct Neg;
template<typename T, typename Arg>
struct Abs;
template<typename T, typename Arg>
struct Sqrt;
template<typename T, typename Arg>
struct Exp;
template<typename T, typename Arg>
struct Log;
template<typename T, typename Arg>
struct Sin;
template<typename T, typename Arg>
struct Cos;
template<typename T, typename Arg>
struct Tan;
template<typename T, typename Arg>
struct Cot;
template<typename T, typename Arg>
struct Sec;
template<typename T, typename Arg>
struct Csc;
template<typename T, typename Arg>
struct Asin;
template<typename T, typename Arg>
struct Acos;
template<typename T, typename Arg>
struct Atan;
template<typename T, typename Arg>
struct Acot;
template<typename T, typename Arg>
struct Asec;
template<typename T, typename Arg>
struct Acsc;
template<typename T, typename Arg>
struct Sinh;
template<typename T, typename Arg>
struct Cosh;
template<typename T, typename Arg>
struct Tanh;
template<typename T, typename Arg>
struct Erf;




// Unary type getter
template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::NEG, Arg>{
    using type = lazy::detail::Neg<T, Arg>;
};

template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::ABS, Arg>{
    using type = lazy::detail::Abs<T, Arg>;
};

template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::SQRT, Arg>{
    using type = lazy::detail::Sqrt<T, Arg>;
};

template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::EXP, Arg>{
    using type = lazy::detail::Exp<T, Arg>;
};

template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::LOG, Arg>{
    using type = lazy::detail::Log<T, Arg>;
};

template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::SIN, Arg>{
    using type = lazy::detail::Sin<T, Arg>;
};

template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::COS, Arg>{
    using type = lazy::detail::Cos<T, Arg>;
};

template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::TAN, Arg>{
    using type = lazy::detail::Tan<T, Arg>;
};

template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::COT, Arg>{
    using type = lazy::detail::Cot<T, Arg>;
};

template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::SEC, Arg>{
    using type = lazy::detail::Sec<T, Arg>;
};

template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::CSC, Arg>{
    using type = lazy::detail::Csc<T, Arg>;
};

template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::ASIN, Arg>{
    using type = lazy::detail::Asin<T, Arg>;
};

template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::ACOS, Arg>{
    using type = lazy::detail::Acos<T, Arg>;
};

template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::ATAN, Arg>{
    using type = lazy::detail::Atan<T, Arg>;
};

template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::ACOT, Arg>{
    using type = lazy::detail::Acot<T, Arg>;
};

template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::ASEC, Arg>{
    using type = lazy::detail::Asec<T, Arg>;
};

template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::ACSC, Arg>{
    using type = lazy::detail::Acsc<T, Arg>;
};

template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::SINH, Arg>{
    using type = lazy::detail::Sinh<T, Arg>;
};

template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::COSH, Arg>{
    using type = lazy::detail::Cosh<T, Arg>;
};

template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::TANH, Arg>{
    using type = lazy::detail::Tanh<T, Arg>;
};

template<typename T, typename Arg>
struct TypeGetter<T, lazy::tags::ERF, Arg>{
    using type = lazy::detail::Erf<T, Arg>;
};

    
} // namespace lazy::detail

#endif // LAZY_UNARY_DECLS_HPP