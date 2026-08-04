#ifndef LAZY_RULES_HPP
#define LAZY_RULES_HPP

#include "core_decls.hpp"
#include <tuple>
#include <utility>
#include <vector>

namespace lazy::detail{

template<typename Derived, typename T>
struct NodalEvaluator{

    template<lazy::traits::isTag tag, typename... Arg>
    LAZY_FORCE_INLINE static void evaluate(tag, auto& out, const Arg&... args){
        Derived::evaluate(tag{}, out, args...);
    }

    template<lazy::traits::isTag tag, lazy::traits::isLazyExpr<T>... Branch>
    LAZY_FORCE_INLINE static void eval_rule(tag, auto& out, const Branch&... branch){
        return eval_rule_impl(std::make_index_sequence<sizeof...(Branch)>{}, tag{}, out, branch...);
    }

private:

    template<lazy::traits::isTag tag, size_t... I, lazy::traits::isLazyExpr<T>... Branch>
    LAZY_FORCE_INLINE static void eval_rule_impl(std::index_sequence<I...>, tag, auto& out, const Branch&... branch){
        // IMPORTANT: out may be the same memory location as at least one of the branches
        static_assert(((not lazy::traits::isLazy<Branch, T>) && ...), "LazyType should not be passed to eval_rule, use RefType instead");
        using node_t = lazy::detail::TypeGetter<T, tag, Branch...>::type;
        static_assert(not std::is_void_v<node_t>, "Invalid node type");

        constexpr size_t required_workers = (lazy::traits::isNode<Branch, T> + ...);
        static thread_local T* worker = [](){
            static thread_local std::array<T, required_workers> arr;  // Allocate contiguous array
            for (T& x: arr){
                LazyType<T>::workers.push_back(&x);  // Register each element in workers
            }
            return arr.data();
        }();

        // get_scalar advances the worker pointer for each branch that is a Node
        T* worker_ref = worker;
        Derived::evaluate(tag{}, out, get_scalar(branch, worker_ref)...);
    }

    template<lazy::traits::isLazyExpr<T> branch_t>
    LAZY_FORCE_INLINE static const auto& get_scalar(const branch_t& lazy_expr, T*& worker_ref){
        // might modify the worker_ref pointer
        if constexpr (lazy::traits::isNode<branch_t, T>){
            return lazy_expr.eval(*(worker_ref++));
        } else {
            return lazy_expr.value();
        }
    }
};


} // namespace lazy::detail

#endif // LAZY_RULES_HPP