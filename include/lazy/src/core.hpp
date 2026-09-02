#ifndef LAZY_CORE_HPP
#define LAZY_CORE_HPP

#include <array>
#include <algorithm>
#include <numeric>
#include <cassert>
#include "core_decls.hpp"
#include <vector>

namespace lazy{

namespace detail{

// ============================================================================
// ExprBase — root of the expression hierarchy
// ============================================================================

/**
 * @brief Non-CRTP root base for all expression types parameterised on value type `T`.
 *
 * Deriving from `ExprBase<T>` opts a type into the `isLazyExpr<D,T>` concept and provides
 * service types used throughout the library:
 *
 * - `lazy_value_type` — the underlying arithmetic type (carries `T` into derived classes via
 *   the nested alias).
 * - `expr_storage_t<E>` — determines how sub-expressions are stored inside composite
 *   nodes: raw `T` values are stored as `const T&` (reference to an existing object),
 *   while sub-expression nodes are stored by value (so the entire tree is embedded
 *   inline without heap allocation).
 *   concrete types override this with a more specific pattern.
 *
 * @tparam T  The underlying arithmetic value type (e.g. `double`, `mpfr::mpreal`).
 */
template<typename T>
struct ExprBase{

    using lazy_value_type = T;

    template<typename E>
    struct expr_storage {
        // Default: store by const reference
        using type = const T&;
    };

    template<traits::isLazyExpr<T> E>
    struct expr_storage<E> {
        // For sub-expressions: store by value
        using type = std::decay_t<E>;
    };

    template<typename E>
    using expr_storage_t = typename expr_storage<E>::type;

};


// ============================================================================
// Expr — CRTP intermediate base
// ============================================================================

template<typename Derived, typename T>
struct Expr : public ExprBase<T> {

    using Base = ExprBase<T>;
    using lazy_value_type = T;

};

// ============================================================================
// Atom — leaf expression nodes (already evaluated)
// ============================================================================

/**
 * @brief CRTP base for leaf nodes that hold a scalar value directly accessible via lazy::detail::get_value(atom).
 */
template<typename Derived, typename T>
struct Atom : public Expr<Derived, T>{

    using Base = Expr<Derived, T>;
    using lazy_value_type = T;
    using branch_t = std::tuple<>;
    static constexpr size_t MAX_DEPTH = 0;
    static constexpr size_t REQUIRED_TEMPORARIES = 0;

    LAZY_FORCE_INLINE constexpr bool contains_lazy_ref(const T& ref) const {
        // Derived classes might override this
        return false;
    }

};

// ============================================================================
// Node — unevaluated composite expression nodes
// ============================================================================

template<typename Derived, typename T, typename... Branches>
struct Node : public Expr<Derived, T>{

    static_assert(sizeof...(Branches) > 0, "Node must have at least one branch");
    static_assert((traits::isLazyExpr<Branches, T> && ...), "All branches must be lazy expressions");

    using Base = Expr<Derived, T>;
    using branch_t = std::tuple<Branches...>;

    static constexpr size_t branch_count = sizeof...(Branches);
    static constexpr size_t MAX_DEPTH = 1 + []<size_t... I>(std::index_sequence<I...>){
        return std::max({size_t{0}, std::tuple_element_t<I, typename Derived::branch_t>::MAX_DEPTH...});
    }(std::make_index_sequence<branch_count>{});

    LAZY_FORCE_INLINE constexpr bool contains_lazy_ref(const T& ref) const {
        return [&]<size_t... I>(std::index_sequence<I...>){
            return ((LAZY_THIS->template get<I>().contains_lazy_ref(ref)) || ...);
        }(std::make_index_sequence<branch_count>{});
    }

    static constexpr auto sort_branches_by_required_workers(){
        std::array<std::size_t, sizeof...(Branches)> indices{};
        std::array<std::size_t, sizeof...(Branches)> nums{ Branches::REQUIRED_TEMPORARIES... };

        std::iota(indices.begin(), indices.end(), 0);

        std::sort(indices.begin(), indices.end(),
            [&nums](std::size_t a, std::size_t b) {
                return nums[a] > nums[b];
            });
        return indices;
    }

    static constexpr std::array<size_t, branch_count> sorted_temporaries(){
        std::array<std::size_t, branch_count> nums = { Branches::REQUIRED_TEMPORARIES... };
        std::sort(nums.begin(), nums.end(), std::greater<size_t>());
        return nums;
    }

