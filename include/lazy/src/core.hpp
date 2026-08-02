#ifndef LAZY_CORE_HPP
#define LAZY_CORE_HPP

#include <vector>
#include <array>
#include "rules.hpp"
#include <iostream>

namespace lazy{




namespace detail{



// ============================================================================
// ExprBase — root of the expression hierarchy
// ============================================================================

/**
 * @brief Non-CRTP root base for all expression types parameterised on value type `T`.
 *
 * Deriving from `ExprBase<T>` opts a type into the `isLazyExpr<D,T>` concept and provides
 * service types used throughout the library:
 *
 * - `value_type` — the underlying arithmetic type (carries `T` into derived classes via
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

    using value_type = T;

    template<typename E>
    struct expr_storage {
        // Default: store by const reference
        using type = const T&;
    };

    template<traits::isLazyExpr<T> E>
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
    using value_type = T;

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
    using value_type = T;
    using branch_t = std::tuple<>;
    static constexpr size_t Depth = 0;
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
    static constexpr size_t Depth = 1 + []<size_t... I>(std::index_sequence<I...>){
        return std::max({size_t{0}, std::tuple_element_t<I, typename Derived::branch_t>::Depth...});
    }(std::make_index_sequence<Branches>{});

    inline static thread_local T tmp{};

    LAZY_FORCE_INLINE T& eval(T& out) const{
        return LAZY_THIS->eval(out);
    }

    operator T() const {
        return LAZY_THIS->eval(tmp);
    }

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
 * @brief Atom that wraps a value of a *different* scalar type `Type` inside a
 *        `T`-typed expression tree.
 *
 * Used when a raw scalar (e.g. `int` or `double`) is mixed into an expression whose
 * `value_type` is a different type (e.g. `mpfr::mpreal`).  `make_expr<T>()` produces
 * an `OtherType<T, S>` when `S != T` and `S` is not already an expression.  The
 * `value()` accessor returns the stored `Type` value which is then passed to
 * the `evaluate(tag, out, ..., scalar)` overloads in `CustomBinaryRules<T>`.
 *
 * @tparam T    The arithmetic value type of the surrounding expression tree.
 * @tparam Type The actual scalar type of the stored value (e.g. `int`, `double`).
 */
template<typename T, typename Type>
struct OtherType : public Atom<OtherType<T, Type>, T>{

    static_assert(traits::isConvertibleTo<Type, T>, "Type must be convertible to T");

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
 * | Any other `isLazyExpr<T>` type  | Forwarded as-is (no copy)                |
 * | Exactly `T` lvalue          | `RefType<T>(value)` (reference to `T`)   |
 * | Exactly `T` rvalue          | `LazyType<T>(value)` (takes ownership)   |
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

    static_assert(lazy::traits::isValidType<R, T>, "Invalid type for make_expr");

    if constexpr (lazy::traits::isLazy<R, T> && std::is_lvalue_reference_v<R>) {
        return RefType<T>(value.value());
    } else if constexpr (std::is_same_v<T, std::decay_t<R>> && std::is_lvalue_reference_v<R>) {
        return RefType<T>(value);
    } else if constexpr (lazy::traits::isLazy<R, T> || std::is_same_v<T, std::decay_t<R>>) {
        return LazyType<T>(std::forward<R>(value));
    } else if constexpr (lazy::traits::isLazyExpr<std::decay_t<R>, T>) {
        return std::forward<R>(value);
    } else {
        return OtherType<T, std::decay_t<R>>(std::forward<R>(value));
    }
}


} // namespace detail


using detail::LazyType;

} // namespace lazy

#endif // LAZY_IMPL_HPP