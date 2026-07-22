#ifndef LAZY_DECLS_HPP
#define LAZY_DECLS_HPP


/**
 * @file lazy.hpp
 * @brief Core expression-template library for lazy arithmetic evaluation.
 *
 * This header implements a compile-time expression tree that defers numeric
 * computation until explicitly requested, enabling:
 *
 * - **Zero-overhead abstraction** — arithmetic operators on `LazyType<T>` or any
 *   other expression node return lightweight node objects; no floating-point work is
 *   done until `eval()` is called (or the node is implicitly converted to `T`).
 * - **Custom evaluation rules** — specialise `CustomBinaryRules<T>` and
 *   `CustomUnaryRules<T>` to override how specific operations are performed for type
 *   `T` (e.g. use MPFR intrinsics instead of `operator+`).
 * - **Thread-safe temporary storage** — each unique expression type gets its own
 *   thread-local scratch allocation via `RuleTree<T,…>`.
 *
 * ### Typical usage
 * @code
 *   #include "lazy.hpp"
 *   using namespace lazy;
 *
 *   LazyType<double> x = 3.0, y = 4.0;
 *   auto expr = x * x + y * y;   // builds a node tree, no arithmetic yet
 *   double r;  expr.eval(r);      // r == 25.0
 *   double s = expr;              // implicit conversion also evaluates
 * @endcode
 *
 * ### Specialising operations for a custom type `Foo`
 * @code
 *   LAZY_SPECIALIZE_OPERATIONS(Foo) {
 *       using T = Foo;
 *       using Base = BinaryOpRules<CustomBinaryRules<Foo>, Foo>;
 *       using Base::evaluate;
 *       LAZY_EVALUATE_OPER(T, a, b, T, PLUS, T) { out = fast_add(a, b); }
 *   };
 * @endcode
 *
 * All types live in the `lazy` namespace.  `patterns.hpp` is a prerequisite.
 */

#include <vector>
#include "patterns.hpp"


// ============================================================================
// Utility macros
// ============================================================================


#define LAZY_THIS static_cast<std::conditional_t<std::is_void_v<Derived>, \
    std::remove_reference_t<decltype(*this)>, \
    lazy::detail::copy_const_t<std::remove_reference_t<decltype(*this)>, Derived>>*>(this)

#define LAZY_FORCE_INLINE __attribute__((always_inline)) inline


// ============================================================================
// Expression-requirement macros
// ============================================================================

/**
 * @brief Constraint: at least one of `L`, `R` is a lazy expression type. The other type may be a lazy expression or a raw value convertible to `T`.
 *
 * Expands to an `&&`-expression that is `true` when `std::decay_t<L>` or
 * `std::decay_t<R>` (or both) expose a `MainType` alias and derive from
 * `ExprBase<MainType>`.  Used as a `requires` clause on all arithmetic and
 * relational operator overloads to ensure they only activate for expression
 * operands and do not shadow built-in arithmetic.
 */
#define LAZY_REQUIREMENT(L, R)\
    ( (requires {typename std::decay_t<L>::MainType;} && lazy::traits::isExpr<std::decay_t<L>, typename std::decay_t<L>::MainType> && lazy::traits::isValidType<std::decay_t<R>, typename std::decay_t<L>::MainType>) || \
      (requires {typename std::decay_t<R>::MainType;} && lazy::traits::isExpr<std::decay_t<R>, typename std::decay_t<R>::MainType> && lazy::traits::isValidType<std::decay_t<L>, typename std::decay_t<R>::MainType>) )


/**
 * @brief Generates a relational operator that produces a lazy `Comparison` node.
 *
 * Expands to a template `operator OP(L&&, R&&)` constrained by `LAZY_REQUIREMENT`.
 * It calls `make_expr<T>()` on both operands to canonicalise them (promoting raw
 * values to `RefType` or `OtherType` atoms), deduces the node types from the
 * resulting expressions, and returns `ClassName<T, LExprType, RExprType>`.
 *
 * @param OP        The operator symbol (e.g. `==`, `>`).
 * @param ClassName The concrete `Comparison`-derived type (e.g. `Eq`, `Gt`).
 */
#define LAZY_DEFINE_RELATIONAL_OP(OP, ClassName)\
template<typename L, typename R>\
requires LAZY_REQUIREMENT(L, R)\
LAZY_FORCE_INLINE auto operator OP(L&& lhs, R&& rhs){\
    using T = lazy::detail::MainTypeOf<L, R>;\
    auto lhs_expr = make_expr<T>(std::forward<L>(lhs));\
    auto rhs_expr = make_expr<T>(std::forward<R>(rhs));\
    return ClassName<T, std::decay_t<decltype(lhs_expr)>, std::decay_t<decltype(rhs_expr)>>(std::move(lhs_expr), std::move(rhs_expr));\
}