    static constexpr std::array<bool, branch_count> sorted_booleans(){
        std::array<std::size_t, branch_count> indices{};
        std::array<std::size_t, branch_count> nums = { Branches::REQUIRED_TEMPORARIES... };
        std::array<bool, branch_count> is_node = { lazy::traits::isNode<Branches, T>... };

        std::iota(indices.begin(), indices.end(), 0);
        std::sort(indices.begin(), indices.end(),
            [&nums](std::size_t a, std::size_t b) {
                return nums[a] > nums[b];
            });

        std::array<bool, branch_count> result{};
        for (size_t i = 0; i < branch_count; i++) {
            result[i] = is_node[indices[i]];
        }
        return result;
    }

    static constexpr size_t REQUIRED_TEMPORARIES = []<size_t... I>(std::index_sequence<I...>){
        std::array<size_t, branch_count> nums = sorted_temporaries();
        std::array<bool, branch_count> is_node = sorted_booleans();
        size_t node_index = 0;
        size_t res = 0;
        for (size_t i = 0; i < branch_count; i++) {
            if (is_node[i]) {
                res = std::max(res, node_index + nums[i]);
                node_index++;
            }
        }
        return res;
    }(std::make_index_sequence<branch_count>{});

    LAZY_FORCE_INLINE T& eval(T& out) const {
        assert(!contains_lazy_ref(out) && "Not safe to evaluate a lazy expression into a reference that is contained in the expression tree. This is possibly caused by calling some_lazy_expression.eval(some_variable), where some_variable is part of the expression tree.");
        Pool<T> workers = reserve_workers<false>();
        return LAZY_THIS->eval_impl(out, workers);
    }


    // TODO Should optimize in case the evaluation is not a T, but e.g. a boolean.
    LAZY_FORCE_INLINE T& eval_worker() const {
        Pool<T> workers = reserve_workers<true>();
        T& out = workers.consume();
        return LAZY_THIS->eval_impl(out, workers);
    }

    LAZY_FORCE_INLINE T& eval_impl(T& out, Pool<T> workers) const {
        return make_eval_impl(out, workers, std::make_index_sequence<branch_count>{});
    }

    template<size_t I>
    inline const auto& get() const {
        return std::get<I>(branches);
    }

    template<size_t I>
    inline auto& get() {
        return std::get<I>(branches);
    }

    template<typename... F>
    requires (std::is_constructible_v<F, Branches&&> && ...)
    LAZY_FORCE_INLINE Node(F&&... f) : branches(std::forward<F>(f)...) {}

protected:

    template<bool reserve_output = false>
    inline static Pool<T> reserve_workers() {
        constexpr size_t required_count =
            Derived::REQUIRED_TEMPORARIES
            + (reserve_output ? 1 : 0)
            + required_workers<T>;

        if (required_count > LazyType<T>::workers.size()){
            LazyType<T>::workers.resize(required_count);
        }
        std::vector<T>& workers = LazyType<T>::workers;
        return Pool<T>(workers.data(), workers.size());
    }

private:

    template<size_t... I>
    LAZY_FORCE_INLINE T& make_eval_impl(T& out, Pool<T> workers, std::index_sequence<I...>) const {
        Derived::eval_rule(typename Derived::tag{}, out, workers, make_expr<T>(this->get<I>())...);
        return out;
    }

    std::tuple<Branches...> branches;

};


/**
 * @brief Atom that wraps a `T` value by const reference.
 *
 * `RefType<T>` is a lightweight non-owning view.  It is produced automatically by
 * `make_expr<T>()` when a `LazyType<T>` lvalue is used as an operand, so that the
 * expression tree references the original variable without copying it.  The
 * referenced value must not be modified or destroyed while the node is alive.
 *
 * between a reference and a direct value.
 *
 * @tparam T The arithmetic value type.
 */
template<typename T>
struct RefType : public Atom<RefType<T>, T>{
    using Base = Atom<RefType<T>, T>;

    LAZY_FORCE_INLINE RefType(const T& v) : value(v) {}

    LAZY_FORCE_INLINE constexpr bool contains_lazy_ref(const T& ref) const {
        return &ref == &value;
    }

