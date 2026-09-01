#ifndef LAZY_LAZYTYPE_HPP
#define LAZY_LAZYTYPE_HPP


#include "binary_ops/binop_decls.hpp"


#define LAZY_REF static_cast<lazy::detail::copy_const_t<std::remove_reference_t<decltype(*this)>, T>*>(this)

namespace lazy::detail{

/**
 * @brief Owning lazy variable — the primary user-facing storage type.
 *
 * `LazyType<T>` is the type users declare variables with.  It is an `Atom` so it
 * can be used directly in expressions without evaluation, and it supports:
 *
 * - Construction from raw `T`, from other atoms, and from `Node` expressions
 *   (which evaluates them immediately via `eval()`).
 * - Compound assignment (`+=`, `-=`, `*=`, `/=`) that update `value_` in place
 *   using `CustomBinaryEvaluator<T>`, correctly handling both atom and node operands
 *   without extra temporaries.
 * - Thread-safety for *reading* the scratch pointer registry via `for_each_worker`.
 *
 * **Note:** `make_expr<T>()` converts an lvalue `LazyType<T>` to `RefType<T>` so
 * that expression trees reference the original variable; an rvalue `LazyType<T>` is
 * moved into a new `LazyType<T>` node.
 *
 * @tparam T The arithmetic value type.
 */
template<typename T>
struct LazyType : public detail::Atom<LazyType<T>, T>, public T{
    
    using Base = detail::Atom<LazyType<T>, T>;
    
    // Main constructors and assignment operators
    LazyType() = default;
    LazyType(const LazyType&) = default;
    LazyType(LazyType&&) noexcept = default;
    LazyType& operator=(const LazyType&) = default;
    LazyType& operator=(LazyType&&) noexcept = default;
    ~LazyType() = default;

    using T::T; // inherit constructors from T
    LazyType(const T& value) : T(value) {}
    LazyType(T&& value) noexcept : T(std::move(value)) {}

    // Construct from lazy expressions
    template<::lazy::traits::isAtom<T> U>
    requires (!::lazy::traits::isLazy<U, T>)
    LazyType(U&& value) : T(value.value()) {}

    template<::lazy::traits::isNode<T> NodeType>
    LazyType(const NodeType& node) {
        node.eval(*LAZY_REF);
    }

    // Assignment operators
    template<::lazy::traits::isNode<T> NodeType>
    LAZY_FORCE_INLINE
    LazyType& operator=(const NodeType& node){
        /*
        Do NOT do node.eval(value_) here, because if the node contains a reference to this LazyType,
        it will lead to invalid results, as the NodeEvaluator assumes `out` is separate memory.
        See eval_rule_impl for details. TODO: try to allow `out` to be one of the branches, without using more temporaries.
        */
        T::operator=(node.eval_worker());
        return *this;
    }

    template<::lazy::traits::isAtom<T> U>
    LAZY_FORCE_INLINE
    LazyType& operator=(U&& other) requires (!::lazy::traits::isLazy<U, T>){
        T::operator=(other.value);
        return *this;
    }

    template<typename U>
    LAZY_FORCE_INLINE
    LazyType& operator=(U&& other) requires (!::lazy::traits::isLazyExpr<U, T>){
        T::operator=(std::forward<U>(other));
        return *this;
    }

    // Compound assignment operators
    template<typename U>
    LAZY_FORCE_INLINE
    LazyType& operator+=(U&& other){
        if constexpr (::lazy::traits::isNode<U, T>){
            *LAZY_REF += other.eval_worker();
        } else if constexpr (::lazy::traits::isAtom<U, T>){
            *LAZY_REF += get_value(other);
        } else {
            *LAZY_REF += other;
        }
        return *this;
    }

    template<typename U>
    LAZY_FORCE_INLINE
    LazyType& operator*=(U&& other){
        if constexpr (::lazy::traits::isNode<U, T>){
            *LAZY_REF *= other.eval_worker();
        } else if constexpr (::lazy::traits::isAtom<U, T>){
            *LAZY_REF *= get_value(other);
        } else {
            *LAZY_REF *= other;
        }
        return *this;
    }

    template<typename U>
    LAZY_FORCE_INLINE
    LazyType& operator-=(U&& other){
        if constexpr (::lazy::traits::isNode<U, T>){
            *LAZY_REF -= other.eval_worker();
        } else if constexpr (::lazy::traits::isAtom<U, T>){
            *LAZY_REF -= get_value(other);
        } else {
            *LAZY_REF -= other;
        }
        return *this;
    }

    template<typename U>
    LAZY_FORCE_INLINE
    LazyType& operator/=(U&& other){
        if constexpr (::lazy::traits::isNode<U, T>){
            *LAZY_REF /= other.eval_worker();
        } else if constexpr (::lazy::traits::isAtom<U, T>){
            *LAZY_REF /= get_value(other);
        } else {
            *LAZY_REF /= other;
        }
        return *this;
    }

    template<typename F>
    inline static void for_each_worker(F&& fn) {
        for (T& p : workers) {fn(p);};
    }

    inline static thread_local std::vector<T> workers{required_workers<T>};

private:

    template<typename A, typename B>
    friend struct NodalEvaluator;


};

} // namespace lazy::detail


#undef LAZY_REF

#endif // LAZY_LAZYTYPE_HPP