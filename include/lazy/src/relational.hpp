#ifndef LAZY_RELATIONAL_HPP
#define LAZY_RELATIONAL_HPP


#include "lazy_core.hpp"


namespace lazy::detail{



/**
 * @brief Evaluation rules for relational comparisons.
 *
 * Extends `BinaryOpRules` to support `isNode` operands in the boolean path.  The
 * key entry point is `eval_bool(tag, a, b)` which:
 * 1. Evaluates any `Node` operands into scratch storage.
 * 2. Calls `get_bool(tag, raw_a, raw_b)` to perform the actual comparison.
 *
 * `get_bool` dispatches on the tag type at compile time to one of `==`, `!=`, `>`,
 * `<`, `>=`, `<=`.
 *
 * The `evaluate(tag, out, a, b)` overload used by the standard `eval_rule` path writes
 * the `bool` result into `out` (cast to `T`) via `get_bool`.
 *
 * @tparam T The arithmetic value type.
 */
template<typename T>
struct CompareRules : public BinaryOpRules<CompareRules<T>, T>{

    template<traits::isBoolTag tag, typename L, typename R>
    LAZY_FORCE_INLINE static void evaluate(tag, T& out, const L& a, const R& b){
        out = get_bool(tag{}, a, b);
    }

    template<traits::isBoolTag tag, typename L, typename R>
    inline static bool get_bool(tag, const L& a, const R& b){
        if constexpr (std::is_same_v<tag, EQ>) {
            return a == b;
        } else if constexpr (std::is_same_v<tag, NEQ>) {
            return a != b;
        } else if constexpr (std::is_same_v<tag, GT>) {
            return a > b;
        } else if constexpr (std::is_same_v<tag, LT>) {
            return a < b;
        } else if constexpr (std::is_same_v<tag, GE>) {
            return a >= b;
        } else if constexpr (std::is_same_v<tag, LE>) {
            return a <= b;
        } else {
            static_assert(traits::isBoolTag<tag>, "Invalid boolean tag");
            return false; // should never reach here
        }
    }


    template<traits::isBoolTag tag, typename L, typename R>
    LAZY_FORCE_INLINE static bool eval_bool(tag, const L& a, const R& b){
        T* worker = RuleTree<T, L, R>::worker;
         if constexpr (traits::isNode<L, T> && traits::isNode<R, T>) {
            // no matter what R is, since this operation has not been overriden,
            // we need to evaluate one of the branches (convention: the left one)
            return get_bool(tag{}, a.eval(worker[0]), b.eval(worker[1]));
        } else if constexpr (traits::isNode<R, T>) {
            return get_bool(tag{}, a.value(), b.eval(worker[1]));
        } else if constexpr (traits::isNode<L, T>) {
            return get_bool(tag{}, a.eval(worker[0]), b.value());
        } else {
            return get_bool(tag{}, a.value(), b.value());
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
 * it calls `Derived::eval_bool(tag{}, this->lhs, this->rhs)`, which internally
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
        return Derived::eval_bool(typename Derived::tag{}, this->lhs, this->rhs);
    }
};


/// @brief Lazy equal-to comparison node (`lhs == rhs`).  `operator bool()` returns the result.
template<typename T, typename L, typename R>
struct Eq : public Comparison<Eq<T, L, R>, T, L, R>{
    using Base = Comparison<Eq<T, L, R>, T, L, R>;
    using Base::Base;
    using tag = EQ;
};


/// @brief Lazy greater-than comparison node (`lhs > rhs`).  `operator bool()` returns the result.
template<typename T, typename L, typename R>
struct Gt : public Comparison<Gt<T, L, R>, T, L, R>{
    using Base = Comparison<Gt<T, L, R>, T, L, R>;
    using Base::Base;
    using tag = GT;
};

/// @brief Lazy less-than comparison node (`lhs < rhs`).  `operator bool()` returns the result.
template<typename T, typename L, typename R>
struct Lt : public Comparison<Lt<T, L, R>, T, L, R>{
    using Base = Comparison<Lt<T, L, R>, T, L, R>;
    using Base::Base;
    using tag = LT;
};

/// @brief Lazy not-equal comparison node (`lhs != rhs`).  `operator bool()` returns the result.
template<typename T, typename L, typename R>
struct Neq : public Comparison<Neq<T, L, R>, T, L, R>{
    using Base = Comparison<Neq<T, L, R>, T, L, R>;
    using Base::Base;
    using tag = NEQ;
};

/// @brief Lazy greater-or-equal comparison node (`lhs >= rhs`).  `operator bool()` returns the result.
template<typename T, typename L, typename R>
struct Ge : public Comparison<Ge<T, L, R>, T, L, R>{
    using Base = Comparison<Ge<T, L, R>, T, L, R>;
    using Base::Base;
    using tag = GE;
};

/// @brief Lazy less-or-equal comparison node (`lhs <= rhs`).  `operator bool()` returns the result.
template<typename T, typename L, typename R>
struct Le : public Comparison<Le<T, L, R>, T, L, R>{
    using Base = Comparison<Le<T, L, R>, T, L, R>;
    using Base::Base;
    using tag = LE;
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