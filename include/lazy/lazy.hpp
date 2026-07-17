#ifndef LAZY_EVAL_HPP
#define LAZY_EVAL_HPP

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

#include <string>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <limits>
#include <complex>
#include <algorithm>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <chrono>
#include "patterns.hpp"


// ============================================================================
// Utility macros
// ============================================================================

/// @brief Generates a unique identifier by pasting @p base with the current `__COUNTER__`.
#define LAZY_UNIQUE_NAME(base) LAZY_CONCAT(base, __COUNTER__)
/// @cond INTERNAL
#define LAZY_CONCAT(a,b) LAZY_CONCAT_IMPL(a,b)
#define LAZY_CONCAT_IMPL(a,b) a##b
/// @endcond

/**
 * @brief Expands to `std::integer_sequence<IntType, I...>` for use in pack expansions.
 * @param IntType The integer type (e.g. `size_t`, `int`).
 * @param I       The pack of integer values.
 */
#define LAZY_INTS(IntType, I) std::integer_sequence<IntType, I...>

/**
 * @brief Expands to `std::make_integer_sequence<IntType, N>{}` — a zero-based index pack.
 * @param IntType The integer type.
 * @param N       The upper bound (exclusive).
 */
#define LAZY_MAKE_INTS(IntType, N) std::make_integer_sequence<IntType, N>{}

/**
 * @brief Expands a lambda that iterates over `[0, N)` with an integer pack named `I`.
 *
 * The lambda is immediately invoked, enabling pack-expansion idioms without a helper
 * function.  Marked `always_inline` and `flatten` for zero-overhead expansion.
 *
 * @param IntType The type of the index (e.g. `size_t`).
 * @param N       Number of iterations.
 * @param I       Name of the integer pack inside the lambda body.
 * @param ...     Body of the lambda.
 */
#define LAZY_EXPAND(IntType, N, I, ...) [&]<IntType... I>(LAZY_INTS(IntType, I)) \
    __attribute__((always_inline, flatten)) { \
    __VA_ARGS__ \
}(LAZY_MAKE_INTS(IntType, N))

/**
 * @brief Marks a function as always-inlined.
 *
 * Equivalent to `inline __attribute__((always_inline))`.  Applied to all hot paths in
 * the expression-tree evaluation to ensure no call overhead remains after optimisation.
 */
#define LAZY_FORCE_INLINE inline __attribute__((always_inline))

/**
 * @brief CRTP helper: casts `this` to `Derived*`, preserving const-ness of the
 *        current object.
 *
 * Used inside `ExprBase`/`Expr`/`Atom`/`Node` members to forward calls down to the
 * most-derived type without a virtual dispatch.  `copy_const_t` ensures that a
 * `const`-qualified `*this` produces a `const Derived*`.
 */
#define LAZY_THIS static_cast<copy_const_t<std::remove_reference_t<decltype(*this)>, Derived>*>(this)



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
    ( (requires {typename std::decay_t<L>::MainType;} && isExpr<std::decay_t<L>, typename std::decay_t<L>::MainType> && (isExpr<std::decay_t<R>, typename std::decay_t<L>::MainType> || std::is_convertible_v<std::decay_t<R>, typename std::decay_t<L>::MainType>)) || \
      (requires {typename std::decay_t<R>::MainType;} && isExpr<std::decay_t<R>, typename std::decay_t<R>::MainType> && (isExpr<std::decay_t<L>, typename std::decay_t<R>::MainType> || std::is_convertible_v<std::decay_t<L>, typename std::decay_t<R>::MainType>)) )


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
    using T = MainTypeOf<L, R>;\
    auto lhs_expr = make_expr<T>(std::forward<L>(lhs));\
    auto rhs_expr = make_expr<T>(std::forward<R>(rhs));\
    return ClassName<T, std::decay_t<decltype(lhs_expr)>, std::decay_t<decltype(rhs_expr)>>(std::move(lhs_expr), std::move(rhs_expr));\
}

/**
 * @brief Copies the `const` qualifier from `From` onto `To`.
 *
 * Used by the `LAZY_THIS` macro so that CRTP casts into `Derived*` respect const-ness:
 * if the CRTP base method is `const`-qualified, the downcast produces `const Derived*`.
 *
 * @tparam From The type whose const-ness is copied.
 * @tparam To   The type to apply that const-ness to.
 */
template<typename From, typename To>
using copy_const_t = std::conditional_t<std::is_const_v<From>, const To, To>;


/**
 * @namespace lazy
 * @brief Namespace for all expression-tree types, concepts, and function templates.
 *
 * Everything defined in `lazy.hpp` and `patterns.hpp` lives here.  Import with
 * `using namespace lazy;` for convenience, or qualify explicitly.
 */