    Base::template expr_storage_t<T> value;

};


/**
 * @brief Atom that wraps a value of a *different* scalar type `Type` inside a
 *        `T`-typed expression tree.
 *
 * Used when a raw scalar (e.g. `int` or `double`) is mixed into an expression whose
 * `lazy_value_type` is a different type (e.g. `mpfr::mpreal`).  `make_expr<T>()` produces
 * an `OtherType<T, S>` when `S != T` and `S` is not already an expression.  The
 * `value` data member is then passed to
 * the `evaluate(tag, out, ..., scalar)` overloads in `CustomBinaryEvaluator<T>`.
 *
 * @tparam T    The arithmetic value type of the surrounding expression tree.
 * @tparam Type The actual scalar type of the stored value (e.g. `int`, `double`).
 */
template<typename T, typename Type>
struct OtherType : public Atom<OtherType<T, Type>, T>{

    static_assert(traits::isConvertibleTo<Type, T>, "Type must be convertible to T");

    using Base = Atom<OtherType<T, Type>, T>;
    
    LAZY_FORCE_INLINE OtherType(const Type& v) : value(v) {}

    Type value;

};


/**
 * @brief Canonicalise any value into an expression node of value type `T`.
 *
 * This function is the central entry-point used by all operator overloads and
 * `BinaryOperator::eval()` to convert heterogeneous operands to a uniform expression
 * type.  The conversion rules are:
 *
 * | Input type                  | Output type                              |
 * |-----------------------------|------------------------------------------|
 * | `LazyType<T>` lvalue        | `RefType<T>(get_value(value))`           |
 * | `LazyType<T>` rvalue        | `LazyType<T>(std::move(value))`          |
 * | Any other `isLazyExpr<T>`   | Forwarded as-is (no copy)            |
 * | Exactly `T` lvalue          | `RefType<T>(value)` (reference to `T`)   |
 * | Exactly `T` rvalue          | `LazyType<T>(value)` (takes ownership)   |
 * | Any other scalar `S`        | `OtherType<T, S>(value)`                 |
 *
 * @tparam T     The target arithmetic value type for the expression tree.
 * @tparam R     The type of the incoming value (deduced).
 * @param  value The value to wrap.  For lvalue expressions, passed by lvalue ref;
 *               for rvalue nodes/scalars, passed by rvalue ref.
 * @return       An expression node of the appropriate type (see table).
 */
template<typename T, typename R>
LAZY_FORCE_INLINE decltype(auto) make_expr(R&& value){

    static_assert(lazy::traits::isValidType<R, T>, "Invalid type for make_expr");

    if constexpr (lazy::traits::isLazy<R, T> && std::is_lvalue_reference_v<R>) {
        return RefType<T>(get_value(std::forward<R>(value)));
    } else if constexpr (std::is_same_v<T, std::decay_t<R>> && std::is_lvalue_reference_v<R>) {
        return RefType<T>(value);
    } else if constexpr (lazy::traits::isLazy<R, T> || std::is_same_v<T, std::decay_t<R>>) {
        return LazyType<T>(std::forward<R>(value));
    } else if constexpr (lazy::traits::isLazyExpr<std::decay_t<R>, T>) {
        return std::forward<R>(value);
    } else {
        return OtherType<T, std::decay_t<R>>(std::forward<R>(value));
    }
}


template<typename F>
LAZY_FORCE_INLINE const auto& get_value(F&& value){
    static_assert(::lazy::traits::isAnyLazyExpr<F>, "F must be a lazy expression");
    using T = typename std::decay_t<F>::lazy_value_type;
    static_assert(::lazy::traits::isAtom<F, T>, "F must be an Atom");
    if constexpr (::lazy::traits::isLazy<F, T>){
        return static_cast<const T&>(value);
    } else {
        return value.value;
    }
}

/*
This is a non-owning memory view of a contiguous block of memory containing objects of type T. It contains information on how many objects are in the block and provides access to the underlying memory,
with assertions to ensure safe access.
*/
template<typename T>
class Pool {
public:
    Pool(T* workers, size_t size) : workers_(workers), size_(size) {}

    [[nodiscard]]
    inline T& consume() {
        assert(size_ > 0 && "Cannot consume from an empty pool");
        size_--;
        return *(workers_++);
    }

    inline const T& operator[](size_t i) const{
        assert(i <= size_ && "Index out of bounds");
        return workers_[i];
    }

    inline size_t size() const {
        return size_;
    }
    
private:
    T* workers_;
    size_t size_;
};


} // namespace detail


using detail::LazyType;
using detail::required_workers;

} // namespace lazy

#endif // LAZY_IMPL_HPP