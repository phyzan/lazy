#ifndef LAZY_LAZYTYPE_HPP
#define LAZY_LAZYTYPE_HPP


#include "binary_ops/binop_decls.hpp"

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
 * - Implicit conversion `operator T()` returning `value_` by copy.
 * - Thread-safety for *reading* the scratch pointer registry via `for_each_worker`.
 *
 * **Note:** `make_expr<T>()` converts an lvalue `LazyType<T>` to `RefType<T>` so
 * that expression trees reference the original variable; an rvalue `LazyType<T>` is
 * moved into a new `LazyType<T>` node.
 *
 * @tparam T The arithmetic value type.
 */
template<typename T>
struct LazyType : public detail::Atom<LazyType<T>, T>{
    
    using Base = detail::Atom<LazyType<T>, T>;
    
    // Main constructors and assignment operators
    LazyType() = default;
    LazyType(const LazyType&) = default;
    LazyType(LazyType&&) noexcept = default;
    LazyType& operator=(const LazyType&) = default;
    LazyType& operator=(LazyType&&) noexcept = default;
    ~LazyType() = default;

    // Construct from lazy expressions
    template<typename U>
    requires (!traits::isLazyExpr<U, T>)
    LazyType(U&& value) : value_(std::forward<U>(value)) {}

    template<typename U>
    requires (traits::isAtom<U, T> && !::lazy::traits::isLazy<U, T>)
    LazyType(U&& value) : value_(value.value()) {}

    template<typename U>
    requires (traits::isNode<U, T>)
    LazyType(U&& node) {
        node.eval(value_);
    }

    // Construct by forwarding values
    template<typename... U>
    requires (!traits::isLazyExpr<U, T> && ...)
    LazyType(U&&... value) : value_(std::forward<U>(value)...) {}

    // Assignment operators
    template<traits::isNode<T> U>
    LAZY_FORCE_INLINE
    LazyType& operator=(U&& node){
        /*
        Do NOT do node.eval(value_) here, because if the node contains a reference to this LazyType,
        it will lead to invalid results, as the NodeEvaluator assumes `out` is separate memory.
        See eval_rule_impl for details. TODO: try to allow `out` to be one of the branches, without using more temporaries.
        */
        value_ = node.eval_worker();
        return *this;
    }

    template<traits::isAtom<T> U>
    LAZY_FORCE_INLINE
    LazyType& operator=(U&& other) requires (!lazy::traits::isLazy<U, T>){
        value_ = other.value();
        return *this;
    }

    template<typename U>
    LAZY_FORCE_INLINE
    LazyType& operator=(U&& other) requires (!traits::isLazyExpr<U, T>){
        value_ = std::forward<U>(other);
        return *this;
    }

    // Compound assignment operators
    template<typename U>
    LAZY_FORCE_INLINE
    LazyType& operator+=(U&& other){
        if constexpr (traits::isNode<U, T>){
            value_ += other.eval_worker();
        } else if constexpr (traits::isAtom<U, T>){
            detail::CustomBinaryEvaluator<T>::evaluate(lazy::tags::PLUS{}, value_, value_, other.value());
        } else {
            detail::CustomBinaryEvaluator<T>::evaluate(lazy::tags::PLUS{}, value_, value_, other);
        }
        return *this;
    }

    template<typename U>
    LAZY_FORCE_INLINE
    LazyType& operator*=(U&& other){
        if constexpr (traits::isNode<U, T>){
            value_ *= other.eval_worker();
        } else if constexpr (traits::isAtom<U, T>){
            detail::CustomBinaryEvaluator<T>::evaluate(lazy::tags::MUL{}, value_, value_, other.value());
        } else {
            detail::CustomBinaryEvaluator<T>::evaluate(lazy::tags::MUL{}, value_, value_, other);
        }
        return *this;
    }

    template<typename U>
    LAZY_FORCE_INLINE
    LazyType& operator-=(U&& other){
        if constexpr (traits::isNode<U, T>){
            value_ -= other.eval_worker();
        } else if constexpr (traits::isAtom<U, T>){
            detail::CustomBinaryEvaluator<T>::evaluate(lazy::tags::MINUS{}, value_, value_, other.value());
        } else {
            detail::CustomBinaryEvaluator<T>::evaluate(lazy::tags::MINUS{}, value_, value_, other);
        }
        return *this;
    }

    template<typename U>
    LAZY_FORCE_INLINE
    LazyType& operator/=(U&& other){
        if constexpr (traits::isNode<U, T>){
            value_ /= other.eval_worker();
        } else if constexpr (traits::isAtom<U, T>){
            detail::CustomBinaryEvaluator<T>::evaluate(lazy::tags::DIV{}, value_, value_, other.value());
        } else {
            detail::CustomBinaryEvaluator<T>::evaluate(lazy::tags::DIV{}, value_, value_, other);
        }
        return *this;
    }

    operator T() const { return value_; }

    LAZY_FORCE_INLINE const T& value() const {return value_;}

    LAZY_FORCE_INLINE T& value() {return value_;}

    template<typename F>
    inline static void for_each_worker(F&& fn) {
        for (T& p : workers) {fn(p);};
    }

    inline static thread_local std::vector<T> workers;

private:

    LAZY_FORCE_INLINE RefType<T> ref() const {return RefType<T>(value_);}
    T value_;

    template<typename A, typename B>
    friend struct NodalEvaluator;


};

} // namespace lazy::detail

#endif // LAZY_LAZYTYPE_HPP