namespace lazy{

// ============================================================================
// Forward declarations
// ============================================================================
/// @cond FORWARD_DECLS
template<typename T> struct ExprBase;

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
template<typename Derived, typename T>
concept isExpr = std::is_base_of_v<ExprBase<T>, std::decay_t<Derived>>;


template<typename Derived, typename T> struct Expr;
template<typename Derived, typename T> struct Atom;
template<typename Derived, typename T, size_t Branches> struct Node;
template<typename Derived, typename T, typename L, typename R> struct BinaryOperator;
template<typename T, typename L, typename R> struct Add;
template<typename T, typename L, typename R> struct Sub;
template<typename T, typename L, typename R> struct Mul;
template<typename T, typename L, typename R> struct Div;
template<typename T, typename L, typename R> struct Pow;
template<typename Derived, typename T, isExpr<T> Arg> struct Unary;
template<typename T, typename Arg> struct Neg;
template<typename T, typename Arg> struct Abs;
template<typename T> struct RefType;
template<typename T> struct LazyType;
template<typename T, typename Type> struct OtherType;

template<typename T, typename R>
LAZY_FORCE_INLINE decltype(auto) make_expr(R&& value);
/// @endcond

// ============================================================================
// Concepts
// ============================================================================
/**
 * @brief Satisfied when `Derived` is a composed (unevaluated) `Node`.
 *
 * A `Node` has a static `Nbranches` member encoding how many sub-expressions it holds.
 * In contrast to `isAtom`, nodes require evaluation (calling `eval()`) before their
 * numeric value is available.  The canonical examples are `Add`, `Mul`, `Neg`, etc.
 *
 * @tparam Derived The candidate type (decay applied).
 * @tparam T       The arithmetic value type.
 */
template<typename Derived, typename T>
concept isNode = requires { std::decay_t<Derived>::Nbranches; } && std::is_base_of_v< Node<std::decay_t<Derived>, T, std::decay_t<Derived>::Nbranches>, std::decay_t<Derived>>;



/**
 * @brief Satisfied when `Derived` is a leaf (atom) in the expression tree.
 *
 * Atoms wrap an existing value rather than computing one.  The value is immediately
 * accessible via `value()` without any temporary storage or evaluation steps.
 * Concrete atom types include `RefType<T>`, `LazyType<T>`, and `OtherType<T,Type>`.
 *
 * @tparam Derived The candidate type (decay applied).
 * @tparam T       The arithmetic value type.
 */
template<typename Derived, typename T>
concept isAtom =std::is_base_of_v<Atom<std::decay_t<Derived>, T>, std::decay_t<Derived>>;

/**
 * @brief Satisfied when `Derived` is a `BinaryOperator` with deducible `LhsType`/`RhsType`.
 * @tparam Derived The candidate type.
 * @tparam T       The arithmetic value type.
 */
template<typename Derived, typename T>
concept isBinOp = requires
    {typename Derived::LhsType; typename Derived::RhsType;} &&
    std::is_base_of_v<BinaryOperator<Derived, T, typename Derived::LhsType, typename Derived::RhsType>, Derived>;

/// @brief Refinement of `isBinOp`: the node is specifically an `Add<T,L,R>`.
template<typename Derived, typename T>
concept isAdd = requires { requires isBinOp<Derived, T>; } && std::is_base_of_v<Add<T, typename Derived::LhsType, typename Derived::RhsType>, Derived>;

/// @brief Refinement of `isBinOp`: the node is specifically a `Sub<T,L,R>`.
template<typename Derived, typename T>
concept isSub = requires { requires isBinOp<Derived, T>; } && std::is_base_of_v<Sub<T, typename Derived::LhsType, typename Derived::RhsType>, Derived>;

/// @brief Refinement of `isBinOp`: the node is specifically a `Mul<T,L,R>`.
template<typename Derived, typename T>
concept isMul = requires { requires isBinOp<Derived, T>; } && std::is_base_of_v<Mul<T, typename Derived::LhsType, typename Derived::RhsType>, Derived>;

/// @brief Refinement of `isBinOp`: the node is specifically a `Div<T,L,R>`.
template<typename Derived, typename T>
concept isDiv = requires { requires isBinOp<Derived, T>; } && std::is_base_of_v<Div<T, typename Derived::LhsType, typename Derived::RhsType>, Derived>;

/// @brief Refinement of `isBinOp`: the node is specifically a `Pow<T,L,R>`.
template<typename Derived, typename T>
concept isPow = requires { requires isBinOp<Derived, T>; } && std::is_base_of_v<Pow<T, typename Derived::LhsType, typename Derived::RhsType>, Derived>;

/// @brief Satisfied when `Derived` is a `RefType<T>` atom (const-reference to `T`).
template<typename Derived, typename T>
concept isRef = std::is_base_of_v<RefType<T>, std::decay_t<Derived>>;

/// @brief Satisfied when `Derived` is a `LazyType<T>` atom (owning mutable variable).
template<typename Derived, typename T>
concept isLazy = std::is_base_of_v<LazyType<T>, std::decay_t<Derived>>;


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
/// @brief Tag for unary negation `-x`.
struct NEG : public Tag{};
/// @brief Tag for `abs(x)`.
struct ABS : public Tag{};
/// @brief Tag for `max(x,y)`.
struct MAX : public Tag{};
/// @brief Tag for `min(x,y)`.
struct MIN : public Tag{};
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

/// @brief Satisfied when `Arg` derives from `Tag` (arithmetic or comparison tag).
template<typename Arg>
concept isTag = std::is_base_of_v<Tag, std::decay_t<Arg>>;

/// @brief Satisfied when `Arg` derives from `BOOL_TAG` (relational comparison tag only).
template<typename Arg>
concept isBoolTag = std::is_base_of_v<BOOL_TAG, std::decay_t<Arg>>;

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



// ============================================================================
// ExprBase — root of the expression hierarchy
// ============================================================================

/**
 * @brief Non-CRTP root base for all expression types parameterised on value type `T`.
 *
 * Deriving from `ExprBase<T>` opts a type into the `isExpr<D,T>` concept and provides
 * service types used throughout the library:
 *
 * - `MainType` — the underlying arithmetic type (carries `T` into derived classes via
 *   the nested alias).
 * - `expr_storage_t<E>` — determines how sub-expressions are stored inside composite
 *   nodes: raw `T` values are stored as `const T&` (reference to an existing object),
 *   while sub-expression nodes are stored by value (so the entire tree is embedded
 *   inline without heap allocation).
 *   concrete types override this with a more specific pattern.
 *
 * @tparam T  The underlying arithmetic value type (e.g. `double`, `mpfr::mpreal`).
 */
template<typename T>
struct ExprBase{

    using MainType = T;

    template<typename E>
    struct expr_storage {
        // Default: store by const reference
        using type = const T&;
    };

    template<isExpr<T> E>
    struct expr_storage<E> {
        // For sub-expressions: store by value
        using type = std::decay_t<E>;
    };

