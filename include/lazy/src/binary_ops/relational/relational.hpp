#ifndef LAZY_RELATIONAL_HPP
#define LAZY_RELATIONAL_HPP


#include "../../core.hpp"
#include "relational_decls.hpp"


namespace lazy::detail{



/**
 * @brief Evaluation rules for relational comparisons.
 */
template<typename T>
struct CompareRules : public BinaryOpRules<CompareRules<T>, T>{

    template<traits::isBoolTag tag, typename L, typename R>
    LAZY_FORCE_INLINE static void evaluate(tag, auto& out, const L& a, const R& b){
        out = get_bool(tag{}, a, b);
    }

    template<traits::isBoolTag tag, typename L, typename R>
    inline static bool get_bool(tag, const L& a, const R& b){
        if constexpr (std::is_same_v<tag, lazy::tags::EQ>) {
            return a == b;
        } else if constexpr (std::is_same_v<tag, lazy::tags::NEQ>) {
            return a != b;
        } else if constexpr (std::is_same_v<tag, lazy::tags::GT>) {
            return a > b;
        } else if constexpr (std::is_same_v<tag, lazy::tags::LT>) {
            return a < b;
        } else if constexpr (std::is_same_v<tag, lazy::tags::GE>) {
            return a >= b;
        } else if constexpr (std::is_same_v<tag, lazy::tags::LE>) {
            return a <= b;
        } else {
            static_assert(traits::isBoolTag<tag>, "Invalid boolean tag");
            return false; // should never reach here
        }
    }

};


/**
 * @brief CRTP base for relational (boolean-valued) binary expression nodes.
 *
 * Inherits from both `BinaryOperator<Derived,T,L,R>` (to participate in the
 * arithmetic tree as a node) and `CompareRules<T>` (to get `eval_bool` and
 * `get_bool`).
 *
 * The `operator bool()` implicit conversion evaluates the comparison lazily:
 * it calls `Derived::eval_rule(tag{}, out, this->lhs, this->rhs)`, which internally
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
        bool out;
        Derived::eval_rule(typename Derived::tag{}, out, this->lhs, this->rhs);
        return out;
    }
};


/// @brief Lazy equal-to comparison node (`lhs == rhs`).  `operator bool()` returns the result.
template<typename T, typename L, typename R>
struct Eq : public Comparison<Eq<T, L, R>, T, L, R>{
    using Base = Comparison<Eq<T, L, R>, T, L, R>;
    using Base::Base;
    using tag = lazy::tags::EQ;
};

/// @brief Lazy not-equal comparison node (`lhs != rhs`).  `operator bool()` returns the result.
template<typename T, typename L, typename R>
struct Neq : public Comparison<Neq<T, L, R>, T, L, R>{
    using Base = Comparison<Neq<T, L, R>, T, L, R>;
    using Base::Base;
    using tag = lazy::tags::NEQ;
};


/// @brief Lazy greater-than comparison node (`lhs > rhs`).  `operator bool()` returns the result.
template<typename T, typename L, typename R>
struct Gt : public Comparison<Gt<T, L, R>, T, L, R>{
    using Base = Comparison<Gt<T, L, R>, T, L, R>;
    using Base::Base;
    using tag = lazy::tags::GT;
};

/// @brief Lazy less-than comparison node (`lhs < rhs`).  `operator bool()` returns the result.
template<typename T, typename L, typename R>
struct Lt : public Comparison<Lt<T, L, R>, T, L, R>{
    using Base = Comparison<Lt<T, L, R>, T, L, R>;
    using Base::Base;
    using tag = lazy::tags::LT;
};

/// @brief Lazy greater-or-equal comparison node (`lhs >= rhs`).  `operator bool()` returns the result.
template<typename T, typename L, typename R>
struct Ge : public Comparison<Ge<T, L, R>, T, L, R>{
    using Base = Comparison<Ge<T, L, R>, T, L, R>;
    using Base::Base;
    using tag = lazy::tags::GE;
};

/// @brief Lazy less-or-equal comparison node (`lhs <= rhs`).  `operator bool()` returns the result.
template<typename T, typename L, typename R>
struct Le : public Comparison<Le<T, L, R>, T, L, R>{
    using Base = Comparison<Le<T, L, R>, T, L, R>;
    using Base::Base;
    using tag = lazy::tags::LE;
};



LAZY_DEFINE_RELATIONAL_OP(==, Eq)
LAZY_DEFINE_RELATIONAL_OP(!=, Neq)
LAZY_DEFINE_RELATIONAL_OP(>, Gt)
LAZY_DEFINE_RELATIONAL_OP(<, Lt)
LAZY_DEFINE_RELATIONAL_OP(>=, Ge)
LAZY_DEFINE_RELATIONAL_OP(<=, Le)

} // namespace lazy::detail


namespace lazy{
    
using lazy::detail::operator==, 
      lazy::detail::operator!=, 
      lazy::detail::operator>, 
      lazy::detail::operator<, 
      lazy::detail::operator>=, 
      lazy::detail::operator<=;

} // namespace lazy


#endif // LAZY_RELATIONAL_HPP