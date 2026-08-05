#ifndef LAZY_PATTERN_MATCHING_HPP
#define LAZY_PATTERN_MATCHING_HPP


#include "../binary_ops/binops.hpp"
#include "../binary_ops/relational/relational.hpp"
#include "../unary_ops/unary_ops.hpp"


/**
 * @brief Override the `eval_rule` dispatch for a specific binary expression pattern.
 *
 * Generates a static member function signature inside a `LAZY_SPECIALIZE_OPERATIONS` block
 * that intercepts evaluation when the left-hand expression matches pattern `LEFT` and
 * the right-hand expression matches pattern `RIGHT`.
 *
 * @param T     The arithmetic value type.
 * @param a     Name for the left expression parameter.
 * @param b     Name for the right expression parameter.
 * @param LEFT  The pattern type of the left sub-expression (e.g. `Addition<T,T>`).
 * @param tag   The operation tag (e.g. `PLUS`).
 * @param RIGHT The pattern type of the right sub-expression.
 */
#define LAZY_OVERRIDE_OPER(T, a, b, LEFT, tag, RIGHT)\
LAZY_FORCE_INLINE static void eval_rule(tag, T& out, T* /**/, const fromPattern<T, LEFT>& a, const fromPattern<T, RIGHT>& b)


namespace lazy::detail{


// ============================================================================
// Pattern -> expression-type mapping
// ============================================================================


template<typename T, typename P>
struct HelperfromPattern{

    using Type = void;
};

template<typename T, lazy::traits::isConvertibleTo<T> P>
struct HelperfromPattern<T, P>{

    using Type = OtherType<T, P>;
};

/// @brief Maps `T` itself (the main value type) to `RefType<T>` (a direct reference atom).
template<typename T>
struct HelperfromPattern<T, T>{

    using Type = RefType<T>;
};

/// @brief Maps `Addition<L,R>` pattern to `Add<T, ...>` node (recursive).
template<typename T, typename L, typename R>
struct HelperfromPattern<T, lazy::patterns::Addition<L, R>>{
    using Type = Add<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};


/// @brief Maps `Multiplication<L,R>` pattern to `Mul<T, ...>` node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, lazy::patterns::Multiplication<L, R>>{
    using Type = Mul<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `Division<L,R>` pattern to `Div<T, ...>` node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, lazy::patterns::Division<L, R>>{
    using Type = Div<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `Subtraction<L,R>` pattern to `Sub<T, ...>` node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, lazy::patterns::Subtraction<L, R>>{
    using Type = Sub<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `Power<L,R>` pattern to `Pow<T, ...>` node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, lazy::patterns::Power<L, R>>{
    using Type = Pow<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `Max<L,R>` pattern to `MaxLazy<T, ...>` node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, lazy::patterns::Max<L, R>>{
    using Type = MaxLazy<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `Min<L,R>` pattern to `MinLazy<T, ...>` node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, lazy::patterns::Min<L, R>>{
    using Type = MinLazy<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `Negation<Arg>` pattern to `Neg<T, ...>` node.
template<typename T, typename Arg>
struct HelperfromPattern<T, lazy::patterns::Negation<Arg>>{
    using Type = Neg<T, typename HelperfromPattern<T, Arg>::Type>;
};

/// @brief Maps `AbsoluteValue<Arg>` pattern to `Abs<T, ...>` node.
template<typename T, typename Arg>
struct HelperfromPattern<T, lazy::patterns::AbsoluteValue<Arg>>{
    using Type = Abs<T, typename HelperfromPattern<T, Arg>::Type>;
};

/// @brief Maps `SquareRoot<Arg>` pattern to `Sqrt<T, ...>` node.
template<typename T, typename Arg>
struct HelperfromPattern<T, lazy::patterns::SquareRoot<Arg>>{
    using Type = Sqrt<T, typename HelperfromPattern<T, Arg>::Type>;
};

/// @brief Maps `Equal<L,R>` pattern to `Eq<T, ...>` comparison node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, lazy::patterns::Equal<L, R>>{
    using Type = Eq<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `GreaterThan<L,R>` pattern to `Gt<T, ...>` comparison node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, lazy::patterns::GreaterThan<L, R>>{
    using Type = Gt<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `LessThan<L,R>` pattern to `Lt<T, ...>` comparison node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, lazy::patterns::LessThan<L, R>>{
    using Type = Lt<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `NotEqual<L,R>` pattern to `Neq<T, ...>` comparison node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, lazy::patterns::NotEqual<L, R>>{
    using Type = Neq<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `GreaterEqual<L,R>` pattern to `Ge<T, ...>` comparison node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, lazy::patterns::GreaterEqual<L, R>>{
    using Type = Ge<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `LessEqual<L,R>` pattern to `Le<T, ...>` comparison node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, lazy::patterns::LessEqual<L, R>>{
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