    template<typename E>
    using expr_storage_t = typename expr_storage<E>::type;

};


// ============================================================================
// Expr — CRTP intermediate base
// ============================================================================

/**
 * @brief CRTP intermediate base that enriches `ExprBase<T>` with static membership flags
 *        and a compile-time `matches_pattern` check.
 *
 * Every concrete expression type inherits from `Expr<Derived, T>` (transitively via
 * `Atom` or `Node`) and overrides the relevant `static constexpr bool isXxx` flag to
 * `true`.  These flags are used in `BinaryOpRules` / `UnaryOpRules` to branch at
 * compile time without virtual dispatch.
 *
 * @tparam Derived The most-derived type (CRTP).
 * @tparam T       The underlying arithmetic value type.
 */
template<typename Derived, typename T>
struct Expr : public ExprBase<T> {

    using Base = ExprBase<T>;
    using MainType = T;

    static constexpr bool isAtom  = false;
    static constexpr bool isNode = false;
    static constexpr bool isBinaryOperator = false;
    static constexpr bool isUnary = false;
    static constexpr bool isAdd = false;
    static constexpr bool isMul = false;
    static constexpr bool isDiv = false;
    static constexpr bool isSub = false;
    static constexpr bool isPow = false;
    static constexpr bool isNeg = false;
    static constexpr bool isLazy = false;
    static constexpr bool isRef = false;

};

// ============================================================================
// Atom — leaf expression nodes (already evaluated)
// ============================================================================

/**
 * @brief CRTP base for leaf nodes that hold a value directly accessible via `value()`.
 *
 * Atoms never need temporary storage for evaluation.  The `value()` call simply returns
 * a reference to the stored or referenced `T` (or compatible) value.  Atoms also
 * provide an `operator T()` implicit conversion for ergonomic use in regular code.
 *
 * Concrete atom types:
 * - `RefType<T>` — holds a `const T&` (non-owning reference into an existing `LazyType`).
 * - `LazyType<T>` — owns a `T` value; the primary user-facing variable type.
 * - `OtherType<T,Type>` — wraps a scalar of a *different* type `Type` (e.g. `int` in a
 *   `double`-typed expression).
 *
 * @tparam Derived The most-derived type (CRTP).
 * @tparam T       The underlying arithmetic value type.
 */
template<typename Derived, typename T>
struct Atom : public Expr<Derived, T>{

    using Base = Expr<Derived, T>;
    using MainType = T;
    static constexpr bool isAtom = true;

    LAZY_FORCE_INLINE const auto& value() const{
        return LAZY_THIS->value();
    }

    operator T() const { return value(); }

};

// ============================================================================
// Node — unevaluated composite expression nodes
// ============================================================================

/**
 * @brief CRTP base for composite (unevaluated) expression nodes.
 *
 * A `Node` holds `Branches` sub-expressions and computes `T` only when `eval()` is
 * called.  The CRTP `eval()` call dispatches to `Derived::eval(T& out)` which
 * typically delegates to the relevant `eval_rule` in `BinaryOpRules` or
 * `UnaryOpRules`.
 *
 * The `operator T()` implicit conversion creates a temporary `T` and calls `eval()`
 * on it — use with care for types (like `mpfr::mpreal`) where default construction
 * is expensive or produces an indeterminate value.
 *
 * @tparam Derived   The most-derived type (CRTP).
 * @tparam T         The underlying arithmetic value type.
 * @tparam Branches  Number of child sub-expressions (1 for unary, 2 for binary, etc.).
 */
template<typename Derived, typename T, size_t Branches>
struct Node : public Expr<Derived, T>{

    using Base = Expr<Derived, T>;
    static constexpr bool isNode = true;
    static constexpr size_t Nbranches = Branches;

    LAZY_FORCE_INLINE T& eval(T& out) const{
        return LAZY_THIS->eval(out);
    }

    operator T() const {
        T out; // TODO: only case where a temporary is created. fix this.
        return LAZY_THIS->eval(out);
    }

};

// ============================================================================
// BinaryOperator — stores lhs/rhs and dispatches to eval_rule
// ============================================================================

/**
 * @brief CRTP base for all two-operand expression nodes.
 *
 * Holds the left- and right-hand sub-expressions (by value, embedded inline) and
 * provides a concrete `eval(T& out)` that delegates to
 * `Derived::eval_rule(tag{}, out, make_expr<T>(lhs), make_expr<T>(rhs))`.
 *
 * Constructing a `BinaryOperator<Derived,T,L,R>` requires that both `L` and `R` are
 * constructible from the supplied arguments.  See `make_add`, `make_mul`, etc. for
 * the canonical factory functions that build these nodes.
 *
 * @tparam Derived The most-derived type (e.g. `Add<T,L,R>`).
 * @tparam T       The underlying arithmetic value type.
 * @tparam L       The type of the left-hand sub-expression (stored by value).
 * @tparam R       The type of the right-hand sub-expression (stored by value).
 */
template<typename Derived, typename T, typename L, typename R>
struct BinaryOperator : public Node<Derived, T, 2>{

    using Base = Node<Derived, T, 2>;
    using LhsType = L;
    using RhsType = R;
    static constexpr bool isBinaryOperator = true;
    
    template<typename L2, typename R2>
    requires (std::is_constructible_v<L, L2&&> && std::is_constructible_v<R, R2&&>)
    LAZY_FORCE_INLINE BinaryOperator(L2&& lhs, R2&& rhs) : lhs(std::forward<L2>(lhs)), rhs(std::forward<R2>(rhs)) {}

    LAZY_FORCE_INLINE T& eval(T& out) const{
        Derived::eval_rule(typename Derived::tag{}, out, make_expr<T>(lhs), make_expr<T>(rhs));
        return out;
    }

    L lhs; R rhs;
};


//=============================== Rules ==============================================

/**
 * @brief Global registry of `T*` scratch pointers used by `RuleTree`.
 *
 * Each `RuleTree<T, Branches...>` specialisation registers its thread-local
 * temporary `T` objects into this list so that external code can iterate over all
 * allocated temporaries (e.g. for bulk operations that need to touch all active
 * scratch buffers).
 *
 * @tparam T The arithmetic value type.
 */
template<typename T>
struct Rules{

    static std::vector<T*> aux_ptrs;
    static std::shared_mutex aux_mutex;

