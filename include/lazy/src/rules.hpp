#ifndef LAZY_RULES_HPP
#define LAZY_RULES_HPP

#include "core_decls.hpp"
#include <tuple>
#include <utility>
#include <vector>

namespace lazy::detail{


//=============================== Rules ==============================================

/**
 * @brief Global registry of `T*` scratch pointers used by `RuleTree`.
 *
 * Each `RuleTree<T, node_t, branch_t...>` specialisation registers its thread-local
 * temporary `T` objects into this list so that external code can iterate over all
 * allocated temporaries (e.g. for bulk operations that need to touch all active
 * scratch buffers).
 *
 * @tparam T The arithmetic value type.
 */
template<typename T>
struct Rules{

    inline static thread_local std::vector<T*> aux_workers;

    template<typename Func>
    LAZY_FORCE_INLINE static void for_each_aux(Func&& fn) {
        for (T* p : aux_workers) {fn(*p);};
    }
};


/**
 * @brief Per-(type, operand-pattern) thread-local temporary storage pool.
 *
 * For each unique combination of value type `T` and branch expression types
 * `Branch...`, exactly `sizeof...(Branch)` `T` objects are allocated as
 * thread-local storage.  These serve as scratch buffers for intermediate results
 * during recursive `eval_rule` calls, avoiding any heap allocation on the hot path.
 *
 * The thread-local array is initialised lazily on first use; each element is
 * registered in `Rules<T>::aux_workers` so that external code can find all scratch
 * buffers of type `T`.
 *
 * @tparam T        The arithmetic value type.
 * @tparam branch_t The expression types of the sub-expressions for which scratch is
 *                  needed.  Must satisfy `isLazyExpr<branch_t, T>` for each element.
 */
template<typename T, traits::isLazyExpr<T> node_t, traits::isLazyExpr<T>... branch>
struct RuleTree : public Rules<T>{

    static constexpr size_t Nb = sizeof...(branch);
    static_assert(sizeof...(branch) > 0, "Branches must be greater than 0");
    static_assert(
    std::is_same_v<typename node_t::branch_t, std::tuple<branch...>>,
    "node_t must have the same branch types as branch...");// we could retrieve the branch types from node_t, but this works for now, so we add this sanity check to make sure that the branch types are consistent with the node type

    using Base = Rules<T>;

    inline static thread_local T* worker = [](){
        static thread_local std::array<T, Nb> arr;  // Allocate contiguous array
        for (T& x: arr){
            Base::aux_workers.push_back(&x);  // Register each element in aux_workers
        }
        return arr.data();
    }();

};


template<typename Derived, typename T>
struct NodalOperatorRules{

    template<lazy::traits::isTag tag, typename... Arg>
    inline static void evaluate(tag, T& out, const Arg&... args){
        Derived::evaluate(tag{}, out, args...);
    }

    template<lazy::traits::isTag tag, lazy::traits::isLazyExpr<T>... Branch>
    LAZY_FORCE_INLINE static void eval_rule(tag, T& out, const Branch&... branch){
        return eval_rule_impl(std::make_index_sequence<sizeof...(Branch)>{}, tag{}, out, branch...);
    }

private:

    template<lazy::traits::isTag tag, size_t... I, lazy::traits::isLazyExpr<T>... Branch>
    LAZY_FORCE_INLINE static void eval_rule_impl(std::index_sequence<I...>, tag, T& out, const Branch&... branch){
        // IMPORTANT: out may be the same memory location as at least one of the branches
        static_assert(((not lazy::traits::isLazy<Branch, T>) && ...), "LazyType should not be passed to eval_rule, use RefType instead");
        using node_t = lazy::detail::TypeGetter<T, tag, Branch...>::type;
        static_assert(not std::is_void_v<node_t>, "Invalid node type");
        T* worker = RuleTree<T, node_t, Branch...>::worker;
        Derived::evaluate(tag{}, out, get_scalar(branch, worker[I])...);
    }

    template<lazy::traits::isLazyExpr<T> branch_t>
    LAZY_FORCE_INLINE static const auto& get_scalar(const branch_t& lazy_expr, T& worker){
        if constexpr (lazy::traits::isNode<branch_t, T>){
            return lazy_expr.eval(worker);
        } else {
            return lazy_expr.value();
        }
    }
};


} // namespace lazy::detail

#endif // LAZY_RULES_HPP