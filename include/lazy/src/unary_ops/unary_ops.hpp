#ifndef LAZY_UNARY_OPS_HPP
#define LAZY_UNARY_OPS_HPP

#include <iostream>
#include "unary_decls.hpp"



namespace lazy::detail{


/**
 * @brief Default evaluation policy for unary operations.
 */
template<typename Derived, typename T>
struct UnaryOpRules : NodalEvaluator<Derived, T> {

    template<lazy::traits::isTag tag, typename Arg>
    inline static void evaluate(tag, T& out, const Arg& a);
};





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
 * @brief CRTP base for single-argument (unary) expression nodes.
 *
 * Stores the single child expression `arg` by value and evaluates via
 * `Derived::eval_rule(tag{}, out, arg)`.  The `UnaryOpRules` default path
 * evaluates `arg` into a scratch slot if it is a `Node`, then calls
 * `Derived::evaluate(tag{}, out, raw_arg)`.
 *
 * @tparam Derived The concrete unary node type (e.g. `Neg<T,Arg>`).
 * @tparam T       The arithmetic value type.
 * @tparam Arg     The type of the single child sub-expression (`isLazyExpr<Arg,T>`).
 */
template<typename Derived, typename T, typename Arg>
struct Unary : public Node<Derived, T, 1>{

    static_assert(traits::isLazyExpr<Arg, T>, "Unary node argument must be an expression");

    using Base = Node<Derived, T, 1>;
    using branch_t = std::tuple<Arg>;
    static constexpr bool isUnary = true;

    template<typename Arg2>
    requires( requires {typename std::decay_t<Arg>::value_type;} && traits::isLazyExpr<std::decay_t<Arg>, typename std::decay_t<Arg>::value_type> )
    LAZY_FORCE_INLINE Unary(Arg2&& arg) : arg(std::forward<Arg2>(arg)) {}

    LAZY_FORCE_INLINE T& eval(T& out) const{
        Derived::eval_rule(typename Derived::tag{}, out, make_expr<T>(arg));
        return out;
    }

    Arg arg;
};



/// @brief `e.g. Abs<T, Arg>` node and lazy `abs(U&&)` overload

LAZY_DEFINE_UNARY_OP(operator-, Neg, lazy::tags::NEG)
LAZY_DEFINE_UNARY_OP(abs, Abs, lazy::tags::ABS)
LAZY_DEFINE_UNARY_OP(sqrt, Sqrt, lazy::tags::SQRT)
LAZY_DEFINE_UNARY_OP(exp, Exp, lazy::tags::EXP)
LAZY_DEFINE_UNARY_OP(log, Log, lazy::tags::LOG)
LAZY_DEFINE_UNARY_OP(sin, Sin, lazy::tags::SIN)
LAZY_DEFINE_UNARY_OP(cos, Cos, lazy::tags::COS)
LAZY_DEFINE_UNARY_OP(tan, Tan, lazy::tags::TAN)
LAZY_DEFINE_UNARY_OP(cot, Cot, lazy::tags::COT)
LAZY_DEFINE_UNARY_OP(sec, Sec, lazy::tags::SEC)
LAZY_DEFINE_UNARY_OP(csc, Csc, lazy::tags::CSC)
LAZY_DEFINE_UNARY_OP(asin, Asin, lazy::tags::ASIN)
LAZY_DEFINE_UNARY_OP(acos, Acos, lazy::tags::ACOS)
LAZY_DEFINE_UNARY_OP(atan, Atan, lazy::tags::ATAN)
LAZY_DEFINE_UNARY_OP(acot, Acot, lazy::tags::ACOT)
LAZY_DEFINE_UNARY_OP(asec, Asec, lazy::tags::ASEC)
LAZY_DEFINE_UNARY_OP(acsc, Acsc, lazy::tags::ACSC)
LAZY_DEFINE_UNARY_OP(sinh, Sinh, lazy::tags::SINH)
LAZY_DEFINE_UNARY_OP(cosh, Cosh, lazy::tags::COSH)
LAZY_DEFINE_UNARY_OP(tanh, Tanh, lazy::tags::TANH)
LAZY_DEFINE_UNARY_OP(erf, Erf, lazy::tags::ERF)

template<lazy::traits::isAnyLazyExpr F>
std::ostream& operator<<(std::ostream& os, const F& expr){
    using T = typename F::value_type;
    T value = expr;
    return os << value;
}

template<typename T>
std::ostream& operator<<(std::ostream& os, const LazyType<T>& expr){
    return os << expr.value();
}

template<typename T>
std::ostream& operator<<(std::ostream& os, const RefType<T>& expr){
    return os << expr.value();
}

} // namespace lazy::detail


namespace lazy{

using lazy::detail::operator<<;

using lazy::detail::abs, 
      lazy::detail::sqrt,
      lazy::detail::exp,
      lazy::detail::log,
      lazy::detail::sin,
      lazy::detail::cos,
      lazy::detail::tan,
      lazy::detail::cot,
      lazy::detail::sec,
      lazy::detail::csc,
      lazy::detail::asin,
      lazy::detail::acos,
      lazy::detail::atan,
      lazy::detail::acot,
      lazy::detail::asec,
      lazy::detail::acsc,
      lazy::detail::sinh,
      lazy::detail::cosh,
      lazy::detail::tanh,
      lazy::detail::erf;

} // namespace lazy::detail


#endif // LAZY_UNARY_OPS_HPP