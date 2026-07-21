#ifndef LAZY_BINARY_OPS_HPP
#define LAZY_BINARY_OPS_HPP

#include "lazy_core.hpp"

namespace lazy::detail{

// ============================== Binary Operation nodes ==============================

/**
 * @brief Expression node for addition: computes `lhs + rhs`.
 * @tparam T   The arithmetic value type.
 * @tparam L   The type of the left sub-expression (`isExpr<L,T>` required).
 * @tparam R   The type of the right sub-expression (`isExpr<R,T>` required).
 *
 * Uses `CustomBinaryRules<T>::evaluate(PLUS{}, out, a, b)` for the actual computation.
 * The default implementation calls the built-in `T::operator+`; specialise
 * `CustomBinaryRules<T>` to override.
 */
template<typename T, typename L, typename R>
struct Add : public BinaryOperator<Add<T, L, R>, T, L, R>, public CustomBinaryRules<T>{

    using Base = BinaryOperator<Add<T, L, R>, T, L, R>;
    static constexpr bool isAdd = true;
    using tag = PLUS;
    using Base::Base;

};

/**
 * @brief Expression node for multiplication: computes `lhs * rhs`.
 * @tparam T   The arithmetic value type.
 * @tparam L   Left sub-expression type.
 * @tparam R   Right sub-expression type.
 */
template<typename T, typename L, typename R>
struct Mul : public BinaryOperator<Mul<T, L, R>, T, L, R>, public CustomBinaryRules<T>{
    
    using Base = BinaryOperator<Mul<T, L, R>, T, L, R>;
    static constexpr bool isMul = true;
    using tag = MUL;
    using Base::Base;

};

/**
 * @brief Expression node for division: computes `lhs / rhs`.
 * @tparam T   The arithmetic value type.
 * @tparam L   Left (numerator) sub-expression type.
 * @tparam R   Right (denominator) sub-expression type.
 */
template<typename T, typename L, typename R>
struct Div : public BinaryOperator<Div<T, L, R>, T, L, R>, public CustomBinaryRules<T>{
    