    LAZY_FORCE_INLINE static void append_ptr(T* ptr){
        std::unique_lock lock(aux_mutex);
        aux_ptrs.push_back(ptr);
    }

    template<typename Func>
    LAZY_FORCE_INLINE static void for_each_aux(Func&& fn) {
        std::shared_lock lock(aux_mutex);
        for (auto* p : aux_ptrs) {fn(p);};
    }
};


/**
 * @brief Per-(type, operand-pattern) thread-local temporary storage pool.
 *
 * For each unique combination of value type `T` and branch expression types
 * `Branch...`, exactly `sizeof...(Branch)` `T` objects are allocated as
 * thread-local storage.  These serve as scratch buffers for intermediate results
 * during recursive `eval_rule` calls, avoiding any heap allocation on the hot path.
 *
 * The thread-local array is initialised lazily on first use; each element is
 * registered in `Rules<T>::aux_ptrs` so that external code can find all scratch
 * buffers of type `T`.
 *
 * @tparam T        The arithmetic value type.
 * @tparam Branch   The expression types of the sub-expressions for which scratch is
 *                  needed.  Must satisfy `isExpr<Branch, T>` for each element.
 */
template<typename T, isExpr<T>... Branch>
struct RuleTree : public Rules<T>{

    static constexpr size_t Nb = sizeof...(Branch);
    using Base = Rules<T>;
    static_assert(sizeof...(Branch) > 0, "Branches must be greater than 0");

    inline static thread_local T* aux = [](){
        static thread_local std::array<T, Nb> aux_storage;
        for (size_t i = 0; i < Nb; ++i){
            Base::append_ptr(&aux_storage[i]);
        }
        return aux_storage.data();
    }();

};

//============================== Binary Operation rules ==============================

/**
 * @brief Default evaluation policy for binary operations.
 *
 * `BinaryOpRules<Derived, T>` is CRTP-mixed into each concrete binary-operation node
 * (via `CustomBinaryRules<T>`) and provides a two-step evaluation strategy:
 *
 * 1. **`eval_rule(tag, out, a, b)`** — accepts two `isExpr<T>` operands.  Recursively
 *    evaluates any `Node` sub-expression (using thread-local scratch from `RuleTree`)
 *    until both operands are atoms, then calls `evaluate`.
 * 2. **`evaluate(tag, out, a, b)`** — accepts raw values `a`, `b` of any type and
 *    writes the result to `out`.  This is the function to *specialise* when providing
 *    type-specific implementations (e.g. MPFR intrinsics).
 *
 * **Important:** `out`, `a`, and `b` may alias the same memory (the library performs
 * in-place updates of `LazyType<T>` variables).  Implementations of `evaluate` must
 * be safe under aliasing.
 *
 * @tparam Derived The concrete rules class (CRTP, typically `CustomBinaryRules<T>`).
 * @tparam T       The arithmetic value type.
 */
template<typename Derived, typename T>
struct BinaryOpRules{

    /**
    eval_rule takes as input T, while "a" and "b" are Expr types, not raw values like T or int, double etc.
    Override this for more control.

    On the other hand, evaluate takes as input raw values like T or int. These must be instanciated necessarily for core operations like addition, multiplication etc.
    */

    // IMPORTANT: a or b might be the same memory location as out.
    template<isTag tag, typename L, typename R>
    inline static void evaluate(tag, T& out, const L& a, const R& b);

    template<isTag tag, isExpr<T> L, isExpr<T> R>
    LAZY_FORCE_INLINE static void eval_rule(tag, T& out, const L& a, const R& b){
        // IMPORTANT: out may be the same memory location as a and b
        
        // make sure L and R are NOT LazyType (RefType should be passed intead ALWAYS)
        static_assert(!isLazy<L, T> && !isLazy<R, T>, "LazyType should not be passed to eval_rule, use RefType instead");

        // By default, eval_rule simply evaluates the sub-expressions and then calls evaluate with the raw values. Override this if you want to do something different, like short-circuiting for addition or multiplication.
        T* worker = RuleTree<T, L, R>::aux;
        if constexpr (isNode<L, T>) {
            // no matter what R is, since this operation has not been overriden,
            // we need to evaluate one of the branches (convention: the left one)
            Derived::eval_rule(tag{}, out, make_expr<T>(a.eval(worker[0])), b);
        } else if constexpr (isNode<R, T>) {
            Derived::evaluate(tag{}, out, a.value(), b.eval(worker[1]));
        } else {
            Derived::evaluate(tag{}, out, a.value(), b.value());
        }
    }
};


/**
 * @brief Default evaluation policy for unary operations.
 *
 * Mirrors `BinaryOpRules` for single-argument operations.  `eval_rule(tag, out, a)`
 * evaluates the argument if it is a `Node` (placing the result in the first slot of
 * `RuleTree<T, Arg>::aux`), then calls `evaluate(tag, out, raw_a)`.
 *
 * Specialise `evaluate` to provide type-specific unary implementations.
 *
 * @tparam Derived The concrete rules class (CRTP, typically `CustomUnaryRules<T>`).
 * @tparam T       The arithmetic value type.
 */
template<typename Derived, typename T>
struct UnaryOpRules{

    template<isTag tag, typename Arg>
    inline static void evaluate(tag, T& out, const Arg& a);

