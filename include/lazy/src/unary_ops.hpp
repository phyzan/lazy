#ifndef LAZY_UNARY_OPS_HPP
#define LAZY_UNARY_OPS_HPP


#include "lazy_core.hpp"


namespace lazy::detail{

/// @brief `Abs<T,Arg>` node and lazy `abs(U&&)` overload, defined via `LAZY_DEFINE_UNARY_OP`.
LAZY_DEFINE_UNARY_OP(abs, Abs, ABS, AbsoluteValue)

/// @brief `Sqrt<T,Arg>` node and lazy `sqrt(U&&)` overload, defined via `LAZY_DEFINE_UNARY_OP`.
LAZY_DEFINE_UNARY_OP(sqrt, Sqrt, SQRT, SquareRoot)

} // namespace lazy::detail


namespace lazy{
    
using lazy::detail::abs, 
      lazy::detail::sqrt;

} // namespace lazy


#endif // LAZY_UNARY_OPS_HPP