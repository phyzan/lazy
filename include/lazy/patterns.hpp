#ifndef PATTERNS_HPP
#define PATTERNS_HPP

/**
 * @file patterns.hpp
 * @brief Compile-time symbolic expression pattern types for the lazy evaluation library.
 *
 * This header defines a hierarchy of pure type-level "pattern" descriptors that mirror
 * the runtime expression tree defined in lazy.hpp.  Patterns have no data members and
 * exist solely at the type level, enabling compile-time structural matching over
 * expression trees.
 *
 * ### Pattern operator overloads
 * Arithmetic and comparison operators are overloaded for any argument satisfying
 * `isAnyPattern`, so you can write e.g.
 * @code
 *   struct X : Pattern {};  struct Y : Pattern {};
 *   using MyPat = decltype(X{} * Y{} + X{});  // Multiplication<X,Y> + X
 * @endcode
 *
 * All types live in the `lazy` namespace.
 */

#include <type_traits>
#include <cstddef>


namespace lazy {
// ======================== Pattern base ========================

/**
 * @brief Base tag for all compile-time pattern types.
 *
 * Inheriting from `Pattern` opts a type into the pattern system.  The concept
 * `isAnyPattern` is satisfied by any type (after decay) that derives from `Pattern`.
 * Pattern objects carry no data; they exist only as types.
 */
struct Pattern {};

// ======================== Forward declarations ========================
/// @cond FORWARD_DECLS
template<typename Derived, size_t Branches> struct PatternNode;
template<typename Derived, typename L, typename R> struct PatternBinaryOp;
template<typename Derived, typename Arg> struct PatternUnaryOp;
template<typename L, typename R> struct Addition;
template<typename L, typename R> struct Subtraction;
template<typename L, typename R> struct Multiplication;
template<typename L, typename R> struct Division;
template<typename L, typename R> struct Power;
template<typename Arg> struct Negation;
template<typename Arg> struct AbsoluteValue;
template<typename Arg> struct SquareRoot;
template<typename Derived, typename L, typename R> struct ComparisonPattern;
template<typename L, typename R> struct LessThan;
template<typename L, typename R> struct GreaterThan;
template<typename L, typename R> struct Equal;
template<typename L, typename R> struct NotEqual;
template<typename L, typename R> struct LessEqual;
template<typename L, typename R> struct GreaterEqual;
template<typename L, typename R> struct Min;
template<typename L, typename R> struct Max;
/// @endcond

// ======================== Concepts ========================

/**
 * @brief Satisfied by any type that derives from `Pattern` (after decay).
 *
 * This is the gate-keeper concept used in operator overloads.  Because pattern
 * operator overloads are constrained with `LAZY_PATTERN_REQUIREMENT(L,R)` (at least one
 * operand satisfies `isAnyPattern`), they do not interfere with runtime arithmetic.
 */
template<typename U>
concept isAnyPattern = std::is_base_of_v<Pattern, std::decay_t<U>>;

/// @brief Satisfied when `U` is any `Addition<L,R>` pattern (or derived type).
template<typename U>
concept isAnyAddition = requires {typename std::decay_t<U>::LhsType; typename std::decay_t<U>::RhsType;} && std::is_base_of_v<Addition<typename std::decay_t<U>::LhsType, typename std::decay_t<U>::RhsType>, std::decay_t<U>>;

/// @brief Satisfied when `U` is any `Subtraction<L,R>` pattern (or derived type).
template<typename U>
concept isAnySubtraction = requires {typename std::decay_t<U>::LhsType; typename std::decay_t<U>::RhsType;} && std::is_base_of_v<Subtraction<typename std::decay_t<U>::LhsType, typename std::decay_t<U>::RhsType>, std::decay_t<U>>;

/// @brief Satisfied when `U` is any `Multiplication<L,R>` pattern (or derived type).
template<typename U>
concept isAnyMultiplication = requires {typename std::decay_t<U>::LhsType; typename std::decay_t<U>::RhsType;} && std::is_base_of_v<Multiplication<typename std::decay_t<U>::LhsType, typename std::decay_t<U>::RhsType>, std::decay_t<U>>;

/// @brief Satisfied when `U` is any `Division<L,R>` pattern (or derived type).
template<typename U>
concept isAnyDivision = requires {typename std::decay_t<U>::LhsType; typename std::decay_t<U>::RhsType;} && std::is_base_of_v<Division<typename std::decay_t<U>::LhsType, typename std::decay_t<U>::RhsType>, std::decay_t<U>>;

/// @brief Satisfied when `U` is any `Power<L,R>` pattern (or derived type).
template<typename U>
concept isAnyPower = requires {typename std::decay_t<U>::LhsType; typename std::decay_t<U>::RhsType;} && std::is_base_of_v<Power<typename std::decay_t<U>::LhsType, typename std::decay_t<U>::RhsType>, std::decay_t<U>>;

/// @brief Satisfied when `U` is any `Negation<Arg>` pattern (or derived type).
template<typename U>
concept isAnyNegation = requires {typename std::decay_t<U>::ArgType;} && std::is_base_of_v<Negation<typename std::decay_t<U>::ArgType>, std::decay_t<U>>;

/// @brief Satisfied when `U` is any `AbsoluteValue<Arg>` pattern (or derived type).
template<typename U>
concept isAnyAbsoluteValue = requires {typename std::decay_t<U>::ArgType;} && std::is_base_of_v<AbsoluteValue<typename std::decay_t<U>::ArgType>, std::decay_t<U>>;

/// @brief Satisfied when `U` is any `SquareRoot<Arg>` pattern (or derived type).
template<typename U>
concept isAnySquareRoot = requires {typename std::decay_t<U>::ArgType;} && std::is_base_of_v<SquareRoot<typename std::decay_t<U>::ArgType>, std::decay_t<U>>;

/// @brief Satisfied when `U` is any `LessThan<L,R>` comparison pattern (or derived type).
template<typename U>
concept isAnyLessThan = requires {typename std::decay_t<U>::LhsType; typename std::decay_t<U>::RhsType;} && std::is_base_of_v<LessThan<typename std::decay_t<U>::LhsType, typename std::decay_t<U>::RhsType>, std::decay_t<U>>;

/// @brief Satisfied when `U` is any `GreaterThan<L,R>` comparison pattern (or derived type).
template<typename U>
concept isAnyGreaterThan = requires {typename std::decay_t<U>::LhsType; typename std::decay_t<U>::RhsType;} && std::is_base_of_v<GreaterThan<typename std::decay_t<U>::LhsType, typename std::decay_t<U>::RhsType>, std::decay_t<U>>;

/// @brief Satisfied when `U` is any `Equal<L,R>` comparison pattern (or derived type).
template<typename U>
concept isAnyEqual = requires {typename std::decay_t<U>::LhsType; typename std::decay_t<U>::RhsType;} && std::is_base_of_v<Equal<typename std::decay_t<U>::LhsType, typename std::decay_t<U>::RhsType>, std::decay_t<U>>;

/// @brief Satisfied when `U` is any `NotEqual<L,R>` comparison pattern (or derived type).
template<typename U>
concept isAnyNotEqual = requires {typename std::decay_t<U>::LhsType; typename std::decay_t<U>::RhsType;} && std::is_base_of_v<NotEqual<typename std::decay_t<U>::LhsType, typename std::decay_t<U>::RhsType>, std::decay_t<U>>;

/// @brief Satisfied when `U` is any `LessEqual<L,R>` comparison pattern (or derived type).
template<typename U>
concept isAnyLessEqual = requires {typename std::decay_t<U>::LhsType; typename std::decay_t<U>::RhsType;} && std::is_base_of_v<LessEqual<typename std::decay_t<U>::LhsType, typename std::decay_t<U>::RhsType>, std::decay_t<U>>;

/// @brief Satisfied when `U` is any `GreaterEqual<L,R>` comparison pattern (or derived type).
template<typename U>
concept isAnyGreaterEqual = requires {typename std::decay_t<U>::LhsType; typename std::decay_t<U>::RhsType;} && std::is_base_of_v<GreaterEqual<typename std::decay_t<U>::LhsType, typename std::decay_t<U>::RhsType>, std::decay_t<U>>;

/// @brief Satisfied when `U` is any `Min<L,R>` pattern (or derived type).
template<typename U>
concept isAnyMin = requires {typename std::decay_t<U>::LhsType; typename std::decay_t<U>::RhsType;} && std::is_base_of_v<Min<typename std::decay_t<U>::LhsType, typename std::decay_t<U>::RhsType>, std::decay_t<U>>;

/// @brief Satisfied when `U` is any `Max<L,R>` pattern (or derived type).
template<typename U>
concept isAnyMax = requires {typename std::decay_t<U>::LhsType; typename std::decay_t<U>::RhsType;} && std::is_base_of_v<Max<typename std::decay_t<U>::LhsType, typename std::decay_t<U>::RhsType>, std::decay_t<U>>;

/**
 * @brief Satisfied when `Derived` is a `PatternNode` with a known `Nbranches`.
 * @details Used internally to detect multi-branch pattern nodes independent of the
 *          specific operation type.
 */
template<typename Derived>
concept isPatternNode = requires {
    std::decay_t<Derived>::Nbranches;
} && std::is_base_of_v<PatternNode<std::decay_t<Derived>, std::decay_t<Derived>::Nbranches>, std::decay_t<Derived>>;

/**
 * @brief Satisfied when `Derived` is a `PatternBinaryOp` with `LhsType` and `RhsType`.
 */
template<typename Derived>
concept isPatternBinaryOp = requires {
    typename std::decay_t<Derived>::LhsType;
    typename std::decay_t<Derived>::RhsType;
} && std::is_base_of_v<PatternBinaryOp<std::decay_t<Derived>, typename std::decay_t<Derived>::LhsType, typename std::decay_t<Derived>::RhsType>, std::decay_t<Derived>>;

/**
 * @brief Satisfied when `Derived` is a `PatternUnaryOp` with an `ArgType`.
 */
template<typename Derived>
concept isPatternUnaryOp = requires {
    typename std::decay_t<Derived>::ArgType;
} && std::is_base_of_v<PatternUnaryOp<std::decay_t<Derived>, typename std::decay_t<Derived>::ArgType>, std::decay_t<Derived>>;


// ======================== Base structs ========================


// ======================== Nodes ========================

/**
 * @brief Base for all pattern nodes that have a fixed number of child branches.
 * @tparam Derived  The concrete pattern type (CRTP).
 * @tparam Branches Number of child branches (1 for unary, 2 for binary, etc.).
 */
template<typename Derived, size_t Branches>
struct PatternNode : public Pattern {
    static constexpr size_t Nbranches = Branches;
};

/**
 * @brief CRTP base for binary-operator pattern nodes.
 * @tparam Derived The concrete pattern type.
 * @tparam L       Pattern type for the left-hand operand.
 * @tparam R       Pattern type for the right-hand operand.
 *
 * Exposes `LhsType` and `RhsType` aliases and a `MakeNew<LNEW,RNEW>` alias for
 * rebuilding the pattern with different child types (used in pattern transformation).
 */
template<typename Derived, typename L, typename R>
struct PatternBinaryOp : public PatternNode<Derived, 2> {
    using LhsType = L;
    using RhsType = R;

