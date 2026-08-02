#ifndef LAZY_BINOP_DECLS_HPP
#define LAZY_BINOP_DECLS_HPP


#include "../rules.hpp"

/**
 * @brief Constraint: at least one of `L`, `R` is a lazy expression type. The other type may be a lazy expression or a raw value convertible to `T`.
 *
 * Expands to an `&&`-expression that is `true` when `std::decay_t<L>` or
 * `std::decay_t<R>` (or both) expose a `value_type` alias and derive from
 * `ExprBase<value_type>`.  Used as a `requires` clause on all arithmetic and
 * relational operator overloads to ensure they only activate for expression
 * operands and do not shadow built-in arithmetic.
 */
#define LAZY_REQUIREMENT(L, R)\
    ( (requires {typename std::decay_t<L>::value_type;} && lazy::traits::isLazyExpr<std::decay_t<L>, typename std::decay_t<L>::value_type> && lazy::traits::isValidType<std::decay_t<R>, typename std::decay_t<L>::value_type>) || \
      (requires {typename std::decay_t<R>::value_type;} && lazy::traits::isLazyExpr<std::decay_t<R>, typename std::decay_t<R>::value_type> && lazy::traits::isValidType<std::decay_t<L>, typename std::decay_t<R>::value_type>) )


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


namespace lazy::tags{

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

} // namespace lazy::tags

namespace lazy::detail {


template<typename Derived, typename T, typename L, typename R> struct BinaryOperator;
template<typename T, typename L, typename R> struct Add;
template<typename T, typename L, typename R> struct Sub;
template<typename T, typename L, typename R> struct Mul;
template<typename T, typename L, typename R> struct Div;
template<typename T, typename L, typename R> struct Pow;
template<typename T, typename L, typename R> struct MaxLazy;
template<typename T, typename L, typename R> struct MinLazy;

// rules
template<typename Derived, typename T> struct BinaryOpRules;
template<typename T> struct CustomBinaryRules;



// Binary type getter
template<typename T, typename L, typename R>
struct TypeGetter<T, lazy::tags::PLUS, L, R>{
    using type = lazy::detail::Add<T, L, R>;
};

template<typename T, typename L, typename R>
struct TypeGetter<T, lazy::tags::MINUS, L, R>{
    using type = lazy::detail::Sub<T, L, R>;
};

template<typename T, typename L, typename R>
struct TypeGetter<T, lazy::tags::MUL, L, R>{
    using type = lazy::detail::Mul<T, L, R>;
};

template<typename T, typename L, typename R>
struct TypeGetter<T, lazy::tags::DIV, L, R>{
    using type = lazy::detail::Div<T, L, R>;
};

template<typename T, typename L, typename R>
struct TypeGetter<T, lazy::tags::POW, L, R>{
    using type = lazy::detail::Pow<T, L, R>;
};

template<typename T, typename L, typename R>
struct TypeGetter<T, lazy::tags::MAX, L, R>{
    using type = lazy::detail::MaxLazy<T, L, R>;
};
template<typename T, typename L, typename R>
struct TypeGetter<T, lazy::tags::MIN, L, R>{
    using type = lazy::detail::MinLazy<T, L, R>;
};

} // namespace lazy::detail



namespace lazy::traits{


template<typename Derived, typename T>
concept isBinOp = requires
    {typename Derived::LhsType; typename Derived::RhsType;} &&
    std::is_base_of_v<lazy::detail::BinaryOperator<Derived, T, typename Derived::LhsType, typename Derived::RhsType>, Derived>;

template<typename Derived, typename T>
concept isAdd = requires { requires isBinOp<Derived, T>; } && std::is_base_of_v<lazy::detail::Add<T, typename Derived::LhsType, typename Derived::RhsType>, Derived>;

template<typename Derived, typename T>
concept isSub = requires { requires isBinOp<Derived, T>; } && std::is_base_of_v<lazy::detail::Sub<T, typename Derived::LhsType, typename Derived::RhsType>, Derived>;

template<typename Derived, typename T>
concept isMul = requires { requires isBinOp<Derived, T>; } && std::is_base_of_v<lazy::detail::Mul<T, typename Derived::LhsType, typename Derived::RhsType>, Derived>;

template<typename Derived, typename T>
concept isDiv = requires { requires isBinOp<Derived, T>; } && std::is_base_of_v<lazy::detail::Div<T, typename Derived::LhsType, typename Derived::RhsType>, Derived>;

template<typename Derived, typename T>
concept isPow = requires { requires isBinOp<Derived, T>; } && std::is_base_of_v<lazy::detail::Pow<T, typename Derived::LhsType, typename Derived::RhsType>, Derived>;

template<typename Derived, typename T>
concept isMin = requires { requires isBinOp<Derived, T>; } && std::is_base_of_v<lazy::detail::MinLazy<T, typename Derived::LhsType, typename Derived::RhsType>, Derived>;

template<typename Derived, typename T>
concept isMax = requires { requires isBinOp<Derived, T>; } && std::is_base_of_v<lazy::detail::MaxLazy<T, typename Derived::LhsType, typename Derived::RhsType>, Derived>;

} // namespace lazy::traits

#endif // LAZY_BINOP_DECLS_HPP