    template<isTag tag, isExpr<T> Arg>
    inline static void eval_rule(tag, T& out, const Arg& a){
        if constexpr (isNode<Arg, T>) {
            T& worker = RuleTree<T, Arg>::aux[0];
            Derived::evaluate(tag{}, out, a.eval(worker));
        } else {
            Derived::evaluate(tag{}, out, a.value());
        }
    }
};


/**
 * @brief Primary binary-operation rules for type `T`.
 *
 * A default-constructed `CustomBinaryRules<T>` provides no specialised `evaluate`
 * overloads.  Users should provide a full specialisation (via `LAZY_SPECIALIZE_OPERATIONS`)
 * for each numeric type `T` to define how `+`, `-`, `*`, `/`, `pow`, `max`, `min`
 * are computed.
 *
 * When no specialisation exists, the library falls back to the default
 * `BinaryOpRules` which uses the built-in operators on raw `T` values.
 *
 * @tparam T The arithmetic value type to specialise for.
 */
template<typename T>
struct CustomBinaryRules : public BinaryOpRules<CustomBinaryRules<T>, T>{};


/**
 * @brief Primary unary-operation rules for type `T`.
 *
 * Analogous to `CustomBinaryRules`.  Specialise (via `LAZY_SPECIALIZE_FUNCTIONS`) to
 * provide custom `evaluate` overloads for unary functions such as `abs`, `sqrt`,
 * `neg`, etc.
 *
 * @tparam T The arithmetic value type to specialise for.
 */
template<typename T>
struct CustomUnaryRules : public UnaryOpRules<CustomUnaryRules<T>, T>{};


/**
 * @brief Evaluation rules for relational comparisons.
 *
 * Extends `BinaryOpRules` to support `isNode` operands in the boolean path.  The
 * key entry point is `eval_bool(tag, a, b)` which:
 * 1. Evaluates any `Node` operands into scratch storage.
 * 2. Calls `get_bool(tag, raw_a, raw_b)` to perform the actual comparison.
 *
 * `get_bool` dispatches on the tag type at compile time to one of `==`, `!=`, `>`,
 * `<`, `>=`, `<=`.
 *
 * The `evaluate(tag, out, a, b)` overload used by the standard `eval_rule` path writes
 * the `bool` result into `out` (cast to `T`) via `get_bool`.
 *
 * @tparam T The arithmetic value type.
 */
template<typename T>
struct CompareRules : public BinaryOpRules<CompareRules<T>, T>{

    template<isBoolTag tag, typename L, typename R>
    LAZY_FORCE_INLINE static void evaluate(tag, T& out, const L& a, const R& b){
        out = get_bool(tag{}, a, b);
    }

    template<isBoolTag tag, typename L, typename R>
    inline static bool get_bool(tag, const L& a, const R& b){
        if constexpr (std::is_same_v<tag, EQ>) {
            return a == b;
        } else if constexpr (std::is_same_v<tag, NEQ>) {
            return a != b;
        } else if constexpr (std::is_same_v<tag, GT>) {
            return a > b;
        } else if constexpr (std::is_same_v<tag, LT>) {
            return a < b;
        } else if constexpr (std::is_same_v<tag, GE>) {
            return a >= b;
        } else if constexpr (std::is_same_v<tag, LE>) {
            return a <= b;
        } else {
            static_assert(isBoolTag<tag>, "Invalid boolean tag");
            return false; // should never reach here
        }
    }


    template<isBoolTag tag, typename L, typename R>
    LAZY_FORCE_INLINE static bool eval_bool(tag, const L& a, const R& b){
        T* worker = RuleTree<T, L, R>::aux;
         if constexpr (isNode<L, T> && isNode<R, T>) {
            // no matter what R is, since this operation has not been overriden,
            // we need to evaluate one of the branches (convention: the left one)
            return get_bool(tag{}, a.eval(worker[0]), b.eval(worker[1]));
        } else if constexpr (isNode<R, T>) {
            return get_bool(tag{}, a.value(), b.eval(worker[1]));
        } else if constexpr (isNode<L, T>) {
            return get_bool(tag{}, a.eval(worker[0]), b.value());
        } else {
            return get_bool(tag{}, a.value(), b.value());
        }
    }

};



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



/**
 * @brief CRTP base for relational (boolean-valued) binary expression nodes.
 *
 * Inherits from both `BinaryOperator<Derived,T,L,R>` (to participate in the
 * arithmetic tree as a node) and `CompareRules<T>` (to get `eval_bool` and
 * `get_bool`).
 *
 * The `operator bool()` implicit conversion evaluates the comparison lazily:
 * it calls `Derived::eval_bool(tag{}, this->lhs, this->rhs)`, which internally
 * evaluates any `Node` sub-expressions via thread-local scratch before comparing.
 *
 * @tparam Derived The concrete comparison type (e.g. `Gt<T,L,R>`).
 * @tparam T       The arithmetic value type.
 * @tparam L       Left sub-expression type.
 * @tparam R       Right sub-expression type.
 */
template<typename Derived, typename T, typename L, typename R>
struct Comparison : public BinaryOperator<Derived, T, L, R>, public CompareRules<T>{
    using Base = BinaryOperator<Derived, T, L, R>;
    using Base::Base;

    operator bool() const{
        return Derived::eval_bool(typename Derived::tag{}, this->lhs, this->rhs);
    }
};


/// @brief Lazy equal-to comparison node (`lhs == rhs`).  `operator bool()` returns the result.
template<typename T, typename L, typename R>
struct Eq : public Comparison<Eq<T, L, R>, T, L, R>{
    using Base = Comparison<Eq<T, L, R>, T, L, R>;
    using Base::Base;
    using tag = EQ;
};


/// @brief Lazy greater-than comparison node (`lhs > rhs`).  `operator bool()` returns the result.
template<typename T, typename L, typename R>
struct Gt : public Comparison<Gt<T, L, R>, T, L, R>{
    using Base = Comparison<Gt<T, L, R>, T, L, R>;
    using Base::Base;
    using tag = GT;
};

/// @brief Lazy less-than comparison node (`lhs < rhs`).  `operator bool()` returns the result.
template<typename T, typename L, typename R>
struct Lt : public Comparison<Lt<T, L, R>, T, L, R>{
    using Base = Comparison<Lt<T, L, R>, T, L, R>;
    using Base::Base;
    using tag = LT;
};

/// @brief Lazy not-equal comparison node (`lhs != rhs`).  `operator bool()` returns the result.
template<typename T, typename L, typename R>
struct Neq : public Comparison<Neq<T, L, R>, T, L, R>{
    using Base = Comparison<Neq<T, L, R>, T, L, R>;
    using Base::Base;
    using tag = NEQ;
};

/// @brief Lazy greater-or-equal comparison node (`lhs >= rhs`).  `operator bool()` returns the result.
template<typename T, typename L, typename R>
struct Ge : public Comparison<Ge<T, L, R>, T, L, R>{
    using Base = Comparison<Ge<T, L, R>, T, L, R>;
    using Base::Base;
    using tag = GE;
};

/// @brief Lazy less-or-equal comparison node (`lhs <= rhs`).  `operator bool()` returns the result.
template<typename T, typename L, typename R>
struct Le : public Comparison<Le<T, L, R>, T, L, R>{
    using Base = Comparison<Le<T, L, R>, T, L, R>;
    using Base::Base;
    using tag = LE;
};








/**
 * @brief CRTP base for single-argument (unary) expression nodes.
 *
 * Stores the single child expression `arg` by value and evaluates via
 * `Derived::eval_rule(tag{}, out, arg)`.  The `UnaryOpRules` default path
 * evaluates `arg` into a scratch slot if it is a `Node`, then calls
 * `Derived::evaluate(tag{}, out, raw_arg)`.
 *
 * @tparam Derived The concrete unary node type (e.g. `Neg<T,Arg>`).
 * @tparam T       The arithmetic value type.
 * @tparam Arg     The type of the single child sub-expression (`isExpr<Arg,T>`).
 */
template<typename Derived, typename T, isExpr<T> Arg>
struct Unary : public Node<Derived, T, 1>{
    using Base = Node<Derived, T, 1>;
    static constexpr bool isUnary = true;

