#ifndef LAZY_UNARY_OPS_HPP
#define LAZY_UNARY_OPS_HPP


#include "lazy_core.hpp"


namespace lazy::detail{

/// @brief `e.g. Abs<T, Arg>` node and lazy `abs(U&&)` overload

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
    return os << T(expr);
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