#define LAZY_DEFINE_ALLOWED_IMPLICIT_CONVERSION(T, ...) \
template<> \
struct lazy::ValidTypes<T> { \
    using type = lazy::detail::TypeList<__VA_ARGS__>; \
};



/**
 * @brief Specialise `std::numeric_limits` for `LazyType<TYPE>` by inheriting from
 *        `std::numeric_limits<TYPE>`.
 *
 * Without this, `std::numeric_limits<LazyType<double>>` is empty (all members zero /
 * false), which can break generic numerical code that queries limits.  Call this macro
 * once per numeric type `TYPE` at namespace scope, typically just below the include of
 * this header.
 *
 * @param TYPE  The underlying arithmetic type (e.g. `double`, `mpfr::mpreal`).
 */
#define LAZY_DECLARE_NUMERIC_TYPE(TYPE) \
namespace std {\
template<>\
class numeric_limits<lazy::detail::LazyType<TYPE>> : public numeric_limits<TYPE>{};\
} \
template<typename F> \
constexpr bool lazy::traits::lazyConvertCondition<F, TYPE> = std::is_arithmetic_v<std::decay_t<F>>;

/**
 * @brief Declare a new unary function node type and its overloaded free function.
 *
 * Expands to:
 * 1. A struct `OP<T, Arg>` inheriting `Unary<OP<T,Arg>, T, Arg>` and
 *    `CustomUnaryRules<T>`, with `tag = TAG`
 * 2. A free function `FUNC(U&&)` constrained to any expression type, returning
 *    OP<detail::MainType<U>, std::decay_t<U>>(std::forward<U>(arg))`.
 *
 * @param FUNC     The function name (e.g. `abs`, `sqrt`).
 * @param OP       The node struct name (e.g. `Abs`, `Sqrt`).
 * @param TAG      The dispatch tag type (e.g. `ABS`, `SQRT`).
 * @param PATTERN  The pattern template (e.g. `AbsoluteValue`, `SquareRoot`).
 */
#define LAZY_DEFINE_UNARY_OP(FUNC, OP, TAG, PATTERN)                             \
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
    requires { typename std::decay_t<U>::MainType; } &&                         \
    traits::isExpr<std::decay_t<U>, typename std::decay_t<U>::MainType>                 \
)                                                                               \
LAZY_FORCE_INLINE auto FUNC(U&& arg) {                                               \
    using T = detail::MainType<U>;                                              \
    return OP<T, std::decay_t<U>>(std::forward<U>(arg));                        \
}


/**
 * @brief Open a specialisation block for binary operation rules for type `Type`.
 *
 * Usage:
 * @code
 *   LAZY_SPECIALIZE_OPERATIONS(MyType) {
 *       using T = MyType;
 *       using Base = BinaryOpRules<CustomBinaryRules<T>, T>;
 *       using Base::evaluate; using Base::eval_rule;
 *       LAZY_EVALUATE_OPER(T, a, b, T, PLUS, T) { out = my_add(a, b); }
 *       // ...
 *   };
 * @endcode
 *
 * @param Type The arithmetic type to specialise for.
 */
#define LAZY_SPECIALIZE_OPERATIONS(Type)\
template<>\
struct CustomBinaryRules<Type> : public BinaryOpRules<CustomBinaryRules<Type>, Type>

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
LAZY_FORCE_INLINE static void eval_rule(tag, T& out, const fromPattern<T, LEFT>& a, const fromPattern<T, RIGHT>& b)

/**
 * @brief Declare an `evaluate` overload for a specific binary operation and operand types.
 *
 * Generates a `static LAZY_FORCE_INLINE void evaluate(tag, T& out, const LEFT& a, const RIGHT& b)`
 * declaration inside a `LAZY_SPECIALIZE_OPERATIONS` block.  The body should follow immediately.
 *
 * @param T     The arithmetic value type.
 * @param a     Name for the left value parameter.
 * @param b     Name for the right value parameter.
 * @param LEFT  The C++ type of the left operand (e.g. `T`, `double`, `int`).
 * @param tag   The operation tag type (e.g. `PLUS`, `MUL`).
 * @param RIGHT The C++ type of the right operand.
 */
#define LAZY_EVALUATE_OPER(T, a, b, LEFT, tag, RIGHT)\
LAZY_FORCE_INLINE static void evaluate(tag, T& out, const LEFT& a, const RIGHT& b)

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