    template<isAnyPattern LNEW, isAnyPattern RNEW>
    using MakeNew = PatternBinaryOp<Derived, LNEW, RNEW>;
};

/**
 * @brief CRTP base for unary-operator pattern nodes.
 * @tparam Derived The concrete pattern type.
 * @tparam Arg     Pattern type for the single child operand.
 *
 * Exposes `ArgType` and a `MakeNew<ArgNEW>` alias for rebuilding with a new child.
 */
template<typename Derived, typename Arg>
struct PatternUnaryOp : public PatternNode<Derived, 1> {
    using ArgType = Arg;

    template<isAnyPattern ArgNEW>
    using MakeNew = PatternUnaryOp<Derived, ArgNEW>;
};

// ======================== Binary operations ========================

/**
 * @brief Pattern type for addition (`L + R`).
 * @tparam L Pattern of the left operand.
 * @tparam R Pattern of the right operand.
 */
template<typename L, typename R>
struct Addition : public PatternBinaryOp<Addition<L, R>, L, R> {
    template<typename LNEW, typename RNEW>
    using MakeNew = Addition<LNEW, RNEW>;
};

/**
 * @brief Pattern type for subtraction (`L - R`).
 * @tparam L Pattern of the left operand.
 * @tparam R Pattern of the right operand.
 */
template<typename L, typename R>
struct Subtraction : public PatternBinaryOp<Subtraction<L, R>, L, R> {
    template<typename LNEW, typename RNEW>
    using MakeNew = Subtraction<LNEW, RNEW>;
};

/**
 * @brief Pattern type for multiplication (`L * R`).
 * @tparam L Pattern of the left operand.
 * @tparam R Pattern of the right operand.
 */
template<typename L, typename R>
struct Multiplication : public PatternBinaryOp<Multiplication<L, R>, L, R> {
    template<typename LNEW, typename RNEW>
    using MakeNew = Multiplication<LNEW, RNEW>;
};

/**
 * @brief Pattern type for division (`L / R`).
 * @tparam L Pattern of the left operand.
 * @tparam R Pattern of the right operand.
 */
template<typename L, typename R>
struct Division : public PatternBinaryOp<Division<L, R>, L, R> {
    template<typename LNEW, typename RNEW>
    using MakeNew = Division<LNEW, RNEW>;
};

/**
 * @brief Pattern type for exponentiation (`pow(L, R)`).
 * @tparam L Pattern of the base.
 * @tparam R Pattern of the exponent.
 */
template<typename L, typename R>
struct Power : public PatternBinaryOp<Power<L, R>, L, R> {
    template<typename LNEW, typename RNEW>
    using MakeNew = Power<LNEW, RNEW>;
};

// ======================== Unary operations ========================

/**
 * @brief Pattern type for unary negation (`-Arg`).
 * @tparam Arg Pattern of the operand.
 */
template<typename Arg>
struct Negation : public PatternUnaryOp<Negation<Arg>, Arg> {
    template<typename ArgNEW>
    using MakeNew = Negation<ArgNEW>;
};

/**
 * @brief Pattern type for absolute value (`abs(Arg)`).
 * @tparam Arg Pattern of the operand.
 */
template<typename Arg>
struct AbsoluteValue : public PatternUnaryOp<AbsoluteValue<Arg>, Arg> {
    template<typename ArgNEW>
    using MakeNew = AbsoluteValue<ArgNEW>;
};

/**
 * @brief Pattern type for square root (`sqrt(Arg)`).
 * @tparam Arg Pattern of the operand.
 */
template<typename Arg>
struct SquareRoot : public PatternUnaryOp<SquareRoot<Arg>, Arg> {
    template<typename ArgNEW>
    using MakeNew = SquareRoot<ArgNEW>;
};

// ======================== Comparison operations ========================

/**
 * @brief Common base for all comparison/relational pattern types.
 * @tparam Derived Concrete comparison pattern (CRTP).
 * @tparam L       Pattern of the left-hand operand.
 * @tparam R       Pattern of the right-hand operand.
 */
template<typename Derived, typename L, typename R>
struct ComparisonPattern : public PatternBinaryOp<Derived, L, R> {};

/// @brief Pattern type for `L < R`.
template<typename L, typename R>
struct LessThan : public ComparisonPattern<LessThan<L, R>, L, R> {
    template<typename LNEW, typename RNEW>
    using MakeNew = LessThan<LNEW, RNEW>;
};

/// @brief Pattern type for `L > R`.
template<typename L, typename R>
struct GreaterThan : public ComparisonPattern<GreaterThan<L, R>, L, R> {
    template<typename LNEW, typename RNEW>
    using MakeNew = GreaterThan<LNEW, RNEW>;
};

/// @brief Pattern type for `L == R`.
template<typename L, typename R>
struct Equal : public ComparisonPattern<Equal<L, R>, L, R> {
    template<typename LNEW, typename RNEW>
    using MakeNew = Equal<LNEW, RNEW>;
};

/// @brief Pattern type for `L != R`.
template<typename L, typename R>
struct NotEqual : public ComparisonPattern<NotEqual<L, R>, L, R> {
    template<typename LNEW, typename RNEW>
    using MakeNew = NotEqual<LNEW, RNEW>;
};

/// @brief Pattern type for `L <= R`.
template<typename L, typename R>
struct LessEqual : public ComparisonPattern<LessEqual<L, R>, L, R> {
    template<typename LNEW, typename RNEW>
    using MakeNew = LessEqual<LNEW, RNEW>;
};

/// @brief Pattern type for `L >= R`.
template<typename L, typename R>
struct GreaterEqual : public ComparisonPattern<GreaterEqual<L, R>, L, R> {
    template<typename LNEW, typename RNEW>
    using MakeNew = GreaterEqual<LNEW, RNEW>;
};

// ======================== Min/Max operations ========================

/// @brief Pattern type for `min(L, R)`.
template<typename L, typename R>
struct Min : public PatternBinaryOp<Min<L, R>, L, R> {
    template<typename LNEW, typename RNEW>
    using MakeNew = Min<LNEW, RNEW>;
};

/// @brief Pattern type for `max(L, R)`.
template<typename L, typename R>
struct Max : public PatternBinaryOp<Max<L, R>, L, R> {
    template<typename LNEW, typename RNEW>
    using MakeNew = Max<LNEW, RNEW>;
};

// ======================== Pattern requirement ========================

/**
 * @brief Constraint macro: true when at least one of `L`, `R` is a pattern type.
 *
 * Used as a `requires` constraint on the pattern operator overloads to ensure they
 * only fire for pattern arguments and do not shadow runtime arithmetic.
 */
#define LAZY_PATTERN_REQUIREMENT(L, R) (isAnyPattern<L> || isAnyPattern<R>)

/**
 * @brief Constraint macro for unary pattern operators: true when `Arg` is a pattern.
 */
#define LAZY_UNARY_PATTERN_REQUIREMENT(Arg) (isAnyPattern<Arg>)
// ======================== Operator overloads ========================
//
// All operators below take pattern arguments *by value* and return a new pattern
// type.  They are purely compile-time: invoking them creates a zero-cost type-level
// composition.  Because they require LAZY_PATTERN_REQUIREMENT, they only match when at
// least one operand is already a Pattern, so they do not interfere with runtime
// arithmetic overloads in lazy.hpp.

/// @brief Compose two patterns into `Addition<L,R>`.
template<typename L, typename R>
requires LAZY_PATTERN_REQUIREMENT(L, R)
constexpr auto operator+(L, R) {
    return Addition<L, R>{};
}

/// @brief Compose two patterns into `Subtraction<L,R>`.
template<typename L, typename R>
requires LAZY_PATTERN_REQUIREMENT(L, R)
constexpr auto operator-(L, R) {
    return Subtraction<L, R>{};
}

/// @brief Compose two patterns into `Multiplication<L,R>`.
template<typename L, typename R>
requires LAZY_PATTERN_REQUIREMENT(L, R)
constexpr auto operator*(L, R) {
    return Multiplication<L, R>{};
}

/// @brief Compose two patterns into `Division<L,R>`.
template<typename L, typename R>
requires LAZY_PATTERN_REQUIREMENT(L, R)
constexpr auto operator/(L, R) {
    return Division<L, R>{};
}

/// @brief Build a `Negation<Arg>` pattern from a unary minus on a pattern.
template<typename Arg>
requires LAZY_UNARY_PATTERN_REQUIREMENT(Arg)
constexpr auto operator-(Arg) {
    return Negation<Arg>{};
}

/// @brief Build a `Power<Base,Exp>` pattern.
template<typename Base, typename Exp>
requires LAZY_PATTERN_REQUIREMENT(Base, Exp)
constexpr auto pow(Base, Exp) {
    return Power<Base, Exp>{};
}

/// @brief Build an `AbsoluteValue<Arg>` pattern.
template<typename Arg>
requires LAZY_UNARY_PATTERN_REQUIREMENT(Arg)
constexpr auto abs(Arg) {
    return AbsoluteValue<Arg>{};
}

/// @brief Build a `SquareRoot<Arg>` pattern.
template<typename Arg>
requires LAZY_UNARY_PATTERN_REQUIREMENT(Arg)
constexpr auto sqrt(Arg) {
    return SquareRoot<Arg>{};
}

/// @brief Build a `Min<L,R>` pattern.
template<typename L, typename R>
requires LAZY_PATTERN_REQUIREMENT(L, R)
constexpr auto min(L, R) {
    return Min<L, R>{};
}

/// @brief Build a `Max<L,R>` pattern.
template<typename L, typename R>
requires LAZY_PATTERN_REQUIREMENT(L, R)
constexpr auto max(L, R) {
    return Max<L, R>{};
}

// ======================== Comparison operator overloads ========================

/// @brief Build a `LessThan<L,R>` comparison pattern.
template<typename L, typename R>
requires LAZY_PATTERN_REQUIREMENT(L, R)
constexpr auto operator<(L, R) {
    return LessThan<L, R>{};
}

/// @brief Build a `GreaterThan<L,R>` comparison pattern.
template<typename L, typename R>
requires LAZY_PATTERN_REQUIREMENT(L, R)
constexpr auto operator>(L, R) {
    return GreaterThan<L, R>{};
}

/// @brief Build an `Equal<L,R>` comparison pattern.
template<typename L, typename R>
requires LAZY_PATTERN_REQUIREMENT(L, R)
constexpr auto operator==(L, R) {
    return Equal<L, R>{};
}

/// @brief Build a `NotEqual<L,R>` comparison pattern.
template<typename L, typename R>
requires LAZY_PATTERN_REQUIREMENT(L, R)
constexpr auto operator!=(L, R) {
    return NotEqual<L, R>{};
}

/// @brief Build a `LessEqual<L,R>` comparison pattern.
template<typename L, typename R>
requires LAZY_PATTERN_REQUIREMENT(L, R)
constexpr auto operator<=(L, R) {
    return LessEqual<L, R>{};
}

/// @brief Build a `GreaterEqual<L,R>` comparison pattern.
template<typename L, typename R>
requires LAZY_PATTERN_REQUIREMENT(L, R)
constexpr auto operator>=(L, R) {
    return GreaterEqual<L, R>{};
}

} // namespace lazy

#endif // PATTERNS_HPP
