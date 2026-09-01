#ifndef LAZY_UNARY_OPS_HPP
#define LAZY_UNARY_OPS_HPP

#include <iostream>
#include "unary_decls.hpp"



namespace lazy::detail{


/**
 * @brief Default evaluation policy for unary operations.
 */
template<typename Derived, typename T>
struct UnaryEvaluator : NodalEvaluator<Derived, T> {

    template<lazy::traits::isTag tag, typename Arg>
    inline static void evaluate(tag, T& /*out*/, Pool<T> /*workers*/, const Arg& /*a*/){
        static_assert(false, "UnaryEvaluator::evaluate must be specialised for each performed operation");
    }

    // Bypassing the `eval_rule` dispatch for unary operations, for clarity only. Performance should not change.
    template<lazy::traits::isTag tag, lazy::traits::isLazyExpr<T> Branch>
    LAZY_FORCE_INLINE static void eval_rule(tag, T& out, Pool<T> workers, const Branch& arg){
        if constexpr (lazy::traits::isNode<Branch, T>) {
            Derived::evaluate(tag{}, out, workers, arg.eval_impl(out, workers));
        } else {
            Derived::evaluate(tag{}, out, workers, get_value(arg));
        }
    }
};


/**
 * @brief Primary unary-operation rules for type `T`.
 *
 * Analogous to `CustomBinaryEvaluator`.  Specialise (via `LAZY_SPECIALIZE_FUNCTIONS`) to
 * provide custom `evaluate` overloads for unary functions such as `abs`, `sqrt`,
 * `neg`, etc.
 *
 * @tparam T The arithmetic value type to specialise for.
 */
template<typename T>
struct CustomUnaryEvaluator : public UnaryEvaluator<CustomUnaryEvaluator<T>, T>{};



template<typename Derived, typename T, typename Arg>
struct Unary : public Node<Derived, T, Arg>{

    static_assert(traits::isLazyExpr<Arg, T>, "Unary node argument must be an expression");

    using Base = Node<Derived, T, Arg>;
    using branch_t = std::tuple<Arg>;
    using Base::Base;
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

template<typename F>
requires lazy::traits::isNode<F, typename std::decay_t<F>::lazy_value_type>
std::ostream& operator<<(std::ostream& os, const F& expr){
    return os << expr.eval_worker();
}

template<typename T>
std::ostream& operator<<(std::ostream& os, const LazyType<T>& expr){
    return os << get_value(expr);
}

template<typename T>
std::ostream& operator<<(std::ostream& os, const RefType<T>& expr){
    return os << get_value(expr);
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

} // namespace lazy


#endif // LAZY_UNARY_OPS_HPP