    template<typename Arg2>
    requires( requires {typename std::decay_t<Arg>::MainType;} && isExpr<std::decay_t<Arg>, typename std::decay_t<Arg>::MainType> )
    LAZY_FORCE_INLINE Unary(Arg2&& arg) : arg(std::forward<Arg2>(arg)) {}

    LAZY_FORCE_INLINE T& eval(T& out) const{
        Derived::eval_rule(typename Derived::tag{}, out, arg);
        return out;
    }

    Arg arg;
};


/// @brief Expression node for unary negation: computes `-arg`.
/// @tparam T   The arithmetic value type.
/// @tparam Arg The child sub-expression type.
template<typename T, typename Arg>
struct Neg : public Unary<Neg<T, Arg>, T, Arg>, public CustomUnaryRules<T>{
    using Base = Unary<Neg<T, Arg>, T, Arg>;
    static constexpr bool isNeg = true;
    using tag = NEG;
};

/**
 * @brief Atom that wraps a `T` value by const reference.
 *
 * `RefType<T>` is a lightweight non-owning view.  It is produced automatically by
 * `make_expr<T>()` when a `LazyType<T>` lvalue is used as an operand, so that the
 * expression tree references the original variable without copying it.  The
 * referenced value must not be modified or destroyed while the node is alive.
 *
 * between a reference and a direct value.
 *
 * @tparam T The arithmetic value type.
 */
template<typename T>
struct RefType : public Atom<RefType<T>, T>{
    using Base = Atom<RefType<T>, T>;


    static constexpr bool isRef = true;

    LAZY_FORCE_INLINE RefType(const T& value) : value_(value) {}

    LAZY_FORCE_INLINE const T& value() const {return value_;}

    Base::template expr_storage_t<T> value_;

};


/**
 * @brief Owning lazy variable — the primary user-facing storage type.
 *
 * `LazyType<T>` is the type users declare variables with.  It is an `Atom` so it
 * can be used directly in expressions without evaluation, and it supports:
 *
 * - Construction from raw `T`, from other atoms, and from `Node` expressions
 *   (which evaluates them immediately via `eval()`).
 * - Compound assignment (`+=`, `-=`, `*=`, `/=`) that update `value_` in place
 *   using `CustomBinaryRules<T>`, correctly handling both atom and node operands
 *   without extra temporaries.
 * - Implicit conversion `operator T()` returning `value_` by copy.
 * - Thread-safety for *reading* the scratch pointer registry via `for_each_aux`.
 *
 * **Note:** `make_expr<T>()` converts an lvalue `LazyType<T>` to `RefType<T>` so
 * that expression trees reference the original variable; an rvalue `LazyType<T>` is
 * moved into a new `LazyType<T>` node.
 *
 * @tparam T The arithmetic value type.
 */
template<typename T>
struct LazyType : public Atom<LazyType<T>, T>{
    using Base = Atom<LazyType<T>, T>;
    
    static constexpr bool isLazy = true;

    inline static thread_local T tmp = 0;


    template<typename F>
    inline static void for_each_aux(F&& fn) {
        Rules<T>::for_each_aux(std::forward<F>(fn));
    }

    LazyType(T value) : value_(std::move(value)) {}

    LazyType() = default;

    template<typename U>
    requires (!isExpr<U, T>)
    LazyType(U&& value) : value_(std::forward<U>(value)) {}

    template<typename U>
    requires (isAtom<U, T> && !::lazy::isLazy<U, T>)
    LazyType(U&& value) : value_(value.value()) {}

    template<typename U>
    requires (isNode<U, T>)
    LazyType(U&& value) {
        value.eval(value_);
    }

    LazyType(LazyType&& other) = default;
    LazyType& operator=(LazyType&& other) = default;

    LazyType(const LazyType& other) = default;
    LazyType& operator=(const LazyType& other) = default;

    template<typename U>
    LazyType& operator+=(U&& other){
        if constexpr (isAtom<U, T>){
            CustomBinaryRules<T>::evaluate(PLUS{}, value_, value_, other.value());
        } else if constexpr (isNode<U, T>){
            CustomBinaryRules<T>::eval_rule(PLUS{}, value_, make_expr<T>(value_), other);
        } else {
            CustomBinaryRules<T>::evaluate(PLUS{}, value_, value_, other);
        }
        return *this;
    }

    template<typename U>
    LazyType& operator*=(U&& other){
        if constexpr (isAtom<U, T>){
            CustomBinaryRules<T>::evaluate(MUL{}, value_, value_, other.value());
        } else if constexpr (isNode<U, T>){
            CustomBinaryRules<T>::eval_rule(MUL{}, value_, make_expr<T>(value_), other);
        } else {
            CustomBinaryRules<T>::evaluate(MUL{}, value_, value_, other);
        }
        return *this;
    }