namespace lazy::detail {


// ============================================================================
// Forward declarations
// ============================================================================

/**
 * @brief Satisfied when `Derived` (after decay) derives from `ExprBase<T>`.
 *
 * This is the root concept for the entire expression-template hierarchy.  Any type
 * that participates in lazy arithmetic must satisfy this concept for the same `T`
 * that its `MainType` alias names.
 *
 * @tparam Derived The candidate expression type (references and cv-qualifiers are stripped).
 * @tparam T       The underlying arithmetic value type (e.g. `double`, `mpfr::mpreal`).
 */



// ============================================================================
// MainType trait  —  extracts the value type from an expression
// ============================================================================

/**
 * @brief Primary trait: extract the `MainType` alias from an expression type `T`.
 *
 * Defaults to `void` when `T` does not have a nested `::MainType` alias.  The
 * partial specialisation below overrides this for all expression types.
 *
 * @tparam T Candidate type (may or may not be an expression).
 */
template<typename T>
struct MainTypeTrait {
    using Type = void;
};

/// @brief Specialisation for types that do expose `::MainType`.
template<typename T>
requires (requires {typename std::decay_t<T>::MainType;})
struct MainTypeTrait<T> {
    using Type = typename std::decay_t<T>::MainType;
};

/// @brief Convenience alias: `MainType<E>` == `MainTypeTrait<E>::Type`.
template<typename E>
using MainType = typename MainTypeTrait<E>::Type;


/**
 * @brief Derives the common `MainType` from a pair of operand types.
 *
 * Rules:
 * - If both `A` and `B` expose a `MainType` and they are the same, the result is that type.
 * - If only one side has a `MainType` (the other is `void`, e.g. a raw scalar), that
 *   side's `MainType` is used.
 * - It is ill-formed for **both** to have `void` `MainType` (neither would be an expression).
 *
 * @tparam A Left operand type.
 * @tparam B Right operand type.
 */
template<typename A, typename B>
requires ((std::is_same_v<MainType<A>, MainType<B>> || std::is_same_v<MainType<A>, void> || std::is_same_v<MainType<B>, void>) && !(std::is_same_v<MainType<A>, void> && std::is_same_v<MainType<B>, void>))
using MainTypeOf = std::conditional_t<std::is_same_v<MainType<A>, void>, MainType<B>, MainType<A>>;




template<typename T> struct ExprBase;
template<typename Derived, typename T> struct Expr;
template<typename Derived, typename T> struct Atom;
template<typename Derived, typename T, size_t Branches> struct Node;
template<typename Derived, typename T, typename L, typename R> struct BinaryOperator;
template<typename T, typename L, typename R> struct Add;
template<typename T, typename L, typename R> struct Sub;
template<typename T, typename L, typename R> struct Mul;
template<typename T, typename L, typename R> struct Div;
template<typename T, typename L, typename R> struct Pow;
template<typename Derived, typename T, typename Arg> struct Unary;
template<typename T, typename Arg> struct Neg;
template<typename T, typename Arg> struct Abs;
template<typename T> struct RefType;
template<typename T, typename Type> struct OtherType;
template<typename T> struct LazyType;


// ============================================================================
// Operation tag types
// ============================================================================
// Tags are empty structs used as the first argument to `evaluate` / `eval_rule`
// to discriminate which arithmetic operation is being requested.  The hierarchy
// is: Tag <- specific arithmetic/comparison tags.
//            BOOL_TAG <- comparison-specific tags.

/// @brief Root tag base.  All operation tags inherit from this.
struct Tag{};
/// @brief Tag for addition `+`.
struct PLUS : public Tag{};
/// @brief Tag for subtraction `-`.
struct MINUS : public Tag{};
/// @brief Tag for multiplication `*`.
struct MUL : public Tag{};
/// @brief Tag for division `/`.
struct DIV : public Tag{};
/// @brief Tag for exponentiation `pow`.
struct POW : public Tag{};
/// @brief Tag for `max(x,y)`.
struct MAX : public Tag{};
/// @brief Tag for `min(x,y)`.
struct MIN : public Tag{};

/// @brief Tag for unary negation `-x`.
struct NEG : public Tag{};
/// @brief Tag for `abs(x)`.
struct ABS : public Tag{};
/// @brief Tag for `sqrt(x)`.
struct SQRT : public Tag{};

/// @brief Base tag for all boolean/comparison operations.
struct BOOL_TAG : public Tag{};

/// @brief Tag for equality comparison `==`.
struct EQ : public BOOL_TAG{};
/// @brief Tag for inequality comparison `!=`.
struct NEQ : public BOOL_TAG{};
/// @brief Tag for greater-than comparison `>`.
struct GT : public BOOL_TAG{};
/// @brief Tag for less-than comparison `<`.
struct LT : public BOOL_TAG{};
/// @brief Tag for greater-or-equal comparison `>=`.
struct GE : public BOOL_TAG{};
/// @brief Tag for less-or-equal comparison `<=`.
struct LE : public BOOL_TAG{};



template<typename T, typename R>
LAZY_FORCE_INLINE decltype(auto) make_expr(R&& value);


template<typename From, typename To>
using copy_const_t = std::conditional_t<std::is_const_v<From>, const To, To>;

} // namespace lazy::detail





#endif // LAZY_DECLS_HPP