    using Base = BinaryOperator<Div<T, L, R>, T, L, R>;
    static constexpr bool isDiv = true;
    using tag = DIV;
    using Base::Base;

};

/**
 * @brief Expression node for subtraction: computes `lhs - rhs`.
 * @tparam T   The arithmetic value type.
 * @tparam L   Left sub-expression type.
 * @tparam R   Right sub-expression type.
 */
template<typename T, typename L, typename R>
struct Sub : public BinaryOperator<Sub<T, L, R>, T, L, R>, public CustomBinaryRules<T>{
    using Base = BinaryOperator<Sub<T, L, R>, T, L, R>;
    static constexpr bool isSub = true;
    using tag = MINUS;
    using Base::Base;

};

/**
 * @brief Expression node for exponentiation: computes `pow(lhs, rhs)`.
 * @tparam T   The arithmetic value type.
 * @tparam L   Base sub-expression type.
 * @tparam R   Exponent sub-expression type.
 */
template<typename T, typename L, typename R>
struct Pow : public BinaryOperator<Pow<T, L, R>, T, L, R>, public CustomBinaryRules<T>{
    using Base = BinaryOperator<Pow<T, L, R>, T, L, R>;
    static constexpr bool isPow = true;
    using tag = POW;
    using Base::Base;

};

/**
 * @brief Expression node for element-wise maximum: computes `max(lhs, rhs)`.
 *
 * Named `MaxLazy` to avoid ambiguity with `std::max` and any user-defined `max`
 * functions.  Built via the `max(L&&, R&&)` free-function overload.
 *
 * @tparam T   The arithmetic value type.
 * @tparam L   Left sub-expression type.
 * @tparam R   Right sub-expression type.
 */
template<typename T, typename L, typename R>
struct MaxLazy : public BinaryOperator<MaxLazy<T, L, R>, T, L, R>, public CustomBinaryRules<T>{
    using Base = BinaryOperator<MaxLazy<T, L, R>, T, L, R>;
    using Base::Base;
    using tag = MAX;
};

/**
 * @brief Expression node for element-wise minimum: computes `min(lhs, rhs)`.
 *
 * Named `MinLazy` for the same reason as `MaxLazy`.
 *
 * @tparam T   The arithmetic value type.
 * @tparam L   Left sub-expression type.
 * @tparam R   Right sub-expression type.
 */
template<typename T, typename L, typename R>
struct MinLazy : public BinaryOperator<MinLazy<T, L, R>, T, L, R>, public CustomBinaryRules<T>{
    using Base = BinaryOperator<MinLazy<T, L, R>, T, L, R>;
    using Base::Base;
    using tag = MIN;

};







// ============================================================================
// Factory functions  —  build specific node types from arbitrary operands
// ============================================================================
//
// Each make_Xxx function calls make_expr<T>() on both operands to canonicalise them,
// then constructs the appropriate node.  Use these when you need a concrete node type
// at a specific value type (e.g. from template code that already knows T).  The
// operator overloads below call these internally.

/**
 * @brief Build an `Add<T,L,R>` node from two operands.
 * @tparam T   The arithmetic value type.
 * @tparam L   Left operand type (any value/expression).
 * @tparam R   Right operand type (any value/expression).
 * @return     `Add<T, expr_L, expr_R>` node capturing both canonicalised operands.
 */
template<typename T, typename L, typename R>
LAZY_FORCE_INLINE auto make_add(L&& lhs, R&& rhs){
    auto lhs_expr = make_expr<T>(std::forward<L>(lhs));
    auto rhs_expr = make_expr<T>(std::forward<R>(rhs));
    return Add<T, std::decay_t<decltype(lhs_expr)>, std::decay_t<decltype(rhs_expr)>>(std::move(lhs_expr), std::move(rhs_expr));
}

/**
 * @brief Build a `Mul<T,L,R>` node from two operands.
 * @tparam T   The arithmetic value type.
 * @tparam L   Left operand type.
 * @tparam R   Right operand type.
 */
template<typename T, typename L, typename R>
LAZY_FORCE_INLINE auto make_mul(L&& lhs, R&& rhs){
    auto lhs_expr = make_expr<T>(std::forward<L>(lhs));
    auto rhs_expr = make_expr<T>(std::forward<R>(rhs));
    return Mul<T, std::decay_t<decltype(lhs_expr)>, std::decay_t<decltype(rhs_expr)>>(std::move(lhs_expr), std::move(rhs_expr));
}

/**
 * @brief Build a `Div<T,L,R>` node from two operands.
 * @tparam T   The arithmetic value type.
 * @tparam L   Numerator operand type.
 * @tparam R   Denominator operand type.
 */
template<typename T, typename L, typename R>
LAZY_FORCE_INLINE auto make_div(L&& lhs, R&& rhs){
    auto lhs_expr = make_expr<T>(std::forward<L>(lhs));
    auto rhs_expr = make_expr<T>(std::forward<R>(rhs));
    return Div<T, std::decay_t<decltype(lhs_expr)>, std::decay_t<decltype(rhs_expr)>>(std::move(lhs_expr), std::move(rhs_expr));
}

/**
 * @brief Build a `Sub<T,L,R>` node from two operands.
 * @tparam T   The arithmetic value type.
 * @tparam L   Left operand type.
 * @tparam R   Right operand type.
 */
template<typename T, typename L, typename R>
LAZY_FORCE_INLINE auto make_sub(L&& lhs, R&& rhs){
    auto lhs_expr = make_expr<T>(std::forward<L>(lhs));
    auto rhs_expr = make_expr<T>(std::forward<R>(rhs));
    return Sub<T, std::decay_t<decltype(lhs_expr)>, std::decay_t<decltype(rhs_expr)>>(std::move(lhs_expr), std::move(rhs_expr));
}

/**
 * @brief Build a `Pow<T,L,R>` node from a base and an exponent.
 * @tparam T    The arithmetic value type.
 * @tparam L    Base operand type.
 * @tparam R    Exponent operand type.
 */
template<typename T, typename L, typename R>
LAZY_FORCE_INLINE auto make_pow(L&& lhs, R&& rhs){
    auto lhs_expr = make_expr<T>(std::forward<L>(lhs));
    auto rhs_expr = make_expr<T>(std::forward<R>(rhs));
    return Pow<T, std::decay_t<decltype(lhs_expr)>, std::decay_t<decltype(rhs_expr)>>(std::move(lhs_expr), std::move(rhs_expr));
}

/**
 * @brief Build a `Neg<T,Arg>` node from a single argument.
 * @tparam T   The arithmetic value type.
 * @tparam Arg Operand type.
 */
template<typename T, typename Arg>
LAZY_FORCE_INLINE auto make_neg(Arg&& arg){
    auto arg_expr = make_expr<T>(std::forward<Arg>(arg));
    return Neg<T, std::decay_t<decltype(arg_expr)>>(std::move(arg_expr));
}

//===================================== operator overloads=========================
//
// All arithmetic and relational operators are constrained with LAZY_REQUIREMENT so
// they only fire when at least one operand is an expression node.  This avoids
// hijacking built-in arithmetic for plain T values.

/// @brief Lazy addition: returns `Add<T,L,R>` when at least one operand is an expression.
template<typename L, typename R>
requires LAZY_REQUIREMENT(L, R)
LAZY_FORCE_INLINE auto operator+(L&& lhs, R&& rhs){
    return make_add<detail::MainTypeOf<L, R>>(std::forward<L>(lhs), std::forward<R>(rhs));
}

/// @brief Lazy multiplication: returns `Mul<T,L,R>` when at least one operand is an expression.
template<typename L, typename R>
requires LAZY_REQUIREMENT(L, R)
LAZY_FORCE_INLINE auto operator*(L&& lhs, R&& rhs){
    return make_mul<detail::MainTypeOf<L, R>>(std::forward<L>(lhs), std::forward<R>(rhs));
}

/// @brief Lazy division: returns `Div<T,L,R>` when at least one operand is an expression.
template<typename L, typename R>
requires LAZY_REQUIREMENT(L, R)
LAZY_FORCE_INLINE auto operator/(L&& lhs, R&& rhs){
    return make_div<detail::MainTypeOf<L, R>>(std::forward<L>(lhs), std::forward<R>(rhs));
}

/// @brief Lazy subtraction: returns `Sub<T,L,R>` when at least one operand is an expression.
template<typename L, typename R>
requires LAZY_REQUIREMENT(L, R)
LAZY_FORCE_INLINE auto operator-(L&& lhs, R&& rhs){
    return make_sub<detail::MainTypeOf<L, R>>(std::forward<L>(lhs), std::forward<R>(rhs));
}

/// @brief Lazy unary negation: returns `Neg<T,Arg>` for any expression argument.
template<typename Arg>
requires( requires {typename std::decay_t<Arg>::MainType;} && traits::isExpr<std::decay_t<Arg>, typename std::decay_t<Arg>::MainType> )
LAZY_FORCE_INLINE auto operator-(Arg&& arg){
    return make_neg<detail::MainTypeOf<Arg, void>>(std::forward<Arg>(arg));
}

/// @brief Lazy exponentiation: returns `Pow<T,Base,Exp>` when at least one operand is an expression.
template<typename Base, typename Exp>
requires LAZY_REQUIREMENT(Base, Exp)
LAZY_FORCE_INLINE auto pow(Base&& base, Exp&& exp){
    return make_pow<detail::MainTypeOf<Base, Exp>>(std::forward<Base>(base), std::forward<Exp>(exp));
}

/**
 * @brief Lazy maximum: returns a `MaxLazy<T,L,R>` node.
 *
 * Only participates in overload resolution when at least one argument is an expression.
 * For two concrete `T` values, `CustomBinaryRules<T>::evaluate(MAX{}, ...)` is called
 * during evaluation — specialise that to use e.g. `mpfr_max`.
 */
template<typename L, typename R>
requires LAZY_REQUIREMENT(L, R)
LAZY_FORCE_INLINE auto max(L&& lhs, R&& rhs){
    auto lhs_expr = make_expr<detail::MainTypeOf<L, R>>(std::forward<L>(lhs));
    auto rhs_expr = make_expr<detail::MainTypeOf<L, R>>(std::forward<R>(rhs));
    return MaxLazy<detail::MainTypeOf<L, R>, std::decay_t<decltype(lhs_expr)>, std::decay_t<decltype(rhs_expr)>>(std::move(lhs_expr), std::move(rhs_expr));
}

/**
 * @brief Lazy minimum: returns a `MinLazy<T,L,R>` node.
 *
 * Only participates in overload resolution when at least one argument is an expression.
 */
template<typename L, typename R>
requires LAZY_REQUIREMENT(L, R)
LAZY_FORCE_INLINE auto min(L&& lhs, R&& rhs){
    auto lhs_expr = make_expr<detail::MainTypeOf<L, R>>(std::forward<L>(lhs));
    auto rhs_expr = make_expr<detail::MainTypeOf<L, R>>(std::forward<R>(rhs));
    return MinLazy<detail::MainTypeOf<L, R>, std::decay_t<decltype(lhs_expr)>, std::decay_t<decltype(rhs_expr)>>(std::move(lhs_expr), std::move(rhs_expr));
}


} // namespace lazy::detail


namespace lazy{
    
using lazy::detail::operator+, 
      lazy::detail::operator-, 
      lazy::detail::operator*, 
      lazy::detail::operator/, 
      lazy::detail::pow, 
      lazy::detail::max, 
      lazy::detail::min;

} // namespace lazy


#endif // LAZY_BINARY_OPS_HPP