    template<typename U>
    LazyType& operator-=(U&& other){
        if constexpr (isAtom<U, T>){
            CustomBinaryRules<T>::evaluate(MINUS{}, value_, value_, other.value());
        } else if constexpr (isNode<U, T>){
            CustomBinaryRules<T>::eval_rule(MINUS{}, value_, make_expr<T>(value_), other);
        } else {
            CustomBinaryRules<T>::evaluate(MINUS{}, value_, value_, other);
        }
        return *this;
    }

    template<typename U>
    LazyType& operator/=(U&& other){
        if constexpr (isAtom<U, T>){
            CustomBinaryRules<T>::evaluate(DIV{}, value_, value_, other.value());
        } else if constexpr (isNode<U, T>){
            CustomBinaryRules<T>::eval_rule(DIV{}, value_, make_expr<T>(value_), other);
        } else {
            CustomBinaryRules<T>::evaluate(DIV{}, value_, value_, other);
        }
        return *this;
    }

    operator T() const { return value_; }

    LAZY_FORCE_INLINE const T& value() const {return value_;}

    T value_;

};


/**
 * @brief Atom that wraps a value of a *different* scalar type `Type` inside a
 *        `T`-typed expression tree.
 *
 * Used when a raw scalar (e.g. `int` or `double`) is mixed into an expression whose
 * `MainType` is a different type (e.g. `mpfr::mpreal`).  `make_expr<T>()` produces
 * an `OtherType<T, S>` when `S != T` and `S` is not already an expression.  The
 * `value()` accessor returns the stored `Type` value which is then passed to
 * the `evaluate(tag, out, ..., scalar)` overloads in `CustomBinaryRules<T>`.
 *
 * @tparam T    The arithmetic value type of the surrounding expression tree.
 * @tparam Type The actual scalar type of the stored value (e.g. `int`, `double`).
 */
template<typename T, typename Type>
struct OtherType : public Atom<OtherType<T, Type>, T>{
    using Base = Atom<OtherType<T, Type>, T>;
    
    static constexpr bool isLazy = true;

    LAZY_FORCE_INLINE OtherType(const Type& value) : value_(value) {}

    LAZY_FORCE_INLINE const Type& value() const {return value_;}
    
    Type value_;

};


/**
 * @brief Canonicalise any value into an expression node of value type `T`.
 *
 * This function is the central entry-point used by all operator overloads and
 * `BinaryOperator::eval()` to convert heterogeneous operands to a uniform expression
 * type.  The conversion rules are:
 *
 * | Input type                  | Output type                              |
 * |-----------------------------|------------------------------------------|
 * | `LazyType<T>` lvalue        | `RefType<T>(value.value())`              |
 * | `LazyType<T>` rvalue        | `LazyType<T>(std::move(value))`          |
 * | Any other `isExpr<T>` type  | Forwarded as-is (no copy)                |
 * | Exactly `T`                 | `RefType<T>(value)` (reference to `T`)  |
 * | Any other scalar `S`        | `OtherType<T, S>(value)`                 |
 *
 * @tparam T     The target arithmetic value type for the expression tree.
 * @tparam R     The type of the incoming value (deduced).
 * @param  value The value to wrap.  For lvalue expressions, passed by lvalue ref;
 *               for rvalue nodes/scalars, passed by rvalue ref.
 * @return       An expression node of the appropriate type (see table).
 */
template<typename T, typename R>
LAZY_FORCE_INLINE decltype(auto) make_expr(R&& value){
    if constexpr (isLazy<R, T> && std::is_lvalue_reference_v<R>) {
        // static_assert(std::is_lvalue_reference_v<R>, "LazyType rvalues are not allowed; bind to a variable first.");
        return RefType<T>(value.value());
    } else if constexpr (isLazy<R, T>) {
        return LazyType<T>(std::move(value));
    } else if constexpr (isExpr<std::decay_t<R>, T>) {
        return std::forward<R>(value);
    } else if constexpr (std::is_same_v<T, std::decay_t<R>>) {
        return RefType<T>(value);
    } else {
        return OtherType<T, std::decay_t<R>>(std::forward<R>(value));
    }
}

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
    return make_add<MainTypeOf<L, R>>(std::forward<L>(lhs), std::forward<R>(rhs));
}

/// @brief Lazy multiplication: returns `Mul<T,L,R>` when at least one operand is an expression.
template<typename L, typename R>
requires LAZY_REQUIREMENT(L, R)
LAZY_FORCE_INLINE auto operator*(L&& lhs, R&& rhs){
    return make_mul<MainTypeOf<L, R>>(std::forward<L>(lhs), std::forward<R>(rhs));
}

/// @brief Lazy division: returns `Div<T,L,R>` when at least one operand is an expression.
template<typename L, typename R>
requires LAZY_REQUIREMENT(L, R)
LAZY_FORCE_INLINE auto operator/(L&& lhs, R&& rhs){
    return make_div<MainTypeOf<L, R>>(std::forward<L>(lhs), std::forward<R>(rhs));
}

/// @brief Lazy subtraction: returns `Sub<T,L,R>` when at least one operand is an expression.
template<typename L, typename R>
requires LAZY_REQUIREMENT(L, R)
LAZY_FORCE_INLINE auto operator-(L&& lhs, R&& rhs){
    return make_sub<MainTypeOf<L, R>>(std::forward<L>(lhs), std::forward<R>(rhs));
}

/// @brief Lazy unary negation: returns `Neg<T,Arg>` for any expression argument.
template<typename Arg>
requires( requires {typename std::decay_t<Arg>::MainType;} && isExpr<std::decay_t<Arg>, typename std::decay_t<Arg>::MainType> )
LAZY_FORCE_INLINE auto operator-(Arg&& arg){
    return make_neg<MainTypeOf<Arg, void>>(std::forward<Arg>(arg));
}

