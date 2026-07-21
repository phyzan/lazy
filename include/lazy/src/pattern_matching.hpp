#ifndef LAZY_PATTERN_MATCHING_HPP
#define LAZY_PATTERN_MATCHING_HPP

#include "binary_ops.hpp"
#include "unary_ops.hpp"
#include "relational.hpp"


namespace lazy::detail{


// ============================================================================
// Pattern -> expression-type mapping
// ============================================================================
//
// HelperfromPattern<T, P> maps a symbolic pattern type P (from patterns.hpp) back to
// its corresponding expression node type for value type T.  This is used in
// LAZY_OVERRIDE_OPER to construct the parameter types of overridden eval_rule functions,
// enabling pattern-matching on the structure of sub-expressions.

/**
 * @brief Primary template: maps pattern `P` to `OtherType<T, P>` (unknown/leaf pattern).
 *
 * This is the fallback when `P` is not a recognised structural pattern.  It wraps `P`
 * as an `OtherType` atom, under the assumption that `P` is a raw scalar type.
 *
 * @tparam T The arithmetic value type.
 * @tparam P The pattern type to map.
 */
template<typename T, typename P>
struct HelperfromPattern{

    using Type = detail::OtherType<T, P>;
};

/// @brief Maps `T` itself (the main value type) to `RefType<T>` (a direct reference atom).
template<typename T>
struct HelperfromPattern<T, T>{

    using Type = detail::RefType<T>;
};

/// @brief Maps `Addition<L,R>` pattern to `Add<T, ...>` node (recursive).
template<typename T, typename L, typename R>
struct HelperfromPattern<T, Addition<L, R>>{
    using Type = detail::Add<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};


/// @brief Maps `Multiplication<L,R>` pattern to `Mul<T, ...>` node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, Multiplication<L, R>>{
    using Type = detail::Mul<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `Division<L,R>` pattern to `Div<T, ...>` node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, Division<L, R>>{
    using Type = detail::Div<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `Subtraction<L,R>` pattern to `Sub<T, ...>` node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, Subtraction<L, R>>{
    using Type = detail::Sub<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `Power<L,R>` pattern to `Pow<T, ...>` node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, Power<L, R>>{
    using Type = detail::Pow<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `Max<L,R>` pattern to `MaxLazy<T, ...>` node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, Max<L, R>>{
    using Type = detail::MaxLazy<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `Min<L,R>` pattern to `MinLazy<T, ...>` node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, Min<L, R>>{
    using Type = detail::MinLazy<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `Negation<Arg>` pattern to `Neg<T, ...>` node.
template<typename T, typename Arg>
struct HelperfromPattern<T, Negation<Arg>>{
    using Type = detail::Neg<T, typename HelperfromPattern<T, Arg>::Type>;
};

/// @brief Maps `AbsoluteValue<Arg>` pattern to `Abs<T, ...>` node.
template<typename T, typename Arg>
struct HelperfromPattern<T, AbsoluteValue<Arg>>{
    using Type = detail::Abs<T, typename HelperfromPattern<T, Arg>::Type>;
};

/// @brief Maps `SquareRoot<Arg>` pattern to `Sqrt<T, ...>` node.
template<typename T, typename Arg>
struct HelperfromPattern<T, SquareRoot<Arg>>{
    using Type = detail::Sqrt<T, typename HelperfromPattern<T, Arg>::Type>;
};

/// @brief Maps `Equal<L,R>` pattern to `Eq<T, ...>` comparison node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, Equal<L, R>>{
    using Type = Eq<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `GreaterThan<L,R>` pattern to `Gt<T, ...>` comparison node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, GreaterThan<L, R>>{
    using Type = Gt<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `LessThan<L,R>` pattern to `Lt<T, ...>` comparison node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, LessThan<L, R>>{
    using Type = Lt<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `NotEqual<L,R>` pattern to `Neq<T, ...>` comparison node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, NotEqual<L, R>>{
    using Type = Neq<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `GreaterEqual<L,R>` pattern to `Ge<T, ...>` comparison node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, GreaterEqual<L, R>>{
    using Type = Ge<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `LessEqual<L,R>` pattern to `Le<T, ...>` comparison node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, LessEqual<L, R>>{
    using Type = Le<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};


/**
 * @brief Convenience alias: map pattern `P` to the corresponding expression node type for `T`.
 *
 * `fromPattern<T, P>` is the expression node type that evaluates the operation described
 * by pattern `P` over values of type `T`.  Used as parameter types in `LAZY_OVERRIDE_OPER`:
 * @code
 *   LAZY_OVERRIDE_OPER(T, a, b, Addition<T,T>, PLUS, T) {
 *       // a is Add<T, RefType<T>, RefType<T>>, b is RefType<T>
 *       out = optimised_fused_add_then_op(a, b);
 *   }
 * @endcode
 *
 * @tparam T The arithmetic value type.
 * @tparam P The pattern type (from `patterns.hpp`).
 */
template<typename T, typename P>
using fromPattern = typename HelperfromPattern<T, P>::Type;

} // namespace lazy::detail


#endif // LAZY_PATTERN_MATCHING_HPP