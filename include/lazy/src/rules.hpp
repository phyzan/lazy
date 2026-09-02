#ifndef LAZY_RULES_HPP
#define LAZY_RULES_HPP

#include "core_decls.hpp"
#include <tuple>
#include <utility>
#include <vector>
#include <algorithm>
#include <cassert>


namespace lazy::detail{


template<std::size_t I, typename FirstType, typename... ArgType>
LAZY_FORCE_INLINE constexpr decltype(auto) helper_pack_elem(FirstType&& x0, ArgType&&... x) {
    if constexpr (I == 0) {
        return std::forward<FirstType>(x0);
    } else {
        static_assert(sizeof...(x) > 0, "Index out of bounds");
        return helper_pack_elem<I - 1>(std::forward<ArgType>(x)...);
    }
}

template<std::size_t I, typename... Args>
LAZY_FORCE_INLINE constexpr decltype(auto) pack_elem(Args&&... args) {
    return helper_pack_elem<I>(std::forward<Args>(args)...);
}

template<typename Derived, typename T>
struct NodalEvaluator{

    template<lazy::traits::isTag tag, typename... Arg>
    LAZY_FORCE_INLINE static void evaluate(tag, T& out, Pool<T> workers, const Arg&... args){
        Derived::evaluate(tag{}, out, workers, args...);
    }

    template<lazy::traits::isTag tag, lazy::traits::isLazyExpr<T>... Branch>
    LAZY_FORCE_INLINE static void eval_rule(tag, T& out, Pool<T> worker, const Branch&... branch){
        // IMPORTANT: out must NOT be the same memory location as at least one of the branches.
        // See eval_rule_impl for details. TODO: try to allow `out` to be one of the branches, without using more temporaries.
        return eval_rule_impl(std::make_index_sequence<sizeof...(Branch)>{}, tag{}, out, worker, branch...);
    }

private:

    LAZY_FORCE_INLINE
    static constexpr void sorted_evaluator(T& out, Pool<T> worker){}

    template<typename F, typename... Rest>
    LAZY_FORCE_INLINE
    static constexpr void sorted_evaluator(T& out, Pool<T> workers, const F& f, const Rest&... branch){
        // now branches are sorted by the number of required temporaries, with the largest first
        // First node evaluates into 'out', subsequent nodes into worker[0], worker[1], etc.
        if constexpr (lazy::traits::isNode<F, T>) {
            f.eval_impl(out, workers);
            constexpr bool more_nodes = (lazy::traits::isNode<Rest, T> || ... || false);
            if constexpr (more_nodes){
                T& out = workers.consume();
                sorted_evaluator(out, workers, branch...);
                return;
            }
        }
        sorted_evaluator(out, workers, branch...);
    }

    // Helper to get the evaluated argument for branch at original index OrigIdx
    template<size_t OrigIdx, typename node_t, lazy::traits::isLazyExpr<T>... Branch>
    LAZY_FORCE_INLINE static const auto& get_evaluated_arg(T& out, Pool<T> workers, const Branch&... branch) {
        const auto& br = pack_elem<OrigIdx>(branch...);
        using branch_t = std::decay_t<decltype(br)>;

        if constexpr (lazy::traits::isAtom<branch_t, T>) {
            return get_value(br);
        } else {
            // Find which node slot this branch was evaluated into
            constexpr std::array<bool, sizeof...(Branch)> is_node_arr = {
                lazy::traits::isNode<Branch, T>...
            };

            // Count how many nodes come before this one in sorted order
            constexpr int slot = [&](){
                constexpr auto sorted_idx = node_t::sort_branches_by_required_workers();
                int node_count = 0;
                for (size_t i = 0; i < sizeof...(Branch); ++i) {
                    size_t orig = sorted_idx[i];
                    if (orig == OrigIdx) { return node_count; }
                    if (is_node_arr[orig]) { node_count++; }
                }
                return -1;
            }();

            static_assert(slot >= 0, "Node not found in sorted order");

            if constexpr (slot == 0) {
                return out;
            } else {
                return workers[slot - 1];
            }
        }
    }

    template<lazy::traits::isTag tag, size_t... I, lazy::traits::isLazyExpr<T>... Branch>
    LAZY_FORCE_INLINE static void eval_rule_impl(std::index_sequence<I...>, tag, T& out, Pool<T> workers, const Branch&... branch){
        // IMPORTANT: out must NOT be the same memory location as at least one of the branches: if `out` is contained as a RefType in one of the branches, it will lead to invalid results. The user must ensure that `out` is a separate memory location.
        static_assert(((not lazy::traits::isLazy<Branch, T>) && ...), "LazyType should not be passed to eval_rule, use RefType instead");
        using node_t = lazy::detail::TypeGetter<T, tag, Branch...>::type;
        static_assert(not std::is_void_v<node_t>, "Invalid node type");

        constexpr auto sorted_idx = node_t::sort_branches_by_required_workers();
        sorted_evaluator(out, workers, pack_elem<sorted_idx[I]>(branch...)...);

        // Call evaluate with arguments in original order
        Derived::evaluate(tag{}, out, workers, get_evaluated_arg<I, node_t, Branch...>(out, workers, branch...)...);
    }
};


} // namespace lazy::detail

#endif // LAZY_RULES_HPP