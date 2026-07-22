#ifndef PATTERNS_HPP
#define PATTERNS_HPP

/**
 * @file patterns.hpp
 * @brief Compile-time symbolic expression pattern types for the lazy evaluation library.
 *
 * This header defines a hierarchy of pure type-level "pattern" descriptors that mirror
 * the compile-time expression tree defined in lazy.hpp.  Patterns have no data members and
 * exist solely at the type level, enabling compile-time structural matching over
 * expression trees.
 */

#include <type_traits>
#include <cstddef>

// ======================== Pattern requirement ========================

/**
 * @brief Constraint macro: true when at least one of `L`, `R` is a pattern type.
 *
 * Used as a `requires` constraint on the pattern operator overloads to ensure they
 * only fire for pattern arguments and do not shadow runtime arithmetic.
 */
#define LAZY_PATTERN_REQUIREMENT(L, R) (traits::isAnyPattern<L> || traits::isAnyPattern<R>)

/**
 * @brief Constraint macro for unary pattern operators: true when `Arg` is a pattern.
 */
#define LAZY_UNARY_PATTERN_REQUIREMENT(Arg) (traits::isAnyPattern<Arg>)


#define LAZY_UNARY_PATTERN_OP(NAME) \
template<typename Arg> \
struct NAME : public PatternUnaryOp<NAME<Arg>, Arg> { \
    template<typename ArgNEW> \
    using MakeNew = NAME<ArgNEW>; \
};

#define LAZY_BINARY_PATTERN_OP(NAME) \
template<typename L, typename R> \
struct NAME : public PatternBinaryOp<NAME<L,R>, L, R> { \
    template<typename LNEW, typename RNEW> \
    using MakeNew = NAME<LNEW, RNEW>; \
};

#define LAZY_COMPARISON_PATTERN_OP(NAME) \
template<typename L, typename R> \
struct NAME : public ComparisonPattern<NAME<L, R>, L, R> { \
    template<typename LNEW, typename RNEW> \
    using MakeNew = NAME<LNEW, RNEW>; \
};

namespace lazy::patterns {
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





namespace traits{
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


} // namespace traits


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

    template<traits::isAnyPattern LNEW, traits::isAnyPattern RNEW>
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

    template<traits::isAnyPattern ArgNEW>
    using MakeNew = PatternUnaryOp<Derived, ArgNEW>;
};


/**
 * @brief Common base for all comparison/relational pattern types.
 * @tparam Derived Concrete comparison pattern (CRTP).
 * @tparam L       Pattern of the left-hand operand.
 * @tparam R       Pattern of the right-hand operand.
 */
template<typename Derived, typename L, typename R>
struct ComparisonPattern : public PatternBinaryOp<Derived, L, R> {};


// ======================== Binary operations ========================


LAZY_BINARY_PATTERN_OP(Addition)
LAZY_BINARY_PATTERN_OP(Subtraction)
LAZY_BINARY_PATTERN_OP(Multiplication)
LAZY_BINARY_PATTERN_OP(Division)
LAZY_BINARY_PATTERN_OP(Power)
LAZY_BINARY_PATTERN_OP(Min)
LAZY_BINARY_PATTERN_OP(Max)

// ======================== Unary operations ========================


LAZY_UNARY_PATTERN_OP(Negation)
LAZY_UNARY_PATTERN_OP(AbsoluteValue)
LAZY_UNARY_PATTERN_OP(SquareRoot)
LAZY_UNARY_PATTERN_OP(Exponential)
LAZY_UNARY_PATTERN_OP(Logarithm)
LAZY_UNARY_PATTERN_OP(Sine)
LAZY_UNARY_PATTERN_OP(Cosine)
LAZY_UNARY_PATTERN_OP(Tangent)
LAZY_UNARY_PATTERN_OP(Cotangent)
LAZY_UNARY_PATTERN_OP(Secant)
LAZY_UNARY_PATTERN_OP(Cosecant)
LAZY_UNARY_PATTERN_OP(ArcSine)
LAZY_UNARY_PATTERN_OP(ArcCosine)
LAZY_UNARY_PATTERN_OP(ArcTangent)
LAZY_UNARY_PATTERN_OP(ArcCotangent)
LAZY_UNARY_PATTERN_OP(ArcSecant)
LAZY_UNARY_PATTERN_OP(ArcCosecant)
LAZY_UNARY_PATTERN_OP(HyperbolicSine)
LAZY_UNARY_PATTERN_OP(HyperbolicCosine)
LAZY_UNARY_PATTERN_OP(HyperbolicTangent)
LAZY_UNARY_PATTERN_OP(ErrorFunction)

// ======================== Comparison operations ========================

LAZY_COMPARISON_PATTERN_OP(LessThan)
LAZY_COMPARISON_PATTERN_OP(GreaterThan)
LAZY_COMPARISON_PATTERN_OP(Equal)
LAZY_COMPARISON_PATTERN_OP(NotEqual)
LAZY_COMPARISON_PATTERN_OP(LessEqual)
LAZY_COMPARISON_PATTERN_OP(GreaterEqual)

} // namespace lazy::patterns

#endif // PATTERNS_HPP
