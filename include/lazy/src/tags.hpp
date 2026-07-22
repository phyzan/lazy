#ifndef LAZY_TAGS_HPP
#define LAZY_TAGS_HPP

namespace lazy::tags{


// ============================================================================
// Operation tag types
// ============================================================================
// Tags are empty structs used as the first argument to `evaluate` / `eval_rule`
// to discriminate which arithmetic operation is being requested.  The hierarchy
// is: Tag <- specific arithmetic/comparison tags.
//            BOOL_TAG <- comparison-specific tags.

/// @brief Root tag base.  All operation tags inherit from this.
struct Tag{};


// ====================== BINARY OPERATOR TAGS ===========================
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

// ====================== COMPARISON OPERATOR TAGS =======================
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

#endif // LAZY_TAGS_HPP