/// @brief Lazy exponentiation: returns `Pow<T,Base,Exp>` when at least one operand is an expression.
template<typename Base, typename Exp>
requires LAZY_REQUIREMENT(Base, Exp)
LAZY_FORCE_INLINE auto pow(Base&& base, Exp&& exp){
    return make_pow<MainTypeOf<Base, Exp>>(std::forward<Base>(base), std::forward<Exp>(exp));
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
    auto lhs_expr = make_expr<MainTypeOf<L, R>>(std::forward<L>(lhs));
    auto rhs_expr = make_expr<MainTypeOf<L, R>>(std::forward<R>(rhs));
    return MaxLazy<MainTypeOf<L, R>, std::decay_t<decltype(lhs_expr)>, std::decay_t<decltype(rhs_expr)>>(std::move(lhs_expr), std::move(rhs_expr));
}

/**
 * @brief Lazy minimum: returns a `MinLazy<T,L,R>` node.
 *
 * Only participates in overload resolution when at least one argument is an expression.
 */
template<typename L, typename R>
requires LAZY_REQUIREMENT(L, R)
LAZY_FORCE_INLINE auto min(L&& lhs, R&& rhs){
    auto lhs_expr = make_expr<MainTypeOf<L, R>>(std::forward<L>(lhs));
    auto rhs_expr = make_expr<MainTypeOf<L, R>>(std::forward<R>(rhs));
    return MinLazy<MainTypeOf<L, R>, std::decay_t<decltype(lhs_expr)>, std::decay_t<decltype(rhs_expr)>>(std::move(lhs_expr), std::move(rhs_expr));
}


// ===========================================================================================================
// ================================== Relational operators and other comparisons =============================
// ===========================================================================================================


template<typename T>
inline std::vector<T*> Rules<T>::aux_ptrs;

template<typename T>
inline std::shared_mutex Rules<T>::aux_mutex;


LAZY_DEFINE_RELATIONAL_OP(==, Eq)

LAZY_DEFINE_RELATIONAL_OP(!=, Neq)

LAZY_DEFINE_RELATIONAL_OP(>, Gt)

LAZY_DEFINE_RELATIONAL_OP(<, Lt)

LAZY_DEFINE_RELATIONAL_OP(>=, Ge)

LAZY_DEFINE_RELATIONAL_OP(<=, Le)

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
class numeric_limits<lazy::LazyType<TYPE>> : public numeric_limits<TYPE>{};\
}

/**
 * @brief Declare a new unary function node type and its overloaded free function.
 *
 * Expands to:
 * 1. A struct `OP<T, Arg>` inheriting `Unary<OP<T,Arg>, T, Arg>` and
 *    `CustomUnaryRules<T>`, with `tag = TAG`
 * 2. A free function `FUNC(U&&)` constrained to any expression type, returning
 *    `OP<MainType<U>, decay_t<U>>(forward(arg))`.
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
template<typename U>                                                            \
requires (                                                                      \
    requires { typename std::decay_t<U>::MainType; } &&                         \
    isExpr<std::decay_t<U>, typename std::decay_t<U>::MainType>                 \
)                                                                               \
LAZY_FORCE_INLINE auto FUNC(U&& arg) {                                               \
    using T = MainType<U>;                                                      \
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


/// @brief `Abs<T,Arg>` node and lazy `abs(U&&)` overload, defined via `LAZY_DEFINE_UNARY_OP`.
LAZY_DEFINE_UNARY_OP(abs, Abs, ABS, AbsoluteValue)

/// @brief `Sqrt<T,Arg>` node and lazy `sqrt(U&&)` overload, defined via `LAZY_DEFINE_UNARY_OP`.
LAZY_DEFINE_UNARY_OP(sqrt, Sqrt, SQRT, SquareRoot)




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

    using Type = OtherType<T, P>;
};

/// @brief Maps `T` itself (the main value type) to `RefType<T>` (a direct reference atom).
template<typename T>
struct HelperfromPattern<T, T>{

    using Type = RefType<T>;
};

/// @brief Maps `Addition<L,R>` pattern to `Add<T, ...>` node (recursive).
template<typename T, typename L, typename R>
struct HelperfromPattern<T, Addition<L, R>>{
    using Type = Add<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};


/// @brief Maps `Multiplication<L,R>` pattern to `Mul<T, ...>` node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, Multiplication<L, R>>{
    using Type = Mul<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `Division<L,R>` pattern to `Div<T, ...>` node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, Division<L, R>>{
    using Type = Div<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `Subtraction<L,R>` pattern to `Sub<T, ...>` node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, Subtraction<L, R>>{
    using Type = Sub<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `Power<L,R>` pattern to `Pow<T, ...>` node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, Power<L, R>>{
    using Type = Pow<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `Max<L,R>` pattern to `MaxLazy<T, ...>` node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, Max<L, R>>{
    using Type = MaxLazy<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `Min<L,R>` pattern to `MinLazy<T, ...>` node.
template<typename T, typename L, typename R>
struct HelperfromPattern<T, Min<L, R>>{
    using Type = MinLazy<T, typename HelperfromPattern<T, L>::Type, typename HelperfromPattern<T, R>::Type>;
};

/// @brief Maps `Negation<Arg>` pattern to `Neg<T, ...>` node.
template<typename T, typename Arg>
struct HelperfromPattern<T, Negation<Arg>>{
    using Type = Neg<T, typename HelperfromPattern<T, Arg>::Type>;
};

/// @brief Maps `AbsoluteValue<Arg>` pattern to `Abs<T, ...>` node.
template<typename T, typename Arg>
struct HelperfromPattern<T, AbsoluteValue<Arg>>{
    using Type = Abs<T, typename HelperfromPattern<T, Arg>::Type>;
};

/// @brief Maps `SquareRoot<Arg>` pattern to `Sqrt<T, ...>` node.
template<typename T, typename Arg>
struct HelperfromPattern<T, SquareRoot<Arg>>{
    using Type = Sqrt<T, typename HelperfromPattern<T, Arg>::Type>;
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

} // namespace lazy

#endif // LAZY_EVAL_HPP